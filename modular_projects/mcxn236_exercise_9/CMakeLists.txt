cmake_minimum_required(VERSION 3.20.0)

# Register the out-of-tree led_bar module BEFORE find_package() so Zephyr
# discovers its CMake, Kconfig and DT bindings (Embedded House "Your first
# Zephyr driver, part II").
set(ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/drivers/led_bar")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ex9_led_bar_driver)

target_sources(app PRIVATE src/main.c)