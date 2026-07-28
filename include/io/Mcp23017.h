#pragma once

#include <cstdint>

// Minimal register-level driver for the MCP23017 16-bit I2C GPIO
// expander -- just enough for REV2's button switches (bank A, inputs
// with internal pull-ups) and button lights (bank B, outputs). One
// physical chip serves both, so ButtonSwitchIo_Mcp23017 and
// ButtonLightIo_Mcp23017 share this single instance rather than each
// managing their own Wire/I2C init.
class Mcp23017 {
public:
    static Mcp23017& instance();

    // Idempotent -- safe to call from both IO backends' begin().
    void begin();

    // Bank A, all 8 pins as inputs with internal pull-ups enabled --
    // same active-low polarity as REV1's native INPUT_PULLUP wiring.
    uint8_t readGpioA();

    // Bank B, all 8 pins as outputs, driving the ULN2803A the same
    // way REV1's native digitalWrite did.
    void writeGpioB(uint8_t value);

private:
    Mcp23017() = default;
    bool began_ = false;
};
