/*
 * Exercise 3: LED mirrors a button's state — ON while pressed, OFF when
 * released. No toggle logic, no debounce needed: the LED simply reflects
 * the current pin level on each loop iteration.
 *
 * Button: on-board SW2 (P0_20 / WUU0_IN4), gpio0 pin 20.
 * No devicetree overlay used on purpose — raw GPIO pin API only.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LED_PORT_NODE    DT_NODELABEL(gpio1)
#define BUTTON_PORT_NODE DT_NODELABEL(gpio0)

#define LED_PIN    0
#define BUTTON_PIN 20 /* SW2, P0_20 / WUU0_IN4 */

#define POLL_DELAY_MS 10

int main(void)
{
	const struct device *led_port = DEVICE_DT_GET(LED_PORT_NODE);
	const struct device *button_port = DEVICE_DT_GET(BUTTON_PORT_NODE);

	if (!device_is_ready(led_port) || !device_is_ready(button_port)) {
		printk("Error: GPIO port device is not ready\n");
		return -1;
	}

	int ret = gpio_pin_configure(led_port, LED_PIN, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		printk("Error %d: failed to configure LED pin\n", ret);
		return -1;
	}

	ret = gpio_pin_configure(button_port, BUTTON_PIN,
				  GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW);
	if (ret < 0) {
		printk("Error %d: failed to configure button pin\n", ret);
		return -1;
	}

	printk("LED (gpio1 pin %d) will mirror SW2 (gpio0 pin %d)\n", LED_PIN, BUTTON_PIN);

	while (1) {
		int pressed = gpio_pin_get(button_port, BUTTON_PIN);

		gpio_pin_set(led_port, LED_PIN, pressed);

		k_msleep(POLL_DELAY_MS);
	}

	return 0;
}
