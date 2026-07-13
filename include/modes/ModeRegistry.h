#pragma once

#include "game/GameModeManager.h"

// Contains the list of available game modes. Adding a mode means:
// 1. Add the new mode's files under modes/.
// 2. Register it in registerAllModes().
// 3. No changes needed to GameModeManager or existing mode handlers.
//
// Currently empty -- no concrete modes exist yet.
namespace ModeRegistry {

void registerAllModes(GameModeManager& manager);

} // namespace ModeRegistry
