/* Host-side unit tests for the button controller (source/Controller.c).
 * Build and run with tests/run_tests.ps1. No framework: each test is a plain function,
 * CHECK_* macros record failures, and main() runs everything and prints a summary.
*/
#include <stdio.h>
#include <string.h>
#include "Controller.h"

/////////////////////
// SECTION: HARNESS //
/////////////////////

static int checks_failed;
static int tests_run;
static int tests_failed;

static const char *action_name(game_action a)
{
    switch (a)
    {
    case INCREMENT_CHANNEL_VALUE: return "INCREMENT_CHANNEL_VALUE";
    case DECREMENT_CHANNEL_VALUE: return "DECREMENT_CHANNEL_VALUE";
    case START_CONT_INC_CHANNEL_VALUE: return "START_CONT_INC_CHANNEL_VALUE";
    case STOP_CONT_INC_CHANNEL_VALUE: return "STOP_CONT_INC_CHANNEL_VALUE";
    case START_CONT_DEC_CHANNEL_VALUE: return "START_CONT_DEC_CHANNEL_VALUE";
    case STOP_CONT_DEC_CHANNEL_VALUE: return "STOP_CONT_DEC_CHANNEL_VALUE";
    case NEXT_COLOR_CHANNEL: return "NEXT_COLOR_CHANNEL";
    case CONFIRM_GUESS: return "CONFIRM_GUESS";
    case START_NEW_GAME: return "START_NEW_GAME";
    case INDICATE_HOLD_THRESHOLD_PASSED: return "INDICATE_HOLD_THRESHOLD_PASSED";
    case NO_OP: return "NO_OP";
    default: return "<not a game_action>";
    }
}

static const char *threshold_name(threshold t)
{
    switch (t)
    {
    case SHORT: return "SHORT";
    case MEDIUM: return "MEDIUM";
    case LONG: return "LONG";
    case NONE: return "NONE";
    default: return "<not a threshold>";
    }
}

static const char *trigger_name(enum threshold_trigger t)
{
    switch (t)
    {
    case TT_UP_BUTTON: return "TT_UP_BUTTON";
    case TT_DOWN_BUTTON: return "TT_DOWN_BUTTON";
    case TT_MAX_DELAY: return "TT_MAX_DELAY";
    default: return "<not a threshold_trigger>";
    }
}

#define FAIL_LINE() printf("    line %d: ", __LINE__)

#define CHECK_ACTION(expected, actual) do { \
        game_action e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s (%d)\n", #actual, action_name(e_), action_name(a_), (int)a_); checks_failed++; } \
    } while (0)

#define CHECK_THRESHOLD(expected, actual) do { \
        threshold e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s (%d)\n", #actual, threshold_name(e_), threshold_name(a_), (int)a_); checks_failed++; } \
    } while (0)

#define CHECK_U64(expected, actual) do { \
        uint64_t e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %llu, got %llu\n", #actual, (unsigned long long)e_, (unsigned long long)a_); checks_failed++; } \
    } while (0)

#define CHECK_STR(expected, actual) do { \
        const char *e_ = (expected), *a_ = (actual); \
        if (strcmp(e_, a_) != 0) { FAIL_LINE(); printf("%s:\n      expected \"%s\"\n      got      \"%s\"\n", #actual, e_, a_); checks_failed++; } \
    } while (0)

#define CHECK_TRUE(cond) do { \
        if (!(cond)) { FAIL_LINE(); printf("expected true: %s\n", #cond); checks_failed++; } \
    } while (0)

#define RUN_TEST(test) do { \
        checks_failed = 0; \
        test(); \
        tests_run++; \
        if (checks_failed) { tests_failed++; printf("FAIL  %s\n\n", #test); } \
        else { printf("ok    %s\n", #test); } \
    } while (0)

//////////////////////////////////
// SECTION: CONTROLLER SIMULATOR //
//////////////////////////////////

/* Drives the controller the same way button_controller_task does: button events go to
 * button_event_controller_handler, and between events the task sleeps until the next
 * threshold deadline and calls threshold_event_controller_handler when it wakes.
 * Every non-NO_OP action from a threshold wake is recorded in the log.
 *
 * Times are in ms after T0. T0 is non-zero so bugs that assume "time 0" can't hide.
*/
#define T0_US 10000000ULL
#define AT_MS(ms) (T0_US + (uint64_t)(ms) * 1000ULL)

// When the deadline rounds down to 0 ticks, xQueueReceive returns right away, but some time still passes.
#define SIM_ZERO_TICK_WAKE_US 50

#define LOG_CAPACITY 32

struct logged_action
{
    game_action action;
    enum threshold_trigger trigger;
    uint64_t at_us;
};

struct sim
{
    struct controller c;
    uint64_t now;
    int log_count;
    int log_dropped;
    struct logged_action log[LOG_CAPACITY];
};

static void sim_init(struct sim *s)
{
    controller_init(&s->c);
    s->now = T0_US;
    s->log_count = 0;
    s->log_dropped = 0;
}

static void sim_idle_until(struct sim *s, uint64_t at_ms)
{
    uint64_t target = AT_MS(at_ms);
    for (int wakes = 0; wakes < 10000; wakes++)
    {
        uint64_t wait_us = controller_us_until_next_threshold(&s->c, s->now);
        if (wait_us == ABSOLUTE_TIME_MAX)
        {
            s->now = target;
            return;
        }
        // pdMS_TO_TICKS(us / 1000) truncates to whole 1 ms ticks, so the task can wake slightly early.
        uint64_t step = (wait_us / 1000) * 1000;
        if (step == 0)
        {
            step = SIM_ZERO_TICK_WAKE_US;
        }
        if (s->now + step > target)
        {
            s->now = target;
            return;
        }
        s->now += step;
        struct threshold_action_and_trigger r = threshold_event_controller_handler(&s->c, s->now);
        if (r.action != NO_OP)
        {
            if (s->log_count < LOG_CAPACITY)
            {
                s->log[s->log_count++] = (struct logged_action){r.action, r.trigger, s->now};
            }
            else
            {
                s->log_dropped++;
            }
        }
    }
    FAIL_LINE();
    printf("controller kept asking to be woken and never went idle (stuck at %.3f ms)\n", (s->now - T0_US) / 1000.0);
    checks_failed++;
    s->now = target;
}

static game_action sim_button(struct sim *s, unsigned int pin, bool is_pressed, uint64_t at_ms)
{
    sim_idle_until(s, at_ms);
    struct button_event e = {.pin = pin, .is_pressed = is_pressed, .time_changed = AT_MS(at_ms)};
    return button_event_controller_handler(&s->c, &e);
}

static game_action press(struct sim *s, unsigned int pin, uint64_t at_ms) { return sim_button(s, pin, true, at_ms); }
static game_action release(struct sim *s, unsigned int pin, uint64_t at_ms) { return sim_button(s, pin, false, at_ms); }

static void print_log(const char *label, const struct sim *s)
{
    const int max_shown = 6;
    printf("      %s:", label);
    if (s->log_count == 0)
    {
        printf(" (nothing)");
    }
    for (int i = 0; i < s->log_count && i < max_shown; i++)
    {
        printf("\n        %8.3fms  %s (%s)", (s->log[i].at_us - T0_US) / 1000.0, action_name(s->log[i].action), trigger_name(s->log[i].trigger));
    }
    int hidden = s->log_count - max_shown + s->log_dropped;
    if (hidden > 0)
    {
        printf("\n        ... and %d more", hidden);
    }
    printf("\n");
}

/* Checks the threshold actions logged since the last check (or since the start), then clears the log.
 * Pass the expected actions in order, or use CHECK_LOG_EMPTY for none. */
static void check_log(struct sim *s, int line, const game_action *expected, int expected_count)
{
    bool match = s->log_count == expected_count && s->log_dropped == 0;
    for (int i = 0; match && i < expected_count; i++)
    {
        match = s->log[i].action == expected[i];
    }
    if (!match)
    {
        printf("    line %d: threshold actions while idle didn't match\n", line);
        printf("      expected:");
        if (expected_count == 0)
        {
            printf(" (nothing)");
        }
        for (int i = 0; i < expected_count; i++)
        {
            printf(" %s", action_name(expected[i]));
        }
        printf("\n");
        print_log("got", s);
        checks_failed++;
    }
    s->log_count = 0;
    s->log_dropped = 0;
}

#define CHECK_LOG(sim, ...) check_log((sim), __LINE__, (game_action[]){__VA_ARGS__}, (int)(sizeof((game_action[]){__VA_ARGS__}) / sizeof(game_action)))
#define CHECK_LOG_EMPTY(sim) check_log((sim), __LINE__, NULL, 0)

// Checks the trigger of logged entry i. Call before CHECK_LOG, which clears the log.
#define CHECK_LOGGED_TRIGGER(sim, i, expected) do { \
        if ((i) >= (sim)->log_count) { FAIL_LINE(); printf("no logged action at index %d to check the trigger of\n", (i)); checks_failed++; } \
        else if ((sim)->log[(i)].trigger != (expected)) { FAIL_LINE(); printf("logged action %d: expected trigger %s, got %s\n", (i), trigger_name(expected), trigger_name((sim)->log[(i)].trigger)); checks_failed++; } \
    } while (0)

#define UP UP_BUTTON_PIN
#define DOWN DOWN_BUTTON_PIN

//////////////////////////////////
// SECTION: PURE FUNCTION TESTS //
//////////////////////////////////

static void test_hold_duration_buckets(void)
{
    CHECK_THRESHOLD(NONE, get_last_threshold_crossed(0));
    CHECK_THRESHOLD(NONE, get_last_threshold_crossed(SHORT_HOLD_CEILING_US));
    CHECK_THRESHOLD(SHORT, get_last_threshold_crossed(SHORT_HOLD_CEILING_US + 1));
    CHECK_THRESHOLD(SHORT, get_last_threshold_crossed(MEDIUM_HOLD_CEILING_US));
    CHECK_THRESHOLD(MEDIUM, get_last_threshold_crossed(MEDIUM_HOLD_CEILING_US + 1));
    CHECK_THRESHOLD(MEDIUM, get_last_threshold_crossed(LONG_HOLD_FLOOR_US));
    CHECK_THRESHOLD(LONG, get_last_threshold_crossed(LONG_HOLD_FLOOR_US + 1));
    CHECK_THRESHOLD(NONE, get_last_threshold_crossed(ABSOLUTE_TIME_MAX));
}

static void test_time_until_next_threshold_for_one_button(void)
{
    CHECK_U64(500000, get_us_until_next_threshold(0));
    CHECK_U64(400000, get_us_until_next_threshold(100000));
    CHECK_U64(0, get_us_until_next_threshold(SHORT_HOLD_CEILING_US));
    CHECK_U64(400000, get_us_until_next_threshold(600000));
    CHECK_U64(1000000, get_us_until_next_threshold(1500000));
    CHECK_U64(ABSOLUTE_TIME_MAX, get_us_until_next_threshold(3000000));
    CHECK_U64(ABSOLUTE_TIME_MAX, get_us_until_next_threshold(ABSOLUTE_TIME_MAX));
}

static void test_time_until_next_threshold_takes_earliest_button(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_U64(ABSOLUTE_TIME_MAX, controller_us_until_next_threshold(&s.c, AT_MS(0)));

    press(&s, UP, 0);
    CHECK_U64(400000, controller_us_until_next_threshold(&s.c, AT_MS(100)));

    press(&s, DOWN, 300);
    // up crosses 500ms at t=500 (100ms away), down crosses at t=800 (400ms away)
    CHECK_U64(100000, controller_us_until_next_threshold(&s.c, AT_MS(400)));
}

static void test_double_release_action_uses_the_shorter_hold(void)
{
    CHECK_ACTION(NEXT_COLOR_CHANNEL, get_game_action_from_double_press_release(NONE, NONE));
    CHECK_ACTION(NEXT_COLOR_CHANNEL, get_game_action_from_double_press_release(SHORT, NONE));
    CHECK_ACTION(NEXT_COLOR_CHANNEL, get_game_action_from_double_press_release(NONE, MEDIUM));
    CHECK_ACTION(CONFIRM_GUESS, get_game_action_from_double_press_release(SHORT, SHORT));
    CHECK_ACTION(CONFIRM_GUESS, get_game_action_from_double_press_release(MEDIUM, SHORT));
    CHECK_ACTION(START_NEW_GAME, get_game_action_from_double_press_release(MEDIUM, MEDIUM));
    CHECK_ACTION(START_NEW_GAME, get_game_action_from_double_press_release(LONG, MEDIUM));
}

static void test_controller_to_string(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_STR("up{released last=NONE} down{released last=NONE} cont_adj=0", controller_to_string(&s.c));

    press(&s, UP, 0); // T0 is 10 s after boot
    CHECK_STR("up{pressed@10000ms last=NONE} down{released last=NONE} cont_adj=0", controller_to_string(&s.c));
}

////////////////////////////////////
// SECTION: SINGLE BUTTON GESTURES //
////////////////////////////////////

static void test_up_click_increments(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_ACTION(INCREMENT_CHANNEL_VALUE, press(&s, UP, 0));
    CHECK_ACTION(NO_OP, release(&s, UP, 100));
    CHECK_LOG_EMPTY(&s);
}

static void test_down_click_decrements(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_ACTION(DECREMENT_CHANNEL_VALUE, press(&s, DOWN, 0));
    CHECK_ACTION(NO_OP, release(&s, DOWN, 100));
    CHECK_LOG_EMPTY(&s);
}

static void test_up_hold_starts_and_stops_continuous_increment(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_ACTION(INCREMENT_CHANNEL_VALUE, press(&s, UP, 0));
    sim_idle_until(&s, 800);
    CHECK_LOGGED_TRIGGER(&s, 0, TT_UP_BUTTON);
    CHECK_LOG(&s, START_CONT_INC_CHANNEL_VALUE);
    CHECK_ACTION(STOP_CONT_INC_CHANNEL_VALUE, release(&s, UP, 800));
    CHECK_TRUE(!s.c.is_cont_adj);
}

static void test_down_hold_starts_and_stops_continuous_decrement(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_ACTION(DECREMENT_CHANNEL_VALUE, press(&s, DOWN, 0));
    sim_idle_until(&s, 800);
    CHECK_LOGGED_TRIGGER(&s, 0, TT_DOWN_BUTTON);
    CHECK_LOG(&s, START_CONT_DEC_CHANNEL_VALUE);
    CHECK_ACTION(STOP_CONT_DEC_CHANNEL_VALUE, release(&s, DOWN, 800));
    CHECK_TRUE(!s.c.is_cont_adj);
}

// The original double/triple "HOLD THRESHOLD PASSED" bug: an early wake must not count as a crossing.
static void test_wake_just_before_threshold_does_nothing(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);

    struct threshold_action_and_trigger early = threshold_event_controller_handler(&s.c, AT_MS(499));
    CHECK_ACTION(NO_OP, early.action);
    CHECK_TRUE(!s.c.is_cont_adj);

    struct threshold_action_and_trigger on_time = threshold_event_controller_handler(&s.c, AT_MS(501));
    CHECK_ACTION(START_CONT_INC_CHANNEL_VALUE, on_time.action);
}

static void test_continuous_adjust_starts_only_once_per_hold(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    sim_idle_until(&s, 4000); // past every threshold
    CHECK_LOG(&s, START_CONT_INC_CHANNEL_VALUE);
    CHECK_ACTION(STOP_CONT_INC_CHANNEL_VALUE, release(&s, UP, 4000));
}

static void test_second_hold_starts_continuous_adjust_again(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    release(&s, UP, 800);
    CHECK_LOG(&s, START_CONT_INC_CHANNEL_VALUE);

    CHECK_ACTION(INCREMENT_CHANNEL_VALUE, press(&s, UP, 1000));
    sim_idle_until(&s, 1800);
    CHECK_LOG(&s, START_CONT_INC_CHANNEL_VALUE);
    CHECK_ACTION(STOP_CONT_INC_CHANNEL_VALUE, release(&s, UP, 1800));
}

static void test_continuous_adjust_ignores_the_other_button(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    sim_idle_until(&s, 800);
    CHECK_LOG(&s, START_CONT_INC_CHANNEL_VALUE);

    CHECK_ACTION(NO_OP, press(&s, DOWN, 900));
    sim_idle_until(&s, 2000); // down held past its own thresholds
    CHECK_LOG_EMPTY(&s);
    CHECK_ACTION(NO_OP, release(&s, DOWN, 2000));
    CHECK_TRUE(s.c.is_cont_adj);

    CHECK_ACTION(STOP_CONT_INC_CHANNEL_VALUE, release(&s, UP, 2100));
}

////////////////////////////////////
// SECTION: DOUBLE BUTTON GESTURES //
////////////////////////////////////

static void test_pressing_second_button_does_nothing_by_itself(void)
{
    struct sim s;
    sim_init(&s);
    CHECK_ACTION(INCREMENT_CHANNEL_VALUE, press(&s, UP, 0));
    CHECK_ACTION(NO_OP, press(&s, DOWN, 100));

    sim_init(&s);
    CHECK_ACTION(DECREMENT_CHANNEL_VALUE, press(&s, DOWN, 0));
    CHECK_ACTION(NO_OP, press(&s, UP, 100));
}

static void test_double_click_moves_to_next_channel(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 100);
    CHECK_ACTION(NEXT_COLOR_CHANNEL, release(&s, UP, 300));
    CHECK_LOG_EMPTY(&s);
    CHECK_ACTION(NO_OP, release(&s, DOWN, 350));
}

// Holding both must never start continuous adjust; only the trailing (later-pressed) button pulses.
static void test_double_hold_pulses_for_trailing_button_only(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 100);
    sim_idle_until(&s, 700); // up crosses 500ms at t=500, down crosses at t=600
    CHECK_LOGGED_TRIGGER(&s, 0, TT_DOWN_BUTTON);
    CHECK_LOG(&s, INDICATE_HOLD_THRESHOLD_PASSED);
    CHECK_TRUE(!s.c.is_cont_adj);
}

static void test_double_hold_past_first_threshold_confirms_guess(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 100);
    sim_idle_until(&s, 900);
    CHECK_LOG(&s, INDICATE_HOLD_THRESHOLD_PASSED);
    CHECK_ACTION(CONFIRM_GUESS, release(&s, UP, 900));
}

static void test_double_hold_past_second_threshold_starts_new_game(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 100);
    sim_idle_until(&s, 1500); // down crosses at t=600 and t=1100
    CHECK_LOG(&s, INDICATE_HOLD_THRESHOLD_PASSED, INDICATE_HOLD_THRESHOLD_PASSED);
    CHECK_ACTION(START_NEW_GAME, release(&s, DOWN, 1500));
}

static void test_double_release_judges_by_the_trailing_button(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 400);
    sim_idle_until(&s, 700); // up has passed 500ms, but down has only been held 300ms
    CHECK_LOG_EMPTY(&s);
    CHECK_ACTION(NEXT_COLOR_CHANNEL, release(&s, UP, 700));
}

// After a two-button gesture resolves, ignore everything until both buttons are up again.
static void test_inputs_ignored_after_double_release_until_both_up(void)
{
    struct sim s;
    sim_init(&s);
    press(&s, UP, 0);
    press(&s, DOWN, 100);
    CHECK_ACTION(NEXT_COLOR_CHANNEL, release(&s, DOWN, 200));

    sim_idle_until(&s, 2000); // up still held well past the continuous adjust threshold
    CHECK_LOG_EMPTY(&s);
    CHECK_ACTION(NO_OP, press(&s, DOWN, 2100));
    CHECK_ACTION(NO_OP, release(&s, DOWN, 2200));
    CHECK_ACTION(NO_OP, release(&s, UP, 2300));

    // back in the base state: buttons behave normally again
    CHECK_ACTION(INCREMENT_CHANNEL_VALUE, press(&s, UP, 3000));
    CHECK_ACTION(NO_OP, release(&s, UP, 3100));
    CHECK_LOG_EMPTY(&s);
}

int main(void)
{
    printf("-- pure functions\n");
    RUN_TEST(test_hold_duration_buckets);
    RUN_TEST(test_time_until_next_threshold_for_one_button);
    RUN_TEST(test_time_until_next_threshold_takes_earliest_button);
    RUN_TEST(test_double_release_action_uses_the_shorter_hold);
    RUN_TEST(test_controller_to_string);

    printf("-- single button gestures\n");
    RUN_TEST(test_up_click_increments);
    RUN_TEST(test_down_click_decrements);
    RUN_TEST(test_up_hold_starts_and_stops_continuous_increment);
    RUN_TEST(test_down_hold_starts_and_stops_continuous_decrement);
    RUN_TEST(test_wake_just_before_threshold_does_nothing);
    RUN_TEST(test_continuous_adjust_starts_only_once_per_hold);
    RUN_TEST(test_second_hold_starts_continuous_adjust_again);
    RUN_TEST(test_continuous_adjust_ignores_the_other_button);

    printf("-- double button gestures\n");
    RUN_TEST(test_pressing_second_button_does_nothing_by_itself);
    RUN_TEST(test_double_click_moves_to_next_channel);
    RUN_TEST(test_double_hold_pulses_for_trailing_button_only);
    RUN_TEST(test_double_hold_past_first_threshold_confirms_guess);
    RUN_TEST(test_double_hold_past_second_threshold_starts_new_game);
    RUN_TEST(test_double_release_judges_by_the_trailing_button);
    RUN_TEST(test_inputs_ignored_after_double_release_until_both_up);

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
