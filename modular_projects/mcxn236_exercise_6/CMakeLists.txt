cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ex6_led_rotate_speed_cycle)

target_sources(app PRIVATE src/main.c)
