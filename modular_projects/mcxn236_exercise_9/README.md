# Exercise 9 — Custom out-of-tree LED bar driver

Board: NXP FRDM-MCXN236. Reference: `zephyr_projects/README.md`,
"Low-level drivers – other peripherals" #9: *"Custom LED-bar driver
(rotate one LED, all-on, all-off, invert state) with error handling and
logging"*, following the Embedded House "Your first Zephyr driver"
tutorial.

This README documents how the project is structured, how it is
configured and built, the validation workflow used so far, and what is
still pending to exercise the bar with 4, 5, 6 and 7 LEDs. It is meant
as the starting point for the next development cycle.

---

## 1. What the project does

A custom, out-of-tree Zephyr GPIO driver (`axolotronix,led-bar`)
controls an LED bar through a single `gpios` phandle-array property, one
entry per LED. The driver keeps a software bitmask as the single source
of truth and only rotates valid states:

- **isolated bit** — exactly one lit LED (popcount == 1), or
- **single-bit state** — exactly one turned-off LED among lit ones
  (popcount == num_leds - 1).

Any other mask (all-on, all-off, anything in between) is self-corrected:
`led_bar_rotate()` resets it to a single isolated bit and returns
`-EINVAL` without moving, so the demo never stalls.

`src/main.c` then runs an infinite 7-phase demo over the bar (BLINK,
direct/left/right and inverse sweeps, plus two per-position
base/opposite/base "ALT_STEP" phases).

The LED count is **not** hardcoded: it is the length of the `gpios`
property, enforced at build time to `LED_BAR_MIN_LEDS..LED_BAR_MAX_LEDS`
(4..8) and re-checked at runtime in `led_bar_init()`.

---

## 2. Repository layout

```
mcxn236_exercise_9/
├── CMakeLists.txt              # registers the out-of-tree module, app target
├── Kconfig                     # app demo module log options (EX9_DEMO)
├── prj.conf                    # CONFIG_LED_BAR=y, CONFIG_LOG=y
├── debug.conf                  # compile-time DBG levels (via EXTRA_CONF_FILE)
├── boards/
│   └── frdm_mcxn236.overlay    # 8-LED bar on gpio1 pins 0-7
├── overlays/                   # experimental LED-count overlays
│   ├── led_bar_4leds.overlay   # 4 LEDs (gpio1 0-3) — builds OK
│   └── led_bar_3leds.overlay   # 3 LEDs — MUST fail BUILD_ASSERT
├── src/
│   └── main.c                  # demo application, 7-phase cycle
├── drivers/led_bar/            # out-of-tree Zephyr module
│   ├── CMakeLists.txt          # zephyr_library guarded by CONFIG_LED_BAR
│   ├── Kconfig                 # LED_BAR + per-module log level
│   ├── led_bar.c               # driver implementation (Part III)
│   ├── led_bar.h               # public API + static inline wrappers
│   ├── zephyr/module.yaml      # module metadata (dts_root, kconfig)
│   └── dts/bindings/
│       ├── axolotronix,led-bar.yaml   # the binding
│       └── vendor-prefixes.txt        # axolotronix vendor prefix
└── build_ex9/                  # default (8-LED, no trace) build
    build_ex9_dbg/              # debug build (-DEXTRA_CONF_FILE=debug.conf)
    build_ex9_4leds/            # 4-LED build
    build_ex9_3leds/            # failed 3-LED build (compile error expected)
```

### How the pieces fit together

| Concern | File | Role |
|---|---|---|
| Module discovery | `CMakeLists.txt:6` | `ZEPHYR_EXTRA_MODULES` points at `drivers/led_bar` before `find_package(Zephyr)` |
| Binding | `dts/bindings/axolotronix,led-bar.yaml` | declares `gpios` (`phandle-array`, required) |
| Driver source | `drivers/led_bar/led_bar.c` | `DEVICE_DT_INST_DEFINE` + `DT_INST_FOREACH_STATUS_OKAY` |
| Public API | `drivers/led_bar/led_bar.h` | API struct + static inline wrappers |
| Variable count | `led_bar.c:54-57` | `struct gpio_dt_spec gpios[]` flexible array, sized by the DT initializer (`DT_FOREACH_PROP_ELEM_SEP`) |
| Build-time range | `led_bar.c:371-377` | `BUILD_ASSERT` on `DT_INST_PROP_LEN(inst, gpios)` (4..8, message shows via `_Static_assert`) |
| Runtime check | `led_bar.c:327-333` | `LOG_ERR` + `-EINVAL` in `led_bar_init()` |
| Demo-side count | `src/main.c:76-82` | `LED_BAR_NODE` / `LED_BAR_NUM_LEDS = DT_PROP_LEN(node, gpios)` |

### Why 4..8?

The mask is `uint8_t` (`led_bar_data.led_mask`, `led_bar.c:65`), i.e. 8
bits, so the driver supports **at most 8** LEDs. The exercise statement
requires **at least 4**. Both bounds live in `led_bar.h:38-39`.

---

## 3. Driver model (Zephyr Parts I–III)

- **Part I — binding:** `axolotronix,led-bar.yaml` exposes one `gpios`
  phandle-array. Pins are read with `GPIO_DT_SPEC_GET_BY_IDX`, levels
  written with `gpio_pin_set_dt()` so DT `gpio_flags` are honored.
- **Part II — packaging:** `zephyr/module.yaml` (+ its own
  `CMakeLists.txt` and `Kconfig`) makes it a self-contained module via
  `ZEPHYR_EXTRA_MODULES`.
- **Part III — driver object:** `led_bar_config` (DT config, with the
  flexible `gpios[]` array), `led_bar_data` (`led_mask`, `direction`,
  `polarity`), `struct led_bar_driver_api` with 7 function pointers, and
  `DEVICE_DT_INST_DEFINE` with `POST_KERNEL` +
  `CONFIG_GPIO_INIT_PRIORITY`.

API surface (all in `led_bar.h`): `configure`, `set_direction`,
`set_polarity`, `rotate`, `all_on`, `all_off`, `invert`.

Semantics worth remembering when reusing the driver:

- `set_direction` / `set_polarity` / `configure` never move the pattern.
  `set_polarity` complements the mask only when the polarity actually
  changes.
- `rotate` is the **only** function that moves bits, cyclically, in the
  stored direction. `-EINVAL` means "previous state was invalid and has
  been self-corrected (no move this call)" — this is expected once per
  full demo cycle (the BLINK leftover at the start of
  SWEEP_RIGHT_DIRECT).

---

## 4. Demo phases (`src/main.c`)

Polled every `POLL_DELAY_MS` (10 ms). All timings are tick counters:

| Constant | Value | Meaning |
|---|---|---|
| `POLL_DELAY_MS` | 10 | main-loop period |
| `BLINK_DELAY_MS` / `BLINK_TICKS` | 500 / 50 | BLINK toggle period |
| `BLINK_TRANSITIONS` | 6 | BLINK transitions (3 full cycles) |
| `ROTATE_DELAY_MS` / `ROTATE_TICKS` | 150 / 15 | sweep step period |

| # | Phase | Steps | Behavior |
|---|---|---|---|
| 1 | BLINK | 6 | all-on/all-off, ends at an invalid mask |
| 2 | SWEEP_RIGHT_DIRECT | `num_leds` | first `rotate()` self-corrects BLINK leftover to bit 0, then sweeps right |
| 3 | SWEEP_LEFT_DIRECT | `num_leds - 1` | recenters and sweeps left |
| 4 | SWEEP_RIGHT_INVERSE | `num_leds - 1` | toggles polarity to INVERSE, sweeps right (off-gap) |
| 5 | SWEEP_LEFT_INVERSE | `num_leds - 1` | sweeps the off-gap left |
| 6 | SWEEP_RIGHT_ALT_STEP | `num_leds * 3` | per position: base / opposite / base (polarity toggles only, no movement), then one `rotate()` |
| 7 | SWEEP_LEFT_ALT_STEP | `num_leds * 3` | same triplets, sweep left |

Phase transitions keep the last state on screen one full tick before the
next setup runs (`g_phase_pending`, `main.c:320-329`), so boundaries
never truncate a state. Direction/polarity are set once per phase in
`demo_phase_setup()`.

The expected write sequence for the ALT_STEP phases (8 LEDs) is captured
externally in `scripts/seq_compare/expected/ex9_alt_step_phase67.txt`
(23 masks per phase; the initial base state is inherited, not written).

---

## 5. Configuration

### Logging / trace discipline

- Trace is **compile-time gated**: `LOG_MODULE_REGISTER` uses a Kconfig
  level (`CONFIG_LED_BAR_LOG_LEVEL`, `CONFIG_EX9_DEMO_LOG_LEVEL`) fed by
  the `Kconfig.template.log_config` pattern (`module = LED_BAR` in
  `drivers/led_bar/Kconfig`, `module = EX9_DEMO` in the app `Kconfig`).
- `prj.conf` sets the levels to the default (`INF`) and never toggles
  debug in source files. DBG is enabled only through the fragment
  `debug.conf` passed with `-DEXTRA_CONF_FILE=debug.conf`.
- The default build therefore contains **no** demo/driver trace strings
  (verified with `strings` on the ELF). `printk` is kept only for
  `Exercise 9: custom LED bar driver demo` and fatal errors.

### Config files

| File | Purpose |
|---|---|
| `prj.conf` | `CONFIG_LED_BAR=y`, `CONFIG_LOG=y` |
| `debug.conf` | `CONFIG_LED_BAR_LOG_LEVEL_DBG=y`, `CONFIG_EX9_DEMO_LOG_LEVEL_DBG=y` |
| `boards/frdm_mcxn236.overlay` | the default 8-LED bar node |
| `overlays/*.overlay` | experimental LED-count variants, applied with `-DEXTRA_DTC_OVERLAY_FILE` |

---

## 6. Build & flash

All commands run from the exercise root. Builds seen so far
(measured FLASH sizes):

| Build | Command | Expected |
|---|---|---|
| default (8 LEDs, no trace) | `west build -b frdm_mcxn236 . -d build_ex9` | OK, FLASH 36480 B |
| debug | `west build -b frdm_mcxn236 . -d build_ex9_dbg -DEXTRA_CONF_FILE=debug.conf` | OK, FLASH 37868 B |
| 4 LEDs | `west build -b frdm_mcxn236 . -d build_ex9_4leds -DEXTRA_DTC_OVERLAY_FILE=overlays/led_bar_4leds.overlay` | OK, FLASH 36448 B |
| 3 LEDs (negative test) | `west build -b frdm_mcxn236 . -d build_ex9_3leds -DEXTRA_DTC_OVERLAY_FILE=overlays/led_bar_3leds.overlay` | **fails** at compile time in `LED_BAR_DEFINE` |

The 3-LED build fails with, e.g.:

```
error: static assertion failed: "\"gpios\" must list at least
LED_BAR_MIN_LEDS (4) LEDs for a led-bar node"
```

Flashed hardware currently runs the **8-LED** default build. Flashing
uses the `pyocd` runner (LinkServer is not installed on this host):

```
west flash -d build_ex9 --skip-rebuild --runner pyocd
```

---

## 7. On-hardware validation workflow

Host-side tools (all under `/workdir/scripts/<topic>/`, each with its
own `.venv`, pinned `requirements.txt`, `README.md`, and git-ignored
`captures/`):

1. **`scripts/serial_capture/`** — generic serial logger.
   `serial_capture.py --port /dev/ttyACM0 --baud 115200 --duration 45
   --output captures/<run>.txt` (optional `--timestamp`, `--raw`).
2. **`scripts/seq_compare/`** — section-aware sequence comparator:
   `seq_compare.py --expected expected/<file>.txt --capture
   captures/<run>.txt --pattern 'mask=0x' ...`. Exercise-specific
   expectations live in `expected/` data files, never in the script.

Current evidence (8 LEDs): debug build flashed, captured, and **PASS**ed
against `ex9_alt_step_phase67.txt` (23/23 masks per phase) on real
hardware, including a full 2-cycle capture; the phase 6→7 timing was
also cross-checked against the tick analysis.

---

## 8. Pending work for a full 4/5/6/7-LED test cycle

Goal: prove the driver + demo behave correctly for every supported count
(4..8), on hardware and by capture comparison, not just visually.

### 8.1 Known open issue — 4-LED visual behavior

A 4-LED build was flashed and exercised, but **some sweeps looked
wrong/unexpected to the eye** (the user reverted to the 8-LED build).
This must be diagnosed before trusting any small-count build:

- Determine whether it is merely a physical artifact (only gpio1 pins
  0-3 driven; LEDs 4-8 of the protoboard stay dark, wrap jumps between
  LED4 and LED1), or a real logic problem (mask/step-count mismatch,
  phase-total off-by-one for non-8 counts, edge rollover).
- The decision driver should be `seq_compare`, not visual: generate
  per-count expected sequences (see 8.3), run on hardware, and only
  accept the count when the capture PASSes.
- Re-check `demo_phase_total()` (`main.c:153-166`) and the ALT_STEP
  step/login for small counts (`main.c:259-287`).

### 8.2 Clean stale 8-LED wording

Now that the bar is variable, remaining hardcoded "8" text is
misleading:

- `drivers/led_bar/Kconfig:8-9` — help text still says "8-LED bar, 8
  entries".
- `led_bar.h` comments still say "starts the isolated bit at LED7" /
  "LED0..LED7" style wording in a few spots.
- `src/main.c` file header (lines 3, 10, 30) still says "9-phase demo",
  "8-LED bar (gpio1 pins 0-7)". Decide whether the file header keeps the
  board-specific 8-LED description (valid for the FRDM protoboard) or
  becomes count-neutral.

### 8.3 Per-count test overlays + expected sequences

- Add `overlays/led_bar_5leds.overlay`, `_6leds.overlay`,
  `_7leds.overlay` (gpio1 pins 0..N-1), mirroring the 4-LED one.
- Generate the expected ALT_STEP step sequences for 4, 5, 6 and 7 LED
  counts into `scripts/seq_compare/expected/`
  (`ex9_alt_step_phase67_<N>leds.txt`).
  Prefer generating them from a validated host-side model of the demo
  (simulator is reference only) and/or from a trusted full cycle on
  hardware; never hand-derive masks.
- Note the 23-mask-per-phase baseline is specific to 8 LEDs
  (`num_leds*3 - 1` writes); smaller bars produce fewer entries
  (4-LED ⇒ 11) — the expected file must track the count.

### 8.4 Run the full matrix

For each count in {4, 5, 6, 7} (8 already done):

1. `west build -b frdm_mcxn236 . -d build_ex9_Nleds
   -DEXTRA_DTC_OVERLAY_FILE=overlays/led_bar_Nleds.overlay`
2. same + `-DEXTRA_CONF_FILE=debug.conf` into `build_ex9_Nleds_dbg`
3. flash the debug build with `--runner pyocd`
4. capture ≥ 2 full cycles with `serial_capture.py`
5. `seq_compare.py` against the per-count expected file → PASS
6. report FLASH footprint per count; flag any count that deviates.

Keep the normal (non-debug) build free of trace strings and re-run the
`strings` check per count.

### 8.5 Possible follow-ups (only if asked)

- Widen `led_mask` to a larger type (e.g. `uint64_t`) if bars with more
  than 8 LEDs are ever wanted — this raises `LED_BAR_MAX_LEDS` and
  touches `BIT(num_leds)` callers.
- The `demo_phase_total()` step tables are length-derived already, so a
  future count change should not require demo edits — verify that
  invariant holds in the 4/5/6/7 runs.

---

## 9. Handy commands (next cycle)

```sh
# from the exercise root
west build -b frdm_mcxn236 . -d build_ex9
west build -b frdm_mcxn236 . -d build_ex9_4leds \
  -DEXTRA_DTC_OVERLAY_FILE=overlays/led_bar_4leds.overlay
west build -b frdm_mcxn236 . -d build_ex9_4leds_dbg \
  -DEXTRA_DTC_OVERLAY_FILE=overlays/led_bar_4leds.overlay \
  -DEXTRA_CONF_FILE=debug.conf
west flash -d build_ex9_4leds_dbg --skip-rebuild --runner pyocd

# host-side capture + compare
/workdir/scripts/serial_capture/.venv/bin/python \
  /workdir/scripts/serial_capture/serial_capture.py \
  --port /dev/ttyACM0 --baud 115200 --duration 60 \
  --output /workdir/scripts/seq_compare/captures/run_Nleds.txt

/workdir/scripts/seq_compare/.venv/bin/python \
  /workdir/scripts/seq_compare/seq_compare.py \
  --expected /workdir/scripts/seq_compare/expected/ex9_alt_step_phase67_4leds.txt \
  --capture   /workdir/scripts/seq_compare/captures/run_4leds.txt
```

> Note: build dirs are git-ignored (`**/build/`); the whole
> `mcxn236_exercise_9/` folder plus `scripts/{serial_capture,seq_compare,
> ex9_demo_sim}/` are currently untracked in git.