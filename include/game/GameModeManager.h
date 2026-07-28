#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/GameMode.h"

// Registers available modes, selects/initializes the active one,
// validates player count against its min/max, routes input events to
// it, and forwards lifecycle notifications (start/pause/stop/reset).
// Contains no game rules of its own -- it only routes to whichever
// GameMode is active. Does not own the modes it registers (the
// registry that constructs them owns their lifetime).
class GameModeManager {
public:
    static constexpr uint8_t MAX_MODES = 8;
    static constexpr uint8_t NO_MODE_SELECTED = 0xFF;

    void begin(GameModeContext& context);

    bool registerMode(GameMode* mode);

    // Selects a mode by id (does not call setupAssignments yet).
    // Player count resets to the mode's default. Returns false if no
    // mode with that id is registered.
    bool selectMode(uint8_t modeId);

    // Calls the active mode's setupAssignments(). No-op if no mode
    // selected or already initialized.
    void initializeActiveMode();

    // Calls the active mode's restoreState() instead of
    // setupAssignments() -- for resuming an in-progress round from
    // GameStorage's checkpoint (see GameMode::restoreState()). On
    // success, marks the mode initialized/game-started/first-timer-
    // started so state tracking matches a normally-started game;
    // returns false (no state changed) if there's no active mode or
    // it has no valid checkpoint to restore, in which case the caller
    // should fall back to initializeActiveMode().
    bool restoreActiveMode();

    GameMode* activeMode() const;
    bool hasActiveMode() const;

    // Enumerates registered modes (for a mode-select menu) without
    // making any of them active. Index is registration order, not id.
    uint8_t modeCount() const;
    GameMode* modeAt(uint8_t index) const;

    // Returns false (and leaves playerCount unchanged) if no mode is
    // selected or count is outside the mode's min/max.
    bool setPlayerCount(uint8_t count);
    uint8_t playerCount() const;

    // Pass-throughs to the active mode's context-needing option
    // methods (see GameMode.h) -- DirectorMenu's live-adjustment tools
    // call these instead of reaching GameModeContext directly, exactly
    // how every notifyX() below already bridges activeMode_/context_.
    bool setPlayerOption(PlayerId player, const char* key, long value);
    long playerOption(PlayerId player, const char* key) const;
    bool setLiveModeOption(const char* key, long value);

    void notifyLocalStart();
    void notifyRemoteStart();
    void notifyGameStart();
    void notifyFirstTimerStart();
    void notifyPause();
    void notifyResume();
    void notifyStop();
    void notifyReset();

    // Clears the active mode selection (stopping it first if the
    // game was running).
    void returnToModeSelection();

    void update();

    // Routes to the active mode's onButtonEvent(); if that reports the
    // event just started the round (e.g. Action button), also fires
    // notifyLocalStart()/notifyGameStart() so isGameStarted() and the
    // rest of the local-start bookkeeping match the remote StartGame
    // path (see DirectorControl.cpp).
    void handleButtonEvent(const ButtonEvent& event);
    void handleEncoderEvent(const EncoderEvent& event);

    bool isGameStarted() const;
    bool isFirstTimerStarted() const;
    bool isPaused() const;

private:
    GameMode* modes_[MAX_MODES] = {};
    uint8_t modeCount_ = 0;

    GameModeContext* context_ = nullptr;
    GameMode* activeMode_ = nullptr;
    uint8_t playerCount_ = 0;
    bool initialized_ = false;
    bool gameStarted_ = false;
    bool firstTimerStarted_ = false;
    bool paused_ = false;
};
