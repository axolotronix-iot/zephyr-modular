if(CONFIG_LED_BAR)

zephyr_library()

zephyr_library_sources(
  led_bar.c
)

# Expose the public driver header (led_bar.h) to the application.
zephyr_include_directories(
  .
)

endif()