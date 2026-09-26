/**
 * @file mode_toggle.c
 * @brief Exercise 8 mode: Toggle an LED with a single button (exercise 5).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Toggle an LED on/off with a single button press" (line 67).
 *
 * The protoboard button at P0_28 (DT alias "sw3", gpio-keys code
 * INPUT_KEY_2) toggles the first LED (gpio1 pin 0) on every press:
 *
 * - The gpio-keys input callback detects a press event and inverts the
 *   LED state.
 * - The fixed 10ms poll loop performs the GPIO write.
 *
 * Debounce is provided by the gpio-keys driver (debounce-interval-ms in
 * the overlay), so no manual debounce code is needed.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "led_rotate.h"

#define LED_PIN 0

#define SW3_CODE DT_PROP(DT_ALIAS(sw3), zephyr_code)

static bool g_led_state;

static void toggle_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type == INPUT_EV_KEY && evt->code == SW3_CODE &&
	    evt->value == 1) {
		g_led_state = !g_led_state;
	}
}

INPUT_CALLBACK_DEFINE(NULL, toggle_input_cb, NULL);

void mode_toggle_run(const struct device *led_port)
{
	printk("LED (gpio1 pin %d) toggles on each sw3 (P0_28) press\n",
	       LED_PIN);

	while (1) {
		gpio_pin_set(led_port, LED_PIN, g_led_state);

		k_msleep(POLL_DELAY_MS);
	}
}