#pragma once

#include <cstdint>

// Drives the five button lights (P1-P4, Action), one bit each.
// Exactly one of ButtonLightIo_NativeGpio.cpp (REV1) or
// ButtonLightIo_Mcp23017.cpp (REV2) is compiled in, chosen per
// PlatformIO environment -- see ButtonSwitchIo.h for the same pattern
// on the input side.
class ButtonLightIo {
public:
    void begin();
    void write(uint8_t index, bool on);
};
