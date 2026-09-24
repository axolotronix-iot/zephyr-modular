/*
 * Exercise 2: Rotate a turned-off LED across a port's pins at a speed
 * perceptible to the human eye. All LEDs stay ON except one, which "walks"
 * across the pins.
 *
 * No devicetree overlay used on purpose — raw GPIO port device only.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LED_PORT_NODE DT_NODELABEL(gpio1)

#define NUM_PINS 8
#define ALL_PINS_MASK 0xFF
#define ROTATE_DELAY_MS 150

int main(void)
{
	const struct device *port = DEVICE_DT_GET(LED_PORT_NODE);

	if (!device_is_ready(port)) {
		printk("Error: GPIO port device is not ready\n");
		return -1;
	}

	for (int pin = 0; pin < NUM_PINS; pin++) {
		int ret = gpio_pin_configure(port, pin, GPIO_OUTPUT_INACTIVE);

		if (ret < 0) {
			printk("Error %d: failed to configure pin %d\n", ret, pin);
			return -1;
		}
	}

	printk("Starting rotating off-LED on gpio1 pins 0-%d\n", NUM_PINS - 1);

	uint8_t pos = 0;

	while (1) {
		uint8_t value = ALL_PINS_MASK & ~BIT(pos);

		gpio_port_set_masked_raw(port, ALL_PINS_MASK, value);

		pos = (pos + 1) % NUM_PINS;

		k_msleep(ROTATE_DELAY_MS);
	}

	return 0;
}