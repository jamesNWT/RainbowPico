#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <math.h>
#include "Hardware.h"
#include "ComponentControl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "Color.h"

void test_leds_task(void *pvParameters) {
    while(1) {
        turn_led_on(RED_LED_PIN);
        vTaskDelay(100);
        turn_led_off(RED_LED_PIN);
        
        turn_led_on(GREEN_LED_PIN);
        vTaskDelay(100);
        turn_led_off(GREEN_LED_PIN);

        turn_led_on(BLUE_LED_PIN);
        vTaskDelay(100);
        turn_led_off(BLUE_LED_PIN);
    }
}

void color_saw_task(void *pvParameters) {

    struct rgb_led the_rgb_led = *((struct rgb_led*)pvParameters);

    struct rgb_color color = {0, 75, 150};

    while(1) {
        if (color.red == 255) {
            color.red = 0;
        }
        if (color.green == 255) {
            color.green = 0;
        }
        if (color.blue == 255) {
            color.blue = 0;
        }

        set_led_rgb_hue(the_rgb_led, color);

        color.red++;
        color.green++;
        color.blue++;

        vTaskDelay(10);
    }

}

void color_sine_task(void *pvParameters) {
    struct rgb_led led = *((struct rgb_led*)pvParameters);

    struct rgb_color color = {0, 0, 0};

    float x = 0;
    float delta = 0.025; // choose this value because it divides the range from 0 to 2*pi into roughly 255 steps.

    float offset = 2.0*3.14159 / 3.f;

    while(1) {
        color.red = (uint8_t)((sin(x) + 1) * 127.5);
        color.green = (uint8_t)((sin(x + offset) + 1) * 127.5);
        color.blue = (uint8_t)((sin(x + 2*offset) + 1) * 127.5);

        set_led_rgb_hue(led, color);
        
        x += delta;
        vTaskDelay(10);

        if (x >= 1000 * 2 * 3.14159) { // reset x at some multiple of 2*pi so that the reset should be smooth.
            x = 0;
        }
    }
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

    struct rgb_led target_rgb_led = build_rgb_led(TARGET_RGB_LED_R_PIN, TARGET_RGB_LED_G_PIN, TARGET_RGB_LED_B_PIN);
    struct rgb_led play_rgb_led = build_rgb_led(PLAY_RGB_LED_R_PIN, PLAY_RGB_LED_G_PIN, PLAY_RGB_LED_B_PIN);

    init_rgb_led(target_rgb_led);
    init_rgb_led(play_rgb_led);


    // DEBUG
    // set_led_rgb_hue(target_rgb_led, (struct rgb_color){.red = 255, .green = 125, .blue = 0});
    // set_led_rgb_hue(play_rgb_led, (struct rgb_color){.red = 50, .green = 50, .blue = 50});
    // turn_led_on(BLUE_LED_PIN);

    // Initialize state variables

    // Precomputations
    compute_gamma_table();

    xTaskCreate(test_leds_task, "TEST_LEDS_TASK", 256, NULL, 1, NULL);
    xTaskCreate(color_sine_task, "TEST_PLAY_RGB_LED", 256, &play_rgb_led, 1, NULL);
    xTaskCreate(color_sine_task, "TEST_TARGET_RGB_LED", 256, &target_rgb_led, 1, NULL);

    vTaskStartScheduler();

    while (1) {};
}