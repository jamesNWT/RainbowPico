#include <stdio.h>
#include "pico/stdlib.h"


int main()
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
