#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// define device's physical configuration
const uint RED_LED_PIN = 13;
const uint GREEN_LED_PIN = 14;
const uint BLUE_LED_PIN = 15;

const uint ON_OFF_BUTTON_PIN = 0;
const uint COLOUR_BUTTON_PIN = 1;

// Define HIGH and LOW for clarity
const bool HIGH = true;
const bool LOW = false;

// Define the colours we support in the LED
typedef enum { RED, GREEN, BLUE } Colours;

void init_button_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void init_led_pin(uint pin, bool initial_state) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, initial_state);
}

void init_pwm_slice(uint slice_num)
{
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255); // Set wrap value for 8-bit resolution
    pwm_config_set_clkdiv(&config, 97.6875f); // Set clock divider to slow pwm frequency to ~5kHz (125MHz / 256 / 97.6875 = ~5kHz)

    /*
     * Invert output to match active-low LED configuration. we can naively set both channels A and B to inverted since any pin using 
     * PWM on this machine is connected to an active-low LED. We'd have to be more careful if we were using PWM for other purposes, but this is a simple example. 
    */ 
    pwm_config_set_output_polarity(&config, true, true);
    pwm_init(slice_num, &config, true);
}

/*
 * Initialize the PWM for a given pin. The configuration of the PWM is set with the other function, init_pwm_slice, which is called once for each slice. This function is called 
 * for each pin that will use PWM.
 */
void init_led_pin_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Get the channel and slice for the pin. We could set manually by looking up the pin in the datasheet, but this is more flexible.
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    pwm_set_chan_level(slice_num, channel, 0); // Start with LED off
}

void set_led_brightness(uint pin, uint8_t brightness) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(slice_num, channel, brightness);
}

bool poll_button_click(uint pin) {
    bool reading = gpio_get(pin);
    if (reading == 0) {
        sleep_ms(20);
        if (gpio_get(pin) != 0) {
            return false; // was noise, not a real press
        }
        while (gpio_get(pin) == 0) {
            sleep_ms(1);
        }
        return HIGH; // released, click complete
    }
    return LOW;
}

char* colour_to_string(Colours colour) {
    switch (colour) {
        case RED:
            return "Red";
        case GREEN:
            return "Green";
        case BLUE:
            return "Blue";
        default:
            return "Unknown";
    }
}

int next_colour(int *current_colour, Colours colours, int num_colours) {
    *current_colour = (*current_colour + 1) % num_colours;
    printf("Colour changed to: %s\n", colour_to_string(*current_colour));
    return *current_colour;
}

void update_light_state(bool led_on, int current_colour_index) {
    if (led_on) {
        switch (current_colour_index) {
            case 0: // Red
                gpio_put(RED_LED_PIN, LOW);

                gpio_put(GREEN_LED_PIN, HIGH);
                gpio_put(BLUE_LED_PIN, HIGH);
                break;
            case 1: // Green
                gpio_put(GREEN_LED_PIN, LOW);

                gpio_put(RED_LED_PIN, HIGH);
                gpio_put(BLUE_LED_PIN, HIGH);
                break;
            case 2: // Blue
                gpio_put(BLUE_LED_PIN, LOW);

                gpio_put(RED_LED_PIN, HIGH);
                gpio_put(GREEN_LED_PIN, HIGH);
                break;
            default:
                break;
        }
    } else {
        // Turn off all LEDs
        gpio_put(RED_LED_PIN, HIGH);
        gpio_put(GREEN_LED_PIN, HIGH);
        gpio_put(BLUE_LED_PIN, HIGH);
    }
}

int main()
{
    stdio_init_all();

    // Turn the default LED on the Pico board on to indicate that the program is running
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, HIGH);

    // Initialize components
    init_button_pin(ON_OFF_BUTTON_PIN);
    init_button_pin(COLOUR_BUTTON_PIN);
    init_led_pin(RED_LED_PIN, HIGH);
    init_led_pin(GREEN_LED_PIN, HIGH);
    init_led_pin(BLUE_LED_PIN, HIGH);

    // Initialize state variables
    bool led_on = LOW;

    int current_colour = RED;

    // Main loop
    while (HIGH) {
        // update program state based on input
        if (poll_button_click(ON_OFF_BUTTON_PIN)) {
            led_on = !led_on; // Toggle LED on/off state
            printf("LED On/Off button clicked. New state: %s\n", led_on ? "ON" : "OFF");
        }
        if (poll_button_click(COLOUR_BUTTON_PIN)) {
            current_colour = next_colour(&current_colour, current_colour, 3); // Cycle through colours
        }

        // update components state based on program state
        update_light_state(led_on, current_colour);
    }
}