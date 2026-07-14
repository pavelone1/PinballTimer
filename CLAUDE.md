# Pinball 4-Player Round-Robin Timer

> **Status: preliminary design, still evolving.** Everything below reflects
> decisions made so far, not a locked spec. Expect hardware choices, pin
> assignments, and even the mode rules to keep changing — check with the
> user before treating anything here as final, especially the TBD items.

## What this is
A chess-clock-style timer for a recurring 4-player pinball contest at a fixed
venue. One shared pinball machine, up to 4 players rotate turns on it. Not
four separate machines — this is a single physical unit that sits beside the
one machine being played.

## Mode 1 rules (current/only mode; more modes planned later)
- Supports 1-4 players (configurable).
- Each player gets an equal, configurable countdown per turn (seconds,
  not a depleting total bank — resets to the full configured value every
  turn, like a chess "move timer with no save").
- A separate **Action button** (distinct from the 4 player buttons) starts
  the round and starts Player 1's countdown.
- Player taps their own button to end their turn; timer advances to the
  next player's turn and resets their countdown to the full value.
- **Undecided / TBD:** what happens when a player's time hits zero
  (auto-advance vs. buzzer-only vs. something else). Don't assume either
  behavior in code without flagging it — ask before hardcoding.
- Timer display range: 00:00 to 99:59 (MM:SS). Configurable seconds-per-turn
  is capped at 5999.
- Each player's display stays lit at all times showing their last remaining
  time (like a physical chess clock) — only the active player's display is
  actually ticking down.

## Config
One-time setup via a WiFi-hosted local web page served by the ESP32
(player count 1-4, seconds-per-turn). Saved to flash. Not meant to be
changed often — no on-device settings screen needed for this, though the
rotary encoder is separately used for mode selection/control (see below).

## Hardware
- **MCU:** Hosyond ESP32-S3, N16R8 (16MB flash, 8MB PSRAM), 3-pack.
  - PSRAM reserves GPIO 35, 36, 37 — don't use these.
  - Strapping pins to avoid for buttons/inputs: GPIO 0, 3, 45, 46.
- **Player/turn displays:** 4x TM1637 4-digit 7-segment (WWZMDiB brand,
  same driver as the common TM1637 modules). Always-on, one per player.
- **Secondary display:** 1x AITRIP 2.0" TFT, ST7789V driver, 240x320, SPI.
  For additional info (not a replacement for the 7-segment player displays).
  Library: Adafruit_ST7789 + Adafruit_GFX.
- **Buttons:** 5x illuminated arcade pushbuttons (4 player + 1 Action).
  Each has separate switch contacts and an LED.
- **Button LED driver:** ULN2803A (Darlington sink array) — button LEDs
  need a 5V supply and more current than ESP32 GPIO can provide directly,
  so GPIO drives a ULN2803A input, which sinks the LED to ground on the
  5V rail when that channel is on.
- **Rotary encoder:** 1x KY-040 (of 5 bought — rest are spares), used for
  mode selection / on-device control. Should be read via the ESP32's
  hardware pulse counter (PCNT) on native GPIO, not through an I2C
  expander — polling quadrature over I2C risks missed steps.
- **GPIO expansion:** MCP23017 (I2C expander) — **currently NOT used**.
  Skipped for the prototyping phase since native GPIO is sufficient.
  Only reintroduce this if we actually run out of native pins. If it's
  added later, it would carry the 5 button inputs + 5 LED outputs, freeing
  native GPIO for the TFT/displays/encoder/buzzer.
- **Buzzer:** active piezo module (of 5 bought, 1 used). Confirmed active
  (has its own oscillator) — drive with a plain digitalWrite HIGH/LOW,
  no tone()/PWM needed.
- **Power:** single 18650 li-ion cell (protected, button-top), in a
  diymore V3 battery shield (1-holder, 5V/2A + 3V/1A outputs, micro USB
  charging in). ESP32 powered via USB cable from the shield's output port
  straight into the ESP32 module's own USB-C, not via GPIO/VIN wiring.
  - Note: the shield's own slide switch is labeled Normal/Hold, NOT a true
    on/off — it changes auto-shutoff-on-no-load behavior, doesn't fully
    disconnect the battery. A separate inline SPST switch between the
    cell and the shield's battery input is the actual power switch for
    storage between events.
- **Enclosure:** portable, 3D printed (PETG), two-shell design with
  heat-set brass inserts so it can be reopened for repairs.

## Current pin assignments (as actually wired in src/main.cpp)
These are the pins currently in code, remapped for the new solder
prototype board (buttons on J1, TFT on J3 — see the swap decided
below). Unlike the original 4/5/6/7... layout, this specific pin set
has not yet been physically verified on hardware — treat it as
code-correct but hardware-unconfirmed until the new board is tested.
Treat any earlier PCB/schematic file that disagrees with this list as
stale and needing a sync, not the other way around.

| Signal              | GPIO |
|----------------------|------|
| Display 1 CLK / DIO  | 40 / 39 |
| Display 2 CLK / DIO  | 9 / 10 |
| Display 3 CLK / DIO  | 13 / 14 |
| Display 4 CLK / DIO  | 47 / 21 |
| TFT SCLK             | 43 |
| TFT MOSI             | 44 |
| TFT RST              | 1 |
| TFT DC               | 2 |
| TFT CS               | 42 |
| Button light P1      | 5 |
| Button light P2      | 7 |
| Button light P3      | 15 |
| Button light P4      | 17 |
| Button light Action  | 8 |

## Button switch / encoder pin assignments
Not read by `src/main.cpp` (the demo sketch never reads inputs), but
implemented in the real firmware (see "Firmware architecture" below)
via `HardwarePins.h`, `ButtonInput`, and `EncoderInput`.

| Signal | GPIO |
|--------|------|
| Button switch P1 | 4 |
| Button switch P2 | 6 |
| Button switch P3 | 16 |
| Button switch P4 | 18 |
| Button switch Action | 19 |
| Encoder CLK | 11 |
| Encoder DT | 12 |
| Encoder SW | 41 |

**Spare:** GPIO38 — free for the buzzer once it's wired.

**Caveat:** GPIO19 (Button switch Action) shares the net with this
board's native USB D− line. Using it as a button input may interact
with USB flashing/serial depending on the external wiring — accepted
as a tradeoff, not yet field-tested.

`docs/images/gpio-header-map.svg` — visual sketch of the full J1/J3
header layout above, color-coded by signal group, for reference while
soldering the prototype board.

## Firmware status
Two separate things now exist in this repo:

- **`src/main.cpp`** — still the original **hardware bring-up / demo
  sketch** (TFT splash, color test, button-light chase, countdown
  output demo). Deliberately left untouched throughout the module
  build-out below, per earlier direction. Does not read any inputs.
- **The real firmware** — see "Firmware architecture" below. Builds
  and links clean (`pio run -e app`), but has not been flashed to
  actual hardware yet — treat it as code-correct/build-verified only
  until tested on the board.

## Firmware architecture
Built module-by-module following the layered structure in the
project's architecture plan (App → GameMode → Managers → Hardware
drivers). Lives under `include/`/`src/` in the same subfolders as
`main.cpp`, but is excluded from the default build; it has its own
PlatformIO environment (`app`) with its own tiny entry point at
`examples/app_main.cpp`, so it never touches or replaces `main.cpp`.

- `App` — central coordinator. Owns every subsystem below, calls each
  one's `update()` every loop, routes input events to the active game
  mode, tracks `SystemState` (re-derived each tick from
  `GameModeManager`/`PowerManager`, not decided by App itself). Never
  enters an `Error` state — no concrete error conditions are defined
  anywhere yet.
- **Input:** `ButtonInput`, `EncoderInput` — debounced polling,
  pressed/released/short/long-press event queues.
- **Output:** `NumericDisplayManager` (the 4 TM1637s — see "Zero
  crossing" below for the negative-time behavior it implements),
  `TftDisplayManager` (generic `showStatusScreen()`, plus real
  ST7789 `sleep()`/`wake()` commands), `ButtonLightManager` (base
  state + priority temporary overrides, digital on/off only — no PWM
  brightness, the hardware doesn't support it).
- **Game:** `TimerManager` (generic count engine, independent of
  players/displays), `PlayerManager`, `ButtonAssignmentManager` /
  `DisplayAssignmentManager` (what each physical button/display
  currently represents), `GameMode` interface + `GameModeManager` +
  `ModeRegistry`.
- **Modes:** `Mode1RoundRobin` — the actual CLAUDE.md Mode 1 rules.
  **Zero-crossing rule change still pending** (see below) — currently
  implements `allowBelowZero=true` (counts negative, flashes) as a
  placeholder, not the "stop at zero + buzzer" behavior that was
  requested but deferred until all modules are built.
- **Storage:** `SettingsStorage`, `GameStorage` — ESP32 `Preferences`
  (NVS), no new library dependency. "Saved presets" and per-timer
  custom values are NOT implemented (no concrete format designed).
- **Network:** `NetworkManager` (WiFi connect/reconnect/standby),
  `DirectorControl` + `StatusReporter` + `RemoteCommand` — HTTP REST
  via ESP32's built-in `WebServer` (no extra library). `GET /status`
  returns hand-built JSON; `POST /command` takes form-encoded
  `type`/`intValue`/`stringKey`/`longValue` fields (command type names
  and full semantics documented in `include/network/DirectorControl.h`
  and `include/network/RemoteCommand.h`). This concrete API shape was
  a judgment call — no existing spec or director client to match, so
  it's the de facto spec now unless changed.
- **Power:** `PowerManager` — idle-timeout standby that blanks the
  TFT/displays/lights but never puts the ESP32 itself into light/deep
  sleep, so remote (WiFi/HTTP) control stays reachable throughout.
  Battery monitoring not implemented (no ADC pin assigned yet).

**Zero-crossing / what happens at zero (still open):** the user
indicated the real intended behavior is that once a countdown reaches
zero, the player is required to stop — a future buzzer sounds and the
timer ceases (no counting into negative overtime) — but asked to
finalize game rules after all modules were built. That discussion
hasn't happened yet. `Mode1RoundRobin` and `NumericDisplayManager`
still implement the earlier placeholder (negative overtime + rapid
flash); this needs revisiting.

## Build
PlatformIO. Four environments, all in `platformio.ini`:

| Environment | What it builds | Touches main.cpp? |
|---|---|---|
| `esp32-s3-devkitc-1` (default) | `src/main.cpp`, the hardware bring-up demo | yes, this *is* main.cpp |
| `display-test` | `examples/display_flash_test.cpp` — per-display digit + TFT color test | no |
| `full-io-test` | `examples/full_io_test.cpp` — every wired input/output, no buzzer | no |
| `app` | The real firmware (`App` + all managers) via `examples/app_main.cpp` | no |

Run any of them with `pio run -e <name> -t upload`. Board config
(16MB flash, `default_16MB.csv` partitions, PSRAM `qio_opi`,
`BOARD_HAS_PSRAM`) is shared via the default env; the others `extends`
it.

## Unit tests
`test/` has native (host machine, no ESP32/board needed) tests for the
pure-logic modules with zero Arduino/hardware dependency:
`PlayerManager`, `ButtonAssignmentManager`, `DisplayAssignmentManager`.
`TimerManager` is deliberately NOT covered here — it calls `millis()`
directly, and injecting a mockable clock into production code just for
testability wasn't asked for.

Run with `pio test -e native`. Requires a host C/C++ compiler on PATH
(this machine didn't have one — installed WinLibs MinGW-w64 via
`winget install BrechtSanders.WinLibs.POSIX.UCRT` to get `gcc`/`g++`).
21 test cases, all passing as of the last run.

## Related files
A KiCad carrier-board project exists (separate from this repo) for a
custom PCB to host the ESP32 module and break out connectors for the
buttons/displays/buzzer. It was generated before the pin table above was
finalized and needs its nets updated to match before routing/ordering
Gerbers from PCBWay.

`docs/images/prototype-esp32-uln2803.jpeg` — photo of the physical
prototype: the ESP32-S3 N16R8 module mounted on a perfboard prototyping
shield, with the ULN2803A driver breakout socketed directly beneath it.
The pin labels visible in the photo match the standard
ESP32-S3-DevKitC-1 pinout assumed elsewhere in this doc and in the KiCad
carrier-board project. Exact GPIO-to-ULN-channel wiring under the
connector isn't fully confirmed from the photo alone — verify against the
physical board before relying on it.
