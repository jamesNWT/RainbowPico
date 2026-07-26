#include <stdio.h>
#include "pico/stdlib.h"

const uint RED_LED_PIN = 13;
const uint GREEN_LED_PIN = 14;
const uint BLUE_LED_PIN = 15;

const uint ON_OFF_BUTTON_PIN = 0;
const uint COLOUR_BUTTON_PIN = 1;

void init_button_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void init_led_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, true);
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
        return true; // released, click complete
    }
    return false;
}

void increment_colour_index(int *current_colour_index, const int *colours, int num_colours) {
    *current_colour_index = (*current_colour_index + 1) % num_colours;
    printf("Colour changed to index: %d\n", *current_colour_index);
}

void update_light_state(bool led_on, int current_colour_index) {
    if (led_on) {
        switch (current_colour_index) {
            case 0: // Red
                gpio_put(RED_LED_PIN, false);
                gpio_put(GREEN_LED_PIN, true);
                gpio_put(BLUE_LED_PIN, true);
                break;
            case 1: // Green
                gpio_put(RED_LED_PIN, true);
                gpio_put(GREEN_LED_PIN, false);
                gpio_put(BLUE_LED_PIN, true);
                break;
            case 2: // Blue
                gpio_put(RED_LED_PIN, true);
                gpio_put(GREEN_LED_PIN, true);
                gpio_put(BLUE_LED_PIN, false);
                break;
            default:
                break;
        }
    } else {
        // Turn off all LEDs
        gpio_put(RED_LED_PIN, true);
        gpio_put(GREEN_LED_PIN, true);
        gpio_put(BLUE_LED_PIN, true);
    }
}

int main()
{
    stdio_init_all();

    // Turn the default LED on the Pico board on to indicate that the program is running
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    // Initialize components
    init_button_pin(ON_OFF_BUTTON_PIN);
    init_button_pin(COLOUR_BUTTON_PIN);
    init_led_pin(RED_LED_PIN);
    init_led_pin(GREEN_LED_PIN);
    init_led_pin(BLUE_LED_PIN);

    // Initialize state variables
    bool led_on = false;

    const int colours[] = {0, 1, 2}; // Example colour states
    int current_colour_index = 0;

    // Main loop
    while (true) {
        if (poll_button_click(ON_OFF_BUTTON_PIN)) {
            led_on = !led_on; // Toggle LED on/off state
            printf("LED On/Off button clicked. New state: %s\n", led_on ? "ON" : "OFF");
        }
        if (poll_button_click(COLOUR_BUTTON_PIN)) {
            increment_colour_index(&current_colour_index, colours, sizeof(colours) / sizeof(colours[0]));
        }

        update_light_state(led_on, current_colour_index);
    }
}


