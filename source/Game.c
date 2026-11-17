#include "Color.h"
#include "ComponentControl.h"

void start_new_game(struct rgb_led *target_led, struct rgb_led *play_led) {
  struct rgb_color target_color = {255, 0, 0};
  struct rgb_color play_color = {0, 255, 0};

  set_led_rgb_hue(target_led, &target_color);
  set_led_rgb_hue(play_led, &play_color);
}