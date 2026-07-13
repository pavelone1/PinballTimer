#pragma once

#include <cstdint>

// All ESP32-S3 pin assignments in one place, matching the confirmed
// prototype board wiring (see CLAUDE.md "Current pin assignments").
// Nothing else in the firmware should contain a raw GPIO number.

namespace HardwarePins {

// Four TM1637 numeric displays
constexpr uint8_t DISPLAY_1_CLK = 40;
constexpr uint8_t DISPLAY_1_DIO = 39;
constexpr uint8_t DISPLAY_2_CLK = 9;
constexpr uint8_t DISPLAY_2_DIO = 10;
constexpr uint8_t DISPLAY_3_CLK = 13;
constexpr uint8_t DISPLAY_3_DIO = 14;
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
// -- accepted tradeoff, see CLAUDE.md.
constexpr uint8_t BUTTON_SWITCHES[] = {4, 6, 16, 18, 19};

// Button lights (outputs, via ULN2803), indexed by ButtonId: P1, P2, P3, P4, Action.
constexpr uint8_t BUTTON_LIGHTS[] = {5, 7, 15, 17, 8};

// KY-040 rotary encoder
constexpr uint8_t ENCODER_CLK = 11;
constexpr uint8_t ENCODER_DT  = 12;
constexpr uint8_t ENCODER_SW  = 41;

// Buzzer: NOT yet wired. GPIO38 is the confirmed spare pin reserved
// for it (see CLAUDE.md) but no buzzer circuit exists on the board
// yet. Do not use this pin until the hardware is actually connected.
constexpr uint8_t BUZZER_SPARE_PIN = 38;

// Battery voltage monitoring: not yet designed or wired. No ADC pin
// has been assigned for this -- ask before adding one.

} // namespace HardwarePins
