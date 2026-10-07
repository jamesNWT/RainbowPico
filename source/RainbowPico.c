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
#include "Controller.h"
#include <string.h>

static QueueHandle_t but_event_queue;
static QueueHandle_t but_irq_queue;

void test_leds_task(void *pvParameters)
{
    while (1)
    {
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

void color_saw_task(void *pvParameters)
{

    struct rgb_led the_rgb_led = *((struct rgb_led *)pvParameters);

    struct rgb_color color = {0, 75, 150};

    while (1)
    {
        if (color.red == 255)
        {
            color.red = 0;
        }
        if (color.green == 255)
        {
            color.green = 0;
        }
        if (color.blue == 255)
        {
            color.blue = 0;
        }

        set_led_rgb_hue(the_rgb_led, color);

        color.red++;
        color.green++;
        color.blue++;

        vTaskDelay(10);
    }
}

void color_sine_task(void *pvParameters)
{
    struct rgb_led led = *((struct rgb_led *)pvParameters);

    struct rgb_color color = {0, 0, 0};

    float x = 0;
    float delta = 0.025; // choose this value because it divides the range from 0 to 2*pi into roughly 255 steps.

    float offset = 2.0 * 3.14159 / 3.f;

    while (1)
    {
        color.red = (uint8_t)((sin(x) + 1) * 127.5);
        color.green = (uint8_t)((sin(x + offset) + 1) * 127.5);
        color.blue = (uint8_t)((sin(x + 2 * offset) + 1) * 127.5);

        set_led_rgb_hue(led, color);

        x += delta;
        vTaskDelay(10);

        if (x >= 1000 * 2 * 3.14159)
        { // reset x at some multiple of 2*pi so that the reset should be smooth.
            x = 0;
        }
    }
}

struct but_irq
{
    uint pin;
    absolute_time_t time;
};

void button_interrupt_cb(uint gpio, uint32_t event_mask)
{
    gpio_set_irq_enabled(gpio, event_mask, false);

    struct but_irq irq = {.pin = gpio, .time = get_absolute_time()};

    xQueueSendFromISR(but_irq_queue, &irq, NULL);
}

void deferred_button_interupt_handler(void *pvParameters)
{
    struct but_irq irq;

    bool last_stable_up_but_state = HIGH, last_stabel_down_but_state = HIGH;

    while (1)
    {
        xQueueReceive(but_irq_queue, &irq, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS));
        bool debounced_reading = gpio_get(irq.pin);

        gpio_set_irq_enabled(irq.pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true); // re-enable irqs after debounce delay

        if ((irq.pin == UP_BUTTON_PIN && debounced_reading != last_stable_up_but_state) || (irq.pin == DOWN_BUTTON_PIN && debounced_reading != last_stabel_down_but_state))
        {
            struct button_event button = {.pin = irq.pin, .is_pressed = (debounced_reading == LOW ? true : false), .time_changed = irq.time};
            xQueueSend(but_event_queue, &button, 0);
            if (irq.pin == UP_BUTTON_PIN)
            {
                last_stable_up_but_state = debounced_reading;
            }
            else
            {
                last_stabel_down_but_state = debounced_reading;
            }
        }
    }
}

static TickType_t us_to_ticks(uint64_t us)
{
    if (us == ABSOLUTE_TIME_MAX)
    {
        return portMAX_DELAY;
    }
    return pdMS_TO_TICKS(us / 1000);
}

void button_controller_task(void *pvParameters)
{
    struct button_event but_event_buf;

    struct controller controller;
    controller_init(&controller);

    game_action next_action = NO_OP;

    TickType_t ticks_until_next_threshold = portMAX_DELAY;

    enum threshold_trigger threshold_trigger = TT_MAX_DELAY;

    while (1)
    {
        BaseType_t queue_receive = xQueueReceive(but_event_queue, &but_event_buf, ticks_until_next_threshold);
        absolute_time_t event_time = get_absolute_time();

        // update the state
        if (queue_receive == pdPASS)
        {
            next_action = button_event_controller_handler(&controller, &but_event_buf);
        }
        else
        {
            struct threshold_action_and_trigger next_action_and_trigger = threshold_event_controller_handler(&controller, event_time);
            next_action = next_action_and_trigger.action;
            threshold_trigger = next_action_and_trigger.trigger;
        }

        ticks_until_next_threshold = us_to_ticks(controller_us_until_next_threshold(&controller, event_time));

        // do the next action
        switch (next_action)
        {
        case NO_OP:
            // printf("CONTROLLER EVENT DETECTED BUT NO OPERATION TO DO\n");
            // char* event_trigger = "unknown";
            // if (queue_receive == errQUEUE_EMPTY) {
            //     switch (threshold_trigger) {
            //         case TT_DOWN_BUTTON:
            //             event_trigger = strcat("down button crossed ", strcat(threshold_to_string(controller.down.last_crossed), " threshold\n"));
            //             break;
            //         case TT_UP_BUTTON:
            //             event_trigger = strcat("up button crossed ", strcat(threshold_to_string(controller.up.last_crossed), " threshold\n"));
            //             break;
            //         case TT_MAX_DELAY:
            //             event_trigger = "max delay detected";
            //             break;
            //         default:
            //             break;
            //     }
            //     printf("\tthreshold event triggered by: %s\n", event_trigger);
            // } else {
            //     printf("button event triggered by %s button %s\n", but_event_buf.pin == UP_BUTTON_PIN ? "up" : "down", but_event_buf.is_pressed ? "press" : "release");
            // }
            break;
        case INCREMENT_CHANNEL_VALUE:
            printf("INCREMENT CHANNEL VALUE\n");
            break;
        case DECREMENT_CHANNEL_VALUE:
            printf("DECREMENT CHANNEL VALUE\n");
            break;
        case START_CONT_INC_CHANNEL_VALUE:
            printf("START CONTINUOUSLY INCREASING CHANNEL VALUE\n");
            break;
        case STOP_CONT_INC_CHANNEL_VALUE:
            printf("STOP CONTINUOUSLY INCREASING CHANNEL VALUE\n");
            break;
        case START_CONT_DEC_CHANNEL_VALUE:
            printf("START CONTINUOUSLY DECREASING CHANNEL VALUE\n");
            break;
        case STOP_CONT_DEC_CHANNEL_VALUE:
            printf("STOP CONTINUOUSLY DECREASING CHANNEL VALUE\n");
            break;
        case NEXT_COLOR_CHANNEL:
            printf("NEXT COLOR CHANNEL\n");
            break;
        case CONFIRM_GUESS:
            printf("CONFIRM GUESS\n");
            break;
        case START_NEW_GAME:
            printf("START NEW GAME\n");
            break;
        case INDICATE_HOLD_THRESHOLD_PASSED:
            const char* trigger_display;
            float duration_display;

            if (threshold_trigger == TT_UP_BUTTON) {
                trigger_display = "up";
                duration_display = get_absolute_time() - controller.up.time_press;
            } else if (threshold_trigger == TT_DOWN_BUTTON) {
                trigger_display = "down";
                duration_display = get_absolute_time() - controller.down.time_press;
            } else {
                printf("Something strange happened\n");
                break;
            }
            printf("HOLD THRESHOLD PASSED: %s button held: %.3f\n", trigger_display, duration_display *0.000001);
            break;
        default:
            printf("UNEXEPECTED NEXT ACTION: %s\n", game_action_to_string(next_action));
            break;
        }

        // Diagnostics: what woke the task and the controller state after handling it.
        // NO_OP threshold wakes are skipped since the task can wake many times just before a threshold.
        if (queue_receive == pdPASS)
        {
            printf("\t%s %s @ %llums -> %s\n",
                   but_event_buf.pin == UP_BUTTON_PIN ? "up" : "down",
                   but_event_buf.is_pressed ? "press" : "release",
                   (unsigned long long)(but_event_buf.time_changed / 1000),
                   controller_to_string(&controller));
        }
        else if (next_action != NO_OP)
        {
            printf("\tthreshold wake @ %llums -> %s\n",
                   (unsigned long long)(event_time / 1000),
                   controller_to_string(&controller));
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

    but_event_queue = xQueueCreate(10, sizeof(struct button_event));
    xTaskCreate(button_controller_task, "RESPOND_TO_BUTTON_TASK", 256, NULL, 1, NULL);

    but_irq_queue = xQueueCreate(10, sizeof(struct but_irq));

    gpio_set_irq_enabled_with_callback(UP_BUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, button_interrupt_cb);
    gpio_set_irq_enabled_with_callback(DOWN_BUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, button_interrupt_cb);

    xTaskCreate(deferred_button_interupt_handler, "DEFERRED_BUTTON_INTERUPT_HANDLER", 256, NULL, 2, NULL);

    while (!stdio_usb_connected())
    {
        sleep_ms(100);
    }
    printf("Hello from RainbowPico!\n");

    vTaskStartScheduler();
}