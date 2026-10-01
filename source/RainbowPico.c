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

struct button_event
{
    uint pin;
    bool is_pressed;
    absolute_time_t time_changed;
};

struct but_irq
{
    uint pin;
    absolute_time_t timestamp;
};

void button_interrupt_cb(uint gpio, uint32_t event_mask)
{
    gpio_set_irq_enabled(gpio, event_mask, false);

    struct but_irq irq = {.pin = gpio, .timestamp = get_absolute_time()};

    xQueueSendFromISR(but_irq_queue, &irq, NULL);
}

void deferred_button_interupt_handler(void *pvParameters)
{
    struct but_irq irq;

    bool last_up_state = HIGH, last_down_state = HIGH;

    while (1)
    {
        xQueueReceive(but_irq_queue, &irq, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS));
        bool check_again = gpio_get(irq.pin);

        gpio_set_irq_enabled(irq.pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true); // re-enable irqs after debounce delay

        if ((irq.pin == UP_BUTTON_PIN && check_again != last_up_state) || (irq.pin == DOWN_BUTTON_PIN && check_again != last_down_state))
        {
            struct button_event button = {.pin = irq.pin, .is_pressed = (check_again == LOW ? true : false), .time_changed = irq.timestamp};
            xQueueSend(but_event_queue, &button, 0);
            if (irq.pin == UP_BUTTON_PIN)
            {
                last_up_state = check_again;
            }
            else
            {
                last_down_state = check_again;
            }
        }
    }
}

#define SHORT_HOLD_CEILING_US 500 * 1000
#define MEDIUM_HOLD_CEILING_US 1000 * 1000
#define LONG_HOLD_FLOOR_US 2500 * 1000

enum thresholds
{
    SHORT,
    MEDIUM,
    LONG,
    NONE
};

struct controller_state
{
    bool up_pressed;
    bool down_pressed;
    absolute_time_t up_dur;
    absolute_time_t up_last_press_timestamp;
    absolute_time_t down_dur;
    absolute_time_t down_last_press_timestamp;
    enum thresholds up_last_threshold_crossed;
    enum thresholds down_last_threshold_crossed;
    bool is_cont_adj; // flag for state where a button is being held for continuous channel adjustment
};

enum game_actions
{
    INCREMENT_COLOR_CHANNEL,
    DECREMENT_COLOR_CHANNEL,
    CONT_INC_COLOR_CHANNEL,
    CONT_DEC_COLOR_CHANNEL,
    SWTICH_COLOR_CHANNEL,
    CONFIRM_GUESS,
    START_NEW_GAME,
    INDICATE_HOLD_THRESHOLD_PASSED,
    NO_OP
};

TickType_t one_button_ticks_to_next_thresh(enum thresholds last_threshold, absolute_time_t dur_us)
{
    switch (last_threshold)
    {
    case NONE:
        return pdMS_TO_TICKS((SHORT_HOLD_CEILING_US - dur_us) * 0.001);
    case SHORT:
        return pdMS_TO_TICKS((MEDIUM_HOLD_CEILING_US - dur_us) * 0.001);
    case MEDIUM:
        return pdMS_TO_TICKS((LONG_HOLD_FLOOR_US - dur_us) * 0.001);
    case LONG:
        return portMAX_DELAY;
    default:
        return portMAX_DELAY;
    }
}

enum thresholds calculate_last_threshold_crossed(absolute_time_t dur_us)
{

    if (dur_us < SHORT_HOLD_CEILING_US)
    {
        return NONE;
    }
    if (dur_us > SHORT_HOLD_CEILING_US && dur_us < MEDIUM_HOLD_CEILING_US)
    {
        return SHORT;
    }
    if (dur_us > MEDIUM_HOLD_CEILING_US && dur_us < LONG_HOLD_FLOOR_US)
    {
        return MEDIUM;
    }
    return LONG;
}

void update_controller_from_button_event(struct controller_state *state, struct button_event *but_event)
{
    if (but_event->pin == UP_BUTTON_PIN)
    {
        if (but_event->is_pressed)
        {
            state->up_pressed = true;
            state->up_last_press_timestamp = get_absolute_time();
        }
        else
        {
            state->up_pressed = false;
            state->up_dur = 0;
            state->up_last_threshold_crossed = NONE;
        }
    }
    else if (but_event->pin == DOWN_BUTTON_PIN)
    {
        if (but_event->is_pressed)
        {
            state->down_pressed = true;
            state->down_last_press_timestamp = get_absolute_time();
        }
        else
        {
            state->down_pressed = false;
            state->down_dur = 0;
            state->down_last_threshold_crossed = NONE;
        }
    }
}

void update_controller_state_from_threshold_event(struct controller_state *state)
{
    if (state->up_pressed)
    {
        state->up_dur = get_absolute_time() - state->up_last_press_timestamp;
        state->up_last_threshold_crossed = calculate_last_threshold_crossed(state->up_dur);
    }
    if (state->down_pressed)
    {
        state->down_dur = get_absolute_time() - state->down_last_press_timestamp;
        state->down_last_threshold_crossed = calculate_last_threshold_crossed(state->down_dur);
    }
}

enum game_actions determine_game_action_from_button_event(struct controller_state *state, struct button_event *event)
{
    enum button_event_type
    {
        UP_PRESS,
        UP_RELEASE,
        DOWN_PRESS,
        DOWN_RELEASE,
        UNDETERMINED
    };

    enum button_event_type event_type;

    if (event->pin == UP_BUTTON_PIN)
    {
        if (!event->is_pressed && state->up_pressed)
        {
            event_type = UP_RELEASE;
        }
        else if (event->is_pressed && !state->up_pressed)
        {
            event_type = UP_PRESS;
        }
        else
        {
            event_type = UNDETERMINED;
        }
    }
    else
    {
        if (!event->is_pressed && state->down_pressed)
        {
            event_type = DOWN_RELEASE;
        }
        else if (event->is_pressed && !state->down_pressed)
        {
            event_type = DOWN_PRESS;
        }
        else
        {
            event_type = UNDETERMINED;
        }
    }

    if (event_type == UP_PRESS)
    {
        if (state->down_pressed)
        {
            return NO_OP;
        }
        else
        {
            return INCREMENT_COLOR_CHANNEL;
        }
    }
    if (event_type == DOWN_PRESS)
    {
        if (state->up_pressed)
        {
            return NO_OP;
        }
        else
        {
            return DECREMENT_COLOR_CHANNEL;
        }
    }
    return NO_OP;
}

void button_controller_task(void *pvParameters)
{
    absolute_time_t time_last_up_press = 0, time_last_down_press = 0, up_dur = 0, down_dur = 0;

    struct button_event but_event_buf;

    struct controller_state curr_state = {
        .up_pressed = false,
        .up_dur = 0,
        .up_last_press_timestamp = 0,
        .up_last_threshold_crossed = NONE,

        .down_pressed = false,
        .down_dur = 0,
        .down_last_press_timestamp = 0,
        .down_last_threshold_crossed = NONE,

        .is_cont_adj = false};

    // struct controller_state last_state;

    enum game_actions next_action = NO_OP;

    TickType_t ticks_until_next_threshold = portMAX_DELAY;
    enum threshold_trigger {
        UP,
        DOWN
    };

    enum threshold_trigger tt;
    while (1)
    {

        
        BaseType_t ret = xQueueReceive(but_event_queue, &but_event_buf, ticks_until_next_threshold);

        // update the state
        if (ret == pdPASS)
        {
            next_action = determine_game_action_from_button_event(&curr_state, &but_event_buf);
            update_controller_from_button_event(&curr_state, &but_event_buf);
        }
        else
        {
            next_action = INDICATE_HOLD_THRESHOLD_PASSED;
            update_controller_state_from_threshold_event(&curr_state);
        }

        // calculate the ticks until the next threshold
        TickType_t up_ticks_to_next_thresh = one_button_ticks_to_next_thresh(curr_state.up_last_threshold_crossed, curr_state.up_dur);
        TickType_t down_ticks_to_next_thresh = one_button_ticks_to_next_thresh(curr_state.down_last_threshold_crossed, curr_state.down_dur);
        if (curr_state.up_pressed && !curr_state.down_pressed)
        {
            ticks_until_next_threshold = up_ticks_to_next_thresh;
            tt = UP;
            
        }
        else if (curr_state.down_pressed && !curr_state.up_pressed)
        {
            ticks_until_next_threshold = down_ticks_to_next_thresh;
            tt = DOWN;
        }
        else if (curr_state.down_pressed && curr_state.up_pressed)
        {
            if (up_ticks_to_next_thresh < down_ticks_to_next_thresh) {
                ticks_until_next_threshold = up_ticks_to_next_thresh;
                tt = UP;
            } else {
                ticks_until_next_threshold = down_ticks_to_next_thresh;
                tt = DOWN;
            }
        }
        else
        {
            ticks_until_next_threshold = portMAX_DELAY;
        }

        // do the next action
        switch (next_action)
        {
        case NO_OP:
            printf("CONTROLLER EVENT DETECTED BUT NO OPERATION TO DO\n");
            break;
        case INCREMENT_COLOR_CHANNEL:
            printf("EVENT DETECTED: INCREMENT COLOR CHANNEL\n");
            break;
        case DECREMENT_COLOR_CHANNEL:
            printf("EVENT DETECTED: DECREMENT_COLOR_CHANNEL\n");
            break;
        case INDICATE_HOLD_THRESHOLD_PASSED:
            printf("HOLD THRESHOLD PASSED: %s button held: %.3f\n", tt == UP ? "up" : "down", tt == UP ? (float)curr_state.up_dur*0.000001 : (float)curr_state.down_dur*0.000001);
            break;
        default:
            printf("UNEXEPECTED NEXT ACTION!\n");
            break;
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