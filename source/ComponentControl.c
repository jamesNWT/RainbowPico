#include "ComponentControl.h"
#include "Hardware.h"

bool is_button_clicked(uint pin) {
    bool reading = gpio_get(pin);
    if (reading == LOW) {
        sleep_ms(DEBOUNCE_DELAY_MS);
        if (gpio_get(pin) != LOW) {
            return false; // was noise, not a real press
        }
        while (gpio_get(pin) == LOW) {
            sleep_ms(1);
        }
        return true; // released, click complete
    }
    return false;
}

void turn_led_on(uint pin) {
    gpio_put(pin, HIGH);
}

void turn_led_off(uint pin) {
    gpio_put(pin, LOW);
}