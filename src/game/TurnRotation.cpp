#include "game/TurnRotation.h"

#include "game/ButtonColors.h"

namespace {

int indexOf(ButtonId button)
{
    for (int i = 0; i < 4; ++i) {
        if (ButtonColors::kColorOrder[i] == button) {
            return i;
        }
    }
    return -1;
}

bool qualifies(const ButtonAssignmentManager& assignments, const PlayerManager& players, ButtonId button)
{
    if (assignments.assignmentType(button) != ButtonAssignmentType::SinglePlayer) {
        return false;
    }

    const PlayerId player = assignments.assignment(button).player;
    return players.status(player) == PlayerStatus::Waiting;
}

} // namespace

namespace TurnRotation {

bool firstButton(const ButtonAssignmentManager& assignments, const PlayerManager& players, ButtonId& outButton)
{
    for (uint8_t i = 0; i < 4; ++i) {
        const ButtonId candidate = ButtonColors::kColorOrder[i];
        if (qualifies(assignments, players, candidate)) {
            outButton = candidate;
            return true;
        }
    }
    return false;
}

bool nextButton(
    const ButtonAssignmentManager& assignments,
    const PlayerManager& players,
    ButtonId current,
    ButtonId& outButton
)
{
    const int base = indexOf(current); // -1 (not a color-order button) falls back to scanning from index 0
    for (uint8_t step = 1; step <= 4; ++step) {
        const int idx = (base + static_cast<int>(step)) % 4;
        const ButtonId candidate = ButtonColors::kColorOrder[idx];
        if (qualifies(assignments, players, candidate)) {
            outButton = candidate;
            return true;
        }
    }
    return false;
}

} // namespace TurnRotation
