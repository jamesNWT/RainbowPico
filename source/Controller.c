#include "Controller.h"
#include "Diagnostics.h"
#include <assert.h>
#include <stdio.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// table-driven way to decide the game_action, where we index with the button as
// defined in the struct controller.active_button enum.
static const game_action_kind CLICK_ACTION[2] = {INCREMENT_CHANNEL_VALUE, DECREMENT_CHANNEL_VALUE};
static const game_action_kind START_CONT_ACTION[2] = {START_CONT_INC_CHANNEL_VALUE, START_CONT_DEC_CHANNEL_VALUE};
static const game_action_kind STOP_CONT_ACTION[2] = {STOP_CONT_INC_CHANNEL_VALUE, STOP_CONT_DEC_CHANNEL_VALUE};

void controller_init(struct controller *controller) {
  *controller = (struct controller){.held = HELD_NONE, .state = CONTROLLER_IDLE, .active_button = 0, .timer_start = 0};
}

uint64_t controller_next_deadline(const struct controller *controller) {
  switch (controller->state) {
  case CONTROLLER_SINGLE_CLICK:
  case CONTROLLER_TWO_CLICK:
    return controller->timer_start + CLICK_CEILING_US;
  case CONTROLLER_TWO_SHORT_HOLD:
    return controller->timer_start + SHORT_HOLD_CEILING_US;
  default:
    return ABSOLUTE_TIME_MAX;
  }
}

game_action_kind controller_handle(struct controller *controller, struct controller_input input) {
  // regardless of state change and next action, update the button mask if this
  // is a button input:
  if (input.event_type != EV_DEADLINE) {
    uint8_t button_mask = 1u << input.affect_button; // affect_button works as
                                                     // index for the bit mask too!
    if (input.event_type == EV_PRESS) {
      controller->held |= button_mask;
    } else {
      controller->held &= ~button_mask;
    }
  }

  // we'll handle every input type for every state explicitly, even input types
  // that should be impossible to trigger in certain states. we should start
  // timers when we change the value of controller->state to a state that has a
  // deadline.
  switch (controller->state) {
  case CONTROLLER_IDLE:
    switch (input.event_type) {
    case EV_DEADLINE:
    case EV_RELEASE:
      return NO_OP;
    case EV_PRESS:
      controller->state = CONTROLLER_SINGLE_CLICK;
      controller->active_button = input.affect_button;
      controller->timer_start = input.event_time;
      return NO_OP;
    }
    break;
  case CONTROLLER_SINGLE_CLICK:
    switch (input.event_type) {
    case EV_DEADLINE:
      controller->state = CONTROLLER_SINGLE_CONT_ADJ;
      return START_CONT_ACTION[controller->active_button];
    case EV_RELEASE:
      controller->state = CONTROLLER_IDLE;
      return CLICK_ACTION[controller->active_button];
    case EV_PRESS:
      assert(input.affect_button != controller->active_button);
      controller->state = CONTROLLER_TWO_CLICK;
      controller->timer_start = input.event_time;
      return NO_OP;
    }
    break;
  case CONTROLLER_SINGLE_CONT_ADJ:
    switch (input.event_type) {
    case EV_DEADLINE:
    case EV_PRESS:
      return NO_OP;
    case EV_RELEASE:
      if (input.affect_button == controller->active_button) {
        // lockout if the non-affect button is pressed, otherwise idle
        if (controller->held & ~(1u << input.affect_button)) {
          controller->state = CONTROLLER_LOCKOUT;
        } else {
          controller->state = CONTROLLER_IDLE;
        }
        return STOP_CONT_ACTION[controller->active_button];
      } else {
        return NO_OP;
      }
    }
    break;
  case CONTROLLER_TWO_CLICK:
    assert(input.event_type != EV_PRESS); // it should be impossible for us to get a press event
                                          // when both buttons are held!
    switch (input.event_type) {
    case EV_PRESS:
      return NO_OP;
    case EV_RELEASE:
      controller->state = CONTROLLER_LOCKOUT;
      return NEXT_COLOR_CHANNEL;
    case EV_DEADLINE:
      controller->state = CONTROLLER_TWO_SHORT_HOLD;
      return INDICATE_HOLD_THRESHOLD_PASSED;
    }
    break;
  case CONTROLLER_TWO_SHORT_HOLD:
    assert(input.event_type != EV_PRESS); // it should be impossible for us to get a press event
                                          // when both buttons are held!
    switch (input.event_type) {
    case EV_PRESS:
      return NO_OP;
    case EV_RELEASE:
      controller->state = CONTROLLER_LOCKOUT;
      return CONFIRM_GUESS;
    case EV_DEADLINE:
      controller->state = CONTROLLER_TWO_LONG_HOLD;
      return INDICATE_HOLD_THRESHOLD_PASSED;
    }
    break;
  case CONTROLLER_TWO_LONG_HOLD:
    assert(input.event_type != EV_PRESS); // it should be impossible for us to get a press event
                                          // when both buttons are held!
    switch (input.event_type) {
    case EV_DEADLINE:
    case EV_PRESS:
      return NO_OP;
    case EV_RELEASE:
      controller->state = CONTROLLER_LOCKOUT;
      return START_NEW_GAME;
    }
    break;
  case CONTROLLER_LOCKOUT:
    switch (input.event_type) {
    case EV_PRESS:
    case EV_DEADLINE:
      return NO_OP;
    case EV_RELEASE:
      if (controller->held == HELD_NONE) {
        controller->state = CONTROLLER_IDLE;
      }
      return NO_OP;
    }
    break;
  default:
    printf("Never should have come here!");
    return NO_OP;
  }
  printf("Never should have come here!");
  return NO_OP;
}