#include "Hardware.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

void init_led_pin(uint pin, bool initial_state) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, initial_state);
}

void init_button_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

// Initialize the configuration of a PWM slice.
void init_pwm_slice(uint slice_num, bool invert_a, bool invert_b) {
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255); // Set wrap value for 8-bit resolution
    pwm_config_set_clkdiv(&config, 100.f); // Set clock divider to slow pwm frequency to ~5kHz (125MHz / 256 / 100 = ~5kHz)

    pwm_config_set_output_polarity(&config, invert_a, invert_b);
    pwm_init(slice_num, &config, true);
}