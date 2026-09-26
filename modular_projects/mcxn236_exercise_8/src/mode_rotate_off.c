/**
 * @file mode_rotate_off.c
 * @brief Exercise 8 mode: Rotate a turned-off LED (exercise 2).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Rotate a turned-off LED across a port's pins at a speed
 * perceptible to the human eye" (line 64).
 *
 * All LEDs stay ON except one, which walks across the pins. The shared
 * led_rotate_step() helper is used with lit = false and a fixed 150ms step
 * (ROTATE_TICKS ~ POLL_DELAY_MS) implemented on the 10ms poll loop.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "led_rotate.h"

#define ROTATE_DELAY_MS 150
#define ROTATE_TICKS (ROTATE_DELAY_MS / POLL_DELAY_MS)

void mode_rotate_off_run(const struct device *led_port)
{
	printk("Starting rotating off-LED on gpio1 pins 0-%d via DT alias\n",
	       NUM_LED - 1);

	while (1) {
		led_rotate_step(led_port, 1, ROTATE_TICKS, false);

		k_msleep(POLL_DELAY_MS);
	}
}