#pragma once

#include <cstdint>
#include <Preferences.h>
#include "SystemTypes.h"

// Persistent game-related configuration (NVS, "gamedata" namespace):
// player names/preferred colors, and Mode 1's configurable
// seconds-per-turn. Active timer values are never written here --
// TimerManager is purely in-memory.
//
// "Saved presets" (named bundles of settings a user can save/reload)
// is NOT implemented -- no concrete UI or preset format exists yet to
// design its shape around. "Custom timer values" beyond
// secondsPerTurn also isn't implemented since Mode 1 gives every
// player an equal timer (no per-player override exists to store).
class GameStorage {
public:
    void begin();

    void setPlayerName(PlayerId player, const char* name);
    const char* playerName(PlayerId player) const;

    void setPlayerPreferredColor(PlayerId player, ColorId color);
    ColorId playerPreferredColor(PlayerId player) const;

    void setMode1SecondsPerTurn(long seconds);
    long mode1SecondsPerTurn() const;

private:
    static constexpr uint8_t PLAYER_COUNT = static_cast<uint8_t>(PlayerId::Count);
    static constexpr uint8_t NAME_MAX_LENGTH = 16;

    Preferences prefs_;

    char playerNames_[PLAYER_COUNT][NAME_MAX_LENGTH] = {};
    ColorId playerPreferredColors_[PLAYER_COUNT] = {};
    long mode1SecondsPerTurn_ = 30;
};
