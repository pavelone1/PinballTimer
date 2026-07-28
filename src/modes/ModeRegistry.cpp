#include "modes/ModeRegistry.h"

namespace ModeRegistry {

Mode1RoundRobin mode1RoundRobin;
Mode2Gauntlet mode2Gauntlet;

void registerAllModes(GameModeManager& manager)
{
    manager.registerMode(&mode1RoundRobin);
    manager.registerMode(&mode2Gauntlet);
}

} // namespace ModeRegistry
