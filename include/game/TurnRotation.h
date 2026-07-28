#pragma once

#include "SystemTypes.h"
#include "game/ButtonAssignmentManager.h"
#include "game/PlayerManager.h"

// Picks the next button whose turn it is, in the fixed physical color
// order (see ButtonColors.h), skipping any button whose assigned
// player is Eliminated/Finished/not currently Waiting. Pure logic, no
// Arduino/millis() dependency -- safe for native unit tests.
namespace TurnRotation {

// The first button to go when a round starts -- the earliest
// Waiting-player button in color order (Red first, if eligible).
// Returns false if no button qualifies (e.g. zero players set up).
bool firstButton(
    const ButtonAssignmentManager& assignments,
    const PlayerManager& players,
    ButtonId& outButton
);

// The next button after `current`'s turn ends, walking the fixed
// color order and wrapping. Deliberately checks `current` itself last
// (after every other button), so a lone remaining Waiting player
// correctly gets handed the turn back. Returns false if nobody
// qualifies at all (including `current`) -- the whole game is over.
bool nextButton(
    const ButtonAssignmentManager& assignments,
    const PlayerManager& players,
    ButtonId current,
    ButtonId& outButton
);

} // namespace TurnRotation
