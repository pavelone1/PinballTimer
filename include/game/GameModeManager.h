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

    GameMode* activeMode() const;
    bool hasActiveMode() const;

    // Returns false (and leaves playerCount unchanged) if no mode is
    // selected or count is outside the mode's min/max.
    bool setPlayerCount(uint8_t count);
    uint8_t playerCount() const;

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
