#pragma once

#include <cstdint>
#include "SystemTypes.h"

enum class PlayerStatus : uint8_t {
    Inactive,
    Waiting,
    Active,
    Eliminated,
    Finished
};

// Stores the logical players. PlayerId is the permanent internal ID
// for each of the up to 4 physical player slots and never changes.
// Everything else (displayed number, colors, current slot, button/
// display assignment, team, status) is mutable and may be reassigned
// by a game mode -- this class only stores it, it doesn't decide when
// or why those change.
class PlayerManager {
public:
    void begin();

    // Sizes the active roster (1-4). Slots within count that were
    // Inactive become Waiting; slots outside count become Inactive.
    // Does not touch slots already Active/Eliminated/Finished.
    void setActivePlayerCount(uint8_t count);
    uint8_t activePlayerCount() const;

    void setDisplayedNumber(PlayerId player, uint8_t number);
    uint8_t displayedNumber(PlayerId player) const;

    void setName(PlayerId player, const char* name);
    const char* name(PlayerId player) const;

    void setAssignedColor(PlayerId player, ColorId color);
    ColorId assignedColor(PlayerId player) const;

    void setPreferredColor(PlayerId player, ColorId color);
    ColorId preferredColor(PlayerId player) const;

    void setCurrentSlot(PlayerId player, PlayerId slot);
    PlayerId currentSlot(PlayerId player) const;

    void setButtonAssignment(PlayerId player, ButtonId button);
    ButtonId buttonAssignment(PlayerId player) const;

    void setDisplayAssignment(PlayerId player, DisplayId display);
    DisplayId displayAssignment(PlayerId player) const;

    // 0 = no team/group assigned.
    void setTeamId(PlayerId player, uint8_t teamId);
    uint8_t teamId(PlayerId player) const;

    void setStatus(PlayerId player, PlayerStatus status);
    PlayerStatus status(PlayerId player) const;

    // Opaque per-player scratch storage for future game modes. This
    // class does not interpret the contents.
    void setModeData(PlayerId player, uint8_t index, uint8_t value);
    uint8_t modeData(PlayerId player, uint8_t index) const;

private:
    static constexpr uint8_t PLAYER_COUNT = static_cast<uint8_t>(PlayerId::Count);
    static constexpr uint8_t MAX_NAME_LENGTH = 16;
    static constexpr uint8_t MODE_DATA_SIZE = 8;

    struct Player {
        uint8_t displayedNumber = 0;
        char name[MAX_NAME_LENGTH] = "";
        ColorId assignedColor = ColorId::White;
        ColorId preferredColor = ColorId::White;
        PlayerId currentSlot = PlayerId::Player1;
        ButtonId buttonAssignment = ButtonId::Player1;
        DisplayId displayAssignment = DisplayId::Display1;
        uint8_t teamId = 0;
        PlayerStatus status = PlayerStatus::Inactive;
        uint8_t modeData[MODE_DATA_SIZE] = {};
    };

    Player players_[PLAYER_COUNT];
    uint8_t activePlayerCount_ = 0;
};
