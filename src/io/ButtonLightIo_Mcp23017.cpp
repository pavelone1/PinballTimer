// REV2 backend: MCP23017 bank B over I2C. Only compiled into
// [env:app-rev2] -- see platformio.ini and CLAUDE.md "REV2 hardware".
// Bank B is NOT in ButtonId order and does NOT mirror bank A --
// confirmed by combining the original ULN-channel<->GPB wiring with
// the confirmed ULN-channel<->button mapping (see
// docs/images/uln2803-pinout.svg): GPB0=P4, GPB1=P2, GPB2=Action,
// GPB3=P3, GPB4=P1. kGpbBitForButton translates a ButtonId index to
// the GPB bit it's actually wired to.
#include "io/ButtonLightIo.h"

#include "io/Mcp23017.h"

namespace {
// MCP23017 has no per-bit write -- track the whole bank so writing
// one light doesn't clobber the others' last-set state.
uint8_t gShadowGpioB = 0;

// Indexed by ButtonId (P1, P2, P3, P4, Action); value is the GPB bit
// number that button's light is physically wired to.
constexpr uint8_t kGpbBitForButton[] = {4, 1, 3, 0, 2};
}

void ButtonLightIo::begin()
{
    Mcp23017::instance().begin();
    gShadowGpioB = 0;
}

void ButtonLightIo::write(uint8_t index, bool on)
{
    const uint8_t bit = kGpbBitForButton[index];
    if (on) {
        gShadowGpioB |= static_cast<uint8_t>(1 << bit);
    } else {
        gShadowGpioB &= static_cast<uint8_t>(~(1 << bit));
    }
    Mcp23017::instance().writeGpioB(gShadowGpioB);
}
