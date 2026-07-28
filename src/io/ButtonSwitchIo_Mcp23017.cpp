// REV2 backend: MCP23017 bank A over I2C. Only compiled into
// [env:app-rev2] -- see platformio.ini and CLAUDE.md "REV2 hardware".
// Bank A is NOT in ButtonId order -- confirmed physical wiring is
// GPA0=P2, GPA1=Action, GPA2=P1, GPA3=P4, GPA4=P3 (see
// docs/images/rev2-pinout.svg). kGpaBitForButton translates a
// ButtonId index to the GPA bit it's actually wired to.
#include "io/ButtonSwitchIo.h"

#include "io/Mcp23017.h"

namespace {
// Indexed by ButtonId (P1, P2, P3, P4, Action); value is the GPA bit
// number that button's switch is physically wired to.
constexpr uint8_t kGpaBitForButton[] = {2, 0, 4, 3, 1};
}

void ButtonSwitchIo::begin()
{
    Mcp23017::instance().begin();
}

bool ButtonSwitchIo::readPressed(uint8_t index)
{
    const uint8_t gpioA = Mcp23017::instance().readGpioA();
    return (gpioA & (1 << kGpaBitForButton[index])) == 0; // active-low, same polarity as REV1
}
