#pragma once

#include <cstdint>
#include "game/GameMode.h"

// Mode 1: 4-Player Round-Robin Timer, per CLAUDE.md's "Mode 1 rules".
//
// - 1-4 players, each with an equal configurable countdown per turn
//   (resets to the full value every turn, chess "move timer" style).
// - The Action button starts the round and Player 1's countdown.
// - A player taps their OWN button to end their turn; the timer
//   advances to the next player and resets their countdown.
// - Zero-crossing: CLAUDE.md flagged "what happens at zero" as TBD.
//   This mode uses allowBelowZero=true on each player's timer (see
//   TimerManager), so time keeps counting past zero as negative
//   overtime rather than auto-advancing -- NumericDisplayManager
//   automatically shows that as the magnitude flashing rapidly. This
//   means turns do NOT auto-advance at zero; only the player's own
//   button press advances the turn. That is an inference from the
//   negative-time display behavior already confirmed, not something
//   independently re-confirmed for this mode -- flag if wrong.
// - secondsPerTurn defaults to 30s here as a placeholder; CLAUDE.md
//   doesn't specify a default (only the 5999s / 99:59 cap), so this
//   should be treated as provisional until set explicitly (e.g. by a
//   future config UI) or confirmed.
// - Active player's button glows solid; others (and Action, once the
//   round has started) are off. This highlighting choice isn't
//   confirmed by CLAUDE.md either -- flag if a different scheme is
//   wanted.
class Mode1RoundRobin : public GameMode {
public:
    static constexpr uint8_t MODE_ID = 1;
    static constexpr long DEFAULT_SECONDS_PER_TURN = 30;
    static constexpr long MAX_SECONDS_PER_TURN = 5999;

    const char* name() const override;
    uint8_t id() const override;

    uint8_t minPlayers() const override;
    uint8_t maxPlayers() const override;
    uint8_t defaultPlayerCount() const override;

    void setSecondsPerTurn(long seconds);
    long secondsPerTurn() const;

    void setupAssignments(GameModeContext& context, uint8_t playerCount) override;

    void update(GameModeContext& context) override;
    void onPause(GameModeContext& context) override;
    void onStop(GameModeContext& context) override;
    void onReset(GameModeContext& context) override;

    void onButtonEvent(GameModeContext& context, const ButtonEvent& event) override;

private:
    static constexpr uint8_t MAX_MODE_PLAYERS = 4;
    static constexpr uint8_t ACTION_START_ROUND = 1;

    long secondsPerTurn_ = DEFAULT_SECONDS_PER_TURN;
    uint8_t playerCount_ = 0;
    uint8_t activePlayerIndex_ = 0;
    bool roundStarted_ = false;
    TimerId playerTimerIds_[MAX_MODE_PLAYERS] = {};

    void startRound(GameModeContext& context);
    void advanceTurn(GameModeContext& context);
    void resetRoundState(GameModeContext& context);
};
