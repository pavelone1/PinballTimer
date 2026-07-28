// REV1 backend: native GPIO, INPUT_PULLUP. Only compiled into
// [env:app] -- see platformio.ini and CLAUDE.md "REV2 hardware".
#include "io/ButtonSwitchIo.h"

#include <Arduino.h>
#include "HardwarePins.h"
#include "SystemTypes.h"

void ButtonSwitchIo::begin()
{
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::Count); ++i) {
        pinMode(HardwarePins::BUTTON_SWITCHES[i], INPUT_PULLUP);
    }
}

bool ButtonSwitchIo::readPressed(uint8_t index)
{
    return digitalRead(HardwarePins::BUTTON_SWITCHES[index]) == LOW;
}
