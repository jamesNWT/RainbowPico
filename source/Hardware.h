/* Contains hardware configuration and initialization definitions specific to 
 * the RainbowPico project.
 *
 * PWM Slice documentation:
 * +------+-----------------------+-------------+
 * | GPIO |    PIN description    | PWM channel |
 * +------+-----------------------+-------------+
 * |    6 | target rgb led blue   | 3A          |
 * |    7 | target rgb led green  | 3B          |
 * |    8 | target rgb led red    | 4A          |
 * |   19 | play rgb led blue     | 1B          |
 * |   20 | play rgb led green    | 2A          |
 * |   21 | play rgb led red      | 2B          |
 * +------+-----------------------+-------------+
 *
*/

#ifndef HARDWARE_H
#define HARDWARE_H

#include "pico/stdlib.h"

// Define HIGH and LOW for clarity
#define HIGH 1
#define LOW 0


// Pin definitions
#define RED_LED_PIN 2U
#define GREEN_LED_PIN 3U
#define BLUE_LED_PIN 4U 

#define TARGET_RGB_LED_B_PIN 6U
#define TARGET_RGB_LED_G_PIN 7U
#define TARGET_RGB_LED_R_PIN 8U

#define UP_BUTTON_PIN 14U
#define DOWN_BUTTON_PIN 15U

#define PLAY_RGB_LED_B_PIN 19U
#define PLAY_RGB_LED_G_PIN 20U
#define PLAY_RGB_LED_R_PIN 21U

// Structure to represent a single PWM-controlled LED
struct led_pwm {
    uint pin;
    uint pwm_channel; // NOTE: pico-sdk encodes A as 0 and B as 1.
    uint pwm_slice;
};

// Structure to represent an RGB LED with its associated pwm pins
struct rgb_led {
    struct led_pwm red;
    struct led_pwm green;
    struct led_pwm blue;
};

struct rgb_led build_rgb_led(uint red_pin, uint green_pin, uint blue_pin);

// Hardware initialization functions
void init_button_pin(uint pin);

void init_led_pin(uint pin, bool initial_state);

void init_rgb_led(struct rgb_led led);

// Other hardware-related definition

#define DEBOUNCE_DELAY_MS 20

#endif