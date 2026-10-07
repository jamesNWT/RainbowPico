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

#define FAIL_LINE() printf("    line %d: ", __LINE__)

// Prints times as ms after T0 (or "none" for ABSOLUTE_TIME_MAX) so failures read like the test.
static void print_time(uint64_t t);

#define CHECK_TIME(expected, actual) do { \
        uint64_t e_ = (expected), a_ = (actual); \
        if (e_ != a_) { FAIL_LINE(); printf("%s: expected ", #actual); print_time(e_); printf(", got "); print_time(a_); printf("\n"); checks_failed++; } \
    } while (0)

#define RUN_TEST(test) do { \
        checks_failed = 0; \
        test(); \
        tests_run++; \
        if (checks_failed) { tests_failed++; printf("FAIL  %s\n\n", #test); } \
        else { printf("ok    %s\n", #test); } \
    } while (0)

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

    printf("\n%d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}
