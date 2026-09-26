/**
 * @file led_rotate.c
 * @brief Implementation of the shared non-blocking LED rotation helper.
 *
 * See led_rotate.h for the public interface. Port write uses the masked raw
 * API (gpio_port_set_masked_raw) exactly like exercises 1, 2, 6 and 7 so the
 * physical pin level on the FRDM-MCXN236 protoboard LEDs matches the masks.
 */

#include <zephyr/drivers/gpio.h>

#include "led_rotate.h"

void led_rotate_step(const struct device *led_port,
		     int delta_pos, uint32_t rotate_ticks, bool lit)
{
	static int rotate_pos = 0;
	static uint32_t rotate_counter = 0;

	rotate_counter++;

	if (rotate_counter < rotate_ticks) {
		return;
	}

	rotate_counter = 0;

	if (lit) {
		gpio_port_set_masked_raw(led_port, ALL_LEDS_MASK,
					 BIT(rotate_pos));
	} else {
		gpio_port_set_masked_raw(led_port, ALL_LEDS_MASK,
					 ALL_LEDS_MASK & ~BIT(rotate_pos));
	}

	rotate_pos = (rotate_pos + delta_pos + NUM_LED) % NUM_LED;
}