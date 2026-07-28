#pragma once

#include "SystemTypes.h"

// Physical button colors are fixed hardware, confirmed by the user:
// the button wired to the Player1 slot is Red, Player2 is Yellow,
// Player3 is Green, Player4 is Blue. This is independent of which
// PlayerId is currently assigned to which slot (see
// PlayerManager::buttonAssignment) -- a logical player's color for a
// given game is whichever button they're sitting on, not a property
// of their PlayerId. Game-rules constant, not wiring -- deliberately
// NOT in HardwarePins.h (that file is raw-GPIO-only, see its own
// header comment).
namespace ButtonColors {

// Fixed turn order: Red -> Yellow -> Green -> Blue -> (wraps to Red).
constexpr ButtonId kColorOrder[4] = {
    ButtonId::Player1, // RED
    ButtonId::Player2, // YELLOW
    ButtonId::Player3, // GREEN
    ButtonId::Player4  // BLUE
};

// Single-expression body (nested ternary, not switch) so this stays a
// valid C++11-style constexpr function under this project's toolchain.
constexpr ColorId colorForButton(ButtonId button)
{
    return button == ButtonId::Player1 ? ColorId::Red
         : button == ButtonId::Player2 ? ColorId::Yellow
         : button == ButtonId::Player3 ? ColorId::Green
         : button == ButtonId::Player4 ? ColorId::Blue
         : ColorId::White;
}

} // namespace ButtonColors
