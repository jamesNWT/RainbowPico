#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <math.h>
#include "Hardware.h"
#include "ComponentControl.h"

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

////////////////////////////
// SECTION: HARDWARE INIT //
////////////////////////////


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


int check_buttons() {
    if (is_button_clicked(UP_BUTTON_PIN)) {
        printf("Up button clicked!\n");
        return 1;
        
    } 
    if (is_button_clicked(DOWN_BUTTON_PIN)) {
        printf("Down button clicked!\n");
        return 2;
    }
    return 0;
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
    init_led_pin(RED_LED_PIN, LOW);
    init_led_pin(GREEN_LED_PIN, LOW);
    init_led_pin(BLUE_LED_PIN, LOW);


    init_button_pin(UP_BUTTON_PIN);
    init_button_pin(DOWN_BUTTON_PIN);

    // Initialize state variables

    // Precomputations
    // compute_gamma_table();

    // Main loop
    int state = 0;

    while (1) {
        

        switch (check_buttons()) {
            case 1:
                state++;
                if (state > 3) state = 0; // Up button clicked
                printf("State changed to %d\n", state);
                break;
            case 2:
                state--; // Down button clicked
                if (state < 0) state = 3;
                printf("State changed to %d\n", state);
                break;
            default:
                break;
        }

        switch (state) {
            case 0:
                turn_led_on(RED_LED_PIN);
                turn_led_off(GREEN_LED_PIN);
                turn_led_off(BLUE_LED_PIN);
                break;
            case 1:
                turn_led_off(RED_LED_PIN);
                turn_led_on(GREEN_LED_PIN);
                turn_led_off(BLUE_LED_PIN);
                break;
            case 2:
                turn_led_off(RED_LED_PIN);
                turn_led_off(GREEN_LED_PIN);
                turn_led_on(BLUE_LED_PIN);
                break;
            case 3:
                turn_led_off(RED_LED_PIN);
                turn_led_off(GREEN_LED_PIN);
                turn_led_off(BLUE_LED_PIN);
                break;
            default:
                turn_led_on(RED_LED_PIN);
                turn_led_on(GREEN_LED_PIN);
                turn_led_on(BLUE_LED_PIN);
                break;
        }
    }
}