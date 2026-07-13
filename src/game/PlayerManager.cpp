#include "game/PlayerManager.h"

#include <cstring>

void PlayerManager::begin()
{
    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        Player& p = players_[i];
        p = Player{};
        p.displayedNumber = i + 1;
        p.currentSlot = static_cast<PlayerId>(i);
        p.buttonAssignment = static_cast<ButtonId>(i);
        p.displayAssignment = static_cast<DisplayId>(i);
    }

    activePlayerCount_ = 0;
}

void PlayerManager::setActivePlayerCount(uint8_t count)
{
    const uint8_t clamped = count > PLAYER_COUNT ? PLAYER_COUNT : count;
    activePlayerCount_ = clamped;

    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        Player& p = players_[i];

        if (i < clamped) {
            if (p.status == PlayerStatus::Inactive) {
                p.status = PlayerStatus::Waiting;
            }
        } else {
            p.status = PlayerStatus::Inactive;
        }
    }
}

uint8_t PlayerManager::activePlayerCount() const
{
    return activePlayerCount_;
}

void PlayerManager::setDisplayedNumber(PlayerId player, uint8_t number)
{
    players_[static_cast<uint8_t>(player)].displayedNumber = number;
}

uint8_t PlayerManager::displayedNumber(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].displayedNumber;
}

void PlayerManager::setName(PlayerId player, const char* name)
{
    char* dest = players_[static_cast<uint8_t>(player)].name;
    strncpy(dest, name, MAX_NAME_LENGTH - 1);
    dest[MAX_NAME_LENGTH - 1] = '\0';
}

const char* PlayerManager::name(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].name;
}

void PlayerManager::setAssignedColor(PlayerId player, ColorId color)
{
    players_[static_cast<uint8_t>(player)].assignedColor = color;
}

ColorId PlayerManager::assignedColor(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].assignedColor;
}

void PlayerManager::setPreferredColor(PlayerId player, ColorId color)
{
    players_[static_cast<uint8_t>(player)].preferredColor = color;
}

ColorId PlayerManager::preferredColor(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].preferredColor;
}

void PlayerManager::setCurrentSlot(PlayerId player, PlayerId slot)
{
    players_[static_cast<uint8_t>(player)].currentSlot = slot;
}

PlayerId PlayerManager::currentSlot(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].currentSlot;
}

void PlayerManager::setButtonAssignment(PlayerId player, ButtonId button)
{
    players_[static_cast<uint8_t>(player)].buttonAssignment = button;
}

ButtonId PlayerManager::buttonAssignment(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].buttonAssignment;
}

void PlayerManager::setDisplayAssignment(PlayerId player, DisplayId display)
{
    players_[static_cast<uint8_t>(player)].displayAssignment = display;
}

DisplayId PlayerManager::displayAssignment(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].displayAssignment;
}

void PlayerManager::setTeamId(PlayerId player, uint8_t teamId)
{
    players_[static_cast<uint8_t>(player)].teamId = teamId;
}

uint8_t PlayerManager::teamId(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].teamId;
}

void PlayerManager::setStatus(PlayerId player, PlayerStatus status)
{
    players_[static_cast<uint8_t>(player)].status = status;
}

PlayerStatus PlayerManager::status(PlayerId player) const
{
    return players_[static_cast<uint8_t>(player)].status;
}

void PlayerManager::setModeData(PlayerId player, uint8_t index, uint8_t value)
{
    if (index >= MODE_DATA_SIZE) {
        return;
    }

    players_[static_cast<uint8_t>(player)].modeData[index] = value;
}

uint8_t PlayerManager::modeData(PlayerId player, uint8_t index) const
{
    if (index >= MODE_DATA_SIZE) {
        return 0;
    }

    return players_[static_cast<uint8_t>(player)].modeData[index];
}
