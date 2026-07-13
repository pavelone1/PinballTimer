#include "storage/GameStorage.h"

#include <cstring>
#include <cstdio>

void GameStorage::begin()
{
    prefs_.begin("gamedata", false);

    char key[16];

    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        snprintf(key, sizeof(key), "p%uname", i);
        prefs_.getString(key, playerNames_[i], NAME_MAX_LENGTH);

        snprintf(key, sizeof(key), "p%ucolor", i);
        playerPreferredColors_[i] = static_cast<ColorId>(
            prefs_.getUChar(key, static_cast<uint8_t>(playerPreferredColors_[i]))
        );
    }

    mode1SecondsPerTurn_ = prefs_.getLong("m1SecPerTurn", mode1SecondsPerTurn_);
}

void GameStorage::setPlayerName(PlayerId player, const char* name)
{
    const uint8_t i = static_cast<uint8_t>(player);

    strncpy(playerNames_[i], name, NAME_MAX_LENGTH - 1);
    playerNames_[i][NAME_MAX_LENGTH - 1] = '\0';

    char key[16];
    snprintf(key, sizeof(key), "p%uname", i);
    prefs_.putString(key, playerNames_[i]);
}

const char* GameStorage::playerName(PlayerId player) const
{
    return playerNames_[static_cast<uint8_t>(player)];
}

void GameStorage::setPlayerPreferredColor(PlayerId player, ColorId color)
{
    const uint8_t i = static_cast<uint8_t>(player);
    playerPreferredColors_[i] = color;

    char key[16];
    snprintf(key, sizeof(key), "p%ucolor", i);
    prefs_.putUChar(key, static_cast<uint8_t>(color));
}

ColorId GameStorage::playerPreferredColor(PlayerId player) const
{
    return playerPreferredColors_[static_cast<uint8_t>(player)];
}

void GameStorage::setMode1SecondsPerTurn(long seconds)
{
    mode1SecondsPerTurn_ = seconds;
    prefs_.putLong("m1SecPerTurn", seconds);
}

long GameStorage::mode1SecondsPerTurn() const
{
    return mode1SecondsPerTurn_;
}
