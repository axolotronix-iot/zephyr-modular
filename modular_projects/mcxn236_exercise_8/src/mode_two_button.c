/**
 * @file mode_two_button.c
 * @brief Exercise 8 mode: two buttons rotate an LED left/right (exercise 7).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Two buttons rotate an LED left/right" (line 69).
 *
 * SW2 (DT alias "sw0", INPUT_KEY_WAKEUP) rotates the active LED left; SW3
 * (DT alias "sw1", INPUT_KEY_0) rotates it right. Rotation speed is fixed
 * at ROTATE_TICKS_MED (30 * POLL_DELAY_MS = 300 ms).
 *
 * Simultaneous-press policy — decision (a): both buttons are sampled in
 * the same poll cycle; if both are pressed, the direction stays unchanged;
 * if only one is active, the rotation moves according to that one. No
 * temporal window is used.
 *
 * Debounce is provided by the gpio-keys driver.
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

#define SW0_CODE DT_PROP(DT_ALIAS(sw0), zephyr_code)
#define SW1_CODE DT_PROP(DT_ALIAS(sw1), zephyr_code)

#define ROTATE_TICKS_MED 30 /* 30 * POLL_DELAY_MS = 300 ms */

#define DIR_LEFT  -1
#define DIR_RIGHT  1

static bool g_left_pressed;
static bool g_right_pressed;

static void two_button_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	if (evt->code == SW0_CODE) {
		g_left_pressed = (evt->value != 0);
	} else if (evt->code == SW1_CODE) {
		g_right_pressed = (evt->value != 0);
	}
}

INPUT_CALLBACK_DEFINE(NULL, two_button_input_cb, NULL);

void mode_two_button_run(const struct device *led_port)
{
	int direction = DIR_RIGHT;

	printk("Rotating lit-LED on gpio1; SW2 left, SW3 right "
	       "(via gpio-keys)\n");

	while (1) {
		/*
		 * Read both button states before deciding the new direction:
		 * when both are pressed the direction stays unchanged.
		 */
		if (g_left_pressed && g_right_pressed) {
			/* keep current direction */
		} else if (g_left_pressed) {
			direction = DIR_LEFT;
		} else if (g_right_pressed) {
			direction = DIR_RIGHT;
		}

		led_rotate_step(led_port, direction, ROTATE_TICKS_MED, true);

		k_msleep(POLL_DELAY_MS);
	}
}