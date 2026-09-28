/**
 * @file main.c
 * @brief Exercise 9 — custom out-of-tree LED bar driver, 9-phase demo.
 *
 * Reference: zephyr_projects/README.md, "Low-level drivers - other
 * peripherals" checklist - "Custom LED-bar driver (rotate one LED,
 * all-on, all-off, invert state) with error handling and logging"
 * (line 74).
 *
 * Control of the 8-LED bar (gpio1 pins 0-7) goes through the custom
 * "axolotronix,led-bar" driver in drivers/led_bar, packaged as an
 * out-of-tree Zephyr module (ZEPHYR_EXTRA_MODULES). The driver keeps the
 * LED pattern in a software bitmask and only rotates valid states: one
 * lit LED (an isolated bit, popcount 1) or one turned-off LED (a
 * single-bit state, popcount num_leds - 1). Any other state is
 * self-corrected on the next led_bar_rotate() call, so the demo never
 * stalls at all-on/all-off.
 *
 * The demo runs an infinite 7-phase cycle, polled every POLL_DELAY_MS:
 *
 *   1. BLINK                    — all-on/all-off alternating, 3 full
 *                                 cycles (BLINK_TRANSITIONS transitions),
 *                                 one every BLINK_TICKS. Leaves led_mask
 *                                 at an invalid 0x00/0xFF state on exit.
 *   2. SWEEP_RIGHT_DIRECT       — configure(DIR_RIGHT, DIRECT) once, then
 *                                 rotate() every ROTATE_TICKS for
 *                                 LED_BAR_NUM_LEDS steps: the first
 *                                 rotate() self-corrects from the BLINK
 *                                 state to bit 0 (-EINVAL, expected), the
 *                                 remaining num_leds - 1 steps sweep 1..7.
 *   3. SWEEP_LEFT_DIRECT        — set_direction(DIR_LEFT) once, then
 *                                 rotate() for num_leds - 1 steps (7..0).
 *   4. SWEEP_RIGHT_INVERSE      — set_polarity(INVERSE) and
 *                                 set_direction(DIR_RIGHT) once, then
 *                                 rotate() for num_leds - 1 steps (0..7).
 *   5. SWEEP_LEFT_INVERSE       — set_direction(DIR_LEFT) once, then
 *                                 rotate() for num_leds - 1 steps (7..0).
 *   6. SWEEP_RIGHT_ALT_STEP     — set_direction(DIR_RIGHT) once. The phase
 *                                 inherits the INVERSE polarity phase 5
 *                                 ended with. For each of the num_leds
 *                                 positions the LED shows three states,
 *                                 each for ROTATE_TICKS polls: the base
 *                                 state, the opposite polarity
 *                                 (set_polarity(), no movement), and the
 *                                 base polarity again (set_polarity(), no
 *                                 movement); one rotate() then advances to
 *                                 the next position (no rotate() after the
 *                                 last position).
 *   7. SWEEP_LEFT_ALT_STEP      — set_direction(DIR_LEFT) once, then the
 *                                 same base/opposite/base triplets per
 *                                 position as phase 6, sweeping right to
 *                                 left (7..0), inheriting the polarity
 *                                 phase 6 ended with.
 *
 * After phase 7 the cycle wraps back to BLINK. set_direction() and
 * set_polarity() never move the pattern, so every sweep phase starts
 * exactly where the previous one ended. The last state of each phase
 * stays on screen a full tick period before the next phase's setup
 * changes anything, so phase boundaries never truncate a state.
 * The only -EINVAL of a full
 * cycle is the first rotate() of phase 2 (self-correction of the BLINK
 * leftover); it is logged as an expected self-correction, not an error.
 * Only other negative returns abort the demo.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led_bar.h"

LOG_MODULE_REGISTER(ex9_demo, CONFIG_EX9_DEMO_LOG_LEVEL);

#define LED_BAR_NODE DT_NODELABEL(led_bar)

/* Number of LEDs = length of the "gpios" phandle-array of the led-bar
 * node. Kept in sync with the driver (BUILD_ASSERT enforces
 * LED_BAR_MIN_LEDS..LED_BAR_MAX_LEDS).
 */
#define LED_BAR_NUM_LEDS DT_PROP_LEN(LED_BAR_NODE, gpios)

#define POLL_DELAY_MS 10
#define BLINK_DELAY_MS 500
#define BLINK_TICKS (BLINK_DELAY_MS / POLL_DELAY_MS)
#define BLINK_TRANSITIONS 6
#define ROTATE_DELAY_MS 150
#define ROTATE_TICKS (ROTATE_DELAY_MS / POLL_DELAY_MS)

/**
 * @brief Demo phase selectors, one per stage of the 7-phase cycle.
 */
typedef enum {
	DEMO_PHASE_BLINK,
	DEMO_PHASE_SWEEP_RIGHT_DIRECT,
	DEMO_PHASE_SWEEP_LEFT_DIRECT,
	DEMO_PHASE_SWEEP_RIGHT_INVERSE,
	DEMO_PHASE_SWEEP_LEFT_INVERSE,
	DEMO_PHASE_SWEEP_RIGHT_ALT_STEP,
	DEMO_PHASE_SWEEP_LEFT_ALT_STEP,
	DEMO_PHASE_COUNT,
} demo_phase_t;

static demo_phase_t g_demo_phase = DEMO_PHASE_BLINK;
static uint32_t g_steps_done = 0;
static uint32_t g_tick_counter = 0;
static bool g_phase_pending = false;
static led_bar_polarity_t g_polarity = DIRECT;

static led_bar_polarity_t demo_opposite(led_bar_polarity_t p)
{
	return (p == DIRECT) ? INVERSE : DIRECT;
}

#define EX9_DEMO_LOG_CALL(fn, ...)						\
	({									\
		int ret;							\
		LOG_DBG("Phase %d/7: %s", (int)g_demo_phase + 1, #fn);		\
		ret = fn(__VA_ARGS__);						\
		ret;								\
	})

/**
 * @brief Poll cycles between two actions of the current phase.
 *
 * BLINK uses the slow BLINK_TICKS; every sweep phase uses ROTATE_TICKS.
 *
 * @return Number of poll cycles (POLL_DELAY_MS each) per action.
 */
static uint32_t demo_phase_ticks(void)
{
	if (g_demo_phase == DEMO_PHASE_BLINK) {
		return BLINK_TICKS;
	}

	return ROTATE_TICKS;
}

/**
 * @brief Total number of steps of the current phase.
 *
 * BLINK uses BLINK_TRANSITIONS transitions. SWEEP_RIGHT_DIRECT runs
 * LED_BAR_NUM_LEDS rotate() calls because the first one only
 * self-corrects the BLINK leftover to bit 0 without moving. The
 * direct/inverse sweep phases run LED_BAR_NUM_LEDS - 1 rotating steps
 * because they start from the last position written by the previous
 * phase. The alternate-per-step phases show per position three states
 * (base, opposite, base), i.e. LED_BAR_NUM_LEDS * 3 steps.
 *
 * @return Total step count of the current phase.
 */
static uint32_t demo_phase_total(void)
{
	switch (g_demo_phase) {
	case DEMO_PHASE_BLINK:
		return BLINK_TRANSITIONS;
	case DEMO_PHASE_SWEEP_RIGHT_DIRECT:
		return LED_BAR_NUM_LEDS;
	case DEMO_PHASE_SWEEP_RIGHT_ALT_STEP:
	case DEMO_PHASE_SWEEP_LEFT_ALT_STEP:
		return LED_BAR_NUM_LEDS * 3;
	default:
		return LED_BAR_NUM_LEDS - 1;
	}
}

/**
 * @brief Run the one-time setup of the current phase.
 *
 * Resets the step/tick counters and, for the sweep phases, issues the
 * configure()/set_direction()/set_polarity() calls that select the
 * direction and polarity at phase entry. None of these calls move the
 * pattern: only set_polarity() may complement the mask, and only when
 * the polarity actually changes.
 *
 * @param dev Pointer to the device structure.
 */
static void demo_phase_setup(const struct device *dev)
{
	g_steps_done = 0;
	g_tick_counter = 0;

	switch (g_demo_phase) {
	case DEMO_PHASE_BLINK:
		LOG_DBG("Phase 1/7: BLINK");
		break;
	case DEMO_PHASE_SWEEP_RIGHT_DIRECT:
		EX9_DEMO_LOG_CALL(led_bar_configure, dev, DIR_RIGHT, DIRECT);
		g_polarity = DIRECT;
		LOG_DBG("Phase 2/7: sweep right, DIRECT");
		break;
	case DEMO_PHASE_SWEEP_LEFT_DIRECT:
		EX9_DEMO_LOG_CALL(led_bar_set_direction, dev, DIR_LEFT);
		LOG_DBG("Phase 3/7: sweep left, DIRECT");
		break;
	case DEMO_PHASE_SWEEP_RIGHT_INVERSE:
		EX9_DEMO_LOG_CALL(led_bar_set_polarity, dev, INVERSE);
		g_polarity = INVERSE;
		EX9_DEMO_LOG_CALL(led_bar_set_direction, dev, DIR_RIGHT);
		LOG_DBG("Phase 4/7: sweep right, INVERSE");
		break;
	case DEMO_PHASE_SWEEP_LEFT_INVERSE:
		EX9_DEMO_LOG_CALL(led_bar_set_direction, dev, DIR_LEFT);
		LOG_DBG("Phase 5/7: sweep left, INVERSE");
		break;
	case DEMO_PHASE_SWEEP_RIGHT_ALT_STEP:
		EX9_DEMO_LOG_CALL(led_bar_set_direction, dev, DIR_RIGHT);
		LOG_DBG("Phase 6/7: sweep right, base/opposite/base per position");
		break;
	case DEMO_PHASE_SWEEP_LEFT_ALT_STEP:
		EX9_DEMO_LOG_CALL(led_bar_set_direction, dev, DIR_LEFT);
		LOG_DBG("Phase 7/7: sweep left, base/opposite/base per position");
		break;
	case DEMO_PHASE_COUNT:
		break;
	}
}

/**
 * @brief Perform one step of the current phase.
 *
 * Steps are dispatched on g_demo_phase; g_steps_done is the step index
 * within the phase. Direction and polarity are set once at phase entry
 * and only rotate() moves the pattern. In the alternate-per-step phases
 * each position shows three states: the base state (held with no driver
 * call), the opposite polarity (set_polarity(), which only inverts, no
 * movement) and the base polarity again; one rotate() then advances to
 * the next position, and no rotate() follows the last position.
 *
 * The only -EINVAL a full cycle returns is the first rotate() of
 * SWEEP_RIGHT_DIRECT, which self-corrects the invalid mask left by
 * BLINK and does not move the pattern. It is an expected
 * self-correction, logged and reported as success.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int demo_phase_step(const struct device *dev)
{
	int ret;

	switch (g_demo_phase) {
	case DEMO_PHASE_BLINK:
		if ((g_steps_done % 2) == 0) {
			ret = EX9_DEMO_LOG_CALL(led_bar_all_on, dev);
		} else {
			ret = EX9_DEMO_LOG_CALL(led_bar_all_off, dev);
		}
		break;

	case DEMO_PHASE_SWEEP_RIGHT_DIRECT:
	case DEMO_PHASE_SWEEP_LEFT_DIRECT:
	case DEMO_PHASE_SWEEP_RIGHT_INVERSE:
	case DEMO_PHASE_SWEEP_LEFT_INVERSE:
		ret = EX9_DEMO_LOG_CALL(led_bar_rotate, dev);
		break;

	case DEMO_PHASE_SWEEP_RIGHT_ALT_STEP:
	case DEMO_PHASE_SWEEP_LEFT_ALT_STEP: {
		uint32_t substate = g_steps_done % 3;

		if (substate == 0) {
			/* a) base state. The first position keeps the mask
			 * left by the previous phase; every following
			 * position rotates into its base here.
			 */
			if (g_steps_done == 0) {
				return 0;
			}
			ret = EX9_DEMO_LOG_CALL(led_bar_rotate, dev);
			break;
		}

		if (substate == 1) {
			/* b) opposite polarity: invert, no movement. */
			g_polarity = demo_opposite(g_polarity);
			ret = EX9_DEMO_LOG_CALL(led_bar_set_polarity, dev,
				       g_polarity);
			break;
		}

		/* c) base polarity again: invert back, no movement. */
		g_polarity = demo_opposite(g_polarity);
		ret = EX9_DEMO_LOG_CALL(led_bar_set_polarity, dev, g_polarity);
		break;
	}

	default:
		return 0;
	}

	if (ret == -EINVAL) {
		LOG_DBG("expected self-correction of the BLINK leftover");
		return 0;
	}

	return ret;
}

int main(void)
{
	const struct device *led_bar_dev = DEVICE_DT_GET(LED_BAR_NODE);

	if (!device_is_ready(led_bar_dev)) {
		printk("Error: led_bar device is not ready\n");
		return -1;
	}

	printk("Exercise 9: custom LED bar driver demo\n");

	demo_phase_setup(led_bar_dev);

	while (1) {
		g_tick_counter++;

		if (g_tick_counter >= demo_phase_ticks()) {
			g_tick_counter = 0;

			if (g_phase_pending) {
				/* The last state of the current phase keeps
				 * the screen a full tick period; only then
				 * does the next phase's setup run.
				 */
				g_phase_pending = false;
				g_demo_phase =
					(demo_phase_t)((g_demo_phase + 1) %
						       DEMO_PHASE_COUNT);
				demo_phase_setup(led_bar_dev);
			} else {
				int ret = demo_phase_step(led_bar_dev);

				if (ret < 0) {
					printk("Error: demo step failed (%d)\n",
					       ret);
					return -1;
				}

				g_steps_done++;

				if (g_steps_done >= demo_phase_total()) {
					g_phase_pending = true;
				}
			}
		}

		k_msleep(POLL_DELAY_MS);
	}

	return 0;
}