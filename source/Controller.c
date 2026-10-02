#include "Controller.h"

void controller_init(struct controller_state *controller)
{
    *controller = (struct controller_state){
        .up = {
            .is_pressed = false,
            .time_press = 0,
            .last_crossed = NONE
        },
        .down = {
            .is_pressed = false,
            .time_press = 0,
            .last_crossed = NONE
        },
        .is_cont_adj = false
    };
}

threshold get_last_threshold_crossed(uint64_t dur_us)
{
    if (dur_us == UINT64_MAX){ // UINT64_MAX is our indication that the button has not been pressed.
        return NONE;
    }
    if (dur_us <= SHORT_HOLD_CEILING_US)
    {
        return NONE;
    }
    if (dur_us > SHORT_HOLD_CEILING_US && dur_us <= MEDIUM_HOLD_CEILING_US)
    {
        return SHORT;
    }
    if (dur_us > MEDIUM_HOLD_CEILING_US && dur_us <= LONG_HOLD_FLOOR_US)
    {
        return MEDIUM;
    }
    return LONG;
}

uint64_t get_us_until_next_threshold(uint64_t dur_us)
{
    if (dur_us == UINT64_MAX){ // UINT64_MAX is our indication that the button has not been pressed.
        return ABSOLUTE_TIME_MAX;
    }
    threshold last_threshold = get_last_threshold_crossed(dur_us);
    switch (last_threshold)
    {
    case NONE:
        return SHORT_HOLD_CEILING_US - dur_us;
    case SHORT:
        return MEDIUM_HOLD_CEILING_US - dur_us;
    case MEDIUM:
        return LONG_HOLD_FLOOR_US - dur_us;
    case LONG:
        return ABSOLUTE_TIME_MAX;
    default:
        return ABSOLUTE_TIME_MAX;
    }
}

uint64_t controller_us_until_next_threshold(const struct controller_state *controller, uint64_t now)
{
    // calculate the button hold durations, letting UINT64_MAX be the value for buttons that are not pressed.
    uint64_t up_dur = controller->up.is_pressed ? now - controller->up.time_press : UINT64_MAX;
    uint64_t down_dur = controller->down.is_pressed ? now - controller->down.time_press : UINT64_MAX;
    uint64_t up_us = get_us_until_next_threshold(up_dur);
    uint64_t down_us = get_us_until_next_threshold(down_dur);
    return up_us < down_us ? up_us : down_us;
}

void update_controller_button_state(struct button *button, struct button_event *event)
{
    if (event->is_pressed) {
        button->is_pressed = true;
        button->time_press = event->time_changed;
    } else {
        button->is_pressed = false;
        button->time_press = ABSOLUTE_TIME_MAX;
    }
}

game_action get_game_action_from_double_press_release(threshold last_crossed_1, threshold last_crossed_2)
{
    threshold shortest_threshold_reached = LONG;
    if (last_crossed_1 <= last_crossed_2)
    {
        shortest_threshold_reached = last_crossed_1;
    }
    else
    {
        shortest_threshold_reached = last_crossed_2;
    }
    switch (shortest_threshold_reached)
    {
    case NONE:
        return SWTICH_COLOR_CHANNEL;
    case SHORT:
        return CONFIRM_GUESS;
    case MEDIUM:
        return START_NEW_GAME;
    case LONG:
        return START_NEW_GAME;
    default:
        return START_NEW_GAME;
    }
}

game_action button_event_controller_handler(struct controller_state *controller, struct button_event *but_event)
{
    game_action ret = NO_OP;
    switch (but_event->pin) {
        case UP_BUTTON_PIN:
            if (but_event->is_pressed) { // PRESS EVENT
                if (!controller->down.is_pressed) {
                    ret = INCREMENT_COLOR_CHANNEL;
                }
            } else { // RELEASE EVENT
                if (controller->is_cont_adj) {
                    if (controller->up.is_pressed) { // CONTINUOUS INCREMENT STOPPED
                        ret = STOP_CONT_INC_COLOR_CHANNEL;
                        controller->is_cont_adj = false;
                    }
                } else if (controller->down.is_pressed){ // RELEASE HAPPENS FROM DOUBLE PRESS STATE, AND WE'RE NOT IN CONTINUOUS ADJUSTMENT MODE
                    ret = get_game_action_from_double_press_release(controller->up.last_crossed, controller->down.last_crossed);
                }
            }
            update_controller_button_state(&controller->up, but_event);
            break;
        case DOWN_BUTTON_PIN:
            if (but_event->is_pressed) {
                if (!controller->up.is_pressed) {
                    ret = DECREMENT_COLOR_CHANNEL;
                }
            } else { // RELEASE EVENT
                if (controller->is_cont_adj) {
                    if (controller->down.is_pressed) { // CONTINUOUS DECREMENT STOPPED
                        ret = STOP_CONT_DEC_COLOR_CHANNEL;
                        controller->is_cont_adj = false;
                    }
                } else if (controller->up.is_pressed){ // RELEASE HAPPENS FROM DOUBLE PRESS STATE, AND WE'RE NOT IN CONTINUOUS ADJUSTMENT MODE
                    ret = get_game_action_from_double_press_release(controller->up.last_crossed, controller->down.last_crossed);
                }
            }
            update_controller_button_state(&controller->down, but_event);
            break;
        default:
            break;
    }
    return ret;
}

struct threshold_action_and_trigger threshold_event_controller_handler(struct controller_state *controller, uint64_t event_time)
{

    // calculate the button hold durations, letting UINT64_MAX be the value for buttons that are not pressed.
    uint64_t button_duration_up = controller->up.is_pressed ? event_time - controller->up.time_press : UINT64_MAX;
    uint64_t button_duration_down = controller->down.is_pressed ? event_time - controller->down.time_press : UINT64_MAX;
    threshold up_threshold_check = get_last_threshold_crossed(button_duration_up);
    threshold down_threshold_check = get_last_threshold_crossed(button_duration_down);

    struct threshold_action_and_trigger ret;

    if (up_threshold_check != controller->up.last_crossed && up_threshold_check == SHORT && !controller->is_cont_adj)
    {
        ret.action = START_CONT_INC_COLOR_CHANNEL;
        ret.trigger = TT_UP_BUTTON;
        controller->up.last_crossed = up_threshold_check;
        controller->is_cont_adj = true;
        return ret;
    }
    else if (down_threshold_check != controller->down.last_crossed && down_threshold_check == SHORT && !controller->is_cont_adj)
    {
        ret.action = START_CONT_DEC_COLOR_CHANNEL;
        ret.trigger = TT_DOWN_BUTTON;
        controller->down.last_crossed = down_threshold_check;
        controller->is_cont_adj = true;
        return ret;
    }
    
    if ((down_threshold_check != controller->down.last_crossed || up_threshold_check != controller->down.last_crossed) && !controller->is_cont_adj)
    {
        // only indicate threshold passed if both buttons are held down, and we're not in continuous adjustment mode
        if (controller->down.is_pressed && controller->up.is_pressed)
        {
            bool down_trailing = controller->down.time_press < controller->up.time_press;
            // furthermore, only indicate threshold passed for the trailing button in the double press.
            if (down_trailing && down_threshold_check != controller->down.last_crossed)
            {
                ret.action = INDICATE_HOLD_THRESHOLD_PASSED;
                ret.trigger = TT_DOWN_BUTTON;
            }
            else if (!down_trailing && up_threshold_check != controller->up.last_crossed)
            {
                ret.action = INDICATE_HOLD_THRESHOLD_PASSED;
                ret.trigger = TT_UP_BUTTON;
            }
        }
    }
    else
    {
        ret.action = NO_OP;
        if (up_threshold_check != controller->up.last_crossed)
        {
            ret.trigger = TT_UP_BUTTON;
        }
        else if (down_threshold_check != controller->down.last_crossed)
        {
            ret.trigger = TT_DOWN_BUTTON;
        }
        else
        {
            ret.trigger = TT_MAX_DELAY;
        }
    }
    return ret;
}
