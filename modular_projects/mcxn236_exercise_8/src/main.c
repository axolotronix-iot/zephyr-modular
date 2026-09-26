/**
 * @file main.c
 * @brief Exercise 8 - gpio-keys, GPIO hogs, and DT aliases dispatcher.
 *
 * Rebuilds exercises 1..7 on top of Zephyr's input subsystem (gpio-keys),
 * boot-time GPIO hogs, and devicetree aliases instead of manual GPIO
 * configuration and polling.
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist - "Rewrite the button/LED exercises using gpio-keys, GPIO hogs,
 * and DT aliases" (line 70).
 *
 * The build variant is selected with the EX8_MODE Kconfig choice; only the
 * matching mode_*.c file is compiled (see CMakeLists.txt,
 * zephyr_library_sources_ifdef).
 *
 * Board: NXP FRDM-MCXN236
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#define LED_PORT_NODE DT_ALIAS(ledbar)

/* Each mode source exports mode_<name>_run(const struct device *led_port) */
#if defined(CONFIG_EX8_MODE_BLINK)
void mode_blink_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_ROTATE_OFF)
void mode_rotate_off_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_LED_ON_PRESS)
void mode_led_on_press_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_THREE_SPEED)
void mode_three_speed_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_TOGGLE)
void mode_toggle_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_SPEED_CYCLE)
void mode_speed_cycle_run(const struct device *led_port);
#elif defined(CONFIG_EX8_MODE_TWO_BTN)
void mode_two_button_run(const struct device *led_port);
#else
#error "No EX8_MODE selected"
#endif

int main(void)
{
	const struct device *led_port = DEVICE_DT_GET(LED_PORT_NODE);

	if (!device_is_ready(led_port)) {
		printk("Error: LED port device is not ready\n");
		return -1;
	}

#if defined(CONFIG_EX8_MODE_BLINK)
	mode_blink_run(led_port);
#elif defined(CONFIG_EX8_MODE_ROTATE_OFF)
	mode_rotate_off_run(led_port);
#elif defined(CONFIG_EX8_MODE_LED_ON_PRESS)
	mode_led_on_press_run(led_port);
#elif defined(CONFIG_EX8_MODE_THREE_SPEED)
	mode_three_speed_run(led_port);
#elif defined(CONFIG_EX8_MODE_TOGGLE)
	mode_toggle_run(led_port);
#elif defined(CONFIG_EX8_MODE_SPEED_CYCLE)
	mode_speed_cycle_run(led_port);
#elif defined(CONFIG_EX8_MODE_TWO_BTN)
	mode_two_button_run(led_port);
#endif

	return 0;
}