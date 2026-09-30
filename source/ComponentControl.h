/* Contains definitions for the control of hardware components for the
 * RainbowPico project.
*/
#ifndef COMPONENTCONTROL_H
#define COMPONENTCONTROL_H

#include "Hardware.h"
#include "pico/time.h"
#include "Color.h"

void set_led_rgb_hue(struct rgb_led led, struct rgb_color color);

void turn_led_on(uint pin);

void turn_led_off(uint pin);

#endif