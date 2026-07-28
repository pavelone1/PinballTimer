// REV1 backend: native GPIO, digitalWrite. Only compiled into
// [env:app] -- see platformio.ini and CLAUDE.md "REV2 hardware".
#include "io/ButtonLightIo.h"

#include <Arduino.h>
#include "HardwarePins.h"
#include "SystemTypes.h"

void ButtonLightIo::begin()
{
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonId::Count); ++i) {
        pinMode(HardwarePins::BUTTON_LIGHTS[i], OUTPUT);
        digitalWrite(HardwarePins::BUTTON_LIGHTS[i], LOW);
    }
}

void ButtonLightIo::write(uint8_t index, bool on)
{
    digitalWrite(HardwarePins::BUTTON_LIGHTS[index], on ? HIGH : LOW);
}
