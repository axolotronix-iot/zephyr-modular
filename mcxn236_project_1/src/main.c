#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Obtiene el LED definido como led0 en el DeviceTree */
static const struct gpio_dt_spec led =
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		gpio_pin_toggle_dt(&led);
		k_sleep(K_MSEC(300));
	}

	return 0;
}