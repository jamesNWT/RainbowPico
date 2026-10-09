#include "Controller.h"
#include <stdio.h>

char *controller_state_to_string(controller_state_kind state) {
    switch (state) {
        case CONTROLLER_IDLE:
            return "IDLE";
        case CONTROLLER_SINGLE_CLICK:
            return "ONE CLICK";
        case CONTROLLER_SINGLE_CONT_ADJ:
            return  "CONTINUOUS ADJUSTMENT";
        case CONTROLLER_TWO_CLICK:
            return "TWO BUTTON CLICK";
        case CONTROLLER_TWO_SHORT_HOLD:
            return "TWO BUTTON SHORT HOLD";
        case CONTROLLER_TWO_LONG_HOLD:
            return "TWO BUTTON LONG HOLD";
        case CONTROLLER_LOCKOUT:
            return "LOCK OUT";
        default:
            return "UNEXPECTED";
    }
}

char *controller_to_string(struct controller *controller)
{
    static char buf[256];
    char up[48];
    char down[48];
    char active[48];
    snprintf(up, sizeof up, "%s", controller->held & (1u << UP_BUTTON_INDEX) ? "pressed" : "released");
    snprintf(down, sizeof down, "%s", controller->held & (1u << DOWN_BUTTON_INDEX) ? "pressed" : "released");
    snprintf(active, sizeof up, "%s", controller->active_button == UP_BUTTON_INDEX ? "up" : "down");

    snprintf(buf, sizeof buf, "up{%s} down{%s}, active_button=%s, timer_start={%llu}, state=%s", up, down, active, (controller->timer_start / 1000), controller_state_to_string(controller->state));
    return buf;
}

char *game_action_to_string(game_action_kind action) {
    switch (action) {
        case INCREMENT_CHANNEL_VALUE:
            return "INCREMENT_CHANNEL_VALUE";
        case DECREMENT_CHANNEL_VALUE:
            return "DECREMENT_CHANNEL_VALUE";
        case START_CONT_INC_CHANNEL_VALUE:
            return "START_CONT_INC_CHANNEL_VALUE";
        case STOP_CONT_INC_CHANNEL_VALUE:
            return "STOP_CONT_INC_CHANNEL_VALUE";
        case START_CONT_DEC_CHANNEL_VALUE:
            return "START_CONT_DEC_CHANNEL_VALUE";
        case STOP_CONT_DEC_CHANNEL_VALUE:
            return "STOP_CONT_DEC_CHANNEL_VALUE";
        case NEXT_COLOR_CHANNEL:
            return "NEXT_COLOR_CHANNEL";
        case CONFIRM_GUESS:
            return "CONFIRM_GUESS";
        case START_NEW_GAME:
            return "START_NEW_GAME";
        case INDICATE_HOLD_THRESHOLD_PASSED:
            return "INDICATE_HOLD_THRESHOLD_PASSED";
        case NO_OP:
            return "NO_OP";
        default:
            return "UNDEFINED GAME ACTION";
    }
}