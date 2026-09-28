/**
 * @file led_bar.c
 * @brief Driver for a GPIO-driven LED bar (compatible
 *        "axolotronix,led-bar").
 *
 * Out-of-tree Zephyr driver module (modular_projects/mcxn236_exercise_9)
 * following the Embedded House "Your first Zephyr driver" series:
 *
 *   - Part I:  DeviceTree binding (dts/bindings/axolotronix,led-bar.yaml)
 *              exposing a single "gpios" phandle-array with one entry per
 *              LED, accessed with GPIO_DT_SPEC_GET_BY_IDX(). The number
 *              of LEDs is the length of the "gpios" property, validated
 *              at build time to LED_BAR_MIN_LEDS..LED_BAR_MAX_LEDS.
 *   - Part II: module packaging (CMakeLists.txt, Kconfig,
 *              zephyr/module.yaml via ZEPHYR_EXTRA_MODULES).
 *   - Part III: DEVICE_DT_INST_DEFINE + DT_INST_FOREACH_STATUS_OKAY,
 *              struct led_bar_config from DT, struct led_bar_driver_api
 *              with function pointers, static inline wrappers in the
 *              header, and gpio readiness + logging on every failure.
 *
 * The driver keeps a software bitmask (led_bar_data.led_mask) as the
 * single source of truth; the actual LED level on the GPIOs uses
 * GPIO_DT_SPEC gpio_pin_set_dt() so the DeviceTree
 * gpio_flags (e.g. active high/low) are respected.
 *
 * led_bar_rotate() only accepts a valid rotation state (exactly one lit
 * LED as an isolated bit, popcount == 1, or exactly one turned-off LED
 * as a single-bit state, popcount == num_leds - 1). Any other state
 * (e.g. all on/off, or anything in between) is self-corrected: the mask
 * is reset to a single isolated bit matching the stored direction and
 * polarity, written out, and -EINVAL is returned without rotating. This
 * keeps the demo moving instead of stalling at 0x00/0xFF.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led_bar.h"

LOG_MODULE_REGISTER(led_bar, CONFIG_LED_BAR_LOG_LEVEL);

#define DT_DRV_COMPAT axolotronix_led_bar

/**
 * @brief Devicetree-sourced configuration of a led-bar device.
 */
struct led_bar_config {
	/** Number of LEDs this instance drives (length of the gpios property). */
	uint8_t num_leds;
	/** One gpio_dt_spec per LED, in order (LED0..LED num_leds - 1). */
	struct gpio_dt_spec gpios[];
};

/**
 * @brief Runtime state of a led-bar device.
 */
struct led_bar_data {
	/** Software state of the LEDs (bit n = LED n). Single source of truth. */
	uint8_t led_mask;
	/** Rotation direction stored for led_bar_rotate(). */
	led_bar_direction_t direction;
	/** Rotation polarity stored for led_bar_rotate(). */
	led_bar_polarity_t polarity;
};

/**
 * @brief Check whether a bit pattern is a valid rotation state.
 *
 * A valid state has exactly one set bit (an isolated bit) or exactly one
 * cleared bit within num_leds (a single-bit state, i.e.
 * popcount == num_leds - 1). The same check is used for both polarities.
 *
 * @param mask     Software LED bitmask (bit n = LED n).
 * @param num_leds Number of LEDs of the device.
 * @return true if the mask is a valid isolated-bit or single-bit state,
 *         false otherwise.
 */
static bool is_power_of_two_or_edge(uint8_t mask, uint8_t num_leds)
{
	uint8_t ones = POPCOUNT(mask);

	return (ones == 1) || (ones == (num_leds - 1));
}

/**
 * @brief Write the current led_mask to all GPIO pins.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_write_mask(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	const struct led_bar_data *data = dev->data;

	LOG_DBG("mask=0x%02X", data->led_mask);

	for (int i = 0; i < cfg->num_leds; i++) {
		int ret = gpio_pin_set_dt(&cfg->gpios[i],
					   (data->led_mask >> i) & 0x01);

		if (ret < 0) {
			LOG_ERR("Failed to write LED %d: %d", i, ret);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Reset the led_mask to a single isolated bit matching the
 *        stored direction and polarity, and write it out.
 *
 * DIR_RIGHT starts the isolated bit at LED0 (bit 0x01); DIR_LEFT starts
 * it at LED7 (bit BIT(num_leds - 1)). For DIRECT polarity the isolated
 * bit is used as-is; for INVERSE polarity the mask is the complement (a
 * single turned-off bit among num_leds - 1 lit LEDs).
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_reset_isolated_bit(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	struct led_bar_data *data = dev->data;

	uint8_t isolated_bit = (data->direction == DIR_RIGHT)
				       ? 0x01
				       : BIT(cfg->num_leds - 1);

	if (data->polarity == INVERSE) {
		data->led_mask = (uint8_t)~isolated_bit;
	} else {
		data->led_mask = isolated_bit;
	}

	return led_bar_write_mask(dev);
}

/**
 * @brief Set the rotation direction for later led_bar_rotate() calls.
 *
 * Only stores the direction; it never writes to the GPIOs nor moves the
 * led_mask. Used at the start of a sweep phase to aim the following
 * rotate() calls without an intermediate move.
 *
 * @param dev       Pointer to the device structure.
 * @param direction New rotation direction.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_set_direction_impl(const struct device *dev,
				      led_bar_direction_t direction)
{
	struct led_bar_data *data = dev->data;

	data->direction = direction;

	return 0;
}

/**
 * @brief Rotate the current isolated-bit/single-bit pattern one step in
 *        the stored direction, self-correcting invalid states.
 *
 * Valid patterns are rotated cyclically in the stored direction. Invalid
 * patterns are first reset to a single isolated bit matching the stored
 * direction and polarity, written out, and -EINVAL is returned without
 * rotating this call.
 *
 * This is the only function in the driver that moves the bit. Neither
 * set_direction() nor set_polarity() performs a rotation.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure; -EINVAL if the
 *         previous state was invalid and has been self-corrected.
 */
static int led_bar_rotate_impl(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	struct led_bar_data *data = dev->data;

	if (!is_power_of_two_or_edge(data->led_mask, cfg->num_leds)) {
		int ret = led_bar_reset_isolated_bit(dev);

		return (ret < 0) ? ret : -EINVAL;
	}

	if (data->direction == DIR_RIGHT) {
		data->led_mask = (uint8_t)((data->led_mask << 1) |
					   (data->led_mask >> (cfg->num_leds - 1)));
	} else {
		data->led_mask = (uint8_t)((data->led_mask >> 1) |
					   (data->led_mask << (cfg->num_leds - 1)));
	}

	return led_bar_write_mask(dev);
}

/**
 * @brief Invert the state of all LEDs.
 *
 * Complements the complete bitmask (led_mask ^= BIT(num_leds) - 1) and
 * writes the new pattern — regardless of which LEDs were on or off, and
 * not only the "active" LED of rotate(). Used by led_bar_set_polarity_impl()
 * to switch the polarity of the pattern.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_invert_impl(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	struct led_bar_data *data = dev->data;

	data->led_mask ^= (uint8_t)(BIT(cfg->num_leds) - 1);

	return led_bar_write_mask(dev);
}

/**
 * @brief Set the rotation polarity for later led_bar_rotate() calls.
 *
 * If polarity equals the stored one, nothing is written. Otherwise the
 * led_mask is complemented (led_bar_invert_impl()) to flip between the
 * isolated-bit and single-bit-state forms, and the new polarity is
 * stored. Never applies a rotation step: only the bits are inverted.
 *
 * @param dev      Pointer to the device structure.
 * @param polarity New rotation polarity.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_set_polarity_impl(const struct device *dev,
				     led_bar_polarity_t polarity)
{
	struct led_bar_data *data = dev->data;

	if (polarity != data->polarity) {
		int ret = led_bar_invert_impl(dev);

		if (ret < 0) {
			return ret;
		}

		data->polarity = polarity;
	}

	return 0;
}

/**
 * @brief Configure the rotation direction and polarity.
 *
 * Equivalent to led_bar_set_direction_impl() followed by
 * led_bar_set_polarity_impl(). It never moves the led_mask: setting the
 * polarity only complements it if the polarity actually changes.
 *
 * @param dev       Pointer to the device structure.
 * @param direction New rotation direction.
 * @param polarity  New rotation polarity.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_configure_impl(const struct device *dev,
				  led_bar_direction_t direction,
				  led_bar_polarity_t polarity)
{
	int ret = led_bar_set_direction_impl(dev, direction);

	if (ret < 0) {
		return ret;
	}

	return led_bar_set_polarity_impl(dev, polarity);
}

/**
 * @brief Turn all LEDs on.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_all_on_impl(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	struct led_bar_data *data = dev->data;

	data->led_mask = (uint8_t)(BIT(cfg->num_leds) - 1);

	return led_bar_write_mask(dev);
}

/**
 * @brief Turn all LEDs off.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_all_off_impl(const struct device *dev)
{
	struct led_bar_data *data = dev->data;

	data->led_mask = 0x00;

	return led_bar_write_mask(dev);
}

/**
 * @brief Initialize a led-bar device instance.
 *
 * Configures every GPIO of the "gpios" property as an inactive output,
 * logging an error on the first one that fails.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static int led_bar_init(const struct device *dev)
{
	const struct led_bar_config *cfg = dev->config;
	struct led_bar_data *data = dev->data;

	if (cfg->num_leds < LED_BAR_MIN_LEDS ||
	    cfg->num_leds > LED_BAR_MAX_LEDS) {
		LOG_ERR("Unsupported LED count %u (must be %u..%u, "
			"LED_BAR_MIN_LEDS..LED_BAR_MAX_LEDS)",
			cfg->num_leds, LED_BAR_MIN_LEDS, LED_BAR_MAX_LEDS);
		return -EINVAL;
	}

	data->led_mask = 0x00;
	data->direction = DIR_RIGHT;
	data->polarity = DIRECT;

	for (int i = 0; i < cfg->num_leds; i++) {
		if (!gpio_is_ready_dt(&cfg->gpios[i])) {
			LOG_ERR("LED bar GPIO %d device is not ready", i);
			return -ENODEV;
		}

		int ret = gpio_pin_configure_dt(&cfg->gpios[i],
						GPIO_OUTPUT_INACTIVE);

		if (ret < 0) {
			LOG_ERR("Failed to configure LED bar GPIO %d: %d",
				i, ret);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief API functions exposed to the application.
 */
static const struct led_bar_driver_api led_bar_api = {
	.configure = led_bar_configure_impl,
	.set_direction = led_bar_set_direction_impl,
	.set_polarity = led_bar_set_polarity_impl,
	.rotate = led_bar_rotate_impl,
	.all_on = led_bar_all_on_impl,
	.all_off = led_bar_all_off_impl,
	.invert = led_bar_invert_impl,
};

#define LED_BAR_DEFINE(inst)							\
	BUILD_ASSERT(DT_INST_PROP_LEN(inst, gpios) >= LED_BAR_MIN_LEDS,		\
		     "\"gpios\" must list at least "				\
		     "LED_BAR_MIN_LEDS (4) LEDs for a led-bar node");		\
	BUILD_ASSERT(DT_INST_PROP_LEN(inst, gpios) <= LED_BAR_MAX_LEDS,		\
		     "\"gpios\" must list at most LED_BAR_MAX_LEDS (8) "	\
		     "LEDs: led_mask is uint8_t");				\
	static struct led_bar_config led_bar_config_##inst = {		\
		.num_leds = DT_INST_PROP_LEN(inst, gpios),			\
		.gpios = {							\
			DT_FOREACH_PROP_ELEM_SEP(DT_DRV_INST(inst),		\
						 gpios,				\
						 GPIO_DT_SPEC_GET_BY_IDX,	\
						 (,)),				\
		},								\
	};									\
										\
	static struct led_bar_data led_bar_data_##inst;				\
										\
	DEVICE_DT_INST_DEFINE(inst,						\
			      led_bar_init,					\
			      NULL,						\
			      &led_bar_data_##inst,				\
			      &led_bar_config_##inst,				\
			      POST_KERNEL,					\
			      CONFIG_GPIO_INIT_PRIORITY,			\
			      &led_bar_api);

DT_INST_FOREACH_STATUS_OKAY(LED_BAR_DEFINE)