#include "Hardware.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

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

struct rgb_led build_rgb_led(uint red_pin, uint green_pin, uint blue_pin) {
  struct rgb_led led;
  led.red.pin = red_pin;
  led.red.pwm_channel = pwm_gpio_to_channel(red_pin);
  led.red.pwm_slice = pwm_gpio_to_slice_num(red_pin);

  led.green.pin = green_pin;
  led.green.pwm_channel = pwm_gpio_to_channel(green_pin);
  led.green.pwm_slice = pwm_gpio_to_slice_num(green_pin);

  led.blue.pin = blue_pin;
  led.blue.pwm_channel = pwm_gpio_to_channel(blue_pin);
  led.blue.pwm_slice = pwm_gpio_to_slice_num(blue_pin);

  return led;
}

static uint8_t initialized_slices_mask = 0; // Track initialized slices globally (8 slices total on RP2040)
static uint16_t inverted_channels_mask = 0; // Track inverted channels globally (2 channels per
                                            // slice, 8 slices total on RP2040)

/* Initialize the configuration of a PWM slice and channel.
 * Is tracking slice initialization overkill? Yes, but its the principle.
 */
void init_pwm_channel(uint slice, enum pwm_chan channel, bool invert) {
  // 1. Handle slice-wide configuration only once
  if (!(initialized_slices_mask & (1 << slice))) {
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255);
    pwm_config_set_clkdiv(&config, 100.f);

    pwm_init(slice, &config, true);

    // Mark slice as configured
    initialized_slices_mask |= (1 << slice);
  }

  // 2. Handle channel polarity configuration. Since we can only set both
  // channels at once, we need to track the state of the other channel.
  if (invert) {
    bool other_channel_inverted = false;
    if (channel == PWM_CHAN_A) {
      other_channel_inverted = (inverted_channels_mask & (1 << (slice * 2 + PWM_CHAN_B))) != 0;
      pwm_set_output_polarity(slice, true, other_channel_inverted);
    } else {
      other_channel_inverted = (inverted_channels_mask & (1 << (slice * 2 + PWM_CHAN_A))) != 0;
      pwm_set_output_polarity(slice, other_channel_inverted, true);
    }
    inverted_channels_mask |= (1 << (slice * 2 + channel));
  }
}

void init_rgb_led(struct rgb_led led) {

  gpio_set_function(led.red.pin, GPIO_FUNC_PWM);
  gpio_set_function(led.green.pin, GPIO_FUNC_PWM);
  gpio_set_function(led.blue.pin, GPIO_FUNC_PWM);

  // Initialize the PWM slices for each color channel
  init_pwm_channel(led.red.pwm_slice, led.red.pwm_channel, true);
  init_pwm_channel(led.green.pwm_slice, led.green.pwm_channel, true);
  init_pwm_channel(led.blue.pwm_slice, led.blue.pwm_channel, true);

  // Set the initial color to off (0, 0, 0)
  pwm_set_chan_level(led.red.pwm_slice, led.red.pwm_channel, 0);
  pwm_set_chan_level(led.green.pwm_slice, led.green.pwm_channel, 0);
  pwm_set_chan_level(led.blue.pwm_slice, led.blue.pwm_channel, 0);
}