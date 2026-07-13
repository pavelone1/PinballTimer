#include "game/ButtonAssignmentManager.h"

void ButtonAssignmentManager::begin()
{
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        assignments_[i] = ButtonAssignment{};
    }
}

void ButtonAssignmentManager::assignToPlayer(ButtonId button, PlayerId player)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::SinglePlayer;
    a.player = player;
}

void ButtonAssignmentManager::assignToPlayers(ButtonId button, uint8_t playerMask)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::MultiplePlayers;
    a.playerMask = playerMask;
}

void ButtonAssignmentManager::assignToTeam(ButtonId button, uint8_t teamId)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::Team;
    a.teamId = teamId;
}

void ButtonAssignmentManager::assignToPhysicalSlot(ButtonId button, uint8_t slotId)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::PhysicalSlot;
    a.slotId = slotId;
}

void ButtonAssignmentManager::assignToRotatingGroup(ButtonId button, uint8_t groupId)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::RotatingGroup;
    a.groupId = groupId;
}

void ButtonAssignmentManager::assignToSharedTimer(ButtonId button, TimerId timerId)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::SharedTimer;
    a.timerId = timerId;
}

void ButtonAssignmentManager::assignToModeAction(ButtonId button, uint8_t modeActionId)
{
    ButtonAssignment& a = assignments_[static_cast<uint8_t>(button)];
    a = ButtonAssignment{};
    a.type = ButtonAssignmentType::ModeAction;
    a.modeActionId = modeActionId;
}

void ButtonAssignmentManager::clearAssignment(ButtonId button)
{
    assignments_[static_cast<uint8_t>(button)] = ButtonAssignment{};
}

ButtonAssignmentType ButtonAssignmentManager::assignmentType(ButtonId button) const
{
    return assignments_[static_cast<uint8_t>(button)].type;
}

const ButtonAssignment& ButtonAssignmentManager::assignment(ButtonId button) const
{
    return assignments_[static_cast<uint8_t>(button)];
}
