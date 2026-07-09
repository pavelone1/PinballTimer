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
These are the real, working pins — treat any earlier PCB/schematic file
that disagrees with this list as stale and needing a sync, not the other
way around.

| Signal              | GPIO |
|----------------------|------|
| Display 1 CLK / DIO  | 4 / 5 |
| Display 2 CLK / DIO  | 6 / 7 |
| Display 3 CLK / DIO  | 15 / 16 |
| Display 4 CLK / DIO  | 17 / 18 |
| TFT SCLK             | 8 |
| TFT MOSI             | 9 |
| TFT RST              | 10 |
| TFT DC               | 11 |
| TFT CS               | 12 |
| Button light P1 (ULN 2B) | 38 |
| Button light P2 (ULN 3B) | 39 |
| Button light P3 (ULN 4B) | 40 |
| Button light P4 (ULN 5B) | 41 |
| Button light Action (ULN 6B) | 42 |

**Not yet assigned in code:** the 5 button *switch inputs* (only the LED
outputs are wired so far), and the rotary encoder pins (CLK/DT/SW).
`ESP32Encoder` is in `platformio.ini` lib_deps but not yet used in
`main.cpp`.

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
