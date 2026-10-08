#include <stdio.h>
#include "Controller.h"
#include "Diagnostics.h"

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

void controller_init(struct controller *controller)
{
    *controller = (struct controller){
        .up = {
            .is_pressed = false,
            .time_press = 0,
        },
        .down = {
            .is_pressed = false,
            .time_press = 0,
        },
        .state = CONTROLLER_IDLE
    };
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

void update_controller_button(struct button *button, uint64_t time, bool is_press) {
    if (is_press) {
        button->is_pressed = true;
        button->time_press = time;
    } else {
        button->is_pressed = false;
        button->time_press = ABSOLUTE_TIME_MAX;
    }
}

uint64_t controller_next_deadline(const struct controller *controller) {
    uint64_t last_press;
    switch(controller->state) {
        case CONTROLLER_SINGLE_CLICK:
            if (controller->up.is_pressed) {
                return controller->up.time_press + CLICK_CEILING_US;
            } else {
                return controller->down.time_press + CLICK_CEILING_US;
            }
        case CONTROLLER_TWO_CLICK:
            last_press = MAX(controller->down.time_press, controller->up.time_press);
            return last_press + CLICK_CEILING_US;
        case CONTROLLER_TWO_SHORT_HOLD:
            last_press = MAX(controller->down.time_press, controller->up.time_press);
            return last_press + SHORT_HOLD_CEILING_US;
        default:
            return ABSOLUTE_TIME_MAX;
    }
}

game_action controller_handle(struct controller *controller, struct controller_input input) {
    switch (controller->state) {
        case CONTROLLER_IDLE:
            switch (input.event_type) {
                case EV_DOWN_PRESSED:
                    controller->down.is_pressed = true;
                    controller->down.time_press = input.event_time;
                    controller->state = CONTROLLER_SINGLE_CLICK;
                    break;
                case EV_UP_PRESSED:
                    controller->up.is_pressed = true;
                    controller->up.time_press = input.event_time;
                    controller->state = CONTROLLER_SINGLE_CLICK;
                    break;
                default:
                    break;
            }
            break;
        case CONTROLLER_SINGLE_CLICK:
            switch (input.event_type) {
                case EV_UP_RELEASED:
                    controller->state = CONTROLLER_IDLE;
                    return INCREMENT_CHANNEL_VALUE;
                case EV_DOWN_RELEASED:
                    controller->state = CONTROLLER_IDLE;
                    return DECREMENT_CHANNEL_VALUE;
                case EV_DEADLINE:
                    controller->state = CONTROLLER_SINGLE_CONT_ADJ;
                    // if (controller->up.is_pressed && controller->down.is_pressed) {
                    //     return controller->up.time_press < controller->down.time_press ? START_CONT_INC_CHANNEL_VALUE : START_CONT_DEC_CHANNEL_VALUE;
                    // }
                    return controller->up.time_press < controller->down.time_press ? START_CONT_INC_CHANNEL_VALUE : START_CONT_DEC_CHANNEL_VALUE;
                default:
                    break;
            }
        case CONTROLLER_SINGLE_CONT_ADJ:
            break;
        case CONTROLLER_TWO_CLICK:
            break;
        case CONTROLLER_TWO_SHORT_HOLD:
            break;
        case CONTROLLER_TWO_LONG_HOLD:
            break;
        case CONTROLLER_LOCKOUT:
            break;
        default:
            break;
    }
    return NO_OP;
}