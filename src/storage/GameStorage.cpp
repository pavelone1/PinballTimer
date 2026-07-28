#include "storage/GameStorage.h"

#include <cstring>
#include <cstdio>

void GameStorage::begin()
{
    prefs_.begin("gamedata", false);

    // Color-coded fallback names for a fresh device (no "p%uname" key
    // saved yet) -- without this, a player whose name was never set
    // shows up blank everywhere (BootMenu, /status JSON) instead of
    // something a director can read off the physical button/light
    // color. BootMenu's Player Info screen overwrites this with a
    // real name on first edit.
    static const char* const kDefaultPlayerNames[PLAYER_COUNT] = {
        "Player 1 RED", "Player 2 YELLOW", "Player 3 GREEN", "Player 4 BLUE"
    };

    char key[16];

    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        snprintf(key, sizeof(key), "p%uname", i);
        const size_t len = prefs_.getString(key, playerNames_[i], NAME_MAX_LENGTH);
        if (len == 0) {
            strncpy(playerNames_[i], kDefaultPlayerNames[i], NAME_MAX_LENGTH - 1);
            playerNames_[i][NAME_MAX_LENGTH - 1] = '\0';
        }

        snprintf(key, sizeof(key), "p%ucolor", i);
        playerPreferredColors_[i] = static_cast<ColorId>(
            prefs_.getUChar(key, static_cast<uint8_t>(playerPreferredColors_[i]))
        );

        // Default identity (button i for player i), same as
        // PlayerManager::begin()'s own default -- only differs once a
        // director explicitly reassigns and this key gets written.
        snprintf(key, sizeof(key), "p%ubutton", i);
        playerButtonAssignments_[i] = static_cast<ButtonId>(prefs_.getUChar(key, i));
    }

    mode1SecondsPerTurn_ = prefs_.getLong("m1SecPerTurn", mode1SecondsPerTurn_);

    prefs_.getString("machineName", machineName_, MACHINE_NAME_MAX_LENGTH);

    savedRoundState_.valid = prefs_.getBool("rsValid", false);
    savedRoundState_.modeId = prefs_.getUChar("rsMode", 0);
    savedRoundState_.playerCount = prefs_.getUChar("rsCount", 0);
    savedRoundState_.activeButtonId = prefs_.getUChar("rsActiveBtn", 0);
    savedRoundState_.roundStarted = prefs_.getBool("rsStarted", false);
    savedRoundState_.activeTimerRunning = prefs_.getBool("rsTmrRun", false);
    savedRoundState_.gameOver = prefs_.getBool("rsOver", false);
    savedRoundState_.manuallyPaused = prefs_.getBool("rsPaused", false);
    savedRoundState_.secondsPerTurn = prefs_.getLong("rsSPT", 0);
    savedRoundState_.ballCount = prefs_.getUChar("rsBalls", 0);

    for (uint8_t i = 0; i < SavedRoundState::MAX_PLAYERS; ++i) {
        snprintf(key, sizeof(key), "rsP%u", i);
        savedRoundState_.playerRemainingSeconds[i] = prefs_.getLong(key, 0);

        snprintf(key, sizeof(key), "rsPR%u", i);
        savedRoundState_.playerRoundsRemaining[i] = prefs_.getUChar(key, 0);

        snprintf(key, sizeof(key), "rsPS%u", i);
        savedRoundState_.playerStatus[i] = prefs_.getUChar(key, 0);
    }
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

void GameStorage::setMachineName(const char* name)
{
    strncpy(machineName_, name, MACHINE_NAME_MAX_LENGTH - 1);
    machineName_[MACHINE_NAME_MAX_LENGTH - 1] = '\0';
    prefs_.putString("machineName", machineName_);
}

const char* GameStorage::machineName() const
{
    return machineName_;
}

void GameStorage::setPlayerButtonAssignment(PlayerId player, ButtonId button)
{
    const uint8_t i = static_cast<uint8_t>(player);
    playerButtonAssignments_[i] = button;

    char key[16];
    snprintf(key, sizeof(key), "p%ubutton", i);
    prefs_.putUChar(key, static_cast<uint8_t>(button));
}

ButtonId GameStorage::playerButtonAssignment(PlayerId player) const
{
    return playerButtonAssignments_[static_cast<uint8_t>(player)];
}

void GameStorage::saveRoundState(const SavedRoundState& state)
{
    savedRoundState_ = state;

    prefs_.putBool("rsValid", state.valid);
    prefs_.putUChar("rsMode", state.modeId);
    prefs_.putUChar("rsCount", state.playerCount);
    prefs_.putUChar("rsActiveBtn", state.activeButtonId);
    prefs_.putBool("rsStarted", state.roundStarted);
    prefs_.putBool("rsTmrRun", state.activeTimerRunning);
    prefs_.putBool("rsOver", state.gameOver);
    prefs_.putBool("rsPaused", state.manuallyPaused);
    prefs_.putLong("rsSPT", state.secondsPerTurn);
    prefs_.putUChar("rsBalls", state.ballCount);

    char key[8];
    for (uint8_t i = 0; i < SavedRoundState::MAX_PLAYERS; ++i) {
        snprintf(key, sizeof(key), "rsP%u", i);
        prefs_.putLong(key, state.playerRemainingSeconds[i]);

        snprintf(key, sizeof(key), "rsPR%u", i);
        prefs_.putUChar(key, state.playerRoundsRemaining[i]);

        snprintf(key, sizeof(key), "rsPS%u", i);
        prefs_.putUChar(key, state.playerStatus[i]);
    }
}

const SavedRoundState& GameStorage::loadRoundState() const
{
    return savedRoundState_;
}

void GameStorage::clearRoundState()
{
    savedRoundState_.valid = false;
    prefs_.putBool("rsValid", false);
}
