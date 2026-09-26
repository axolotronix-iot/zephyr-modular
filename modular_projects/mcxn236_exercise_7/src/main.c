/**
 * @file main.c
 * @brief Exercise 7 — Two buttons rotate an LED left/right.
 *
 * Reference: zephyr_projects/README.md, GPIO checklist —
 * "Two buttons rotate an LED left/right".
 *
 * SW2 (P0_20) rotates the active LED left; SW3 (P0_06) rotates it right.
 * Rotation speed is fixed at ROTATE_TICKS_MED (30 * POLL_DELAY_MS =
 * 300 ms). Debounce uses DEBOUNCE_TICKS consecutive stable samples,
 * polled every POLL_DELAY_MS.
 *
 * Simultaneous-press policy: if SW2 and SW3 are both detected clicked
 * in the same poll cycle, the two presses cancel out and the rotation
 * direction stays unchanged.
 *
 * Board: NXP FRDM-MCXN236
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_PORT_NODE       DT_NODELABEL(gpio1)
#define BUTTON_PORT_NODE    DT_NODELABEL(gpio0)

#define BTN_LEFT_PIN        20
#define BTN_RIGHT_PIN       6

#define DEBOUNCE_TICKS      5   /* 5 * 10ms = 50ms debounce */

#define NUM_LED             8
#define POLL_DELAY_MS       10
#define ALL_LEDS_MASK ((1u << NUM_LED) - 1)

#define ROTATE_TICKS_MED    30  /* 30 * 10ms = 300ms */

typedef enum {
    DIR_LEFT = -1,
    DIR_RIGHT = 1,
} direction_t;

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


/**
 * @brief Initialize a button GPIO with pull-up and active-low config.
 *
 * @param btn Pointer to the button_t instance to initialize.
 * @return 0 on success, negative errno on failure.
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


/**
 * @brief Initialize all LEDs on the given port as inactive outputs.
 *
 * @param led_port GPIO device driving the LEDs.
 * @return 0 on success, negative errno on failure.
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


/**
 * @brief Report whether the button was clicked this poll.
 *
 * Returns 1 when the debounced logical state changes from released (0)
 * to pressed (1), i.e. after DEBOUNCE_TICKS consecutive identical samples.
 *
 * @param btn Pointer to the button_t instance to poll.
 * @return 1 if a press was debounced this poll cycle, 0 otherwise.
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
        if (btn->debounce_count < DEBOUNCE_TICKS) {
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
    if (btn->debounce_count < DEBOUNCE_TICKS) {
        return 0;
    }

    /*
     * A new stable logical state has been confirmed.
     */
    if (raw != btn->stable_level) {
        int previous_level = btn->stable_level;

        btn->stable_level = raw;

        /*
         * Logical rising edge.
         */
        if (previous_level == 0 && btn->stable_level == 1) {
            return 1;
        }
    }

    return 0;
}


/**
 * @brief Move the active LED one step in the given direction.
 *
 * Non-blocking: called once per poll cycle; it counts a tick and only
 * moves the LED once rotate_ticks ticks have elapsed.
 *
 * @param led_port     GPIO device driving the LEDs.
 * @param direction    Rotation direction: DIR_LEFT (-1) or DIR_RIGHT (+1).
 * @param rotate_ticks Number of poll cycles (POLL_DELAY_MS each) between
 *                     LED movements.
 */
static void led_rotate_step(const struct device *led_port,
                            direction_t direction, uint32_t rotate_ticks)
{
    static int rotate_pos = 0;
    static uint32_t rotate_counter = 0;

    rotate_counter++;

    if (rotate_counter < rotate_ticks) {
        return;
    }

    rotate_counter = 0;

    gpio_port_set_masked_raw(led_port, ALL_LEDS_MASK, BIT(rotate_pos));

    rotate_pos = (rotate_pos + direction + NUM_LED) % NUM_LED;
}


int main(void)
{
    static direction_t direction = DIR_RIGHT;

    button_t sw2_left = {
        .port = g_button_port,
        .pin = BTN_LEFT_PIN,
        .last_raw = 0,
        .stable_level = 0,
        .debounce_count = 0,
    };

    button_t sw3_right = {
        .port = g_button_port,
        .pin = BTN_RIGHT_PIN,
        .last_raw = 0,
        .stable_level = 0,
        .debounce_count = 0,
    };

    if (led_init(g_led_port) < 0) {
        return -1;
    }

    if (button_init(&sw2_left) < 0) {
        return -1;
    }

    if (button_init(&sw3_right) < 0) {
        return -1;
    }

    while (1) {

        /*
         * Read both buttons before deciding the new direction. When
         * SW2 and SW3 are both clicked in the same poll cycle they
         * cancel out and the direction stays unchanged; evaluation is
         * not order-dependent.
         */
        int left_clicked = button_was_clicked(&sw2_left);
        int right_clicked = button_was_clicked(&sw3_right);

        if (left_clicked && !right_clicked) {
            direction = DIR_LEFT;
        } else if (!left_clicked && right_clicked) {
            direction = DIR_RIGHT;
        }

        /*
         * Move the LED according to the selected direction.
         */
        led_rotate_step(g_led_port, direction, ROTATE_TICKS_MED);

        /*
         * Poll buttons and LED logic every 10 ms.
         */
        k_msleep(POLL_DELAY_MS);
    }

    return 0;
}