#include "modes/ModeRegistry.h"

namespace ModeRegistry {

Mode1RoundRobin mode1RoundRobin;

void registerAllModes(GameModeManager& manager)
{
    manager.registerMode(&mode1RoundRobin);
}

} // namespace ModeRegistry
