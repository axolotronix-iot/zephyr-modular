/*
 * Exercise 4: Rotate an LED across a port with 3 selectable speeds,
 * chosen by 3 separate buttons. Speed selection is "latched" — it stays
 * at the last button pressed until another one is pressed.
 *
 * Buttons: P0_27, P0_28, P0_29 (protoboard, no external pull-up assumed).
 * No devicetree overlay used on purpose — raw GPIO API only.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LED_PORT_NODE    DT_NODELABEL(gpio1)
#define BUTTON_PORT_NODE DT_NODELABEL(gpio0)

#define NUM_LED 8
#define ALL_PINS_MASK 0xFF

#define NUM_BTN 3
#define BTN_SLOW_PIN 27
#define BTN_MED_PIN  28
#define BTN_FAST_PIN 29

#define SPEED_SLOW_MS 600
#define SPEED_MED_MS  300
#define SPEED_FAST_MS 100

#define POLL_DELAY_MS 10

uint8_t pos = 0;
uint32_t poll_count = 0;
uint32_t rotate_delay = SPEED_MED_MS; /* default speed at boot */

const struct device *led_port = DEVICE_DT_GET(LED_PORT_NODE);
const struct device *button_port = DEVICE_DT_GET(BUTTON_PORT_NODE);

const int button_pins[] = {BTN_SLOW_PIN, BTN_MED_PIN, BTN_FAST_PIN};

int main(void)
{
	if (!device_is_ready(led_port) || !device_is_ready(button_port)) {
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

	for (int pin = 0; pin < NUM_BTN; pin++) {
		int ret = gpio_pin_configure(button_port, button_pins[pin],
					      GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW);
		if (ret < 0) {
			printk("Error %d: failed to configure button pin %d\n", ret, button_pins[pin]);
			return -1;
		}
	}

	printk("Rotating LED on gpio1, speed selected by buttons on gpio0 %d/%d/%d\n",
	       BTN_SLOW_PIN, BTN_MED_PIN, BTN_FAST_PIN);


	while (1) {
		if (gpio_pin_get(button_port, BTN_SLOW_PIN)) {
			rotate_delay = SPEED_SLOW_MS;
		} else if (gpio_pin_get(button_port, BTN_MED_PIN)) {
			rotate_delay = SPEED_MED_MS;
		} else if (gpio_pin_get(button_port, BTN_FAST_PIN)) {
			rotate_delay = SPEED_FAST_MS;
		}

		poll_count = (poll_count + POLL_DELAY_MS) % rotate_delay;
		if (!poll_count) {
			uint8_t value = ALL_PINS_MASK & ~BIT(pos);

			gpio_port_set_masked_raw(led_port, ALL_PINS_MASK, value);

			pos = (pos + 1) % NUM_LED;
		}

		k_msleep(POLL_DELAY_MS);
	}

	return 0;
}