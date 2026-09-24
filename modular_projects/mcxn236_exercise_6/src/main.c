#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_PORT_NODE       DT_NODELABEL(gpio1)
#define BUTTON_PORT_NODE    DT_NODELABEL(gpio0)

#define NUM_BTN             1
#define BTN_PIN             28
#define BTN_CNT_TH          5

#define NUM_LED             8
#define POLL_DELAY_MS       10
#define ALL_LEDS_MASK ((1u << NUM_LED) - 1)

#define NUM_SPEEDS          4

typedef struct {
    const struct device *port;
    gpio_pin_t pin;

    int last_raw;
    int stable_level;
    uint8_t debounce_count;
} button_t;

static const struct device *g_led_port =
    DEVICE_DT_GET(LED_PORT_NODE);

static const struct device *g_button_port =
    DEVICE_DT_GET(BUTTON_PORT_NODE);


/*
 * Initialize button GPIO.
 *
 * The GPIO is configured as active-low, but gpio_pin_get()
 * gives us the logical state:
 *
 *      0 -> button released
 *      1 -> button pressed
 */
static int button_init(button_t *btn)
{
    int ret;
    int raw;

    if (!device_is_ready(btn->port)) {
        printk("Error: Button port device is not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure(
        btn->port,
        btn->pin,
        GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW
    );

    if (ret < 0) {
        printk("Error %d: failed to configure button pin %d\n",
               ret, btn->pin);
        return ret;
    }

    raw = gpio_pin_get(btn->port, btn->pin);

    if (raw < 0) {
        printk("Error %d: failed to read button pin %d\n",
               raw, btn->pin);
        return raw;
    }

    /*
     * Initialize the debounce state with the current
     * logical GPIO level.
     */
    btn->last_raw = raw;
    btn->stable_level = raw;
    btn->debounce_count = 1;

    return 0;
}


/*
 * Initialize all LEDs.
 */
static int led_init(const struct device *led_port)
{
    int ret;

    if (!device_is_ready(led_port)) {
        printk("Error: LED port device is not ready\n");
        return -ENODEV;
    }

    for (int pin = 0; pin < NUM_LED; pin++) {
        ret = gpio_pin_configure(
            led_port,
            pin,
            GPIO_OUTPUT_INACTIVE
        );

        if (ret < 0) {
            printk("Error %d: failed to configure LED pin %d\n",
                   ret, pin);
            return ret;
        }
    }

    return 0;
}


/*
 * Return BTN_CLICKED when the button changes from
 * logical released (0) to logical pressed (1), after
 * BTN_CNT_TH consecutive identical samples.
 */
static int button_was_clicked(button_t *btn)
{
    int raw = gpio_pin_get(btn->port, btn->pin);

    if (raw < 0) {
        printk("Error %d: failed to read button pin %d\n",
               raw, btn->pin);
        return 0;
    }

    /*
     * Continue the current raw-state streak.
     */
    if (raw == btn->last_raw) {
        if (btn->debounce_count < BTN_CNT_TH) {
            btn->debounce_count++;
        }
    } else {
        /*
         * Raw state changed: start a new streak.
         */
        btn->last_raw = raw;
        btn->debounce_count = 1;
    }

    /*
     * The new raw state has not been stable long enough.
     */
    if (btn->debounce_count < BTN_CNT_TH) {
        return 0;
    }

    /*
     * A new stable logical state has been confirmed.
     */
    if (raw != btn->stable_level) {
        int previous_level = btn->stable_level;

        btn->stable_level = raw;

        /*
         * Logical rising edge:
         *
         *      0 -> 1 = button pressed
         */
        if (previous_level == 0 && btn->stable_level == 1) {
            return 1;
        }
    }

    return 0;
}


/*
 * Rotate the active LED.
 *
 * speed_ms indicates the time between LED movements.
 *
 * This function is non-blocking: it only moves the LED when
 * enough time has elapsed. Therefore the button can continue
 * being polled every POLL_DELAY_MS.
 */
static void led_rotate(const struct device *led_port, uint32_t speed_ms)
{
    static int pos = 0;
    static int64_t last_step = 0;

    int64_t now = k_uptime_get();

    if ((now - last_step) < speed_ms) {
        return;
    }

    last_step = now;

    gpio_port_set_masked_raw(led_port, ALL_LEDS_MASK, BIT(pos));

    pos = (pos + 1) % NUM_LED;
}


int main(void)
{
    /*
     * Four speeds, from slowest to fastest.
     */
    const uint32_t speeds[NUM_SPEEDS] = {
        600,
        300,
        150,
        75
    };

    uint8_t speed_index = 0;

    button_t button_handler = {
        .port = g_button_port,
        .pin = BTN_PIN,
        .last_raw = 0,
        .stable_level = 0,
        .debounce_count = 0,
    };

    if (led_init(g_led_port) < 0) {
        return -1;
    }

    if (button_init(&button_handler) < 0) {
        return -1;
    }

    while (1) {

        /*
         * A click changes to the next speed.
         */
        if (button_was_clicked(&button_handler)) {
            speed_index = (speed_index + 1) % NUM_SPEEDS;

            printk("Speed: %d ms\n", speeds[speed_index]);
        }

        /*
         * Move the LED according to the selected speed.
         */
        led_rotate(g_led_port, speeds[speed_index]);

        /*
         * Poll button and LED logic every 10 ms.
         */
        k_msleep(POLL_DELAY_MS);
    }

    return 0;
}