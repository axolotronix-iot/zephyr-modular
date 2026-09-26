/**
 * @file led_rotate.h
 * @brief Shared non-blocking LED rotation used by several exercise 8 modes.
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - GPIO"
 * checklist; the tick-based rotation logic comes from exercises 6 and 7
 * (README.md "Single button cycles through four rotation speeds" and
 * "Two buttons rotate an LED left/right").
 */

#ifndef MODULAR_PROJECTS_EX8_LED_ROTATE_H_
#define MODULAR_PROJECTS_EX8_LED_ROTATE_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define NUM_LED 8
#define ALL_LEDS_MASK ((1u << NUM_LED) - 1)

#define POLL_DELAY_MS 10

/**
 * @brief Advance the rotating LED by one step (fixed-period poll loop).
 *
 * Non-blocking: called once per poll cycle, it counts a tick and only
 * moves the LED once rotate_ticks ticks have elapsed. The tick counter is
 * deliberately NOT reset when the caller changes rotate_ticks, so a speed
 * change is perceived as a natural acceleration instead of a glitch.
 *
 * @param led_port    GPIO device driving the LEDs.
 * @param delta_pos   Step size per LED movement: +1 for one direction,
 *                    +1/-1 for left/right rotation.
 * @param rotate_ticks Number of poll cycles (POLL_DELAY_MS each) between
 *                    LED movements.
 * @param lit         true to rotate a single lit LED (others off);
 *                    false to rotate a single turned-off LED (others on).
 */
void led_rotate_step(const struct device *led_port,
		     int delta_pos, uint32_t rotate_ticks, bool lit);

#endif /* MODULAR_PROJECTS_EX8_LED_ROTATE_H_ */