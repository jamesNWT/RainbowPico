/* Host-side unit tests for the button controller (source/Controller.c).
 * Build and run with tests/run_tests.ps1. No framework: each test is a plain function,
 * CHECK_* macros record failures, and main() runs everything and prints a summary.
 *
 * Controller.c uses assert() for "impossible" inputs. A failed assert aborts the whole run
 * with the file and line of the assert, and the summary is never printed.
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

#define UP UP_BUTTON_INDEX
#define DOWN DOWN_BUTTON_INDEX
#define HELD_BOTH (HELD_UP | HELD_DOWN)

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

static const char *action_name(game_action_kind a)
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
    default: return "<not a game_action_kind>";
    }
}

static const char *state_name(controller_state_kind s)
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
    default: return "<not a controller_state_kind>";
    }
}

static const char *button_name(button_index b)
{
    switch (b)
    {
    case UP_BUTTON_INDEX: return "UP";
    case DOWN_BUTTON_INDEX: return "DOWN";
    default: return "<not a button_index>";
    }
}

static const char *held_name(unsigned held)
{
    switch (held)
    {
    case HELD_NONE: return "none";
    case HELD_UP: return "UP";
    case HELD_DOWN: return "DOWN";
    case HELD_BOTH: return "UP|DOWN";
    default: return "<not a held mask>";
    }
}

static void print_input(struct controller_input in)
{
    switch (in.event_type)
    {
    case EV_PRESS: printf("press(%s)", button_name(in.affect_button)); break;
    case EV_RELEASE: printf("release(%s)", button_name(in.affect_button)); break;
    case EV_DEADLINE: printf("deadline"); break;
    default: printf("<not a controller_event>"); break;
    }
    printf(" @ ");
    print_time(in.event_time);
}

#define FAIL_LINE() printf("    line %d: ", __LINE__)

#define CHECK_TIME(expected, actual) do { \
        uint64_t e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected ", #actual); print_time(e_); printf(", got "); print_time(a_); printf("\n"); checks_failed++; } \
    } while (0)

#define CHECK_STATE(expected, actual) do { \
        controller_state_kind e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s\n", #actual, state_name(e_), state_name(a_)); checks_failed++; } \
    } while (0)

#define CHECK_HELD(expected, actual) do { \
        unsigned e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s\n", #actual, held_name(e_), held_name(a_)); checks_failed++; } \
    } while (0)

#define CHECK_BUTTON(expected, actual) do { \
        button_index e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected %s, got %s\n", #actual, button_name(e_), button_name(a_)); checks_failed++; } \
    } while (0)

#define RUN_TEST(test) do { \
        checks_failed = 0; \
        test(); \
        tests_run++; \
        if (checks_failed) { tests_failed++; printf("FAIL  %s\n\n", #test); } \
        else { printf("ok    %s\n", #test); } \
        fflush(stdout); \
    } while (0)

/* For states that don't read active_button / timer_start. Anything passed there should be ignored. */
#define ANY_BUTTON UP_BUTTON_INDEX
#define NO_TIMER (-1)

/* Builds a controller directly in a given state: which buttons are held (HELD_* mask), the
 * button that started the gesture, and the ms the deadline timer started at. */
static struct controller controller_in(controller_state_kind state, unsigned held, button_index active, long timer_start_ms)
{
    struct controller c;
    controller_init(&c);
    c.state = state;
    c.held = held;
    c.active_button = active;
    if (timer_start_ms != NO_TIMER)
    {
        c.timer_start = AT_MS(timer_start_ms);
    }
    return c;
}

static struct controller_input press(button_index button, long at_ms)
{
    return (struct controller_input){.event_type = EV_PRESS, .affect_button = button, .event_time = AT_MS(at_ms)};
}

static struct controller_input release(button_index button, long at_ms)
{
    return (struct controller_input){.event_type = EV_RELEASE, .affect_button = button, .event_time = AT_MS(at_ms)};
}

/* affect_button is left zeroed (UP): a deadline has no button, so the controller mustn't read it. */
static struct controller_input deadline_at_us(uint64_t at_us)
{
    return (struct controller_input){.event_type = EV_DEADLINE, .event_time = at_us};
}

static struct controller_input deadline(long at_ms)
{
    return deadline_at_us(AT_MS(at_ms));
}

/* One cell of the state table: feed one input to a controller built with controller_in,
 * and check the action and the state it lands in. Returns the controller for extra checks. */
static struct controller check_transition(int line, struct controller c, struct controller_input in,
                                          game_action_kind want_action, controller_state_kind want_state)
{
    controller_state_kind from = c.state;
    game_action_kind got_action = controller_handle(&c, in);
    if (got_action != want_action || c.state != want_state)
    {
        printf("    line %d: %s + ", line, state_name(from));
        print_input(in);
        printf(": expected %s -> %s, got %s -> %s\n",
               action_name(want_action), state_name(want_state),
               action_name(got_action), state_name(c.state));
        checks_failed++;
    }
    return c;
}

#define CHECK_TRANSITION(from, in, want_action, want_state) \
    check_transition(__LINE__, (from), (in), (want_action), (want_state))

//////////////////////////////////
// SECTION: GESTURE SIMULATOR //
//////////////////////////////////

/* Plays out a whole gesture the way button_controller_task does: before each button event,
 * every deadline that falls at or before it is fed to controller_handle as EV_DEADLINE (at the
 * deadline's own time, as if the task woke exactly on time). Every non-NO_OP action is logged. */
#define LOG_CAPACITY 16

struct logged_action
{
    game_action_kind action;
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

static void sim_record(struct sim *s, game_action_kind action, uint64_t at_us)
{
    if (action != NO_OP && s->log_count < LOG_CAPACITY)
    {
        s->log[s->log_count++] = (struct logged_action){action, at_us};
    }
}

static void sim_idle_until(struct sim *s, uint64_t until_us)
{
    while (1)
    {
        uint64_t next = controller_next_deadline(&s->c);
        if (next == ABSOLUTE_TIME_MAX || next > until_us)
        {
            return;
        }
        sim_record(s, controller_handle(&s->c, deadline_at_us(next)), next);
        if (controller_next_deadline(&s->c) == next)
        {
            printf("    deadline at ");
            print_time(next);
            printf(" in state %s didn't move after handling it; the task would spin forever\n", state_name(s->c.state));
            checks_failed++;
            return;
        }
    }
}

static void sim_input(struct sim *s, struct controller_input in)
{
    sim_idle_until(s, in.event_time);
    sim_record(s, controller_handle(&s->c, in), in.event_time);
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
    game_action_kind action;
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
    struct controller c = controller_in(CONTROLLER_IDLE, HELD_NONE, ANY_BUTTON, NO_TIMER);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));

    // a leftover timer from an earlier gesture mustn't create a deadline
    struct controller stale = controller_in(CONTROLLER_IDLE, HELD_NONE, ANY_BUTTON, 0);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&stale));
}

static void test_single_click_deadline_is_click_ceiling_after_press(void)
{
    struct controller up = controller_in(CONTROLLER_SINGLE_CLICK, HELD_UP, UP, 0);
    CHECK_TIME(AT_MS(200), controller_next_deadline(&up));

    struct controller down = controller_in(CONTROLLER_SINGLE_CLICK, HELD_DOWN, DOWN, 1234);
    CHECK_TIME(AT_MS(1434), controller_next_deadline(&down));
}

static void test_single_cont_adj_has_no_deadline(void)
{
    struct controller c = controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_UP, UP, 0);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));

    // the other button is ignored during continuous adjust, so it mustn't create a deadline either
    struct controller other_held = controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_BOTH, UP, 500);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&other_held));
}

// The timer for the two-button states starts at the press that made it a two-button gesture (the second press).
static void test_two_click_deadline_is_click_ceiling_after_second_press(void)
{
    struct controller c = controller_in(CONTROLLER_TWO_CLICK, HELD_BOTH, ANY_BUTTON, 150);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&c));
}

static void test_two_short_hold_deadline_is_short_hold_ceiling_after_second_press(void)
{
    struct controller c = controller_in(CONTROLLER_TWO_SHORT_HOLD, HELD_BOTH, ANY_BUTTON, 150);
    CHECK_TIME(AT_MS(1150), controller_next_deadline(&c));
}

static void test_two_long_hold_has_no_deadline(void)
{
    struct controller c = controller_in(CONTROLLER_TWO_LONG_HOLD, HELD_BOTH, ANY_BUTTON, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&c));
}

static void test_lockout_has_no_deadline(void)
{
    struct controller both_held = controller_in(CONTROLLER_LOCKOUT, HELD_BOTH, ANY_BUTTON, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&both_held));

    struct controller one_held = controller_in(CONTROLLER_LOCKOUT, HELD_DOWN, ANY_BUTTON, 150);
    CHECK_TIME(NO_DEADLINE, controller_next_deadline(&one_held));
}

////////////////////////////////////////////////////
// SECTION: controller_handle, one state at a time //
////////////////////////////////////////////////////

static void test_idle_transitions(void)
{
    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_IDLE, HELD_NONE, ANY_BUTTON, NO_TIMER),
                                           press(UP, 5), NO_OP, CONTROLLER_SINGLE_CLICK);
    CHECK_HELD(HELD_UP, c.held);
    CHECK_BUTTON(UP, c.active_button);
    CHECK_TIME(AT_MS(205), controller_next_deadline(&c));

    c = CHECK_TRANSITION(controller_in(CONTROLLER_IDLE, HELD_NONE, ANY_BUTTON, NO_TIMER),
                         press(DOWN, 5), NO_OP, CONTROLLER_SINGLE_CLICK);
    CHECK_HELD(HELD_DOWN, c.held);
    CHECK_BUTTON(DOWN, c.active_button);
    CHECK_TIME(AT_MS(205), controller_next_deadline(&c));
}

// A click only counts on release, because the press might turn out to be the start of a two-button gesture.
static void test_single_click_transitions(void)
{
    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_UP, UP, 0),
                                           release(UP, 100), INCREMENT_CHANNEL_VALUE, CONTROLLER_IDLE);
    CHECK_HELD(HELD_NONE, c.held);
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_DOWN, DOWN, 0),
                     release(DOWN, 100), DECREMENT_CHANNEL_VALUE, CONTROLLER_IDLE);

    // a deadline has no button: it mustn't change which buttons are held or which one is adjusting
    c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_UP, UP, 0),
                         deadline(200), START_CONT_INC_CHANNEL_VALUE, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_HELD(HELD_UP, c.held);
    CHECK_BUTTON(UP, c.active_button);
    c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_DOWN, DOWN, 0),
                         deadline(200), START_CONT_DEC_CHANNEL_VALUE, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_HELD(HELD_DOWN, c.held);
    CHECK_BUTTON(DOWN, c.active_button);

    // the second press restarts the timer for the two-button timings
    c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_UP, UP, 0),
                         press(DOWN, 150), NO_OP, CONTROLLER_TWO_CLICK);
    CHECK_HELD(HELD_BOTH, c.held);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&c));
    c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CLICK, HELD_DOWN, DOWN, 0),
                         press(UP, 150), NO_OP, CONTROLLER_TWO_CLICK);
    CHECK_HELD(HELD_BOTH, c.held);
    CHECK_TIME(AT_MS(350), controller_next_deadline(&c));
}

// Only the release of the button doing the adjusting (the one pressed first) matters.
static void test_single_cont_adj_transitions(void)
{
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_UP, UP, NO_TIMER),
                     release(UP, 900), STOP_CONT_INC_CHANNEL_VALUE, CONTROLLER_IDLE);
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_DOWN, DOWN, NO_TIMER),
                     release(DOWN, 900), STOP_CONT_DEC_CHANNEL_VALUE, CONTROLLER_IDLE);

    // the other button is tracked but otherwise ignored
    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_UP, UP, NO_TIMER),
                                           press(DOWN, 300), NO_OP, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_HELD(HELD_BOTH, c.held);
    c = CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_BOTH, UP, NO_TIMER),
                         release(DOWN, 500), NO_OP, CONTROLLER_SINGLE_CONT_ADJ);
    CHECK_HELD(HELD_UP, c.held);
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_BOTH, DOWN, NO_TIMER),
                     release(UP, 500), NO_OP, CONTROLLER_SINGLE_CONT_ADJ);

    // stopping while the other button is still down waits for both buttons to come up
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_BOTH, UP, NO_TIMER),
                     release(UP, 900), STOP_CONT_INC_CHANNEL_VALUE, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(controller_in(CONTROLLER_SINGLE_CONT_ADJ, HELD_BOTH, DOWN, NO_TIMER),
                     release(DOWN, 900), STOP_CONT_DEC_CHANNEL_VALUE, CONTROLLER_LOCKOUT);
}

static void test_two_click_transitions(void)
{
    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_TWO_CLICK, HELD_BOTH, ANY_BUTTON, 150),
                                           release(UP, 250), NEXT_COLOR_CHANNEL, CONTROLLER_LOCKOUT);
    CHECK_HELD(HELD_DOWN, c.held);
    c = CHECK_TRANSITION(controller_in(CONTROLLER_TWO_CLICK, HELD_BOTH, ANY_BUTTON, 150),
                         release(DOWN, 250), NEXT_COLOR_CHANNEL, CONTROLLER_LOCKOUT);
    CHECK_HELD(HELD_UP, c.held);

    // the short-hold deadline is measured from the second press too, not from the click deadline
    c = CHECK_TRANSITION(controller_in(CONTROLLER_TWO_CLICK, HELD_BOTH, ANY_BUTTON, 150),
                         deadline(350), INDICATE_HOLD_THRESHOLD_PASSED, CONTROLLER_TWO_SHORT_HOLD);
    CHECK_HELD(HELD_BOTH, c.held);
    CHECK_TIME(AT_MS(1150), controller_next_deadline(&c));
}

static void test_two_short_hold_transitions(void)
{
    CHECK_TRANSITION(controller_in(CONTROLLER_TWO_SHORT_HOLD, HELD_BOTH, ANY_BUTTON, 150),
                     release(UP, 600), CONFIRM_GUESS, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(controller_in(CONTROLLER_TWO_SHORT_HOLD, HELD_BOTH, ANY_BUTTON, 150),
                     release(DOWN, 600), CONFIRM_GUESS, CONTROLLER_LOCKOUT);
    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_TWO_SHORT_HOLD, HELD_BOTH, ANY_BUTTON, 150),
                                           deadline(1150), INDICATE_HOLD_THRESHOLD_PASSED, CONTROLLER_TWO_LONG_HOLD);
    CHECK_HELD(HELD_BOTH, c.held);
}

static void test_two_long_hold_transitions(void)
{
    CHECK_TRANSITION(controller_in(CONTROLLER_TWO_LONG_HOLD, HELD_BOTH, ANY_BUTTON, NO_TIMER),
                     release(UP, 1500), START_NEW_GAME, CONTROLLER_LOCKOUT);
    CHECK_TRANSITION(controller_in(CONTROLLER_TWO_LONG_HOLD, HELD_BOTH, ANY_BUTTON, NO_TIMER),
                     release(DOWN, 1500), START_NEW_GAME, CONTROLLER_LOCKOUT);
}

// Everything is ignored until both buttons are up, but presses and releases are still tracked.
static void test_lockout_transitions(void)
{
    CHECK_TRANSITION(controller_in(CONTROLLER_LOCKOUT, HELD_UP, ANY_BUTTON, NO_TIMER),
                     release(UP, 2000), NO_OP, CONTROLLER_IDLE);
    CHECK_TRANSITION(controller_in(CONTROLLER_LOCKOUT, HELD_DOWN, ANY_BUTTON, NO_TIMER),
                     release(DOWN, 2000), NO_OP, CONTROLLER_IDLE);

    struct controller c = CHECK_TRANSITION(controller_in(CONTROLLER_LOCKOUT, HELD_BOTH, ANY_BUTTON, NO_TIMER),
                                           release(UP, 2000), NO_OP, CONTROLLER_LOCKOUT);
    CHECK_HELD(HELD_DOWN, c.held);
    c = CHECK_TRANSITION(controller_in(CONTROLLER_LOCKOUT, HELD_DOWN, ANY_BUTTON, NO_TIMER),
                         press(UP, 2000), NO_OP, CONTROLLER_LOCKOUT);
    CHECK_HELD(HELD_BOTH, c.held);
}

//////////////////////////////////////////
// SECTION: whole gestures, end to end //
//////////////////////////////////////////

static void test_gesture_up_click(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, release(UP, 100));
    CHECK_LOG(&s, {INCREMENT_CHANNEL_VALUE, 100});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_down_click(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(DOWN, 0));
    sim_input(&s, release(DOWN, 100));
    CHECK_LOG(&s, {DECREMENT_CHANNEL_VALUE, 100});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_up_hold(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, release(UP, 1500));
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_down_hold(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(DOWN, 0));
    sim_input(&s, release(DOWN, 1500));
    CHECK_LOG(&s, {START_CONT_DEC_CHANNEL_VALUE, 200}, {STOP_CONT_DEC_CHANNEL_VALUE, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_other_button_tapped_during_hold_is_ignored(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 300));
    sim_input(&s, release(DOWN, 500));
    sim_input(&s, release(UP, 900));
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 900});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_hold_released_while_other_held_waits_for_both_up(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 300));
    sim_input(&s, release(UP, 600));
    CHECK_STATE(CONTROLLER_LOCKOUT, s.c.state);
    sim_input(&s, release(DOWN, 2000)); // down held well past every threshold: still nothing
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);

    sim_input(&s, press(UP, 3000));
    sim_input(&s, release(UP, 3100));
    CHECK_LOG(&s, {START_CONT_INC_CHANNEL_VALUE, 200}, {STOP_CONT_INC_CHANNEL_VALUE, 600}, {INCREMENT_CHANNEL_VALUE, 3100});
}

static void test_gesture_two_button_click_moves_to_next_channel(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 150));
    sim_input(&s, release(UP, 250));
    sim_input(&s, release(DOWN, 300));
    CHECK_LOG(&s, {NEXT_COLOR_CHANNEL, 250});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

// The click window restarts at the second press: 190ms + 180ms is past 200ms from the first press, but still a click.
static void test_gesture_two_button_timing_starts_at_second_press(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 190));
    sim_input(&s, release(UP, 370));
    CHECK_LOG(&s, {NEXT_COLOR_CHANNEL, 370});
}

static void test_gesture_two_button_short_hold_confirms_guess(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(DOWN, 0));
    sim_input(&s, press(UP, 150));
    sim_input(&s, release(DOWN, 600));
    sim_input(&s, release(UP, 650));
    CHECK_LOG(&s, {INDICATE_HOLD_THRESHOLD_PASSED, 350}, {CONFIRM_GUESS, 600});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_two_button_long_hold_starts_new_game(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 150));
    sim_input(&s, release(UP, 1500));
    sim_input(&s, release(DOWN, 1600));
    CHECK_LOG(&s, {INDICATE_HOLD_THRESHOLD_PASSED, 350}, {INDICATE_HOLD_THRESHOLD_PASSED, 1150}, {START_NEW_GAME, 1500});
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);
}

static void test_gesture_lockout_after_two_button_gesture(void)
{
    struct sim s;
    sim_init(&s);
    sim_input(&s, press(UP, 0));
    sim_input(&s, press(DOWN, 150));
    sim_input(&s, release(UP, 250));  // NEXT_COLOR_CHANNEL, down still held
    sim_input(&s, press(UP, 3000));   // down held far past every threshold, then up pressed and released again
    sim_input(&s, release(UP, 3200));
    sim_input(&s, release(DOWN, 3300));
    CHECK_STATE(CONTROLLER_IDLE, s.c.state);

    sim_input(&s, press(UP, 4000));
    sim_input(&s, release(UP, 4050));
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
