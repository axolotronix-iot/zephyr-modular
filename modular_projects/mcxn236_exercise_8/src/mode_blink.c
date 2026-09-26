/**
 * @file mode_blink.c
 * @brief Exercise 8 mode: Blink (exercise 1).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Blink 8 LEDs connected to a single port, alternating between
 * two groups: {p0, p2, p4, p6} and {p1, p3, p5, p7}" (line 63).
 *
 * The LED port is reached through the DT alias "ledbar" (overlay maps it to
 * gpio1). Timing uses the fixed-period poll loop; the two groups alternate
 * every BLINK_TICKS (BLINK_DELAY_MS / POLL_DELAY_MS) ticks.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "led_rotate.h"

#define GROUP_A_MASK 0x55 /* pins p0,p2,p4,p6 of the port */
#define GROUP_B_MASK 0xAA /* pins p1,p3,p5,p7 of the port */
#define ALL_PINS_MASK 0xFF

#define BLINK_DELAY_MS 500
#define BLINK_TICKS (BLINK_DELAY_MS / POLL_DELAY_MS)

void mode_blink_run(const struct device *led_port)
{
	bool show_group_b = false;
	uint32_t tick = 0;

	printk("Starting alternating blink on gpio1 pins 0-7 via DT alias "
	       "(gpio-keys/GPIO hogs build)\n");
	printk("Group A mask\n");
	gpio_port_set_masked_raw(led_port, ALL_PINS_MASK, GROUP_A_MASK);

	while (1) {
		tick++;

		if (tick >= BLINK_TICKS) {
			tick = 0;
			show_group_b = !show_group_b;

			gpio_port_set_masked_raw(
				led_port, ALL_PINS_MASK,
				show_group_b ? GROUP_B_MASK : GROUP_A_MASK);

			printk(show_group_b ? "Group B mask\n"
					    : "Group A mask\n");
		}

		k_msleep(POLL_DELAY_MS);
	}
}