#include "zephyr/device.h"
#include "zephyr/drivers/gpio.h"
#include "zephyr/dt-bindings/gpio/gpio.h"
#include "zephyr/sys/printk.h"
#include <stdint.h>
#include <zephyr/kernel.h>

#define LED_PORT_NODE DT_NODELABEL(gpio1)
#define BUTTON_PORT_NODE DT_NODELABEL(gpio0)

#define BTN_PIN 28
#define NUM_LED 8

#define POLL_DELAY_MS 10
#define DEBOUNCE_MS 50

enum {
    LED_PIN_0,
    LED_PIN_1,
    LED_PIN_2,
    LED_PIN_3,
    LED_PIN_4,
    LED_PIN_5,
    LED_PIN_6,
    LED_PIN_7
} led_pin_e;

const struct device *led_port = DEVICE_DT_GET(LED_PORT_NODE);
const struct device *button_port = DEVICE_DT_GET(BUTTON_PORT_NODE);

int led_state = 0;
int button_state_prev = 0;
int button_state_curr = 0;

int main(void) {

    if ( !device_is_ready(led_port) || !device_is_ready(button_port)) {
        printk("Error: GPIO port device is not ready\n");
        return -1;
    }

    for (int pin = 0; pin < NUM_LED; pin++) {
        int ret = gpio_pin_configure(led_port, pin, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("Error %d: failed to configure LED pin %d\n", ret, pin);
            return -1;
        }
    }

    int ret = gpio_pin_configure(button_port, BTN_PIN, GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW);
    if (ret < 0) {
        printk("Error %d: failed to configure button pin %d\n", ret, BTN_PIN);
        return -1;
    }
    
    button_state_prev = gpio_pin_get(button_port, BTN_PIN);
    while (1) {
        button_state_curr = gpio_pin_get(button_port, BTN_PIN);

        if (button_state_curr == 1 && button_state_prev == 0) {
            led_state = !led_state;
            gpio_pin_set(led_port, 0, led_state);
            k_msleep(DEBOUNCE_MS);
        }
        button_state_prev = button_state_curr;

        k_msleep(POLL_DELAY_MS);
    }
    return 0;
}