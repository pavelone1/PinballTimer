#include "game/DisplayAssignmentManager.h"

void DisplayAssignmentManager::begin()
{
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        assignments_[i] = DisplayAssignment{};
    }
}

void DisplayAssignmentManager::assignToPlayer(DisplayId display, PlayerId player)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::SinglePlayer;
    a.player = player;
}

void DisplayAssignmentManager::assignToPlayers(DisplayId display, uint8_t playerMask)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::MultiplePlayers;
    a.playerMask = playerMask;
}

void DisplayAssignmentManager::assignToTeam(DisplayId display, uint8_t teamId)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::Team;
    a.teamId = teamId;
}

void DisplayAssignmentManager::assignToSharedTimer(DisplayId display, TimerId timerId)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::SharedTimer;
    a.timerId = timerId;
}

void DisplayAssignmentManager::assignToScore(DisplayId display, uint8_t scoreSourceId)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::Score;
    a.sourceId = scoreSourceId;
}

void DisplayAssignmentManager::assignToRound(DisplayId display, uint8_t roundSourceId)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::Round;
    a.sourceId = roundSourceId;
}

void DisplayAssignmentManager::assignToPlayerNumber(DisplayId display, PlayerId player)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::PlayerNumber;
    a.player = player;
}

void DisplayAssignmentManager::assignToModeValue(DisplayId display, uint8_t modeValueId)
{
    DisplayAssignment& a = assignments_[static_cast<uint8_t>(display)];
    a = DisplayAssignment{};
    a.type = DisplayAssignmentType::ModeValue;
    a.sourceId = modeValueId;
}

void DisplayAssignmentManager::clearAssignment(DisplayId display)
{
    assignments_[static_cast<uint8_t>(display)] = DisplayAssignment{};
}

DisplayAssignmentType DisplayAssignmentManager::assignmentType(DisplayId display) const
{
    return assignments_[static_cast<uint8_t>(display)].type;
}

const DisplayAssignment& DisplayAssignmentManager::assignment(DisplayId display) const
{
    return assignments_[static_cast<uint8_t>(display)];
}
