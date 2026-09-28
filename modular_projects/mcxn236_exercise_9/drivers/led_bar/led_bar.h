/**
 * @file led_bar.h
 * @brief Public API of the "axolotronix,led-bar" LED bar driver.
 *
 * Public interface of the out-of-tree Zephyr module drivers/led_bar
 * (modular_projects/mcxn236_exercise_9). Wraps the driver API struct in
 * static inline functions following the Zephyr driver model (Embedded
 * House "Your first Zephyr driver, part III").
 *
 * The driver keeps a software bitmask (led_bar_data.led_mask) as the
 * single source of truth. A valid rotation state is either one lit LED
 * (an isolated bit, popcount == 1) or one turned-off LED among lit ones
 * (a single-bit state, popcount == num_leds - 1); led_bar_rotate()
 * rejects every other state by resetting it to a single isolated bit and
 * returning -EINVAL.
 *
 * Board: NXP FRDM-MCXN236
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_LED_BAR_H_
#define ZEPHYR_INCLUDE_DRIVERS_LED_BAR_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of LEDs controlled by a led-bar device.
 *
 * This is a compile-time upper bound derived from the number of bits in
 * the software bitmask (led_bar_data.led_mask is uint8_t). The value is
 * validated against the actual length of the Devicetree "gpios"
 * phandle-array with BUILD_ASSERT in LED_BAR_DEFINE: at least 4 LEDs,
 * at most LED_BAR_MAX_LEDS.
 */
#define LED_BAR_MIN_LEDS 4
#define LED_BAR_MAX_LEDS 8

/**
 * @brief Rotation direction of the active LED pattern.
 */
typedef enum {
	DIR_LEFT = -1,
	DIR_RIGHT = 1,
} led_bar_direction_t;

/**
 * @brief Polarity of the rotation pattern.
 */
typedef enum {
	DIRECT = 0,
	INVERSE = 1,
} led_bar_polarity_t;

/**
 * @brief LED bar driver API.
 *
 * All operations act on the whole LED bar. The driver keeps a software
 * bitmask (led_bar_data.led_mask) as the single source of truth; it is
 * never read back from the GPIO hardware.
 */
struct led_bar_driver_api {
	/**
	 * @brief Configure the rotation direction and polarity.
	 *
	 * Equivalent to led_bar_set_direction() followed by
	 * led_bar_set_polarity(). Never moves the led_mask.
	 *
	 * @param dev       Pointer to the device structure.
	 * @param direction New rotation direction.
	 * @param polarity  New rotation polarity.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*configure)(const struct device *dev, led_bar_direction_t direction,
			 led_bar_polarity_t polarity);
	/**
	 * @brief Set the rotation direction for later led_bar_rotate() calls.
	 *
	 * Only stores the direction; never writes to the GPIOs nor moves the
	 * led_mask.
	 *
	 * @param dev       Pointer to the device structure.
	 * @param direction New rotation direction.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*set_direction)(const struct device *dev, led_bar_direction_t direction);
	/**
	 * @brief Set the rotation polarity for later led_bar_rotate() calls.
	 *
	 * If polarity equals the stored one, nothing happens. Otherwise it
	 * complements the led_mask (led_bar_invert()) and stores the new
	 * polarity. Never applies a rotation step.
	 *
	 * @param dev      Pointer to the device structure.
	 * @param polarity New rotation polarity.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*set_polarity)(const struct device *dev, led_bar_polarity_t polarity);
	/**
	 * @brief Rotate the current isolated-bit/single-bit pattern one step.
	 *
	 * If the current led_mask is a valid rotation state (exactly one
	 * lit isolated bit, popcount == 1, or exactly one turned-off LED as
	 * a single-bit state, popcount == num_leds - 1), applies a cyclic
	 * rotate in the stored direction and writes the resulting mask.
	 * Otherwise the mask is reset to a single isolated bit (see below)
	 * and written immediately, and -EINVAL is returned without rotating
	 * this call.
	 *
	 * The reset mask uses the stored direction: DIR_RIGHT starts at
	 * LED0 (bit 0x01), DIR_LEFT starts at LED7 (bit BIT(num_leds - 1)).
	 * For DIRECT polarity the single isolated bit is written as-is; for
	 * INVERSE polarity the single-bit state is written as its
	 * complement (one turned-off LED among lit ones).
	 *
	 * @param dev Pointer to the device structure.
	 * @return 0 on success, negative errno on failure; -EINVAL if the
	 *         previous state was invalid and has been self-corrected.
	 */
	int (*rotate)(const struct device *dev);
	/**
	 * @brief Turn all LEDs on.
	 *
	 * @param dev Pointer to the device structure.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*all_on)(const struct device *dev);
	/**
	 * @brief Turn all LEDs off.
	 *
	 * @param dev Pointer to the device structure.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*all_off)(const struct device *dev);
	/**
	 * @brief Invert the state of all LEDs.
	 *
	 * Complements the complete software bitmask (led_mask ^= all ones)
	 * and writes the resulting pattern, regardless of the current state.
	 *
	 * @param dev Pointer to the device structure.
	 * @return 0 on success, negative errno on failure.
	 */
	int (*invert)(const struct device *dev);
};

/**
 * @brief Configure the rotation direction and polarity.
 *
 * @param dev       Pointer to the device structure.
 * @param direction New rotation direction.
 * @param polarity  New rotation polarity.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_configure(const struct device *dev,
				    led_bar_direction_t direction,
				    led_bar_polarity_t polarity)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->configure(dev, direction, polarity);
}

/**
 * @brief Rotate the current isolated-bit/single-bit pattern one step.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_rotate(const struct device *dev)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->rotate(dev);
}

/**
 * @brief Set the rotation direction for later led_bar_rotate() calls.
 *
 * @param dev       Pointer to the device structure.
 * @param direction New rotation direction.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_set_direction(const struct device *dev,
					led_bar_direction_t direction)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->set_direction(dev, direction);
}

/**
 * @brief Set the rotation polarity for later led_bar_rotate() calls.
 *
 * @param dev      Pointer to the device structure.
 * @param polarity New rotation polarity.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_set_polarity(const struct device *dev,
				       led_bar_polarity_t polarity)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->set_polarity(dev, polarity);
}

/**
 * @brief Turn all LEDs on.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_all_on(const struct device *dev)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->all_on(dev);
}

/**
 * @brief Turn all LEDs off.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_all_off(const struct device *dev)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->all_off(dev);
}

/**
 * @brief Invert the state of all LEDs.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, negative errno on failure.
 */
static inline int led_bar_invert(const struct device *dev)
{
	const struct led_bar_driver_api *api =
		(const struct led_bar_driver_api *)dev->api;

	return api->invert(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_LED_BAR_H_ */