#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Define HIGH and LOW for clarity
const bool HIGH = true;
const bool LOW = false;

//////////////////////////////
// SECTION: PIN DEFINITIONS //
//////////////////////////////
const uint RED_LED_PIN = 13;
const uint GREEN_LED_PIN = 14;
const uint BLUE_LED_PIN = 15;

const uint ON_OFF_BUTTON_PIN = 0;
const uint COLOUR_BUTTON_PIN = 1;

/////////////////////////////////////
// SECTION: LED COLOUR DEFINITIONS //
/////////////////////////////////////
typedef enum { RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, VIOLET, WHITE } Colours;

typedef struct {
    const char* name;
    uint8_t r, g, b;
} ColourDef;

static const ColourDef COLOUR_TABLE[] = {
    [RED] = { "RED", 255, 0, 0 },
    [ORANGE] = { "ORANGE", 255, 127, 0 },
    [YELLOW] = { "YELLOW", 255, 255, 0 },
    [GREEN] = { "GREEN", 0, 255, 0 },
    [BLUE] = { "BLUE", 0, 0, 255 },
    [INDIGO] = { "INDIGO", 75, 0, 130 },
    [VIOLET] = { "VIOLET", 148, 0, 211 },
    [WHITE] = { "WHITE", 255, 255, 255 }
};

#define NUM_COLOURS (sizeof(COLOUR_TABLE) / sizeof(COLOUR_TABLE[0]))

const char* colour_to_string(Colours colour) {
    return COLOUR_TABLE[colour].name;
}

Colours next_colour(Colours current) {
    Colours next = (current + 1) % NUM_COLOURS;
    printf("Colour changed to: %s\n", colour_to_string(next));
    return next;
}

// Compensaste for different LEDs having different brightnesses at the same PWM level. These values are determined experimentally.
#define RED_CHANNEL_SCALE 1.0f
#define GREEN_CHANNEL_SCALE 0.5f
#define BLUE_CHANNEL_SCALE 1.0f

////////////////////////////
// SECTION: HARDWARE INIT //
////////////////////////////
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

////////////////////////////////
// SECTION: COMPONENT CONTROL //
////////////////////////////////
void set_led_brightness(uint pin, uint8_t brightness) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(slice_num, channel, brightness);
}

void set_led_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    set_led_brightness(RED_LED_PIN, (uint8_t)(red * RED_CHANNEL_SCALE));
    set_led_brightness(GREEN_LED_PIN, (uint8_t)(green * GREEN_CHANNEL_SCALE));
    set_led_brightness(BLUE_LED_PIN, (uint8_t)(blue * BLUE_CHANNEL_SCALE));
}

void update_light_state(bool led_on, Colours colour) {
    if (!led_on) {
        set_led_rgb(0, 0, 0); // Turn off all LEDs
        return;
    }
    const ColourDef *c = &COLOUR_TABLE[colour];
    set_led_rgb(c->r, c->g, c->b);
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
            current_colour = next_colour(current_colour); // Cycle through colours
        }

        // update components state based on program state
        update_light_state(led_on, current_colour);
    }
}