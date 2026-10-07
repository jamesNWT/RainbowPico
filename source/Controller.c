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