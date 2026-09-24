/*
 * Exercise 1: Blink 8 LEDs connected to a single port, alternating between
 * two groups: {p0, p2, p4, p6} and {p1, p3, p5, p7}.
 *
 * No devicetree overlay used on purpose — this exercise works directly
 * with the raw GPIO port device, as required by the exercise statement.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* Port that physically exposes P1_0..P1_7 on the FRDM-MCXN236 headers */
#define LED_PORT_NODE DT_NODELABEL(gpio1)

#define BLINK_DELAY_MS 500

/* Bit masks for the two alternating groups (pins 0-7 of the port) */
#define GROUP_A_MASK 0x55 /* 0b01010101 -> p0, p2, p4, p6 */
#define GROUP_B_MASK 0xAA /* 0b10101010 -> p1, p3, p5, p7 */
#define ALL_PINS_MASK 0xFF

int main(void)
{
	const struct device *port = DEVICE_DT_GET(LED_PORT_NODE);

	if (!device_is_ready(port)) {
		printk("Error: GPIO port device is not ready\n");
		return -1;
	}

	/* Configure pins 0-7 as outputs, starting inactive (logic low) */
	for (int pin = 0; pin < 8; pin++) {
		int ret = gpio_pin_configure(port, pin, GPIO_OUTPUT_INACTIVE);

		if (ret < 0) {
			printk("Error %d: failed to configure pin %d\n", ret, pin);
			return -1;
		}
	}

	printk("Starting alternating blink on gpio1 pins 0-7\n");

	while (1) {
		printk("Group A mask\n");
		gpio_port_set_masked_raw(port, ALL_PINS_MASK, GROUP_A_MASK);
		k_msleep(BLINK_DELAY_MS);

		printk("Group B mask\n");
		gpio_port_set_masked_raw(port, ALL_PINS_MASK, GROUP_B_MASK);
		k_msleep(BLINK_DELAY_MS);
	}

	return 0;
}