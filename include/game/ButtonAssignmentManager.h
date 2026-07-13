#pragma once

#include <cstdint>
#include "SystemTypes.h"

// Separate from physical button polling (ButtonInput). Manages what
// each physical button currently represents -- stores the assignment
// only, does not decide what a press does. A GameMode reads this to
// interpret ButtonInput events.
enum class ButtonAssignmentType : uint8_t {
    None,
    SinglePlayer,
    MultiplePlayers,
    Team,
    PhysicalSlot,
    RotatingGroup,
    SharedTimer,
    ModeAction
};

struct ButtonAssignment {
    ButtonAssignmentType type = ButtonAssignmentType::None;
    PlayerId player = PlayerId::Player1;   // valid when type == SinglePlayer
    uint8_t playerMask = 0;                // valid when type == MultiplePlayers (bit i = PlayerId i)
    uint8_t teamId = 0;                    // valid when type == Team
    uint8_t slotId = 0;                    // valid when type == PhysicalSlot
    uint8_t groupId = 0;                   // valid when type == RotatingGroup
    TimerId timerId = 0;                   // valid when type == SharedTimer
    uint8_t modeActionId = 0;              // valid when type == ModeAction, opaque action code defined by the game mode
};

class ButtonAssignmentManager {
public:
    void begin();

    void assignToPlayer(ButtonId button, PlayerId player);
    void assignToPlayers(ButtonId button, uint8_t playerMask);
    void assignToTeam(ButtonId button, uint8_t teamId);
    void assignToPhysicalSlot(ButtonId button, uint8_t slotId);
    void assignToRotatingGroup(ButtonId button, uint8_t groupId);
    void assignToSharedTimer(ButtonId button, TimerId timerId);
    void assignToModeAction(ButtonId button, uint8_t modeActionId);
    void clearAssignment(ButtonId button);

    ButtonAssignmentType assignmentType(ButtonId button) const;
    const ButtonAssignment& assignment(ButtonId button) const;

private:
    static constexpr uint8_t BUTTON_COUNT = static_cast<uint8_t>(ButtonId::Count);

    ButtonAssignment assignments_[BUTTON_COUNT];
};
