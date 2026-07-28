#pragma once

#include <cstdint>

// All ESP32-S3 pin assignments in one place. Two board revisions
// coexist here, selected by the PINBALLTIMER_REV2 build flag (set by
// platformio.ini's [env:app-rev2] -- see CLAUDE.md "REV2 hardware").
// REV1 is the soldered prototype actually on the bench; [env:app]
// builds against it, unchanged. REV2 is the confirmed pin table for
// the not-yet-built revision that moves the TFT off GPIO43/44 (UART0)
// and the buttons/lights onto an MCP23017 I2C expander (see
// io/ButtonSwitchIo_Mcp23017.cpp / io/ButtonLightIo_Mcp23017.cpp).
// Nothing else in the firmware should contain a raw GPIO number.

namespace HardwarePins {

// Four TM1637 numeric displays
constexpr uint8_t DISPLAY_3_CLK = 13;
constexpr uint8_t DISPLAY_3_DIO = 14;

#ifdef PINBALLTIMER_REV2

// Display 2 shifted down one position from its REV1 slot (J1
// positions 15-16) to positions 16-17 -- GPIO11 (position 17) was
// free (vacated by the encoder's move to J3, see ENCODER_* below), so
// this didn't collide with anything. GPIO9 (position 15) is now free
// instead. Still CLK-upper/DIO-lower, same as REV1 and Display 3 --
// only the two left-header displays' handedness is unchanged; the
// right-header ones (Display 1/4) are mirrored, see below.
constexpr uint8_t DISPLAY_2_CLK = 10; // J1 position 16 (upper)
constexpr uint8_t DISPLAY_2_DIO = 11; // J1 position 17 (lower)

// Display 1 and Display 4 both live on the J3 (right) header. Right-
// side convention is deliberately the MIRROR of the left (J1) header,
// where CLK is always the physically upper pin of the pair and DIO
// the lower one (see Display 2/3 below, and REV1's branch) -- on the
// right, DIO is upper and CLK is lower instead.
constexpr uint8_t DISPLAY_1_DIO = 40; // J3 position 8 (upper)
constexpr uint8_t DISPLAY_1_CLK = 39; // J3 position 9 (lower)

// Display 4 moved off J1 onto the free J3 positions 17-18 -- these
// are literally its own original REV1 pins (GPIO47/21), freed back up
// once REV1's Display 4 assignment there was replaced by the TFT/I2C
// shuffle earlier in this file's history. Same DIO-upper/CLK-lower
// convention as Display 1 above.
constexpr uint8_t DISPLAY_4_DIO = 47; // J3 position 17 (upper)
constexpr uint8_t DISPLAY_4_CLK = 21; // J3 position 18 (lower)

// ST7789 TFT -- moved off GPIO43/44 (UART0 TX/RX) so the serial
// console survives the TFT initializing. All 5 signals are chosen to
// land on 5 PHYSICALLY CONSECUTIVE pins on the J1 header (positions
// 4-8: GPIO4,5,6,7,15) -- not just consecutive GPIO numbers, which
// don't correspond to adjacent header pins on this board (position 8
// is GPIO15, not GPIO8; see docs/images/rev2-pinout.svg). Confirmed
// signal order across those 5 positions: SCLK, MOSI, RST, DC, CS.
// I2C (below) is a separate contiguous pair one position further
// down, not merged with this block -- see I2C_SDA/I2C_SCL.
constexpr uint8_t TFT_SCLK = 4;
constexpr uint8_t TFT_MOSI = 5;
constexpr uint8_t TFT_RST  = 6;
constexpr uint8_t TFT_DC   = 7;
constexpr uint8_t TFT_CS   = 15;

// I2C bus to the MCP23017 GPIO expander (buttons + button lights).
// Shifted down one position from the TFT block -- positions 10-11 on
// J1, leaving position 9 (GPIO16) free as a gap between the TFT block
// (positions 4-8) and I2C. Part address confirmed 0x20 -- A0/A1/A2
// wire directly to GND (static CMOS config pins, no resistor needed,
// don't leave floating). Needs its own SDA/SCL pull-ups (4.7-10k to
// 3.3V); GPIO17/18 have none of their own.
constexpr uint8_t I2C_SDA = 17;
constexpr uint8_t I2C_SCL = 18;
constexpr uint8_t MCP23017_I2C_ADDR = 0x20;

// GPIO19 deliberately left unused: shares a net with this board's
// native USB D- line (same tradeoff REV1 accepted for the Action
// button switch), and putting anything more active there would be a
// worse version of the same risk.

// KY-040 rotary encoder -- moved off J1 positions 17/18 + J3 position
// 7 (physically split across two different headers in REV1) onto
// GPIO1/2/42, 3 PHYSICALLY CONSECUTIVE pins at J3 positions 4-6
// (freed by the TFT's move off them, see TFT_RST/DC/CS above in the
// REV1 branch). Still native GPIO/PCNT, not on the I2C expander, to
// avoid missing quadrature steps.
constexpr uint8_t ENCODER_CLK = 1;
constexpr uint8_t ENCODER_DT  = 2;
constexpr uint8_t ENCODER_SW  = 42;

#else

constexpr uint8_t DISPLAY_2_CLK = 9;
constexpr uint8_t DISPLAY_2_DIO = 10;
constexpr uint8_t DISPLAY_1_CLK = 40;
constexpr uint8_t DISPLAY_1_DIO = 39;
constexpr uint8_t DISPLAY_4_CLK = 47;
constexpr uint8_t DISPLAY_4_DIO = 21;

// ST7789 TFT
constexpr uint8_t TFT_SCLK = 43;
constexpr uint8_t TFT_MOSI = 44;
constexpr uint8_t TFT_RST  = 1;
constexpr uint8_t TFT_DC   = 2;
constexpr uint8_t TFT_CS   = 42;

// Button switches (inputs), indexed by ButtonId: P1, P2, P3, P4, Action.
// GPIO19 (Action) shares the net with this board's native USB D- line
// -- accepted tradeoff, see CLAUDE.md. REV2 moves these to the
// MCP23017 -- this array only exists/is used under REV1.
constexpr uint8_t BUTTON_SWITCHES[] = {4, 6, 16, 18, 19};

// Button lights (outputs, via ULN2803), indexed by ButtonId: P1, P2, P3, P4, Action.
// REV2 moves these to the MCP23017 too.
constexpr uint8_t BUTTON_LIGHTS[] = {5, 7, 15, 17, 8};

// KY-040 rotary encoder. REV2 moves this to GPIO1/2/42 -- see the
// REV2 branch above -- to make all 3 encoder pins physically
// consecutive; this REV1 set stays split across J1 (CLK/DT) and J3
// (SW), matching the board that's actually soldered.
constexpr uint8_t ENCODER_CLK = 11;
constexpr uint8_t ENCODER_DT  = 12;
constexpr uint8_t ENCODER_SW  = 41;

#endif

// Buzzer: confirmed on GPIO38. Passive piezo module -- no oscillator
// of its own, driven via the ESP32's LEDC peripheral at an actual
// frequency (see BuzzerManager, and CLAUDE.md "Hardware"). REV1 used
// an active module here (plain digitalWrite HIGH/LOW); the swap to
// passive is a hardware-only change, same pin/wiring.
constexpr uint8_t BUZZER_PIN = 38;

// Battery voltage monitoring: sensing hardware not yet designed or
// wired -- no free ADC1 pin remains on this board (see CLAUDE.md's
// "Battery monitoring" section for the tradeoffs). PowerManager's
// percentage/threshold/notification logic is implemented against a
// pluggable BatteryVoltageReader (power/PowerManager.h) so it doesn't
// block on this; ask before assigning a pin/IC here.

} // namespace HardwarePins
