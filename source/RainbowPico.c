#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <math.h>
#include "Hardware.h"
#include "ComponentControl.h"

// SECTION: COLOUR STUFF

// Perceptual gamma - human brightness perception is nonlinear (~2.2 exponent is standard)
uint8_t apply_gamma(uint8_t linear_value) {
    return (uint8_t)(powf(linear_value / 255.0f, 2.2f) * 255.0f + 0.5f);
}

// Precompute a gamma correction table to avoid calling powf every main loop iteration.
uint8_t GAMMA_TABLE[256];

void compute_gamma_table(void) {
    for (int i = 0; i < 256; ++i) {
        GAMMA_TABLE[i] = apply_gamma(i);
    }
}

uint8_t correct_channel(uint8_t raw, float channel_scale) {
    uint8_t scaled = (uint8_t)(raw * channel_scale);
    return GAMMA_TABLE[scaled];
}

// Compensaste for different LEDs having different brightnesses at the same PWM level. These values are determined experimentally.
#define RED_CHANNEL_SCALE 1.0f
#define GREEN_CHANNEL_SCALE 0.7f
#define BLUE_CHANNEL_SCALE 1.0f


///////////////////////////
// SECTION: MAIN PROGRAM //
///////////////////////////
int main()
{
    stdio_init_all();

    // Turn the default LED on the Pico board on to indicate that the program is running
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, HIGH);

    // Initialize components
    init_led_pin(RED_LED_PIN, LOW);
    init_led_pin(GREEN_LED_PIN, LOW);
    init_led_pin(BLUE_LED_PIN, LOW);


    init_button_pin(UP_BUTTON_PIN);
    init_button_pin(DOWN_BUTTON_PIN);

    struct rgb_led target_rgb_led = build_rgb_led(TARGET_RGB_LED_R_PIN, TARGET_RGB_LED_G_PIN, TARGET_RGB_LED_B_PIN);
    struct rgb_led play_rgb_led = build_rgb_led(PLAY_RGB_LED_R_PIN, PLAY_RGB_LED_G_PIN, PLAY_RGB_LED_B_PIN);

    init_rgb_led(target_rgb_led);
    init_rgb_led(play_rgb_led);


    // DEBUG
    set_led_rgb_hue(target_rgb_led, (struct rgb_color){.red = 255, .green = 125, .blue = 0});
    set_led_rgb_hue(play_rgb_led, (struct rgb_color){.red = 50, .green = 50, .blue = 50});
    turn_led_on(BLUE_LED_PIN);

    // Initialize state variables

    // Precomputations
    // compute_gamma_table();

    // Main loop
    while (1) {
    }
}