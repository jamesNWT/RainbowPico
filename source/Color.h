#ifndef COLOR_H
#define COLOR_H
#include "pico/stdlib.h"

struct rgb_color {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

// Compensate for different LEDs having different brightnesses at the same PWM
// level. These values are determined experimentally.
#define RED_CHANNEL_SCALE 1.0f
#define GREEN_CHANNEL_SCALE 0.7f
#define BLUE_CHANNEL_SCALE 1.0f

uint8_t correct_channel(uint8_t raw, float channel_scale);

void compute_gamma_table(void);

#endif