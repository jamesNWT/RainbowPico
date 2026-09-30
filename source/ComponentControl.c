#include "ComponentControl.h"
#include "Hardware.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include "Color.h"

void turn_led_on(uint pin) {
    gpio_put(pin, HIGH);
}

void turn_led_off(uint pin) {
    gpio_put(pin, LOW);
}

void set_led_rgb_hue(struct rgb_led led, struct rgb_color color) {
    pwm_set_chan_level(led.red.pwm_slice, led.red.pwm_channel, correct_channel(color.red, RED_CHANNEL_SCALE));
    pwm_set_chan_level(led.green.pwm_slice, led.green.pwm_channel, correct_channel(color.green, GREEN_CHANNEL_SCALE));
    pwm_set_chan_level(led.blue.pwm_slice, led.blue.pwm_channel, correct_channel(color.blue, BLUE_CHANNEL_SCALE));
}