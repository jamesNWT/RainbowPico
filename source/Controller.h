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

#define CLICK_CEILING_US (200 * 1000)
#define SHORT_HOLD_CEILING_US (1000 * 1000)
#define LONG_HOLD_FLOOR_US SHORT_HOLD_CEILING_US // pedantic but maybe helpful for reasoning.

// Used both as "button is not pressed" for a duration/press time and "no deadline" for a wait.
#define ABSOLUTE_TIME_MAX UINT64_MAX

struct button_event
{
    unsigned int pin;
    bool is_pressed;
    uint64_t time_changed;
};

struct button {
    bool is_pressed;
    uint64_t time_press;
};

typedef enum
{
    CONTROLLER_IDLE,
    CONTROLLER_SINGLE_CLICK,
    CONTROLLER_SINGLE_CONT_ADJ,
    CONTROLLER_TWO_CLICK, // Two as in two buttons pressed, as opposed to calling it "double click"
    CONTROLLER_TWO_SHORT_HOLD,
    CONTROLLER_TWO_LONG_HOLD,
    CONTROLLER_LOCKOUT
} controller_state_kind;

// this enum can give a name to both the index for our action arrays, and the position in the bit mask for the buttons.
typedef enum {
    UP_BUTTON_INDEX = 0u,
    DOWN_BUTTON_INDEX = 1u,
} button_index;

struct controller
{
    enum {
        HELD_NONE = 0u,
        HELD_UP = 1u << UP_BUTTON_INDEX, 
        HELD_DOWN = 1u << DOWN_BUTTON_INDEX
    } held; // bitmask indicating which buttons are held. eg. 00 = none, 01 = up, 11 = both.
    controller_state_kind state;
    button_index active_button; // meaningful in single-click and continuous-adjust states.
    uint64_t timer_start; // meaningful in states that have a deadline
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
} game_action_kind;

typedef enum
{
    EV_PRESS,
    EV_RELEASE,
    EV_DEADLINE
} controller_event;

struct controller_input {
    controller_event event_type;
    uint64_t event_time;
    button_index affect_button;
};

void controller_init(struct controller *controller);

game_action_kind controller_handle(struct controller *controller, struct controller_input input);

uint64_t controller_next_deadline(const struct controller *controller);

#endif
