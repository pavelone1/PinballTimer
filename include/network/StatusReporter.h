#pragma once

#include <cstddef>
#include "game/GameModeManager.h"
#include "game/PlayerManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/TimerManager.h"
#include "network/NetworkManager.h"
#include "storage/SettingsStorage.h"
#include "storage/GameStorage.h"
#include "power/PowerManager.h"

class DirectorControl; // forward-declared to avoid a circular include with DirectorControl.h

// Produces the timer's remote status as a small hand-built JSON
// document (no JSON library dependency -- the payload shape is small
// and fixed, not deeply dynamic). Battery fields are read live from
// PowerManager; batteryAvailable is false until a real sensing source
// (or a debug stub) is installed there -- see PowerManager.h.
//
// "Current four displayed players" / timer states are read generically
// via DisplayAssignmentManager (whatever each display is currently
// bound to), not by reaching into any specific mode's internals --
// this stays mode-agnostic.
class StatusReporter {
public:
    void begin(
        GameModeManager& modeManager,
        PlayerManager& players,
        DisplayAssignmentManager& displayAssignments,
        TimerManager& timers,
        NetworkManager& network,
        SettingsStorage& settings,
        DirectorControl& directorControl,
        PowerManager& power,
        GameStorage& gameStorage
    );

    // Writes a NUL-terminated JSON document into buffer. Returns the
    // number of bytes written (excluding the NUL terminator).
    size_t buildStatusJson(char* buffer, size_t bufferSize) const;

private:
    GameModeManager* modeManager_ = nullptr;
    PlayerManager* players_ = nullptr;
    DisplayAssignmentManager* displayAssignments_ = nullptr;
    TimerManager* timers_ = nullptr;
    NetworkManager* network_ = nullptr;
    SettingsStorage* settings_ = nullptr;
    DirectorControl* directorControl_ = nullptr;
    PowerManager* power_ = nullptr;
    GameStorage* gameStorage_ = nullptr;
};
