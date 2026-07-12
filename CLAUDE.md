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

## Planned pin assignments (not yet in src/main.cpp)
Everything else (displays, TFT, button lights) has been remapped into
code above. Still not implemented: the button switch inputs and the
rotary encoder.

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

**Spare:** GPIO38 — free for the buzzer if it comes back in.

**Caveat:** GPIO19 (Button switch Action) shares the net with this
board's native USB D− line. Using it as a button input may interact
with USB flashing/serial depending on the external wiring — accepted
as a tradeoff, not yet field-tested.

`ESP32Encoder` is in `platformio.ini` lib_deps but not yet used in
`main.cpp`.

`docs/images/gpio-header-map.svg` — visual sketch of the full J1/J3
header layout above, color-coded by signal group, for reference while
soldering the prototype board.

## Firmware status
`src/main.cpp` is currently a **hardware bring-up / demo sketch**, not the
real timer logic. It exercises every output (TFT splash, color test,
button-light chase, per-player display/color show, a countdown output
demo, a flashing finale) to confirm the hardware works together. It does
NOT yet:
- Read any button presses (inputs).
- Use the rotary encoder.
- Implement the actual round-robin turn logic, countdown state machine,
  or the WiFi config page.

Building the real firmware means adding all of the above on top of this
verified-working output layer — the display/TFT/LED driving code here is
already proven and can be reused as-is.

## Build
PlatformIO, `env:esp32-s3-devkitc-1`, Arduino framework. 16MB flash,
`default_16MB.csv` partitions, PSRAM enabled (`qio_opi`, `BOARD_HAS_PSRAM`).

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
