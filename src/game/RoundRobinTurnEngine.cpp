#include "game/RoundRobinTurnEngine.h"

#include "game/TurnRotation.h"

void RoundRobinTurnEngine::reset()
{
    activeButton_ = ButtonId::Action;
    started_ = false;
    timerRunning_ = false;
    paused_ = false;
    complete_ = false;
}

bool RoundRobinTurnEngine::start(
    const ButtonAssignmentManager& assignments,
    const PlayerManager& players)
{
    ButtonId first;
    if (!TurnRotation::firstButton(assignments, players, first)) {
        complete_ = true;
        return false;
    }
    activeButton_ = first;
    started_ = true;
    timerRunning_ = true;
    paused_ = false;
    complete_ = false;
    return true;
}

bool RoundRobinTurnEngine::advance(
    const ButtonAssignmentManager& assignments,
    const PlayerManager& players)
{
    ButtonId next;
    timerRunning_ = false;
    if (!TurnRotation::nextButton(assignments, players, activeButton_, next)) {
        complete_ = true;
        return false;
    }
    activeButton_ = next;
    complete_ = false;
    return true;
}

RoundRobinTurnEngine::PlayerPressResult
RoundRobinTurnEngine::handlePlayerPress(ButtonId button)
{
    if (!started_ || complete_ || paused_ || button != activeButton_) {
        return PlayerPressResult::Ignored;
    }
    if (!timerRunning_) {
        timerRunning_ = true;
        return PlayerPressResult::StartTimer;
    }
    timerRunning_ = false;
    return PlayerPressResult::EndBall;
}

bool RoundRobinTurnEngine::togglePause()
{
    if (!started_ || complete_) {
        return false;
    }
    paused_ = !paused_;
    return true;
}

void RoundRobinTurnEngine::restore(ButtonId activeButton, bool started,
                                   bool timerRunning, bool paused, bool complete)
{
    activeButton_ = activeButton;
    started_ = started;
    timerRunning_ = timerRunning;
    paused_ = paused;
    complete_ = complete;
}

ButtonId RoundRobinTurnEngine::activeButton() const { return activeButton_; }
bool RoundRobinTurnEngine::started() const { return started_; }
bool RoundRobinTurnEngine::timerRunning() const { return timerRunning_; }
bool RoundRobinTurnEngine::paused() const { return paused_; }
bool RoundRobinTurnEngine::complete() const { return complete_; }

