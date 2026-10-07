/* Button controller state machine: turns debounced button events and hold-time
 * deadlines into game actions.
 *
 * Must stay free of Pico SDK and FreeRTOS includes so it can be compiled and unit
 * tested on the host (see tests/). Times are microseconds since boot as plain
 * uint64_t, which is what absolute_time_t is in the default SDK configuration.
*/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "Pins.h"

#define SHORT_HOLD_CEILING_US (500 * 1000)
#define MEDIUM_HOLD_CEILING_US (1000 * 1000)
#define LONG_HOLD_FLOOR_US (2500 * 1000)

// Used both as "button is not pressed" for a duration/press time and "no deadline" for a wait.
#define ABSOLUTE_TIME_MAX UINT64_MAX

struct button_event
{
    unsigned int pin;
    bool is_pressed;
    uint64_t time_changed;
};

typedef enum
{
    NONE = 0,
    SHORT = 1,
    MEDIUM = 2,
    LONG = 3
} threshold;

struct button {
    bool is_pressed;
    threshold last_crossed;
    uint64_t time_press;
};

struct controller
{
    struct button up;
    struct button down;
    enum {
        IDLE,
        SINGLE_CLICK,
        SINGLE_CONT_ADJ,
        DOUBLE_CLICK,
        DOUBLE_SHORT,
        DOUBLE_LONG,
        LOCKOUT
    } state;
};

typedef enum
{
    INCREMENT_CHANNEL_VALUE,
    DECREMENT_CHANNEL_VALUE,
    START_CONT_INC_CHANNEL_VALUE,
    STOP_CONT_INC_CHANNEL_VALUE,
    START_CONT_DEC_CHANNEL_VALUE,
    STOP_CONT_DEC_CHANNEL_VALUE,
    NEXT_COLOR_CHANNEL,
    CONFIRM_GUESS,
    START_NEW_GAME,
    INDICATE_HOLD_THRESHOLD_PASSED,
    NO_OP
} game_action;

enum threshold_trigger
{
    TT_UP_BUTTON,
    TT_DOWN_BUTTON,
    TT_MAX_DELAY
};

struct threshold_action_and_trigger {
    game_action action;
    enum threshold_trigger trigger;
};

void controller_init(struct controller *controller);

threshold get_last_threshold_crossed(uint64_t dur_us);

// Returns ABSOLUTE_TIME_MAX when there is no further threshold to wait for.
uint64_t get_us_until_next_threshold(uint64_t dur_us);

// Earliest threshold deadline across both buttons, or ABSOLUTE_TIME_MAX if there is none.
uint64_t controller_us_until_next_threshold(const struct controller *controller, uint64_t now);

void update_controller_button_state(struct button *button, struct button_event *event);

game_action get_game_action_from_double_press_release(threshold last_crossed_1, threshold last_crossed_2);

game_action button_event_controller_handler(struct controller *controller, struct button_event *but_event);

struct threshold_action_and_trigger threshold_event_controller_handler(struct controller *controller, uint64_t event_time);

char *threshold_to_string(threshold thresh);
char *game_action_to_string(game_action action);
// Returns a static buffer that the next call overwrites, so only call it from one task.
char *controller_to_string(struct controller *controller);

#endif
