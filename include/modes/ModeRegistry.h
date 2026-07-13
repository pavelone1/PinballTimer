#pragma once

#include "game/GameModeManager.h"
#include "modes/Mode1RoundRobin.h"

// Contains the list of available game modes. Adding a mode means:
// 1. Add the new mode's files under modes/.
// 2. Register it in registerAllModes().
// 3. No changes needed to GameModeManager or existing mode handlers.
namespace ModeRegistry {

// Owns the single Mode1RoundRobin instance registered below.
// GameModeManager stores a pointer to it but does not own it.
extern Mode1RoundRobin mode1RoundRobin;

void registerAllModes(GameModeManager& manager);

} // namespace ModeRegistry
