cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ex7_led_rotate_two_buttons)

target_sources(app PRIVATE src/main.c)
