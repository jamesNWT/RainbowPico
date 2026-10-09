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
    
    // button bookkeeping
    if (input.event_type == EV_UP_PRESSED || input.event_type == EV_UP_RELEASED) {
        update_controller_button(&controller->up, input.event_time, input.event_type == EV_UP_PRESSED);
    } else if (input.event_type == EV_DOWN_PRESSED || input.event_type == EV_DOWN_RELEASED) {
        update_controller_button(&controller->down, input.event_time, input.event_type == EV_DOWN_PRESSED);
    }

    switch (controller->state) {
        case CONTROLLER_IDLE:
            switch (input.event_type) {
                case EV_DOWN_PRESSED:
                    controller->state = CONTROLLER_SINGLE_CLICK;
                    break;
                case EV_UP_PRESSED:
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
                case EV_UP_PRESSED:
                case EV_DOWN_PRESSED:
                    controller->state = CONTROLLER_TWO_CLICK;
                    return NO_OP;
                case EV_DEADLINE:
                    controller->state = CONTROLLER_SINGLE_CONT_ADJ;
                    return controller->up.time_press < controller->down.time_press ? START_CONT_INC_CHANNEL_VALUE : START_CONT_DEC_CHANNEL_VALUE;
                default:
                    break;
            }
            break;
        case CONTROLLER_SINGLE_CONT_ADJ:
            bool was_continuous_decrement = controller->down.time_press < controller->up.time_press;
            switch (input.event_type) {
            }
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