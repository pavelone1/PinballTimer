#pragma once

#include "SystemTypes.h"
#include "game/ButtonAssignmentManager.h"
#include "game/PlayerManager.h"

// Hardware-independent state machine shared by round-robin modes. Modes remain
// responsible for timers and presentation, while this class owns the common
// start/handoff/press/pause state and color-order advancement.
class RoundRobinTurnEngine {
public:
    enum class PlayerPressResult : uint8_t {
        Ignored,
        StartTimer,
        EndBall
    };

    void reset();
    bool start(const ButtonAssignmentManager& assignments, const PlayerManager& players);
    bool advance(const ButtonAssignmentManager& assignments, const PlayerManager& players);

    PlayerPressResult handlePlayerPress(ButtonId button);
    bool togglePause();

    void restore(ButtonId activeButton, bool started, bool timerRunning,
                 bool paused, bool complete);

    ButtonId activeButton() const;
    bool started() const;
    bool timerRunning() const;
    bool paused() const;
    bool complete() const;

private:
    ButtonId activeButton_ = ButtonId::Action;
    bool started_ = false;
    bool timerRunning_ = false;
    bool paused_ = false;
    bool complete_ = false;
};

