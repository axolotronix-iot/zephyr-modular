/**
 * @file mode_three_speed.c
 * @brief Exercise 8 mode: Rotate an LED with three speeds (exercise 4).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Rotate an LED with three speeds" (line 66).
 *
 * The three protoboard buttons (DT aliases sw2/sw3/sw4 -> gpio-keys codes
 * INPUT_KEY_1/2/3, pins P0_27/P0_28/P0_29) select the rotation speed of a
 * turned-off LED that walks across the 8 LED pins:
 *
 *   sw2 (P0_27) -> slow  (ROTATE_TICKS_SLOW  = 60 ticks = 600ms)
 *   sw3 (P0_28) -> med   (ROTATE_TICKS_MED   = 30 ticks = 300ms)
 *   sw4 (P0_29) -> fast  (ROTATE_TICKS_FAST  = 10 ticks = 100ms)
 *
 * Selection is latched: it stays at the last pressed button until another
 * one is pressed (precedence slow > med > fast, as in the original
 * exercise). The input callback only records which buttons are active; the
 * poll loop applies the precedence on each iteration.
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

#define SW2_CODE DT_PROP(DT_ALIAS(sw2), zephyr_code) /* P0_27 */
#define SW3_CODE DT_PROP(DT_ALIAS(sw3), zephyr_code) /* P0_28 */
#define SW4_CODE DT_PROP(DT_ALIAS(sw4), zephyr_code) /* P0_29 */

#define ROTATE_TICKS_SLOW 60 /* 60 * POLL_DELAY_MS = 600 ms */
#define ROTATE_TICKS_MED  30 /* 30 * POLL_DELAY_MS = 300 ms */
#define ROTATE_TICKS_FAST 10 /* 10 * POLL_DELAY_MS = 100 ms */

static bool g_slow_pressed;
static bool g_med_pressed;
static bool g_fast_pressed;

static void three_speed_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	if (evt->code == SW2_CODE) {
		g_slow_pressed = (evt->value != 0);
	} else if (evt->code == SW3_CODE) {
		g_med_pressed = (evt->value != 0);
	} else if (evt->code == SW4_CODE) {
		g_fast_pressed = (evt->value != 0);
	}
}

INPUT_CALLBACK_DEFINE(NULL, three_speed_input_cb, NULL);

void mode_three_speed_run(const struct device *led_port)
{
	uint32_t rotate_ticks = ROTATE_TICKS_MED; /* default speed at boot */

	printk("Rotating off-LED on gpio1, speed selected by sw2/sw3/sw4 "
	       "via gpio-keys\n");

	while (1) {
		if (g_slow_pressed) {
			rotate_ticks = ROTATE_TICKS_SLOW;
		} else if (g_med_pressed) {
			rotate_ticks = ROTATE_TICKS_MED;
		} else if (g_fast_pressed) {
			rotate_ticks = ROTATE_TICKS_FAST;
		}

		led_rotate_step(led_port, 1, rotate_ticks, false);

		k_msleep(POLL_DELAY_MS);
	}
}