#include "Controller.h"
#include <stdio.h>

char *controller_state_to_string(controller_state state) {
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

static void button_to_string(const struct button *button, char *out, size_t out_len)
{
    if (button->is_pressed)
    {
        snprintf(out, out_len, "pressed@%llums", (unsigned long long)(button->time_press / 1000));
    }
    else
    {
        snprintf(out, out_len, "released");
    }
}

char *controller_to_string(struct controller *controller)
{
    static char buf[128];
    char up[48];
    char down[48];
    button_to_string(&controller->up, up, sizeof up);
    button_to_string(&controller->down, down, sizeof down);
    snprintf(buf, sizeof buf, "up{%s} down{%s} state=%s", up, down, controller_state_to_string(controller->state));
    return buf;
}

char *game_action_to_string(game_action action) {
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