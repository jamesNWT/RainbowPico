/* GPIO pin assignments. Deliberately has no includes so host-side code (e.g. the
 * controller unit tests) can use the pin numbers without the Pico SDK.
*/
#ifndef PINS_H
#define PINS_H

#define RED_LED_PIN 2U
#define GREEN_LED_PIN 3U
#define BLUE_LED_PIN 4U

#define TARGET_RGB_LED_B_PIN 6U
#define TARGET_RGB_LED_G_PIN 7U
#define TARGET_RGB_LED_R_PIN 8U

#define UP_BUTTON_PIN 14U
#define DOWN_BUTTON_PIN 15U

#define PLAY_RGB_LED_B_PIN 19U
#define PLAY_RGB_LED_G_PIN 20U
#define PLAY_RGB_LED_R_PIN 21U

#endif
