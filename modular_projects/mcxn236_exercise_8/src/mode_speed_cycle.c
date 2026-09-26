/**
 * @file mode_speed_cycle.c
 * @brief Exercise 8 mode: single button cycles through four rotation speeds
 *        (exercise 6).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Single button cycles through four rotation speeds, wrapping
 * around" (line 68).
 *
 * The protoboard button at P0_28 (DT alias "sw3", gpio-keys code
 * INPUT_KEY_2) advances a lit-LED rotation through four speeds, wrapping
 * from the fastest back to the slowest:
 *
 *   ROTATE_TICKS_SLOWEST   60 ticks (600 ms)
 *   ROTATE_TICKS_SLOW      30 ticks (300 ms)
 *   ROTATE_TICKS_FAST      15 ticks (150 ms)
 *   ROTATE_TICKS_FASTEST    8 ticks ( 80 ms)
 *
 * Debounce is provided by the gpio-keys driver; the input callback only
 * advances the speed index, and the poll loop drives led_rotate_step().
 *
 * Board: NXP FRDM-MCXN236
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "led_rotate.h"

#define SW3_CODE DT_PROP(DT_ALIAS(sw3), zephyr_code)

#define NUM_SPEEDS 4
#define ROTATE_TICKS_SLOWEST 60 /* 60 * POLL_DELAY_MS = 600 ms */
#define ROTATE_TICKS_SLOW    30 /* 30 * POLL_DELAY_MS = 300 ms */
#define ROTATE_TICKS_FAST    15 /* 15 * POLL_DELAY_MS = 150 ms */
#define ROTATE_TICKS_FASTEST  8 /*  8 * POLL_DELAY_MS =  80 ms */

static const uint32_t g_speed_ticks[NUM_SPEEDS] = {
	ROTATE_TICKS_SLOWEST,
	ROTATE_TICKS_SLOW,
	ROTATE_TICKS_FAST,
	ROTATE_TICKS_FASTEST,
};

static uint8_t g_speed_index;

static void speed_cycle_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type == INPUT_EV_KEY && evt->code == SW3_CODE &&
	    evt->value == 1) {
		g_speed_index = (g_speed_index + 1) % NUM_SPEEDS;
	}
}

INPUT_CALLBACK_DEFINE(NULL, speed_cycle_input_cb, NULL);

void mode_speed_cycle_run(const struct device *led_port)
{
	printk("Rotating lit-LED on gpio1; sw3 (P0_28) cycles 4 speeds\n");
	printk("Speed: %u ticks\n", g_speed_ticks[g_speed_index]);

	while (1) {
		led_rotate_step(led_port, 1, g_speed_ticks[g_speed_index],
				true);

		k_msleep(POLL_DELAY_MS);
	}
}