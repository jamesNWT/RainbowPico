/* Host-side unit tests for the button controller (source/Controller.c).
 * Build and run with tests/run_tests.ps1. No framework: each test is a plain function,
 * CHECK_* macros record failures, and main() runs everything and prints a summary.
*/
#include <stdio.h>
#include "Controller.h"

/////////////////////
// SECTION: HARNESS //
/////////////////////

static int checks_failed;
static int tests_run;
static int tests_failed;

/* Times in the tests are ms after T0. T0 is non-zero so bugs that assume "time 0" can't hide. */
#define T0_US 10000000ULL
#define AT_MS(ms) (T0_US + (uint64_t)(ms) * 1000ULL)
#define NO_DEADLINE ABSOLUTE_TIME_MAX

static void print_time(uint64_t t)
{
    if (t == ABSOLUTE_TIME_MAX)
    {
        printf("none");
    }
    else if (t >= T0_US)
    {
        printf("T0+%.3fms", (t - T0_US) / 1000.0);
    }
    else
    {
        printf("%lluus (before T0)", (unsigned long long)t);
    }
}

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

static const char *state_name(controller_state s)
{
    switch (s)
    {
    case CONTROLLER_IDLE: return "IDLE";
    case CONTROLLER_SINGLE_CLICK: return "SINGLE_CLICK";
    case CONTROLLER_SINGLE_CONT_ADJ: return "SINGLE_CONT_ADJ";
    case CONTROLLER_TWO_CLICK: return "TWO_CLICK";
    case CONTROLLER_TWO_SHORT_HOLD: return "TWO_SHORT_HOLD";
    case CONTROLLER_TWO_LONG_HOLD: return "TWO_LONG_HOLD";
    case CONTROLLER_LOCKOUT: return "LOCKOUT";
    default: return "<not a controller_state>";
    }
}

static const char *event_name(controller_event e)
{
    switch (e)
    {
    case EV_UP_PRESSED: return "EV_UP_PRESSED";
    case EV_UP_RELEASED: return "EV_UP_RELEASED";
    case EV_DOWN_PRESSED: return "EV_DOWN_PRESSED";
    case EV_DOWN_RELEASED: return "EV_DOWN_RELEASED";
    case EV_DEADLINE: return "EV_DEADLINE";
    default: return "<not a controller_event>";
    }
}

#define FAIL_LINE() printf("    line %d: ", __LINE__)

#define CHECK_TIME(expected, actual) do { \
        uint64_t e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected ", #actual); print_time(e_); printf(", got "); print_time(a_); printf("\n"); checks_failed++; } \
    } while (0)

#define CHECK_STATE(expected, actual) do { \
        controller_state e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s\n", #actual, state_name(e_), state_name(a_)); checks_failed++; } \
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

// Pass RELEASED for a button that isn't held.
#define RELEASED (-1)

static struct controller controller_in(controller_state state, long up_press_ms, long down_press_ms)
{
    struct controller c;
    controller_init(&c);
    c.state = state;
    c.up = (struct button){.is_pressed = up_press_ms != RELEASED,
                           .time_press = up_press_ms != RELEASED ? AT_MS(up_press_ms) : ABSOLUTE_TIME_MAX};
    c.down = (struct button){.is_pressed = down_press_ms != RELEASED,
                             .time_press = down_press_ms != RELEASED ? AT_MS(down_press_ms) : ABSOLUTE_TIME_MAX};
    return c;
}

static struct controller_input input_at(controller_event event, long at_ms)
{
    return (struct controller_input){.event_type = event, .event_time = AT_MS(at_ms)};
}

/* One cell of the state table: start in `from` with the given buttons held, feed one input,
 * and check the action and the state it lands in. Returns the controller for extra checks. */
static struct controller check_transition(int line, controller_state from, long up_press_ms, long down_press_ms,
                                          controller_event event, long at_ms,
                                          game_action want_action, controller_state want_state)
{
    struct controller c = controller_in(from, up_press_ms, down_press_ms);
    game_action got_action = controller_handle(&c, input_at(event, at_ms));
    if (got_action != want_action || c.state != want_state)
    {
        printf("    line %d: %s + %s @ %ldms: expected %s -> %s, got %s -> %s\n", line,
               state_name(from), event_name(event), at_ms,
               action_name(want_action), state_name(want_state),
               action_name(got_action), state_name(c.state));
        checks_failed++;
    }
    return c;
}

#define CHECK_TRANSITION(from, up_ms, down_ms, event, at_ms, want_action, want_state) \
    check_transition(__LINE__, (from), (up_ms), (down_ms), (event), (at_ms), (want_action), (want_state))

//////////////////////////////////
// SECTION: GESTURE SIMULATOR //
//////////////////////////////////

/* Plays out a whole gesture the way button_controller_task does: before each button event,
 * every deadline that falls at or before it is fed to controller_handle as EV_DEADLINE (at the
 * deadline's own time, as if the task woke exactly on time). Every non-NO_OP action is logged. */
#define LOG_CAPACITY 16

struct logged_action
{
    game_action action;
    uint64_t at_us;
};

struct sim
{
    struct controller c;
    int log_count;
    struct logged_action log[LOG_CAPACITY];
};

static void sim_init(struct sim *s)
{
    controller_init(&s->c);
    s->log_count = 0;
}

static void sim_record(struct sim *s, game_action action, uint64_t at_us)
{
    if (action != NO_OP && s->log_count < LOG_CAPACITY)
    {
        s->log[s->log_count++] = (struct logged_action){action, at_us};
    }
}

static void sim_idle_until(struct sim *s, long at_ms)
{
    uint64_t until = AT_MS(at_ms);
    while (1)
    {
        uint64_t deadline = controller_next_deadline(&s->c);
        if (deadline == ABSOLUTE_TIME_MAX || deadline > until)
        {
            return;
        }
        sim_record(s, controller_handle(&s->c, (struct controller_input){.event_type = EV_DEADLINE, .event_time = deadline}), deadline);
        if (controller_next_deadline(&s->c) == deadline)
        {
            printf("    deadline at ");
            print_time(deadline);
            printf(" in state %s didn't move after handling it; the task would spin forever\n", state_name(s->c.state));
            checks_failed++;
            return;
        }
    }
}

static void sim_input(struct sim *s, controller_event event, long at_ms)
{
    sim_idle_until(s, at_ms);
    sim_record(s, controller_handle(&s->c, input_at(event, at_ms)), AT_MS(at_ms));
}

static void print_log(const struct sim *s)
{
    if (s->log_count == 0)
    {
        printf(" (nothing)");
    }
    for (int i = 0; i < s->log_count; i++)
    {
        printf("\n        %s @ ", action_name(s->log[i].action));
        print_time(s->log[i].at_us);
    }
    printf("\n");
}

/* Checks every non-NO_OP action from the start of the scenario, in order, with the time (ms) it happened. */
struct expected_action
{
    game_action action;
    long at_ms;
};

static void check_log(const struct sim *s, int line, const struct expected_action *expected, int expected_count)
{
    bool match = s->log_count == expected_count;
    for (int i = 0; match && i < expected_count; i++)
    {
        match = s->log[i].action == expected[i].action && s->log[i].at_us == AT_MS(expected[i].at_ms);
    }
    if (!match)
    {
        printf("    line %d: actions didn't match\n      expected:", line);
        if (expected_count == 0)
        {
            printf(" (nothing)");
        }
        for (int i = 0; i < expected_count; i++)
        {
            printf("\n        %s @ T0+%ld.000ms", action_name(expected[i].action), expected[i].at_ms);
        }
        printf("\n      got:");
        print_log(s);
        checks_failed++;
    }
}

#define CHECK_LOG(sim, ...) check_log((sim), __LINE__, (struct expected_action[]){__VA_ARGS__}, \
    (int)(sizeof((struct expected_action[]){__VA_ARGS__}) / sizeof(struct expected_action)))
#define CHECK_LOG_EMPTY(sim) check_log((sim), __LINE__, NULL, 0)

/////////////////////////////////////////
// SECTION: controller_next_deadline //
/////////////////////////////////////////

static void test_idle_has_no_deadline(void)
{
    struct controller c = controller_in(CONTROLLER_IDLE, RELEASED, RELEASED);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));
}

static void test_single_click_deadline_is_click_ceiling_after_press(void)
{
    struct controller up = controller_in(CONTROLLER_SINGLE_CLICK, 0, RELEASED);
    CHECK_TIME(AT_MS(200), controller_next_deadline(&up));

    struct controller down = controller_in(CONTROLLER_SINGLE_CLICK, RELEASED, 1234);
    CHECK_TIME(AT_MS(1434), controller_next_deadline(&down));
}

static void test_single_cont_adj_has_no_deadline(void)
{
    struct controller c = controller_in(CONTROLLER_SINGLE_CONT_ADJ, 0, RELEASED);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));

    // the other button is ignored during continuous adjust, so it mustn't create a deadline either
    struct controller other_held = controller_in(CONTROLLER_SINGLE_CONT_ADJ, 0, 500);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&other_held));
}

// FROM is the press that made it a two-button gesture, i.e. the later of the two presses.
static void test_two_click_deadline_is_click_ceiling_after_second_press(void)
{
    struct controller up_first = controller_in(CONTROLLER_TWO_CLICK, 0, 150);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&up_first));

    struct controller down_first = controller_in(CONTROLLER_TWO_CLICK, 150, 0);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&down_first));
}

static void test_two_short_hold_deadline_is_short_hold_ceiling_after_second_press(void)
{
    struct controller up_first = controller_in(CONTROLLER_TWO_SHORT_HOLD, 0, 150);
    CHECK_TIME(AT_MS(1150), controller_next_deadline(&up_first));

    struct controller down_first = controller_in(CONTROLLER_TWO_SHORT_HOLD, 150, 0);
    CHECK_TIME(AT_MS(1150), controller_next_deadline(&down_first));
}

static void test_two_long_hold_has_no_deadline(void)
{
    struct controller c = controller_in(CONTROLLER_TWO_LONG_HOLD, 0, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));
}

static void test_lockout_has_no_deadline(void)
{
    struct controller both_held = controller_in(CONTROLLER_LOCKOUT, 0, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&both_held));

    struct controller one_held = controller_in(CONTROLLER_LOCKOUT, RELEASED, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&one_held));
}

////////////////////////////////////////////////////
// SECTION: controller_handle, one state at a time //
////////////////////////////////////////////////////

static void test_idle_transitions(void)
{
    struct controller c = CHECK_TRANSITION(CONTROLLER_IDLE, RELEASED, RELEASED, EV_UP_PRESSED, 5, NO_OP, CONTROLLER_SINGLE_CLICK);
    CHECK_TRUE(c.up.is_pressed);
    CHECK_TIME(AT_MS(5), c.up.time_press);

    c = CHECK_TRANSITION(CONTROLLER_IDLE, RELEASED, RELEASED, EV_DOWN_PRESSED, 5, NO_OP, CONTROLLER_SINGLE_CLICK);
    CHECK_TRUE(c.down.is_pressed);
    CHECK_TIME(AT_MS(5), c.down.time_press);
}

// A click only counts on release, because the press might turn out to be the start of a two-button gesture.
static void test_single_click_transitions(void)
{
    struct controller c = CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, 0, RELEASED, EV_UP_RELEASED, 100, INCREMENT_CHANNEL_VALUE, CONTROLLER_IDLE);
    CHECK_TRUE(!c.up.is_pressed);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, RELEASED, 0, EV_DOWN_RELEASED, 100, DECREMENT_CHANNEL_VALUE, CONTROLLER_IDLE);

    CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, 0, RELEASED, EV_DEADLINE, 200, START_CONT_INC_CHANNEL_VALUE, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, RELEASED, 0, EV_DEADLINE, 200, START_CONT_DEC_CHANNEL_VALUE, CONTROLLER_SINGLE_CONT_ADJ);

    // the second press is the FROM for the two-button timings, so its time has to be recorded
    c = CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, 0, RELEASED, EV_DOWN_PRESSED, 150, NO_OP, CONTROLLER_TWO_CLICK);
    CHECK_TIME(AT_MS(150), c.down.time_press);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&c));
    CHECK_TRANSITION(CONTROLLER_SINGLE_CLICK, RELEASED, 0, EV_UP_PRESSED, 150, NO_OP, CONTROLLER_TWO_CLICK);
}

// Only the release of the button doing the adjusting (the one pressed first) matters.
static void test_single_cont_adj_transitions(void)
{
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 0, RELEASED, EV_UP_RELEASED, 900, STOP_CONT_INC_CHANNEL_VALUE, CONTROLLER_IDLE);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, RELEASED, 0, EV_DOWN_RELEASED, 900, STOP_CONT_DEC_CHANNEL_VALUE, CONTROLLER_IDLE);

    // the other button is tracked but otherwise ignored
    struct controller c = CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 0, RELEASED, EV_DOWN_PRESSED, 300, NO_OP, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_TRUE(c.down.is_pressed);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 0, 300, EV_DOWN_RELEASED, 500, NO_OP, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 300, 0, EV_UP_RELEASED, 500, NO_OP, CONTROLLER_SINGLE_CONT_ADJ);

    // stopping while the other button is still down waits for both buttons to come up
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 0, 300, EV_UP_RELEASED, 900, STOP_CONT_INC_CHANNEL_VALUE, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_SINGLE_CONT_ADJ, 300, 0, EV_DOWN_RELEASED, 900, STOP_CONT_DEC_CHANNEL_VALUE, CONTROLLER_LOCKOUT);
}

static void test_two_click_transitions(void)
{
    CHECK_TRANSITION(CONTROLLER_TWO_CLICK, 0, 150, EV_UP_RELEASED, 250, NEXT_COLOR_CHANNEL, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_TWO_CLICK, 0, 150, EV_DOWN_RELEASED, 250, NEXT_COLOR_CHANNEL, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_TWO_CLICK, 0, 150, EV_DEADLINE, 350, INDICATE_HOLD_THRESHOLD_PASSED, CONTROLLER_TWO_SHORT_HOLD);
}

static void test_two_short_hold_transitions(void)
{
    CHECK_TRANSITION(CONTROLLER_TWO_SHORT_HOLD, 0, 150, EV_UP_RELEASED, 600, CONFIRM_GUESS, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_TWO_SHORT_HOLD, 0, 150, EV_DOWN_RELEASED, 600, CONFIRM_GUESS, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_TWO_SHORT_HOLD, 0, 150, EV_DEADLINE, 1150, INDICATE_HOLD_THRESHOLD_PASSED, CONTROLLER_TWO_LONG_HOLD);
}

static void test_two_long_hold_transitions(void)
{
    CHECK_TRANSITION(CONTROLLER_TWO_LONG_HOLD, 0, 150, EV_UP_RELEASED, 1500, START_NEW_GAME, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(CONTROLLER_TWO_LONG_HOLD, 0, 150, EV_DOWN_RELEASED, 1500, START_NEW_GAME, CONTROLLER_LOCKOUT);
}

// Everything is ignored until both buttons are up, but presses and releases are still tracked.
static void test_lockout_transitions(void)
{
    CHECK_TRANSITION(CONTROLLER_LOCKOUT, 0, RELEASED, EV_UP_RELEASED, 2000, NO_OP, CONTROLLER_IDLE);
    CHECK_TRANSITION(CONTROLLER_LOCKOUT, RELEASED, 0, EV_DOWN_RELEASED, 2000, NO_OP, CONTROLLER_IDLE);

    CHECK_TRANSITION(CONTROLLER_LOCKOUT, 0, 150, EV_UP_RELEASED, 2000, NO_OP, CONTROLLER_LOCKOUT);
    struct controller c = CHECK_TRANSITION(CONTROLLER_LOCKOUT, RELEASED, 150, EV_UP_PRESSED, 2000, NO_OP, CONTROLLER_LOCKOUT);
    CHECK_TRUE(c.up.is_pressed);
}

//////////////////////////////////////////
// SECTION: whole gestures, end to end //
//////////////////////////////////////////

#define UP_P EV_UP_PRESSED
#define UP_R EV_UP_RELEASED
#define DOWN_P EV_DOWN_PRESSED
#define DOWN_R EV_DOWN_RELEASED

static void test_gesture_up_click(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, UP_R, 100);
    CHECK_LOG(&s, {INCREMENT_CHANNEL_VALUE, 100});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_down_click(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, DOWN_P, 0);
    sim_input(&s, DOWN_R, 100);
    CHECK_LOG(&s, {DECREMENT_CHANNEL_VALUE, 100});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_up_hold(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, UP_R, 1500);
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_down_hold(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, DOWN_P, 0);
    sim_input(&s, DOWN_R, 1500);
    CHECK_LOG(&s, {START_CONT_DEC_CHANNEL_VALUE, 200}, {STOP_CONT_DEC_CHANNEL_VALUE, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_other_button_tapped_during_hold_is_ignored(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 300);
    sim_input(&s, DOWN_R, 500);
    sim_input(&s, UP_R, 900);
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 900});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_hold_released_while_other_held_waits_for_both_up(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 300);
    sim_input(&s, UP_R, 600);
    CHECK_STATE(CONTROLLER_LOCKOUT, s.c.state);
    sim_input(&s, DOWN_R, 2000); // down held well past every threshold: still nothing
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);

    sim_input(&s, UP_P, 3000);
    sim_input(&s, UP_R, 3100);
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 600}, {INCREMENT_CHANNEL_VALUE, 3100});
}

static void test_gesture_two_button_click_moves_to_next_channel(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 150);
    sim_input(&s, UP_R, 250);
    sim_input(&s, DOWN_R, 300);
    CHECK_LOG(&s, {NEXT_COLOR_CHANNEL, 250});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

// The click window restarts at the second press: 190ms + 180ms is past 200ms from the first press, but still a click.
static void test_gesture_two_button_timing_starts_at_second_press(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 190);
    sim_input(&s, UP_R, 370);
    CHECK_LOG(&s, {NEXT_COLOR_CHANNEL, 370});
}

static void test_gesture_two_button_short_hold_confirms_guess(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, DOWN_P, 0);
    sim_input(&s, UP_P, 150);
    sim_input(&s, DOWN_R, 600);
    sim_input(&s, UP_R, 650);
    CHECK_LOG(&s, {INDICATE_HOLD_THRESHOLD_PASSED, 350}, {CONFIRM_GUESS, 600});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_two_button_long_hold_starts_new_game(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 150);
    sim_input(&s, UP_R, 1500);
    sim_input(&s, DOWN_R, 1600);
    CHECK_LOG(&s, {INDICATE_HOLD_THRESHOLD_PASSED, 350}, {INDICATE_HOLD_THRESHOLD_PASSED, 1150}, {START_NEW_GAME, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_lockout_after_two_button_gesture(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, UP_P, 0);
    sim_input(&s, DOWN_P, 150);
    sim_input(&s, UP_R, 250);  // NEXT_COLOR_CHANNEL, down still held
    sim_input(&s, UP_P, 3000); // down held far past every threshold, then up pressed and released again
    sim_input(&s, UP_R, 3200);
    sim_input(&s, DOWN_R, 3300);
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);

    sim_input(&s, UP_P, 4000);
    sim_input(&s, UP_R, 4050);
    CHECK_LOG(&s, {NEXT_COLOR_CHANNEL, 250}, {INCREMENT_CHANNEL_VALUE, 4050});
}

int main(void)
{
    printf("-- controller_next_deadline\n");
    RUN_TEST(test_idle_has_no_deadline);
    RUN_TEST(test_single_click_deadline_is_click_ceiling_after_press);
    RUN_TEST(test_single_cont_adj_has_no_deadline);
    RUN_TEST(test_two_click_deadline_is_click_ceiling_after_second_press);
    RUN_TEST(test_two_short_hold_deadline_is_short_hold_ceiling_after_second_press);
    RUN_TEST(test_two_long_hold_has_no_deadline);
    RUN_TEST(test_lockout_has_no_deadline);

    printf("-- controller_handle, one state at a time\n");
    RUN_TEST(test_idle_transitions);
    RUN_TEST(test_single_click_transitions);
    RUN_TEST(test_single_cont_adj_transitions);
    RUN_TEST(test_two_click_transitions);
    RUN_TEST(test_two_short_hold_transitions);
    RUN_TEST(test_two_long_hold_transitions);
    RUN_TEST(test_lockout_transitions);

    printf("-- whole gestures\n");
    RUN_TEST(test_gesture_up_click);
    RUN_TEST(test_gesture_down_click);
    RUN_TEST(test_gesture_up_hold);
    RUN_TEST(test_gesture_down_hold);
    RUN_TEST(test_gesture_other_button_tapped_during_hold_is_ignored);
    RUN_TEST(test_gesture_hold_released_while_other_held_waits_for_both_up);
    RUN_TEST(test_gesture_two_button_click_moves_to_next_channel);
    RUN_TEST(test_gesture_two_button_timing_starts_at_second_press);
    RUN_TEST(test_gesture_two_button_short_hold_confirms_guess);
    RUN_TEST(test_gesture_two_button_long_hold_starts_new_game);
    RUN_TEST(test_gesture_lockout_after_two_button_gesture);

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
