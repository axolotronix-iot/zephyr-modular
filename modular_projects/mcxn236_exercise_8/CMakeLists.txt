cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ex8_gpio_keys_hogs)

# Common sources: dispatcher plus the shared LED rotation helper.
target_sources(app PRIVATE src/main.c src/led_rotate.c)

# Each mode is compiled only when its EX8_MODE_* symbol is selected. The mode
# files are added to the "app" library when the corresponding Kconfig value is
# "y" (see cmake/modules/extensions.cmake, zephyr_library_sources_ifdef).
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_BLINK src/mode_blink.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_ROTATE_OFF src/mode_rotate_off.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_LED_ON_PRESS src/mode_led_on_press.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_THREE_SPEED src/mode_three_speed.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_TOGGLE src/mode_toggle.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_SPEED_CYCLE src/mode_speed_cycle.c)
zephyr_library_sources_ifdef(CONFIG_EX8_MODE_TWO_BTN src/mode_two_button.c)