# ZephyrOS — Zephyr RTOS Exercises

This repository collects hands-on exercises for learning Zephyr RTOS, developed while going
through a training program delivered by **Modular** ([LinkedIn](https://www.linkedin.com/company/modular-mx)).
The suggested exercises are adapted from the **Embedded House** tutorial series, originally
written for STM32 + FreeRTOS, and migrated here to run on NXP **MCX** boards
(FRDM-MCXN236 / MCXA) under Zephyr.

## Environment

- **Target board**: NXP FRDM-MCXN236 (MCXN236 SoC)
- **RTOS**: Zephyr
- **Build system**: `west`
- **Dev environment**: Docker-based devcontainer

Standard build/flash cycle for any exercise folder:

```bash
west build -b frdm_mcxn236 <exercise_path> --pristine
west flash
```

Serial console (MCU-LINK VCOM):

```bash
picocom /dev/ttyACM0 -b 115200
```

## Repository structure

Each exercise lives in its own folder with the standard Zephyr application layout:

```
<exercise_folder>/
├── CMakeLists.txt
├── prj.conf
├── src/
│   └── main.c
└── boards/                # overlay files, only where the exercise requires them
```

New exercise folders can be scaffolded with the included helper script:

```bash
./scripts/zephyr_init.sh <path> [project_name]
```

## Tooling

A couple of Python helper scripts (also in this repo) were built alongside the exercises to
make sense of the board's Device Tree and board directory:

- `dts_to_md.py` — parses a `.dts`/`.dtsi` file into a readable Markdown summary (nodes,
  properties, `chosen`, `aliases`, enabled peripherals).
- `board_report.py` — generates a combined Markdown or HTML report for an entire Zephyr board
  directory (Devicetree, Kconfig, YAML metadata, CMake, and C source), including a heuristic
  pin-usage map per GPIO port.

## Exercise checklist

### Low-level drivers — GPIO

- [✓] Blink 8 LEDs on a single port, alternating groups (p0,p2,p4,p6 / p1,p3,p5,p7)
- [✓] Rotate a turned-off LED across a port at a human-perceptible speed
- [✓] LED on while button is pressed, off when released
- [✓] Rotate an LED with three speeds, selected by three separate buttons
- [✓] Toggle an LED on/off with a single button press
- [ ] Single button cycles through four rotation speeds, wrapping around
- [ ] Two buttons rotate an LED left/right
- [ ] Rewrite the button/LED exercises using `gpio-keys`, GPIO hogs, and DT aliases

### Low-level drivers — other peripherals

- [ ] Custom LED-bar driver (rotate one LED, all-on, all-off, invert state) with error handling
      and logging — following the *Embedded House "Your first Zephyr driver"* tutorial
- [ ] I2C or SPI EEPROM read/write using only a low-level driver (no built-in Zephyr EEPROM driver)
- [ ] Read the on-board I2C temperature sensor and print it via `printk`
- [ ] Read a potentiometer over ADC; print value in volts, ohms, and binary, updated every second
- [ ] Same as above, but only print when the value changes more than 5% from the last reading
- [ ] Use Zephyr's INPUT driver: LED on while button held, off on release
- [ ] Same as above, but toggle the LED state on each press
- [ ] Configure short-press / long-press detection via Device Tree + Kconfig, print a message
      for each

### Kernel (migrated from Embedded House's FreeRTOS series)

- [ ] Part 1 — Tasks
- [ ] Part 2 — Queues
- [ ] Part 3 — Timers
- [ ] Part 5 — Mutexes
- [ ] Queues exercise repeated using Zephyr FIFOs instead of message queues — compare both
- [ ] Part 9 — Command Line Interface, migrated to Zephyr's Shell subsystem

### More than you can handle

- [ ] Part 03 — Timers module
- [ ] Part 04 — UART
- [ ] Part 06 — ADC
- [ ] Part 08 — PWM
- [ ] Part 09 — CAN (CANable USB-to-CAN adapter or similar analyzer required)

### VIP section

- [ ] Clock/date display over serial, updated every second; SHELL commands to set time, date,
      and an alarm. Alarm blinks an LED at 300 ms; board button silences it.
      Uses the RTC driver (hardware or emulated), the INPUT driver, `gpio-leds`, and `printk`.
- [ ] Multi-task architecture: one task for SHELL command processing, one for display, one for
      alarm/other control logic
- [ ] Flash the board via MCUboot over serial, or bring up the USB stack with a CDC-ACM class to
      expose an extra virtual serial port

### "I hate myself" section (requires FRDM-MCXN947)

- [ ] Bring up Ethernet and the networking stack
- [ ] Implement a TCP/IP client/server
- [ ] Dynamic IP assignment via DHCP
- [ ] Minimal HTTPS server on-device
- [ ] MQTT client/server work

## Notes

- Exercises marked "for STM32" in the original Embedded House material were adapted to the
  MCX GPIO/peripheral driver APIs; pin numbers and Device Tree node labels differ accordingly.
- Overlay files are intentionally avoided in the early GPIO exercises (as instructed) to build
  familiarity with the raw `struct device *` / port-level API before moving to `gpio-leds` /
  `gpio-keys` and DT-based abstractions later in the sequence.

## Credits

- Training and suggested exercise list: [Modular](https://www.linkedin.com/company/modular-mx)
- Original tutorial series (FreeRTOS/STM32, migrated here to Zephyr/MCX): Embedded House