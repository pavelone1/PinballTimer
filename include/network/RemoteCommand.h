#pragma once

#include <cstdint>

// Director-issued command types and results. This is the concrete
// wire-level vocabulary for the HTTP REST director API (see
// DirectorControl) -- values map directly to the "type" field
// clients POST to /command.
enum class DirectorCommandType : uint8_t {
    Unknown = 0,
    SelectMode,
    SetPlayerCount,
    SetModeOption,
    StartGame,
    StartFirstTimer,
    Pause,
    Resume,
    Reset,
    LockLocalControls,
    UnlockLocalControls,
    IdentifyTimer,
    RequestFullStatus,

    // Debug/testing only -- lets a director simulate battery voltage
    // without real sensing hardware wired (see PowerManager.h).
    // longValue is millivolts, e.g. 3700 for 3.70V.
    SetStubBatteryVoltage,
    ClearBatteryVoltageOverride,

    // Game-setup metadata, for the /game-setup dashboard page (see
    // DirectorDashboard). Always allowed regardless of active mode,
    // same treatment as SelectMode/lock commands.
    SetPlayerName,   // intValue = PlayerId (0-3), stringKey = name
    SetPlayerButton, // intValue = PlayerId (0-3), longValue = ButtonId (0-3)
    SetMachineName   // stringKey = machine name
};

enum class DirectorCommandResult : uint8_t {
    Ok,
    Rejected,
    InvalidRequest,
    NoActiveMode
};

struct DirectorCommand {
    DirectorCommandType type = DirectorCommandType::Unknown;
    uint8_t intValue = 0;      // SelectMode(modeId), SetPlayerCount(count), IdentifyTimer(playerIndex), SetPlayerName/SetPlayerButton(PlayerId)
    char stringKey[24] = "";   // SetModeOption(key), SetPlayerName(name), SetMachineName(name)
    long longValue = 0;        // SetModeOption(value), SetPlayerButton(ButtonId)
};
