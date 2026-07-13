#pragma once

#include <cstdint>
#include "SystemTypes.h"

// Manages the relationship between logical game data and the four
// physical numeric displays. Tells NumericDisplayManager what source
// belongs on each display -- stores the assignment (what kind of
// thing, and which one) only. Pushing the live numeric value into
// NumericDisplayManager each tick is App/GameMode's job, using this
// assignment to know where to read the value from.
enum class DisplayAssignmentType : uint8_t {
    None,
    SinglePlayer,
    MultiplePlayers,
    Team,
    SharedTimer,
    Score,
    Round,
    PlayerNumber,
    ModeValue
};

struct DisplayAssignment {
    DisplayAssignmentType type = DisplayAssignmentType::None;
    PlayerId player = PlayerId::Player1;   // valid when type == SinglePlayer or PlayerNumber
    uint8_t playerMask = 0;                // valid when type == MultiplePlayers (bit i = PlayerId i)
    uint8_t teamId = 0;                    // valid when type == Team
    TimerId timerId = 0;                   // valid when type == SharedTimer
    uint8_t sourceId = 0;                  // valid when type == Score/Round/ModeValue, opaque source code defined by the game mode
};

class DisplayAssignmentManager {
public:
    void begin();

    void assignToPlayer(DisplayId display, PlayerId player);
    void assignToPlayers(DisplayId display, uint8_t playerMask);
    void assignToTeam(DisplayId display, uint8_t teamId);
    void assignToSharedTimer(DisplayId display, TimerId timerId);
    void assignToScore(DisplayId display, uint8_t scoreSourceId);
    void assignToRound(DisplayId display, uint8_t roundSourceId);
    void assignToPlayerNumber(DisplayId display, PlayerId player);
    void assignToModeValue(DisplayId display, uint8_t modeValueId);
    void clearAssignment(DisplayId display);

    DisplayAssignmentType assignmentType(DisplayId display) const;
    const DisplayAssignment& assignment(DisplayId display) const;

private:
    static constexpr uint8_t DISPLAY_COUNT = static_cast<uint8_t>(DisplayId::Count);

    DisplayAssignment assignments_[DISPLAY_COUNT];
};
