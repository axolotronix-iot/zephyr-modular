/**
 * @file mode_led_on_press.c
 * @brief Exercise 8 mode: LED on while a button is pressed (exercise 3).
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "LED on while button is pressed" (line 65).
 *
 * The on-board SW2 (DT alias "sw0", gpio-keys code from the board node,
 * INPUT_KEY_WAKEUP) mirrors the first LED (gpio1 pin 0):
 *
 * - The gpio-keys input callback only updates the LED state (pressed or
 *   not) — decision (b).
 * - The fixed 10ms poll loop performs the GPIO write.
 *
 * Debounce is provided by the gpio-keys driver, so no manual debounce code
 * is needed.
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

#define SW0_CODE DT_PROP(DT_ALIAS(sw0), zephyr_code)

static bool g_pressed;

static void led_on_press_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type == INPUT_EV_KEY && evt->code == SW0_CODE) {
		g_pressed = (evt->value != 0);
	}
}

INPUT_CALLBACK_DEFINE(NULL, led_on_press_input_cb, NULL);

void mode_led_on_press_run(const struct device *led_port)
{
	printk("LED (gpio1 pin %d) will mirror SW2 via gpio-keys\n", LED_PIN);

	while (1) {
		gpio_pin_set(led_port, LED_PIN, g_pressed);

		k_msleep(POLL_DELAY_MS);
	}
}