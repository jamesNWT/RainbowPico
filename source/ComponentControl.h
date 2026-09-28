/* Contains definitions for the control of hardware components for the
 * RainbowPico project.
*/
#ifndef COMPONENTCONTROL_H
#define COMPONENTCONTROL_H


#include "Hardware.h"
#include "pico/time.h"

void set_led_rgb_hue(struct rgb_led led);

void turn_rgb_led_on(struct rgb_led led);

void turn_rgb_led_off(struct rgb_led led);

void turn_led_on(uint pin);

void turn_led_off(uint pin);

bool is_button_pressed(uint pin);

bool is_button_clicked(uint pin);

#endif