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
//
// A mode-agnostic in-progress-round checkpoint (SavedRoundState) is
// also stored here -- see GameMode::restoreState() and
// Mode1RoundRobin's checkpointRoundState(). It's only accurate as of
// the last checkpoint (round start, each turn advance, and pause),
// not continuously, to avoid writing NVS on every timer tick -- a
// resume after an abrupt power loss mid-turn (no pause) restores with
// that turn's starting time, not its exact mid-turn remainder. This
// was a deliberate tradeoff, not an oversight.
struct SavedRoundState {
    static constexpr uint8_t MAX_PLAYERS = 4;

    bool valid = false;
    uint8_t modeId = 0;
    uint8_t playerCount = 0;

    // Whose turn it is, by ButtonId (not player index) -- turn order
    // follows the fixed physical button color order (see
    // game/ButtonColors.h), not PlayerId, since a player can be
    // reassigned to any button.
    uint8_t activeButtonId = 0;

    bool roundStarted = false;

    // False while the active player's button is still flashing,
    // waiting for their own first press to start their clock (see
    // Mode1RoundRobin::onButtonEvent) -- restoring mid-flash needs to
    // resume flashing, not silently start their timer running.
    bool activeTimerRunning = false;

    // True once every player has been Eliminated/Finished (or all
    // rounds completed) -- the whole game is over, no more turns.
    bool gameOver = false;

    // True while the "tap the white Action button to pause" toggle
    // (Mode1RoundRobin::togglePause()) is active -- distinct from
    // activeTimerRunning above (that's "hasn't pressed to start their
    // OWN clock yet"; this is "was running/waiting, then manually
    // held"). Restoring mid-pause must NOT auto-resume the timer.
    bool manuallyPaused = false;

    long secondsPerTurn = 0;
    uint8_t ballCount = 0;
    long playerRemainingSeconds[MAX_PLAYERS] = {};

    // Rounds ("balls") left and PlayerStatus (cast to uint8_t) per
    // player -- both must survive a resume, since a player already
    // Eliminated/Finished before a reboot must NOT come back as
    // Waiting.
    uint8_t playerRoundsRemaining[MAX_PLAYERS] = {};
    uint8_t playerStatus[MAX_PLAYERS] = {};
};

class GameStorage {
public:
    void begin();

    void setPlayerName(PlayerId player, const char* name);
    const char* playerName(PlayerId player) const;

    void setPlayerPreferredColor(PlayerId player, ColorId color);
    ColorId playerPreferredColor(PlayerId player) const;

    void setMode1SecondsPerTurn(long seconds);
    long mode1SecondsPerTurn() const;

    // Per-game-session label -- which physical pinball machine this
    // round is being played on. Distinct from SettingsStorage's
    // deviceName() (permanent device identity used for AP SSID/mDNS);
    // this is metadata attached to a Game, re-enterable each session.
    void setMachineName(const char* name);
    const char* machineName() const;

    // Which physical button a player is assigned to for this game.
    // PlayerManager::begin() resets this to identity every boot, so it
    // must be persisted separately or a director's reassignment would
    // silently vanish on reboot (see App::begin()'s load loop).
    void setPlayerButtonAssignment(PlayerId player, ButtonId button);
    ButtonId playerButtonAssignment(PlayerId player) const;

    void saveRoundState(const SavedRoundState& state);
    const SavedRoundState& loadRoundState() const;
    void clearRoundState();

private:
    static constexpr uint8_t PLAYER_COUNT = static_cast<uint8_t>(PlayerId::Count);
    static constexpr uint8_t NAME_MAX_LENGTH = 16;
    static constexpr uint8_t MACHINE_NAME_MAX_LENGTH = 25;

    Preferences prefs_;

    char playerNames_[PLAYER_COUNT][NAME_MAX_LENGTH] = {};
    ColorId playerPreferredColors_[PLAYER_COUNT] = {};
    ButtonId playerButtonAssignments_[PLAYER_COUNT] = {};
    long mode1SecondsPerTurn_ = 30;
    char machineName_[MACHINE_NAME_MAX_LENGTH] = "";
    SavedRoundState savedRoundState_;
};
