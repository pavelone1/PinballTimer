#pragma once

#include <cstdint>

// Reads the raw state of the five physical button switches (P1-P4,
// Action), true when pressed. Exactly one of
// ButtonSwitchIo_NativeGpio.cpp (REV1) or ButtonSwitchIo_Mcp23017.cpp
// (REV2) is compiled in, chosen per PlatformIO environment via
// platformio.ini's build_src_filter -- ButtonInput itself never knows
// which board revision it's running on.
class ButtonSwitchIo {
public:
    void begin();
    bool readPressed(uint8_t index);
};
