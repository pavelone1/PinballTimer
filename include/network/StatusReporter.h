#pragma once

#include <cstddef>
#include "game/GameModeManager.h"
#include "game/PlayerManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/TimerManager.h"
#include "network/NetworkManager.h"
#include "storage/SettingsStorage.h"

class DirectorControl; // forward-declared to avoid a circular include with DirectorControl.h

// Produces the timer's remote status as a small hand-built JSON
// document (no JSON library dependency -- the payload shape is small
// and fixed, not deeply dynamic). Battery status is not implemented:
// no ADC pin has been assigned for battery voltage monitoring yet
// (see HardwarePins.h), so this reports batteryAvailable:false rather
// than a fabricated reading.
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
        DirectorControl& directorControl
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
};
