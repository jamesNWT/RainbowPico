#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <math.h>
#include "Hardware.h"
#include "ComponentControl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "Color.h"
#include "queue.h"

static QueueHandle_t but_event_queue;
static QueueHandle_t but_irq_queue;


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
struct button_state {
    uint pin;
    bool is_pressed;
    absolute_time_t time_changed;
};

struct but_irq {
    uint pin;
    absolute_time_t timestamp;
};

void button_interrupt_cb(uint gpio, uint32_t event_mask){
    gpio_set_irq_enabled(gpio, event_mask, false);

    struct but_irq irq = {.pin = gpio, .timestamp = get_absolute_time()};

    xQueueSendFromISR(but_irq_queue, &irq, NULL);
}

void deferred_button_interupt_handler(void *pvParameters) {
    struct but_irq irq;

    bool last_up_state = HIGH, last_down_state = HIGH;

    while(1) {
        xQueueReceive(but_irq_queue, &irq, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS));
        bool check_again = gpio_get(irq.pin);

        gpio_set_irq_enabled(irq.pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true); // re-enable irqs after debounce delay

        if ((irq.pin == UP_BUTTON_PIN && check_again != last_up_state) || (irq.pin == DOWN_BUTTON_PIN && check_again != last_down_state)) {
            struct button_state button = {.pin = irq.pin, .is_pressed = (check_again == LOW ? true : false), .time_changed = irq.timestamp};
            xQueueSend(but_event_queue, &button, 0);
            if (irq.pin == UP_BUTTON_PIN) {
                last_up_state = check_again;
            } else {
                last_down_state = check_again;
            }
        }
    }
}

void respond_to_button_task(void *pvParameters) {

    absolute_time_t time_last_up_press = 0, time_last_down_press = 0;
    
    while(1) {
        struct button_state state;
        xQueueReceive(but_event_queue, &state, portMAX_DELAY);

        if(state.is_pressed) {
            printf("%s button pressed!\n", state.pin == UP_BUTTON_PIN ? "up" : "down");
            turn_led_on(RED_LED_PIN);
            if(state.pin == UP_BUTTON_PIN) {
                time_last_up_press = state.time_changed;
            } else {
                time_last_down_press = state.time_changed;
            }
        }
        if(!state.is_pressed) {
            float time_held;
            if(state.pin == UP_BUTTON_PIN) {
                time_held = ((float)(state.time_changed - time_last_up_press)) * 0.000001;
            } else {
                time_held = ((float)(state.time_changed - time_last_down_press)) * 0.000001;
            }
            printf("%s button released! Time held: %.3f\n", state.pin == UP_BUTTON_PIN ? "up" : "down", time_held);
            turn_led_off(RED_LED_PIN);
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

    // Initialize state variables

    // Precomputations
    compute_gamma_table();

    // DEBUG LEDs
    // xTaskCreate(test_leds_task, "TEST_LEDS_TASK", 256, NULL, 1, NULL);
    xTaskCreate(color_sine_task, "TEST_PLAY_RGB_LED", 256, &play_rgb_led, 1, NULL);
    xTaskCreate(color_sine_task, "TEST_TARGET_RGB_LED", 256, &target_rgb_led, 1, NULL);
    
    but_event_queue = xQueueCreate(10, sizeof(struct button_state));
    xTaskCreate(respond_to_button_task, "RESPOND_TO_BUTTON_TASK", 256, NULL, 1, NULL);

    but_irq_queue = xQueueCreate(10, sizeof(struct but_irq));

    gpio_set_irq_enabled_with_callback(UP_BUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, button_interrupt_cb);
    gpio_set_irq_enabled_with_callback(DOWN_BUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, button_interrupt_cb);

    xTaskCreate(deferred_button_interupt_handler, "DEFERRED_BUTTON_INTERUPT_HANDLER", 256, NULL, 2, NULL);
    
 
    while(!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("Hello from RainbowPico!\n");
    
    vTaskStartScheduler();
}