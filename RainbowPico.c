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
typedef enum { RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, VIOLET } Colours;

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
            return "RED";
        case ORANGE:
            return "ORANGE";
        case YELLOW:
            return "YELLOW";
        case GREEN:
            return "GREEN";
        case BLUE:
            return "BLUE";
        case INDIGO:
            return "INDIGO";
        case VIOLET:
            return "VIOLET";
        default:
            return "UNKNOWN";
    }
}

int next_colour(int *current_colour, Colours colours, int num_colours) {
    *current_colour = (*current_colour + 1) % num_colours;
    printf("Colour changed to: %s\n", colour_to_string(*current_colour));
    return *current_colour;
}

void set_led_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    set_led_brightness(RED_LED_PIN, red);
    set_led_brightness(GREEN_LED_PIN, green);
    set_led_brightness(BLUE_LED_PIN, blue);
}

void update_light_state(bool led_on, Colours colour) {
    if (led_on) {
        switch (colour) {
            case RED:
                set_led_rgb(255, 0, 0);
                break;
            case ORANGE:
                set_led_rgb(255, 40, 0);
                break;
            case YELLOW:
                set_led_rgb(255, 100, 0);
                break;
            case GREEN:
                set_led_rgb(0, 255, 0);
                break;
            case BLUE:
                set_led_rgb(0, 0, 255);
                break;
            case INDIGO:
                set_led_rgb(75, 0, 130);
                break;
            case VIOLET:
                set_led_rgb(148, 0, 211);
                break;
            default:
                break;
        }
    } else {
        // Turn off all LEDs
        set_led_rgb(0, 0, 0);
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
    
    init_pwm_slice(pwm_gpio_to_slice_num(RED_LED_PIN));
    init_pwm_slice(pwm_gpio_to_slice_num(GREEN_LED_PIN)); // Green and blue both use slice 7, so we only need to initialize it once.

    init_led_pin_pwm(RED_LED_PIN);
    init_led_pin_pwm(GREEN_LED_PIN);
    init_led_pin_pwm(BLUE_LED_PIN);

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
            current_colour = next_colour(&current_colour, current_colour, 7); // Cycle through colours
        }

        // update components state based on program state
        update_light_state(led_on, current_colour);
    }
}