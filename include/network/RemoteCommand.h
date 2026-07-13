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
    RequestFullStatus
};

enum class DirectorCommandResult : uint8_t {
    Ok,
    Rejected,
    InvalidRequest,
    NoActiveMode
};

struct DirectorCommand {
    DirectorCommandType type = DirectorCommandType::Unknown;
    uint8_t intValue = 0;      // SelectMode(modeId), SetPlayerCount(count), IdentifyTimer(playerIndex)
    char stringKey[16] = "";   // SetModeOption(key)
    long longValue = 0;        // SetModeOption(value)
};
