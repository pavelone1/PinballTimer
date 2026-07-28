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
- Each player gets ONE configurable time budget for the WHOLE game
  (chess-clock style, default 3:00) — NOT a fresh allowance reset every
  turn (an earlier version of this doc, and an earlier bug, both said
  otherwise; see "Ball count / round elimination" below for the full
  resolved ruleset and CLAUDE.md's own postmortem of that bug).
- A separate **Action button** (distinct from the 4 player buttons, white
  in REV2 hardware) starts the round and Player 1's turn.
- Player taps their own button to end their turn; passes to the next
  player in fixed color order, their own clock now paused (not reset)
  until the rotation comes back to them.
- Timing out (reaching zero) ends that player's whole game — see "Ball
  count / round elimination" below for the full resolved ruleset.
- Tapping the Action button once a round is in play pauses/resumes the
  active player's clock (`Mode1RoundRobin::togglePause()`) — the active
  player's button goes dark, Action flashes, TFT says PAUSED. Distinct
  from *holding* Action 5s, which still opens DirectorMenu as before.
  The toggle fires on the button's Released event, not Pressed —
  deliberately, so it can never race with the 5s hold: once that
  gesture opens DirectorMenu mid-hold, `App::handleButtonEvent()`'s
  menu-open guard swallows the eventual release of that same press
  before it reaches the mode at all, so a long hold always ends up
  cleanly paused with no partial un-pause blip (an earlier version
  fired on Pressed instead, which had exactly that bug).
- Timer display range: 00:00 to 99:59 (MM:SS). Configurable seconds-per-turn
  is capped at 5999.
- Each player's display stays lit at all times showing their last remaining
  time (like a physical chess clock) — only the active player's display is
  actually ticking down.

## Config
`BootMenu` (see "UI" under Firmware architecture) is now the on-device
settings/setup screen, shown every boot: start/resume a game, pick a
mode, set player names, and configure WiFi (join a network, or run the
device as its own hotspot) all live there, navigated with the rotary
encoder. The originally-planned separate "WiFi-hosted local web page
for one-time setup" exists too, but only for WiFi specifically
(`WifiPortal`) — there's no equivalent web page for player
count/seconds-per-turn/mode selection, those are BootMenu/on-device
only for now.

## Hardware
- **MCU:** Hosyond ESP32-S3, N16R8 (16MB flash, 8MB PSRAM), 3-pack.
  - PSRAM reserves GPIO 33, 34, 35, 36, 37 (this is an R8 module —
    octal PSRAM uses SPIIO4-7 + SPIDQS on all five of those pins, not
    just 35-37) — don't use these.
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
  5V rail when that channel is on. In REV2, the "GPIO" driving each
  ULN2803A input is MCP23017 GPB0-4, not native ESP32 GPIO -- kept at
  VDD=3.3V (matching the ESP32 I2C bus) specifically so the ULN2803A
  stays the only place the design crosses into the 5V domain; powering
  the MCP23017 itself at 5V instead was considered and rejected, since
  its I2C input thresholds scale with VDD (~0.7x) and would stop
  reliably recognizing a 3.3V-safe bus as logic-high. Confirmed 18-pin
  pinout: `docs/images/uln2803-pinout.svg`.
- **Rotary encoder:** 1x KY-040 (of 5 bought — rest are spares), used for
  mode selection / on-device control. Should be read via the ESP32's
  hardware pulse counter (PCNT) on native GPIO, not through an I2C
  expander — polling quadrature over I2C risks missed steps.
- **GPIO expansion:** MCP23017 (I2C expander) — **REV2 decision: now
  adopted** (see "Pin assignments — REV2" below). Carries the 5 button
  switches (GPA0-4) + 5 button lights (GPB0-4, still through the
  ULN2803A same as before), freeing 10 native GPIO so the TFT can move
  off GPIO43/44 — those double as UART0 TX/RX, which was killing the
  serial console the instant the screen initialized, the whole reason
  for this revision. Read via plain I2C polling each `App` loop tick,
  same as the rest of `ButtonInput` — no INTA/INTB interrupt wiring,
  a deliberate simplicity call: buttons are already software-debounced,
  unlike the encoder's quadrature signal, which does need native PCNT
  to avoid missed steps. I2C address confirmed 0x20 -- A0/A1/A2 wire
  directly to GND (no resistor needed, unlike SDA/SCL; they're static
  CMOS config inputs, not signal lines -- don't leave them floating).
  Only matters if a second MCP23017 is ever added later (it would tie
  A0 to VDD instead, giving 0x21); not applicable with just the one
  expander this design uses. Needs its own SDA/SCL pull-ups (4.7-10k
  to 3.3V) and 3.3V VDD to match the ESP32's logic levels -- not yet
  physically wired, and the firmware
  (`ButtonInput`/`ButtonLightManager`) hasn't been converted from
  native GPIO to I2C yet either.
- **Buzzer:** passive piezo module (switched from the active module
  this project started with) — no oscillator of its own, driven via
  the ESP32's LEDC peripheral at an actual frequency, see
  `BuzzerManager` and "Buzzer" below.
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

## REV1 pin assignments (soldered prototype — confirmed on hardware)
The pins the REV1 prototype board was actually built with (buttons on
J1, TFT on J3). Confirmed on real hardware — two Hosyond N16R8 units
have booted the real `app` firmware on this exact pin set all the way
to `BootMenu` (see "Firmware status" below). **Superseded by REV2**
(see below) to get the TFT off GPIO43/44 — treat this table as
historical/frozen, don't wire a new board to it. Treat any earlier
PCB/schematic file that disagrees with this list as stale and needing
a sync, not the other way around.

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

## REV1 button switch / encoder pin assignments
Not read by `src/main.cpp` (the demo sketch never reads inputs), but
implemented in the real firmware (see "Firmware architecture" below)
via `HardwarePins.h`, `ButtonInput`, and `EncoderInput`. Button
switches here are REV1-only — REV2 (below) moves them to the
MCP23017. Encoder pins here are REV1-only too — REV2 moves them to
GPIO1/2/42 so all 3 are physically adjacent (REV1 has them split
across two different headers, J1 for CLK/DT and J3 for SW).

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

**Buzzer:** GPIO38 — confirmed pin for the passive piezo buzzer (was listed as spare/free before; REV1 used an active module on this same pin, since swapped for passive). Unchanged between REV1 and REV2.

**Caveat:** GPIO19 (Button switch Action) shares the net with this
board's native USB D− line. Using it as a button input may interact
with USB flashing/serial depending on the external wiring — accepted
as a tradeoff, not yet field-tested.

`docs/images/gpio-header-map.svg` — visual sketch of the full J1/J3
header layout above, color-coded by signal group, for reference while
soldering the prototype board.

`docs/images/rev2-pinout.svg` — same J1/J3 header diagram, redrawn for
REV2: TFT/Display 4/I2C moved onto their new pins, which GPIOs become
fully free, GPIO19 marked deliberately-unused rather than reused, plus
the MCP23017's own bank A/GPA0-4 (switches) and bank B/GPB0-4 (lights)
pin map. Board not built yet -- reference for when it is.

## REV2 hardware (pin table + firmware CONFIRMED — physical board BUILT, now primary)
Pin table below is confirmed, the firmware side is written and
building clean (`pio run -e app-rev2`) — see "REV2 firmware (MCP23017
IO backend)" under "Firmware architecture" below — and as of this
update the physical board itself is built and connected (MCP23017
wired in per the confirmed bank A/B mapping, TFT/Display 4 on their
new pins). **REV2 is now the primary board going forward.** REV1
still physically exists as a separate board (kept as a
secondary/reference unit, not disassembled or repurposed into REV2),
but is no longer the main development target. Not yet done:
`[env:app-rev2]` (the real `App` firmware) hasn't been flashed to the
REV2 board yet -- only the standalone diagnostics (`display-test`,
`full-io-test`, see "Build" below) have been rewritten for and target
this hardware so far. `HardwarePins.h`'s REV1 constants stay
untouched regardless; the REV2 numbers live behind `#ifdef
PINBALLTIMER_REV2` in the same file.

**Why:** the REV1 pin table above puts `TFT_SCLK`/`TFT_MOSI` on
GPIO43/44, which are also this board's UART0 TX/RX -- the pins the
CH343 USB-serial console uses. The instant `TftDisplayManager::begin()`
runs, the serial console becomes permanently unusable for the rest of
that boot (confirmed harmless to game logic, but it made diagnosing
the unrelated boot crash in "Firmware status" below dramatically
harder than it needed to be -- at one point diagnosis required
pulling a coredump out of the flash partition instead of just reading
normal boot logs, specifically because the console was gone by the
time anything worth seeing happened). REV2 adds an MCP23017 I2C GPIO
expander (already anticipated, never wired, see "GPIO expansion" in
the Hardware section above) to carry the 5 button switches + 5 button
lights over I2C instead of native GPIO, freeing enough native pins
that the TFT can move off GPIO43/44 entirely -- console and screen
both stay usable together, permanently.

**Confirmed pin table** (corrected -- see below, GPIO numbers alone
were misleading):

| Signal | REV1 (current) | REV2 (confirmed) | Physical position |
|---|---|---|---|
| TFT SCLK | 43 | 4 | J1 pos 4 |
| TFT MOSI | 44 | 5 | J1 pos 5 |
| TFT RST | 1 | 6 | J1 pos 6 |
| TFT DC | 2 | 7 | J1 pos 7 |
| TFT CS | 42 | 15 | J1 pos 8 |
| *(freed -- gap by design)* | -- | 16 | J1 pos 9 |
| I2C SDA (→ MCP23017) | -- | 17 | J1 pos 10 |
| I2C SCL (→ MCP23017) | -- | 18 | J1 pos 11 |
| *(freed -- was Display 4 DIO)* | -- | 8 | J1 pos 12 |
| *(deliberately unused)* | -- | 19 | J3 pos 20 |
| Display 1 DIO | 39 | 40 | J3 pos 8 |
| Display 1 CLK | 40 | 39 | J3 pos 9 |
| Display 4 DIO | 21 | 47 | J3 pos 17 |
| Display 4 CLK | 47 | 21 | J3 pos 18 |
| *(freed -- was Display 2 CLK)* | -- | 9 | J1 pos 15 |
| Display 2 CLK | 9 | 10 | J1 pos 16 |
| Display 2 DIO | 10 | 11 | J1 pos 17 |
| Display 3 | unchanged | unchanged | J1 pos 19-20 |
| Encoder CLK | 11 | 1 | J3 pos 4 |
| Encoder DT | 12 | 2 | J3 pos 5 |
| Encoder SW | 41 | 42 | J3 pos 6 |
| Button switches P1-4/Action | 4, 6, 16, 18, 19 | → MCP23017 GPA2(P1), GPA0(P2), GPA4(P3), GPA3(P4), GPA1(Action) -- NOT sequential, see below | -- |
| Button lights P1-4/Action | 5, 7, 15, 17, 8 | → MCP23017 GPB4(P1), GPB1(P2), GPB2(Action), GPB3(P3), GPB0(P4) -- NOT sequential, does NOT mirror bank A, still through ULN2803A | -- |

GPIO8, 9, 12, 16, 41, 43, 44 become fully free. **Correction from the
first draft:** "TFT on GPIO4-8" was contiguous by *number* but not by
*physical header position* -- on this board's J1 header, position 8
is GPIO15, not GPIO8 (GPIO8 doesn't show up until position 12, four
slots further down; see `docs/images/rev2-pinout.svg`, which is what
caught this). Putting TFT_RST on GPIO8 would've left it physically
stranded from the other 4 TFT pins. Fixed by moving the TFT's 5th
signal onto GPIO15 (position 8, physically adjacent to the rest of
the TFT block) and cascading I2C SDA/SCL and (at the time) Display 4
CLK/DIO one slot down each -- per the original goal of easy physical
header routing. (Display 4 has since moved off J1 entirely, I2C has
since shifted again, and which *signal* sits on GPIO15 changed too --
see the corrections below; this paragraph is kept for the history of
*why* GPIO15 rather than GPIO8 is the TFT block's 5th pin at all,
which is still current -- just not which signal.)

**Second correction, same day:** REV1's encoder pins (CLK/DT on J1
positions 17-18, SW on J3 position 7) were split across two different
physical headers entirely. Moved to GPIO1/2/42 -- J3 positions 4-6,
freed by the TFT's move off GPIO1/2/42 (its old RST/DC/CS pins) --
so all 3 encoder pins are now physically consecutive on the same
header. This is what pushed GPIO11/12/41 onto the freed list instead
of GPIO1/2/42 (which are now the encoder's, not free).

**Third correction, same day:** Display 4 moved off J1 (positions
11-12, freed by the encoder no longer needing them -- see above) onto
J3's free positions 17-18, which happen to be its own original REV1
pins (GPIO47/21) coming back free again. Also established a
deliberate mirrored convention for the two right-side (J3) displays:
Display 1 and Display 4 both put DIO on the physically upper pin and
CLK on the lower one, the reverse of the left (J1) header's
convention, where Display 2/3 keep CLK-upper/DIO-lower. Display 1's
GPIO39/40 assignment therefore swapped too (was CLK=40/DIO=39, now
CLK=39/DIO=40) even though neither pin itself moved -- only which
signal each one carries changed. This is what moved GPIO18/8 (J1,
freed) in and GPIO21/47 (J3, now Display 4) out of the freed list.

**Fourth correction, same day:** user asked to shift the I2C
connections down one position. I2C SDA/SCL moved from J1 positions
9-10 (GPIO16/17) to positions 10-11 (GPIO17/18) -- GPIO18 was free
(vacated by Display 4's move to J3, see above) so this didn't collide
with anything. Net effect: GPIO16 (position 9) is now a deliberate
one-pin gap between the TFT block (positions 4-8) and I2C (positions
10-11) -- they're no longer one merged contiguous run, by request,
each is its own contiguous group instead. This is what moved GPIO18
off the freed list (now I2C SCL) and put GPIO16 on it instead.

**Fifth correction, same day:** user asked to shift Display 2 down one
position too. Moved from J1 positions 15-16 (GPIO9/10) to positions
16-17 (GPIO10/11) -- GPIO11 (position 17) was free (vacated by the
encoder's move to J3) so no collision. GPIO9 (position 15) is now
free instead. Display 2 keeps the left header's CLK-upper/DIO-lower
convention (unlike the right-header displays, which are mirrored) --
only its physical position shifted, not its handedness. This is what
moved GPIO11 off the freed list and put GPIO9 on it instead.
`DISPLAY_2_CLK/DIO` had to move from shared constants into the
`#ifdef PINBALLTIMER_REV2`/`#else` split in `HardwarePins.h`, same
reason as Display 1 and the encoder before it -- REV1 and REV2 now
genuinely differ there.

**Sixth correction, same day:** MCP23017 bank A (button switches) is
**not** wired in ButtonId order. Confirmed physical wiring: GPA0=P2,
GPA1=Action, GPA2=P1, GPA3=P4, GPA4=P3 (GPA4=P3 inferred by
elimination -- the user gave 4 of 5 explicitly, P3 was the only
button left unassigned; flagged to them for confirmation, not
independently verified). This is a real firmware bug fix, not just a
diagram update: `src/io/ButtonSwitchIo_Mcp23017.cpp` previously
assumed `GPA[index] == ButtonId[index]` (the naive sequential
mapping) and read the wrong bit for every button except P1's original
assumption. Fixed with a `kGpaBitForButton[]` lookup table indexed by
`ButtonId`, translating to the actual wired GPA bit before reading.
`pio run -e app-rev2` still builds clean after the fix. Bank B
(button lights, driven through the ULN2803A) is unaffected -- still
GPB0=P1, GPB1=P2, GPB2=P3, GPB3=P4, GPB4=Action, confirmed separately
via the ULN2803A channel-mapping conversation.

**Seventh correction, same day -- retracts part of the sixth:** bank B
is actually **not** sequential either. User said "fix the MCP outputs
like i told you" with no new numbers given, which was interpreted as
"mirror bank A's just-confirmed bit-to-button pattern onto bank B":
GPB0=P2, GPB1=Action, GPB2=P1, GPB3=P4, GPB4=P3 (identical
bit-for-bit to GPA's mapping). This is an inference, not a
word-for-word instruction -- flagged to the user, not independently
confirmed. Also a real firmware fix: `src/io/ButtonLightIo_Mcp23017.cpp`
had the same `GPB[index]==ButtonId[index]` bug as bank A, fixed the
same way with a `kGpbBitForButton[]` lookup table (values identical to
`kGpaBitForButton[]` -- `{2,0,4,3,1}` -- since the two banks mirror
each other). Every downstream reference to "GPB0=P1...GPB4=Action" in
this doc (including the sixth correction above) is now stale --
`docs/images/uln2803-pinout.svg`'s IN/OUT button labels were
re-derived through the ULN-channel-to-GPB mapping too (channel1=P2,
channel3=P4 unchanged, channel4=Action, channel5=P3, channel7=P1).
`pio run -e app-rev2` still builds clean.

**Eighth correction, same day -- the seventh's "mirrors bank A" guess
was wrong too:** user said the mirroring assumption was "blindly
assigned" and not what they gave, and pointed at the actual source:
"check MCP outputs to the ULN inputs." The real derivation combines
two facts given much earlier in the same conversation and never
individually wrong, just wrongly discarded: (1) the very first ULN
instruction's channel<->GPB pairing (channel1=GPB4, channel2=GPB3,
channel3=GPB0, channel4=GPB1, channel5=GPB2) -- channel2 isn't a used
button channel, so its GPB3 was left over; (2) the confirmed
channel<->button mapping (channel1=P1, channel3=P4, channel4=P2,
channel5=Action, channel7=P3) -- channel7 has no GPB from (1), so it's
left over. Matching the two leftovers (GPB3 <-> channel7's P3) closes
the set. Final, actually-correct GPB<->button mapping: **GPB0=P4,
GPB1=P2, GPB2=Action, GPB3=P3, GPB4=P1.** The bug in the sixth/seventh
corrections was recomputing GPB numbers from a guessed pattern
(sequential, then mirrored-A) instead of preserving the literal
channel<->GPB pairing from that first instruction throughout. Fixed
`kGpbBitForButton[]` again (now `{4,1,3,0,2}`), re-fixed both SVGs'
per-channel GPB+button labels to match, `pio run -e app-rev2` still
builds clean.

**Ninth correction, same day:** confirmed TFT signal order across its
5-pin block. Signals were already on the right 5 physical positions
(J1 4-8), but RST and CS were on the wrong two of them -- confirmed
order is SCLK, MOSI, RST, DC, CS, so `TFT_RST` moved to GPIO6
(position 6, was CS) and `TFT_CS` moved to GPIO15 (position 8, was
RST). GPIO4/5/7 (SCLK/MOSI/DC) unchanged. `pio run -e app-rev2` still
builds clean.

GPIO19 is left unused on purpose: it shares a net with the module's
native USB D− line, and REV1 already flagged that as an
accepted-but-imperfect tradeoff for a button switch (low activity);
putting a constantly-toggling TFT or I2C signal there instead would
be a worse version of the same tradeoff, so it's left idle rather
than reused.

**Resolved:** this N16R8 module does reserve GPIO33/34 in addition to
35-37 for octal PSRAM — Espressif's own ESP32-S3 GPIO docs confirm any
R8-or-higher module uses all five pins (SPIIO4-7 + SPIDQS) for octal
PSRAM, not just 35-37 (see the corrected note under "Hardware" above).
Doesn't change the table above (neither pin is used by it), but rules
out ever assigning 33/34 to anything later.

**Resolved:** button/light MCP23017 I/O will be plain I2C polling each
loop tick, same as the rest of `ButtonInput` — no INTA/INTB interrupt
wiring. Buttons are already software-debounced, so poll-rate latency
is a non-issue here (unlike the encoder's quadrature signal, which
does need native PCNT to avoid missed steps).

**Resolved:** REV2 firmware support is a parallel selectable code path,
not a replacement — see "REV2 firmware (MCP23017 IO backend)" under
"Firmware architecture" below. Chosen because REV2 hardware is
unproven and REV1 is still the only working reference board (it also
still has an open, unresolved encoder-pushbutton bug -- see "Firmware
status" below -- that needs the REV1 board to keep working to chase
down), so replacing REV1's code in place was too risky for what's
otherwise a from-scratch, unflashed revision.

## Firmware status
Two separate things now exist in this repo:

- **`src/main.cpp`** — still the original **hardware bring-up / demo
  sketch** (TFT splash, color test, button-light chase, countdown
  output demo). Deliberately left untouched throughout the module
  build-out below, per earlier direction. Does not read any inputs.
- **The real firmware** — see "Firmware architecture" below. Builds
  and links clean (`pio run -e app`), and as of the fix described
  below, boots clean on real hardware (two Hosyond N16R8 units
  confirmed) all the way to `BootMenu`.

  **Postmortem of a boot crash found on first real-hardware flash**
  (kept in detail because the eventual root cause was two full layers
  removed from where the crash actually manifested, and the same
  false trail is easy to fall into again): every fresh-NVS boot
  reliably reset the chip (`rst:0xc RTC_SW_CPU_RST`, no panic text
  visible over serial) at the exact moment `TftDisplayManager::begin()`
  called `SPI.begin()` on `HardwarePins::TFT_SCLK`/`TFT_MOSI`
  (GPIO43/44). Since those pins double as UART0 TX/RX, that was the
  obvious first suspect -- but it was wrong, disproven by disabling
  `Serial` before the SPI call and by testing standalone with no PC
  attached at all, same crash either way. Also ruled out along the
  way: WiFi generally, PSRAM (initializes cleanly every boot, and a
  full 7-pattern read/write test across all of internal SRAM + PSRAM
  passed with zero errors -- `examples/ram_test.cpp`,
  `pio run -e ram-test -t upload`, kept as a standing diagnostic), a
  flaky/counterfeit flash chip (its 16MB capacity is genuine per the
  chip's own JEDEC ID via `esptool flash_id`), QIO vs DIO flash mode,
  an older platform/core version, and every individual class `App`
  constructs (tested standalone, all safe). Bisection eventually
  pinned the *trigger* (not yet the cause) to `DirectorControl::begin()`
  specifically -- calling it (which starts the HTTP server) flipped
  the crash on even though, in the reordered test that isolated this,
  it hadn't executed yet at the moment `SPI.begin()` actually crashed.
  That non-causal-looking correlation was the real clue, but at the
  time it looked like "linking WebServer's code footprint alone
  somehow destabilizes an unrelated SPI call" -- so `DirectorControl`
  and `WifiPortal` were rewritten from Arduino's `WebServer` to
  ESP-IDF's own `esp_http_server` to test that theory. That rewrite
  is still in the tree (see both classes' header comments) and is a
  legitimate simplification (no Arduino `WebServer` dependency left
  anywhere), but on its own it did **not** fix the crash -- same
  signature, same timing, just a new binary. Neither did pinning the
  HTTP server's FreeRTOS task to a specific CPU core.

  **The actual root cause**, found by reading `esp-coredump`'s output
  against the coredump partition (`default_16MB.csv` includes one at
  `0xFF0000`; `pip install esp-coredump`, then
  `esptool.py read_flash 0xFF0000 0x10000 out.bin` +
  `esp-coredump info_corefile --core out.bin --core-format raw --gdb
  <xtensa-esp32s3-elf-gdb path> <matching firmware.elf>` -- reading
  raw from a file instead of live over serial sidesteps
  `esp-coredump`'s otherwise-mandatory full ESP-IDF/`IDF_PATH`
  requirement):
  ```
  assert failed: tcpip_send_msg_wait_sem
  lwip/src/api/tcpip.c:455 (Invalid mbox)
  ```
  with a backtrace through `httpd_start() -> httpd_server_init() ->
  socket() -> lwip_socket() -> netconn_new_with_proto_and_callback()
  -> tcpip_send_msg_wait_sem()`. In plain terms: `httpd_start()`
  needs lwIP's TCP/IP background task already running to open a
  listening socket, and on a completely fresh device (no saved WiFi
  SSID yet -- true for every board tested, hence 100% reproducible)
  `NetworkManager::begin()` saw no credentials and returned
  immediately *without ever calling `WiFi.mode()`*, so that stack
  never initialized. `DirectorControl::begin()` then called
  `server_.begin()`/`httpd_start()` against a TCP/IP stack that
  didn't exist, lwIP's own sanity check caught it and called
  `abort()`. This is also why swapping HTTP server libraries didn't
  help (both `WebServer` and `esp_http_server` end up at the same
  `socket()` call), and why the crash appeared to happen inside
  `SPI.begin()`: the "Saved PC" the ROM bootloader prints on reset is
  *not* the fault location, just whatever the other CPU core happened
  to be doing when the system-wide abort fired -- a red herring that
  cost significant time before the coredump made the real backtrace
  visible.

  **The fix** (`src/network/NetworkManager.cpp`): `WiFi.mode(...)`
  now always runs in `begin()`, even with no saved SSID -- it just
  skips the follow-up `WiFi.begin()` connection attempt when there's
  nothing to connect to yet. `WiFi.mode()` alone is what brings up
  the underlying netif/lwIP stack, so it now always happens before
  `DirectorControl`/`WifiPortal` start their HTTP server, regardless
  of whether the device has ever been configured with WiFi
  credentials. Confirmed fixed on real hardware: boots clean to
  `BootMenu` on first try, no crash-reset loop.

  Two standalone diagnostic sketches from this investigation are kept
  in the tree as they're generically useful for isolating future
  hardware-vs-firmware questions the same way: `examples/
  tft_httpd_test.cpp` (`pio run -e tft-httpd-test`, TFT + bare
  `httpd_start()`, nothing else) and `examples/wifi_menu_test.cpp`
  (`pio run -e wifi-menu-test`, TFT + the real `WifiPortal` captive
  portal, no other PinballTimer classes) -- both now boot clean too.
- `boards/esp32-s3-n16r8.json` -- project-local custom board
  definition (Espressif's own `esp32-s3-devkitc-1` board defaults to
  the N8 variant: 8MB flash, no PSRAM, wrong for our actual N16R8
  hardware). Same physical pinout/variant, just corrected
  `maximum_size`/flash/PSRAM metadata baked in directly instead of the
  previous `board_build.*`/`build_flags` patches in `platformio.ini`.

**WiFi heap corruption (open, disabled for now)** -- first real flash
of `app-rev2` (REV2 board, full firmware including this session's
ball-count/dashboard work) bootlooped intermittently, alternating
`BROWNOUT_RST` and `TG1WDT_SYS_RST` reset reasons. Bisection (building
back from a stripped-down App::begin() that skipped the whole WiFi/
HTTP block, then re-adding one call at a time) isolated the trigger to
`WiFi.mode(WIFI_AP_STA)` itself -- a line unchanged by this session's
work -- but the actual cause turned out not to be a power/brownout
issue at all. Pulling the coredump partition (`0xFF0000`, size
`0x10000` per `default_16MB.csv`; `esptool.py read_flash 0xFF0000
0x10000 out.bin`, then `python -m esp_coredump --chip esp32s3
info_corefile --core out.bin --core-format raw --gdb
<xtensa-esp32s3-elf-gdb path> <matching firmware.elf>`, same technique
as the postmortem above) showed a real heap-corruption assert:
```
assert failed: block_trim_free heap_tlsf.c:371 (block_is_free(block) && "block must be free")
Crashed task: 'wifi'
Backtrace: ieee80211_recv_action -> sta_recv_mgmt -> sta_input ->
sta_rx_cb -> wifi_zalloc_wrapper -> tlsf_malloc -> block_locate_free -> panic
```
Detected while the WiFi driver's own task allocates memory for an
incoming 802.11 management frame -- several stack frames are
unresolved (`?? ()`), meaning the fault is inside Espressif's
precompiled WiFi/PHY library itself, not this project's source.
Ruled out as the cause, each with direct evidence: this session's own
new code (`heap_caps_check_integrity_all()` inserted after every
single `begin()` call plus periodically at runtime stayed clean
straight through boot and a live WiFi connection); `StatusReporter`'s
512->1024 stack buffer bump (its `appendf()`/`vsnprintf` pattern is
properly bounds-checked); `RemoteCommand::stringKey`'s 16->24 resize
(every write site uses `sizeof()`); lowering WiFi TX power; disabling
the hardware brownout detector (masked the symptom -- traded a clean
reset for an unpredictable hang, worse for a device driving real
relays/lights); disabling `ArduinoOTA`/mDNS (a common culprit in
community reports of this exact assert, e.g. espressif/esp-idf#7746);
and the platform/core version (already the latest available,
`espressif32 @ 7.0.1`, confirmed via `pio pkg outdated`).

Root cause not yet found -- would need a custom ESP-IDF build with
heap-poisoning canaries to catch the actual overflow at the moment it
happens rather than whenever some unrelated allocation later stumbles
on it, which isn't a quick change under this project's plain
`framework = arduino` (precompiled libraries, no user-editable
sdkconfig). **Decision**: rather than block game-mode work on this,
the whole WiFi/HTTP subsystem is disabled behind one flag,
`kWifiFeatureEnabled` in `include/FeatureFlags.h` (set to `false` at
the time of this investigation).
`App::begin()`/`App::update()` skip `WiFi.mode()`/`network_`/
`directorControl_`/`ota_`/`wifiPortal_`/`directorDashboard_` entirely
when it's off; `BootMenu`'s "WiFi Setup" item and `DirectorMenu`'s two
WiFi rows stay visible (so the menu shape doesn't silently change) but
are labeled "(disabled)" and are inert -- selecting them does nothing
rather than handing off to a WiFi flow that was never brought up.
Flip the flag back to `true` once the real fix lands.

**Replacement WiFi lifecycle (July 2026):** WiFi is enabled again,
but the firmware no longer enters `WIFI_AP_STA`, the mode implicated
by the REV2 coredump. The radio now has one owner and one role at a
time: station mode for venue WiFi, or AP mode for the setup/director
hotspot. A web-portal submission saves credentials, gives the browser
a four-second acknowledgement window, then closes the AP before
station connection starts. The legacy persisted `Both` value migrates
to `StationOnly`. Reconnect attempts back off from 5 to 60 seconds,
SDK auto-reconnect and flash credential persistence are disabled, and
OTA/mDNS starts only after STA receives an address. HTTP is still
started only after an AP or STA netif exists, retaining the earlier
`Invalid mbox` fix. This deliberately trades simultaneous hotspot +
venue-network service for radio stability; either interface still
provides the same director API/dashboard when selected.

The normal product flow is now explicitly power-gated. Every physical
boot starts with `WiFi.mode(WIFI_OFF)` and does not initialize the
network stack/HTTP services until the operator selects `Turn WiFi ON`
in Boot Menu's WiFi submenu. Saved credentials survive, but radio
power and `Keep WiFi Alive` do not. On explicit enable, a device with
no credentials broadcasts `PinballTimerXXXX`, where `XXXX` is the
final four uppercase hexadecimal digits of its WiFi station MAC
address; one with credentials tries station mode. After 30 seconds
disconnected it switches to the fallback AP without taking TFT/input
focus away from a running game. `Turn WiFi OFF` stops DNS/AP/STA and
sets `WIFI_OFF`, disabling the RF modem rather than merely enabling
power save.
The AP and LAN use the same `esp_http_server` instance and routes
(`/`, `/status`, `/command`, `/game-setup`, `/game-live`, and the
WiFi setup endpoints). Submitting new credentials acknowledges the
browser request, closes the AP, and returns to exclusive station mode.
The shared interface also exposes a read-only machine database at
`/machines` and a streamed JSON connector at `/api/machines`. The API
returns each record's stable ID, name, type, ball count, configured
play-time fields, and resolved play time; it has no mutation routes.
See `CODEX.md` under "WiFi Stability, Automatic Provisioning, and
Database Read API" for the implementation log and verification record.
The subsequent connection-status, one-level menu navigation, forget-network,
web password visibility, keep-alive/modem-sleep, full-length SSID, and failed
credential rollback work is recorded in `CODEX.md` under "WiFi Setup
Usability and Connection Recovery".
The later boot-off/runtime-only keep-alive policy is recorded under
"WiFi Radio Power Policy".

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
  `GameModeManager`/`PowerManager`, not decided by App itself).
  `Error` state's only current trigger is `PowerManager` reporting
  `PowerState::Critical` (5% battery) — see "Battery monitoring"
  below.
- **Input:** `ButtonInput`, `EncoderInput` — debounced polling,
  pressed/released/short/long-press event queues.
- **Output:** `NumericDisplayManager` (the 4 TM1637s — see "Zero
  crossing" below for the negative-time behavior it implements),
  `TftDisplayManager` (generic `showStatusScreen()`, plus real
  ST7789 `sleep()`/`wake()` commands), `ButtonLightManager` (base
  state + priority temporary overrides, digital on/off only — no PWM
  brightness, the hardware doesn't support it), `BuzzerManager`
  (GPIO38, **passive** piezo -- switched from REV1's active module,
  which drove GND/no-oscillator via plain digitalWrite; a passive
  module has no oscillator of its own and must be driven at an actual
  frequency via the ESP32's LEDC peripheral, `ledcSetup`/
  `ledcAttachPin`/`ledcWriteTone`, channel 0 -- confirmed unused
  elsewhere in the firmware). Six named presets, each a short
  `Note{frequencyHz, durationMs}` sequence stepped by `update()`
  (non-blocking, same polling pattern as the rest of this layer, plus
  a generic `playTone(freq, ms)` escape hatch for anything not covered
  by a preset): `click()` (4000Hz/10ms, encoder rotation),
  `beep()` (1800Hz/60ms, any button press), `tone()` (1200Hz/80ms,
  encoder short-press -- menu confirm/select), `boop()` (450Hz/90ms,
  encoder long-press -- menu back/cancel), `buzz()` (10-note
  160/220Hz tremolo, 3000ms total, a player's timeout/elimination --
  see "Ball count / round elimination" below), `littleTune()` (a
  4-note ascending C5-E5-G5-C6 jingle with short gaps between notes,
  fired once when the whole game ends). `click()`/`beep()`/`tone()`/
  `boop()` are wired centrally in `App::handleButtonEvent()`/
  `handleEncoderEvent()`, unconditionally, before any menu/game
  routing, same placement as `power_.notifyActivity()` just above
  each -- so they fire regardless of what's currently routing the
  event. `buzz()`/`littleTune()` are called directly from
  `Mode1RoundRobin` at the moments they represent. Not yet wired to
  anything else (battery events) -- see "Battery monitoring" below,
  still an open decision.
- **REV2 firmware (MCP23017 IO backend):** `ButtonInput` and
  `ButtonLightManager`'s debounce/event-queue/blink-pattern logic
  (already hardware-agnostic) now go through two tiny IO classes
  instead of calling `pinMode`/`digitalRead`/`digitalWrite` directly:
  `io/ButtonSwitchIo` and `io/ButtonLightIo`. Each has exactly one
  concrete implementation compiled in per board revision, chosen by
  `platformio.ini`'s `build_src_filter` per environment (not a
  virtual-dispatch interface — this is a compile-time swap, no
  runtime cost): `*_NativeGpio.cpp` (REV1, `[env:app]`) is the
  original logic unchanged; `*_Mcp23017.cpp` (REV2, `[env:app-rev2]`)
  reads/writes bank A/B of a single shared `io/Mcp23017` register
  driver (`Wire`-based, no extra `lib_deps` — GPIO A = inputs +
  internal pull-ups for the 5 switches, GPIO B = outputs for the 5
  lights, address assumed `0x20`). `HardwarePins.h` selects REV1 vs.
  REV2 constants via the same `PINBALLTIMER_REV2` build flag. REV1's
  `[env:app]` is completely unmodified by any of this (verified: both
  `pio run -e app` and `pio run -e app-rev2` build clean, and all 21
  native unit tests still pass) — this is deliberately a parallel
  path, not an in-place rewrite, since REV2 hardware doesn't exist yet
  and REV1 is the only board that's actually been tested. `[env:app-rev2]`
  has only ever been compiled, never flashed (no REV2 board exists to
  flash it to).
- **Game:** `TimerManager` (generic count engine, independent of
  players/displays), `PlayerManager`, `ButtonAssignmentManager` /
  `DisplayAssignmentManager` (what each physical button/display
  currently represents), `GameMode` interface + `GameModeManager` +
  `ModeRegistry`. `GameModeManager::modeCount()`/`modeAt()` let
  `BootMenu` enumerate registered modes generically (not hardcoded to
  Mode 1) for its Select Game Mode submenu. `GameModeContext` (the
  struct every mode's methods take) now also carries a `GameStorage&`
  — see "Round-state checkpoint/resume" below — and `GameMode` gained
  a `restoreState()` hook (default: unsupported) alongside the
  existing `setupAssignments()`; a mode that implements it is
  responsible for calling `setupAssignments()` itself as part of
  restoring (the caller must not also call it, or `TimerManager`
  timer slots leak). `GameModeManager` tracks `gameStarted_`
  (`isGameStarted()`, read by `App::syncSystemState()` for
  `SystemState::GameRunning` and by `StatusReporter`'s `/status`
  JSON) the same way for both start paths: the remote `StartGame`
  command pairs `notifyRemoteStart()` with `notifyGameStart()`
  (`DirectorControl.cpp`), and a local Action-button press does the
  same via `GameMode::onButtonEvent()`'s return value — it returns
  `true` only on the event that actually transitions the mode from
  not-started to started (e.g. `Mode1RoundRobin::startRound()`), and
  `GameModeManager::handleButtonEvent()` then calls
  `notifyLocalStart()`/`notifyGameStart()` itself. This exists
  because `GameModeManager` has no other visibility into
  mode-internal round state (e.g. `Mode1RoundRobin`'s own
  `roundStarted_`) purely from routing a button event to the active
  mode.
- **Modes:** `Mode1RoundRobin` — the actual CLAUDE.md Mode 1 rules,
  now including the full ball-count/round-elimination ruleset (see
  "Ball count / round elimination" below) — the zero-crossing rule
  change is resolved, `stopAtZero=true` (not `allowBelowZero=true`).
- **Storage:** `SettingsStorage` (also now owns `WifiOperatingMode`,
  see "WiFi & OTA" below), `GameStorage` — ESP32 `Preferences` (NVS),
  no new library dependency. "Saved presets" (named bundles a user can
  save/reload) are still NOT implemented. Per-timer custom values
  beyond `secondsPerTurn` also aren't (Mode 1 gives every player an
  equal timer, no per-player override to store) — separate from the
  **round-state checkpoint** GameStorage now does implement:
  `SavedRoundState` (mode id, player count, whose turn, each player's
  remaining seconds) is written by `Mode1RoundRobin::checkpointRoundState()`
  on round start / each turn advance / pause (not continuously, to
  avoid writing NVS every tick — so a resume after an abrupt power
  loss mid-turn, with no pause first, restores with that turn's
  *starting* time, not its exact mid-turn remainder; a deliberate
  tradeoff, see the doc comment on `SavedRoundState` in
  `GameStorage.h`), cleared on reset, and consumed by
  `GameModeManager::restoreActiveMode()` /
  `Mode1RoundRobin::restoreState()` when `BootMenu`'s "Resume Game"
  item is picked. Player names also now round-trip through
  `PlayerManager` (RAM-only, reset every boot) — `App::begin()` loads
  `GameStorage`'s saved names into it every boot, and `BootMenu`'s
  Player Info submenu writes through to both on edit.
- **Network:** `NetworkManager` (WiFi connect/reconnect/standby),
  `DirectorControl` + `StatusReporter` + `RemoteCommand` — HTTP REST
  via ESP-IDF's own `esp_http_server` (not Arduino's `WebServer` --
  see "Firmware status" below for why that switch happened). `GET
  /status` returns hand-built JSON; `POST /command` takes form-encoded
  `type`/`intValue`/`stringKey`/`longValue` fields (command type names
  and full semantics documented in `include/network/DirectorControl.h`
  and `include/network/RemoteCommand.h`) -- now including
  `SetPlayerName`/`SetPlayerButton`/`SetMachineName` for the game-setup
  dashboard below. This concrete API shape was a judgment call — no
  existing spec or director client to match, so it's the de facto spec
  now unless changed. `DirectorDashboard` (own class, same shared
  `httpd_handle_t` as `DirectorControl`/`WifiPortal`, see
  `include/network/DirectorDashboard.h`) serves two real HTML pages
  reusing that same API: `GET /game-setup` (mode/player
  count/seconds-per-turn/ball count/machine name/per-player
  name+button assignment, fetch()-driven against `/status` +
  `/command`, no server-side templating) and `GET /game-live` (a
  placeholder live view -- proves the extended `/status` data contract
  works: names, colors, rounds remaining, timers, machine name,
  gameOver -- NOT the final visual design, which is pending a
  reference photo of the physical device to model it after). WiFi
  credentials for the one fixed venue network come from
  `include/Secrets.h` (gitignored, template at `Secrets.h.example`),
  written into `SettingsStorage` (NVS) once on first boot only — see
  "WiFi & OTA" below. `OtaManager` wraps `ArduinoOTA`
  (framework-bundled, no lib_deps entry) so firmware can be pushed over
  WiFi once a device is enclosed — password-gated via `OTA_PASSWORD` in
  the same `Secrets.h`.
- **Power:** `PowerManager` — idle-timeout standby that blanks the
  TFT/displays/lights but never puts the ESP32 itself into light/deep
  sleep, so remote (WiFi/HTTP) control stays reachable throughout.
  Battery monitoring: percentage/threshold/notification logic is
  implemented, but the physical sensing source is still stubbed — see
  "Battery monitoring" below.
- **UI:** on-device menus share two small building blocks, both in
  `include/ui/`: `ScrollList.h` (pure free functions — scroll-window
  math + rotate-with-wrap, no rendering) and `TextEntry` (one-encoder
  character picker: rotate cycles a fixed charset, click appends,
  continuing to rotate past the alphabet reaches DONE/DEL, long-press
  signals cancel via a `Result` enum the caller interprets — it has no
  Arduino dependency, unlike almost everything else in this layer).
  Every modal menu below returns a shared `MenuHandoff` enum
  (`include/ui/MenuHandoff.h`: None/Close/OpenWifiSetup/
  OpenWifiPortal/RevertToAdhoc) from its `handleEncoderEvent()` so
  `App` can uniformly react when an item hands off to a *different*
  class's flow instead of acting in place — `App` owns the
  `WifiSetupMenu`/`WifiPortal` instances, so only it can open them.
  - `BootMenu` — the screen shown on **every boot** (`App::begin()`
    no longer auto-resumes anything by itself). Top-level items,
    computed fresh each `open()`, in **fixed order**: **Select Game
    Mode** (always first — submenu over `GameModeManager::modeAt()`,
    mode-agnostic, not hardcoded to Mode 1), **Continue Game** (only
    shown when `GameStorage::loadRoundState().valid` — see
    "Round-state checkpoint/resume" below; internally still
    `TopItem::ResumeGame`, only the label changed), **Select Machine**
    (submenu over the common `MachineCatalog` — see "Common machine
    catalog" note below), **Player Info** (submenu: per-player name
    entry via `TextEntry`, written through to both `GameStorage`
    (persists) and `PlayerManager` (live) on save), **WiFi Setup**
    (submenu: join via encoder/web, adhoc-only, persistent-hotspot
    toggle — see "WiFi & OTA" below). There is deliberately **no
    top-level "Start Game" or "Mode Config"** — both only make sense
    once a mode's been picked, so both live in `ModeMenu` instead (see
    below). Long-press backs out one submenu level; at the top level
    it's a no-op, since there's always something to pick at boot. List
    screens show a trailing "hold knob to go back" hint line whenever
    there's room left under `TftDisplayManager::MAX_LINES` (5).

    Picking an entry in **Select Game Mode** only calls
    `GameModeManager::selectMode()` (nothing started) and drops into
    `State::ModeMenu`, whose items are recomputed fresh each entry
    (`refreshModeMenuItems()`, same "don't hardcode" convention as the
    top menu): **Start &lt;ModeName&gt;** (runs the same
    select+`setPlayerCount()`+`initializeActiveMode()`+
    `setLastSelectedMode()` sequence `startGameWithMode()` already
    did — guarded to only apply `defaultPlayerCount()` if
    `GameModeManager::playerCount()` is still 0, so it doesn't clobber
    a count the director explicitly set below), **Number of Players**
    (numeric editor, writes through `setPlayerCount()` — only shown
    when the active mode's `maxPlayers() > 1`, a provisional stand-in
    for "every mode except single-player-only ones" since there's only
    ever been one multi-player mode to test the real rule against —
    revisit once a genuine single-player mode exists), **Mode Config**
    (the turn-timer/ball-count editor — `GameModeManager::activeMode()`
    is guaranteed non-null by the time you can reach it, since
    `ModeMenu` itself requires a mode to already be selected),
    **Return to Main Menu** (jumps straight back to `TopMenu`, skipping
    `ModeSelect` — the explicit two-levels-at-once shortcut; long-press
    instead backs out just one level, to `ModeSelect`, so a different
    mode can be picked). `ModeConfigList`'s own long-press target is
    `ModeMenu`, not `TopMenu` — it's only reachable through there now.
    A known minor gap: picking "Select Game Mode" then "Start" while an
    old Continue-Game checkpoint still exists doesn't clear that
    checkpoint until the new round's own first checkpoint (on
    `startRound()`) overwrites it — a reboot in that narrow window
    would offer to resume the *old* abandoned round instead. Not
    fixed; flagged here instead.

    **Common machine catalog:** `MachineCatalog`/`MachineDatabase`
    (`game/MachineCatalog.h`, `storage/MachineDatabase.h`) are shared
    infrastructure — not owned by any one mode — originally built in
    an isolated Codex phase (see `CODEX.md`) alongside the unregistered
    `Mode2Gauntlet` and the `RoundRobinTurnEngine` extraction. `App`
    now owns one `MachineDatabase` instance (`.begin()`'d alongside
    `settings_`/`gameStorage_`) and passes its `const MachineCatalog&`
    into `BootMenu::begin()`. Picking a record in **Select Machine**
    currently only calls `GameStorage::setMachineName()` (the same
    field `DirectorControl::SetMachineName` already writes) so it
    shows up in the TFT/status JSON — it deliberately does NOT yet push
    the record's ball count/play time into any mode's config, since
    that deeper catalog↔mode integration is explicitly still
    undecided (see `CODEX.md`'s "Common Machine Catalog" section).
  - `DirectorMenu` — on-device admin menu, opened by `App` when the
    Action button is held 5s during `SystemState::GameRunning` or the
    idle pre-round `Setup` state (a distinct, App-level hold-duration
    check against `ButtonInput::heldDurationMs()` — not the same thing
    as `ButtonInput`'s own generic 600ms `LongPress` event, unrelated
    to this), but **not** while `BootMenu` is open (guarded explicitly
    — both would otherwise be able to open at once during the
    pre-round `Setup` state). Opening it pauses the active mode
    (`GameModeManager::notifyPause()` — a no-op if no round had
    started yet) and hands the rotary encoder exclusive input focus.
    Restructured from a flat label+function-pointer table into a
    state machine mirroring `BootMenu`'s own multi-screen design
    (`State` enum + per-state `render<X>()`/`handle<X>Encoder()`
    pairs), since two of its items need real sub-navigation. Top-level
    items: Resume Game, Reset Round (restarts the SAME game fresh,
    stays in gameplay), **Ball Count (All)** (numeric editor,
    redistributes the delta to every already-assigned player's
    `roundsRemaining` via `GameMode::setLiveModeOption(context,
    "ballCount", ...)` — distinct from `setModeOption("ballCount",
    ...)`, which only affects the *next* game's `setupAssignments()`),
    **Player Ball Count** / **Player Time** (each opens a shared
    player-picker submenu, then a numeric editor for that one player's
    `roundsRemaining`/timer value via `GameMode::setPlayerOption(context,
    player, key, value)` — either edit may revive an
    Eliminated/Finished player back into rotation, see
    `Mode1RoundRobin::tryRevive()`: revival requires BOTH
    `roundsRemaining` and timer value to be positive, checked after
    either edit, and re-derives `gameOver_`/whose-turn-it-is via
    `TurnRotation::nextButton()` if the whole game had already ended),
    **Toggle Local Lock** (fixed real deadlock: `App::updateDirectorMenuHold()`
    used to also refuse to reopen `DirectorMenu` while
    `DirectorControl::localControlsLocked()` was true, same gate as the
    in-game button/encoder routing -- but locking it from *inside*
    `DirectorMenu` immediately closes the menu, and with WiFi disabled
    (see "WiFi & OTA" below) there was no remote `UnlockLocalControls`
    path either, so this trapped the director on the paused game screen
    with no way back short of a power cycle. The reopen-hold gesture is
    now deliberately NOT gated by the lock -- it protects the game from
    local *player* input, not the director from their own menu), **End Game** (calls the same `notifyReset()` as
    Reset Round, but returns `MenuHandoff::EndGame` instead of `Close`
    so `App` sends it back to `BootMenu` instead of resuming gameplay
    — the two are deliberately different actions), Setup WiFi
    (Encoder)/(Web) (inert "- disabled" placeholders while WiFi is
    off, same convention as `BootMenu`'s WiFi item — see
    `FeatureFlags.h`). `GameMode::setPlayerOption()`/`playerOption()`/
    `setLiveModeOption()` all take a `GameModeContext&` (unlike
    `setModeOption()`/`modeOption()`, which don't) since they touch
    live per-player state (`PlayerManager`/`TimerManager`/
    `ButtonLightManager`/`TurnRotation`) that a mode can't reach from
    its own members alone; `GameModeManager` bridges them the same way
    it already bridges `notifyPause()` etc. (it stores
    `GameModeContext&` internally from its own `begin()`). Numeric
    editors follow `BootMenu`'s Mode Config confirm/cancel shape
    exactly: seed a local working value on entry, rotation only
    adjusts that copy, short-press writes through and steps back one
    level (a confirmed player edit returns to the picker, not the top
    menu, so a director can fix up several players in one visit),
    long-press discards and steps back one level. Renders via
    `TftDisplayManager::showStatusScreen()`.
  - `WifiSetupMenu` — no keyboard exists on this hardware, so this is
    built entirely around the one rotary encoder (via `TextEntry`):
    scans for nearby networks (`WiFi.scanNetworks`) and lets you pick
    one instead of typing an SSID (`[Manual Entry]` row covers hidden
    networks), then — if the network isn't open — collects a password.
    Saves into `SettingsStorage`/NVS and re-runs
    `NetworkManager::begin()` with the new credentials.
  - `WifiPortal` — same job, but from a phone: the ESP32 broadcasts
    its own WPA2 access point (`<deviceName> Setup`) with a captive
    portal (DNSServer redirects all DNS queries to itself, and
    `onNotFound` redirects any unmatched HTTP path, covering the
    probe URLs iOS/Android/Windows use to auto-popup a browser). The
    AP's address is a fixed `WiFi.softAPConfig()` default,
    **10.10.10.1** (`/24`), not the ESP32 SDK's stock 192.168.4.1 —
    same on every device. The setup page is served from
    `DirectorControl`'s existing WebServer instance
    (`DirectorControl::webServer()`) rather than a second server —
    that stays reachable at the AP's gateway IP with no STA
    connection, since `WebServer` binds all interfaces regardless of
    WiFi mode. A candidate network is tested via a raw `WiFi.begin()`
    in `WIFI_AP_STA` mode (so the portal AP survives a bad password)
    rather than immediately through `NetworkManager`, which forces
    STA-only and would otherwise drop the very AP the phone is using.
    Also owns the **persistent-hotspot / adhoc** mechanism —
    `setPersistentHotspot()`/`revertToAdhoc()`/`applyStartupMode()` —
    covered under "WiFi & OTA" below along with how `NetworkManager`
    and `WifiPortal` coordinate to let AP+STA run concurrently without
    one clobbering the other's `WiFi.mode()`.
  Both `WifiSetupMenu` and `WifiPortal` can close themselves
  autonomously (connect success/failure/timeout), not just from an
  input event, so `App::update()` polls `isOpen()` on both every tick
  rather than reacting to input events alone to know when to resume —
  and, since either can now be reached from `BootMenu` as well as
  `DirectorMenu`, `App` tracks `wifiHandoffFromBootMenu_` so it knows
  whether "done" means resume an in-progress game or return to
  `BootMenu`.

**Ball count / round elimination (resolved -- was "Zero-crossing,
still open"):** full ruleset now implemented in `Mode1RoundRobin`.
Ball count ("rounds," configurable 1-5, default 3) is shared across
all players, not per-player-independent. Turn order is fixed by
physical button color, Red→Yellow→Green→Blue→Red (`ButtonColors.h`,
confirmed wiring: Player1 slot=Red, Player2=Yellow, Player3=Green,
Player4=Blue), independent of which `PlayerId` is assigned to which
button (`PlayerManager::buttonAssignment`, reassignable via the new
`SetPlayerButton` remote command) -- a player's color for a game
follows whichever button they're on, not their `PlayerId`.
`TurnRotation::firstButton()`/`nextButton()` (`game/TurnRotation.h`,
fully native-unit-tested, zero Arduino dependency) walk that fixed
order skipping Eliminated/Finished players.

A turn ends one of two ways, with different display outcomes:
- **Timeout** (timer reaches zero before the player presses their
  button): ends that player's WHOLE game immediately, regardless of
  rounds left -- not just that one round. Display freezes at 0 and
  *flashes* (`NumericDisplayManager::setFlashing`,
  `GAME_OVER_FLASH_INTERVAL_MS` = 300ms), button goes fully dark,
  buzzer sounds a 3s tremolo alert (`BuzzerManager::buzz()`),
  `PlayerStatus::Eliminated`.
- **Button press** (before timeout): decrements that player's rounds
  remaining. If rounds remain, the timer PAUSES (chess-clock style --
  `secondsPerTurn` is one time budget for the whole game, not a
  per-ball allowance; a fixed bug once had this incorrectly `reset()`
  back to the full value here instead) at whatever value it's at,
  `PlayerStatus::Waiting`, and resumes counting down from that same
  value once the rotation comes back around and they press their
  button to start their next turn. If that was their last round,
  display freezes and *flashes* (`GAME_OVER_FLASH_INTERVAL_MS`, same
  interval and same visual treatment as Eliminated -- their game is
  over even if others are still playing, so it must not read as
  "just waiting for their next turn") at whatever real time was
  left, `PlayerStatus::Finished`.

After any turn-ending event, the next available player's button
*flashes* (`LightPattern::Blink`) rather than lighting solid, and
their timer does NOT start until they press their own button once
(`Mode1RoundRobin::onButtonEvent()`'s two-press split: first press
starts the clock, second press ends the turn) -- except the very
first hand-off (Action press -> first turn), which still starts
immediately as before. The whole game ends
(`Mode1RoundRobin::isRoundOver()`) once nobody Waiting remains, and
the TFT shows "GAME OVER / Press White Button to Start a New Game /
or Hold for Main Menu" (`Mode1RoundRobin::renderGameStatus()`), with
a little ascending jingle (`BuzzerManager::littleTune()`) fired once
right as `gameOver_` flips true. Both
gestures reuse the Action button's existing tap-vs-5s-hold split:
tapping it now (`Released`, `gameOver_` true) calls
`resetRoundState()` then `startRound()` in one gesture -- a fresh
game with the same players/settings -- while holding it 5s still
opens `DirectorMenu` as always (`App::updateDirectorMenuHold()`'s
independent poll), whose eventual `Released` never reaches the mode
(swallowed by the menu-open guard in `App::handleButtonEvent()`), so
there's no race between the two.

Timers themselves switched from `allowBelowZero=true` (the old
placeholder) to `allowBelowZero=false, stopAtZero=true` --
`TimerManager::update()` already clamped-and-stopped correctly for
this, no `TimerManager` changes were needed.

**Battery monitoring (sensing hardware still stubbed):** `PowerManager`
implements the full percentage/threshold/notification pipeline —
voltage-to-percent conversion, edge-triggered notifications at 20% and
10% (each re-arms once the level recovers a few percent, so a later
discharge notifies again), and a new `PowerState::Critical` at 5% that
blanks every output the same way idle Standby does. It does **not**
literally power off or deep-sleep the ESP32 at 5%: the existing design
principle that the board never sleeps so remote/WiFi control stays
reachable was kept, so "shut down" surfaces as `SystemState::Error`
(the first concrete trigger for that state) instead — game/outputs
stop, WiFi/HTTP stays up. Unlike Standby, Critical does not clear on
local button activity, only on the battery voltage itself recovering.

What's still open: no battery-sensing GPIO or IC has been chosen.
Every ADC1 pin (GPIO1-10, the only ADC range safe to use alongside
WiFi) on the current board is already assigned to another peripheral —
only GPIO20 (shares the native USB D- pair, same tradeoff already
accepted for the Action button on GPIO19) and GPIO48 (likely this
board's onboard RGB LED) are free at all. An I2C fuel-gauge IC
(MAX17048/LC709203F) on those two free pins was suggested as probably
the better fit — sidesteps the ADC pin shortage and is far more
accurate than a raw-voltage linear guess, since the Li-ion discharge
curve is too flat near empty for that to be reliable — but this has
not been decided or wired. Until it is, `PowerManager` reports
`batteryAvailable:false` unless fed via `setStubBatteryVoltage()`
(volts) or the debug-only `SetStubBatteryVoltage` remote command
(millivolts, always allowed regardless of active mode) — this is also
how to exercise the 20%/10%/Critical behavior without hardware. The
0%/100% voltage mapping (3.0V/4.2V) is a placeholder pending
calibration once real sensing exists.

Battery fields are exposed over `GET /status`:
`batteryAvailable`, `batteryVoltage`, `batteryPercent`, `powerState`
(`PowerState` cast to int: 0=Active, 1=Standby, 2=Critical). Threshold
crossings are currently only `Serial.println`'d from `App::update()` —
no buzzer is wired yet, and no low-battery screen is wired to the TFT
(the TFT's first real content is `BootMenu`, see "UI" above), so a
director dashboard is the intended consumer of the JSON fields for now.

**WiFi & OTA:** how a device uses WiFi is `SettingsStorage`'s
`WifiOperatingMode` (NVS-backed, three values):
- `StationOnly` (default) — joins the saved venue network only.
- `AccessPointOnly` ("adhoc") — broadcasts `WifiPortal`'s own hotspot
  only, never attempts the venue network at all. Reached via
  `WifiPortal::revertToAdhoc()` (BootMenu/DirectorMenu's "Use Hotspot
  Only" item) — for when a director would rather connect straight to
  the device's own hotspot and skip the venue network entirely.
- `Both` — joins the venue network **and** keeps the hotspot
  broadcasting alongside it, so DirectorControl's `/status`/`/command`
  stay reachable either way. Toggled via
  `WifiPortal::setPersistentHotspot(true)` (BootMenu's "Keep Hotspot:
  ON/OFF" item), applied immediately, not just next boot.

Making AP+STA coexist took real care: `NetworkManager::begin()` and
`WifiPortal::startAp()` are both mode-aware (check `WiFi.getMode()`
before calling `WiFi.mode()`) so bringing one up doesn't silently tear
the other down, and `NetworkManager`'s `disconnect()`/`enterStandby()`/
`reconnect()` all route through a shared helper that only fully powers
off the WiFi driver (`WiFi.disconnect(true)`) when no AP needs to
survive. `App::begin()` calls `WifiPortal::applyStartupMode()` once,
after `NetworkManager::begin()`, to bring the hotspot up in the
background (no UI/input focus taken) if the saved mode calls for it;
`App::update()` also skips `NetworkManager::update()` entirely in
`AccessPointOnly` mode (not just while an interactive WiFi flow is
open) so its reconnect-retry loop can't fight an explicit "adhoc only"
choice.

Getting a device onto a network in the first place:
`include/Secrets.h` (gitignored — copy `Secrets.h.example`, fill in
real values) supplies `OTA_PASSWORD` and `WIFI_PORTAL_PASSWORD`
always, but `WIFI_SSID`/`WIFI_PASSWORD` can be left empty (the
supported default) — `App::begin()` only writes those into
`SettingsStorage`/NVS if no SSID is saved yet, so editing `Secrets.h`
after a device's first boot does nothing, NVS is the source of truth
from then on. To (re)configure or switch networks, use `BootMenu`'s or
`DirectorMenu`'s WiFi Setup item — either `WifiSetupMenu` (rotary
encoder) or `WifiPortal` (phone browser). Once connected, `App::update()`
logs the assigned IP over Serial (`[WiFi] Connected, IP: ...`) on the
disconnected→connected edge; `OtaManager` also registers an mDNS
hostname for `ArduinoOTA`, `<deviceName>.local`,
which can sometimes be used directly in place of an IP but hasn't been
verified reliable on every network/client.

OTA (so firmware can be pushed without opening the enclosure): `pio
run -e app-ota -t upload` after editing `upload_port` in
`platformio.ini` to the device's IP/hostname and `upload_flags
--auth=` to match `OTA_PASSWORD`. The device must already be running
OTA-capable firmware (i.e. built from `[env:app]` or `[env:app-ota]`)
for this to work at all — the very first flash onto a fresh board
still has to be over USB via `[env:app]`. `DirectorControl`'s
`/command` API remains unauthenticated (can pause/reset/reconfigure a
game — a much smaller blast radius than OTA, which can replace the
running code outright), so it was left as-is rather than gated the
same way; revisit if that's not an acceptable gap.

Not built yet: a **multi**-device director dashboard (one place to
see/control every unit by name rather than hitting each device's IP
individually) — deferred until there are 2+ physical units actually
running to design it against. A **single**-device dashboard now
exists (`DirectorDashboard`, see "Network" above,
`/game-setup`+`/game-live`) -- this note is about the multi-unit case
specifically, not dashboards in general.

## Build
PlatformIO, all environments in `platformio.ini`:

| Environment | What it builds | Touches main.cpp? |
|---|---|---|
| `esp32-s3-devkitc-1` (default) | `src/main.cpp`, the hardware bring-up demo | yes, this *is* main.cpp |
| `display-test` | `examples/display_flash_test.cpp` — per-display digit + TFT color test. REV2 pins (own hardcoded constants, not `HardwarePins.h`) | no |
| `full-io-test` | `examples/full_io_test.cpp` — every wired input/output including the buzzer. REV2 pins; buttons/lights via MCP23017 (own inline I2C code, not the `io/` classes) | no |
| `button-input-test` | `examples/button_input_test.cpp` — press a button, see its name + MCP23017 GPA pin on the TFT, its own LED lights, and (P1-4 only) its assigned player display flashes its number | no |
| `wifi-menu-test` / `tft-httpd-test` / `ram-test` | standalone hardware diagnostics, see their comments in `platformio.ini` | no |
| `app` | The real firmware (`App` + all managers) via `examples/app_main.cpp`, REV1 pins, uploaded over USB | no |
| `app-ota` | Same firmware as `app`, uploaded over WiFi via `ArduinoOTA` instead — see "WiFi & OTA" above | no |
| `app-rev2` | Same firmware as `app`, built for REV2 pins + the MCP23017 IO backend (see "REV2 firmware" under "Firmware architecture") — compile-only, no REV2 board exists to upload to yet | no |

Run any of them with `pio run -e <name> -t upload` (`app-rev2`:
`pio run -e app-rev2`, no `-t upload` — see above). Board config
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
