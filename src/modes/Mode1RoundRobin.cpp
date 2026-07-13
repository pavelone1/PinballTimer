#include "modes/Mode1RoundRobin.h"

const char* Mode1RoundRobin::name() const
{
    return "Round Robin";
}

uint8_t Mode1RoundRobin::id() const
{
    return MODE_ID;
}

uint8_t Mode1RoundRobin::minPlayers() const
{
    return 1;
}

uint8_t Mode1RoundRobin::maxPlayers() const
{
    return MAX_MODE_PLAYERS;
}

uint8_t Mode1RoundRobin::defaultPlayerCount() const
{
    return MAX_MODE_PLAYERS;
}

void Mode1RoundRobin::setSecondsPerTurn(long seconds)
{
    if (seconds < 1) {
        seconds = 1;
    } else if (seconds > MAX_SECONDS_PER_TURN) {
        seconds = MAX_SECONDS_PER_TURN;
    }

    secondsPerTurn_ = seconds;
}

long Mode1RoundRobin::secondsPerTurn() const
{
    return secondsPerTurn_;
}

void Mode1RoundRobin::setupAssignments(GameModeContext& context, uint8_t playerCount)
{
    playerCount_ = playerCount > MAX_MODE_PLAYERS ? MAX_MODE_PLAYERS : playerCount;
    activePlayerIndex_ = 0;
    roundStarted_ = false;

    for (uint8_t i = 0; i < MAX_MODE_PLAYERS; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        const ButtonId button = static_cast<ButtonId>(i);
        const DisplayId display = static_cast<DisplayId>(i);

        if (i < playerCount_) {
            playerTimerIds_[i] = context.timers.createTimer(
                CountDirection::CountDown,
                secondsPerTurn_,
                /*allowBelowZero=*/true,
                /*stopAtZero=*/false,
                /*loopEnabled=*/false
            );
            context.timers.setAssociatedPlayer(playerTimerIds_[i], player);
            context.timers.setDisplayAssignment(playerTimerIds_[i], display);

            context.buttonAssignments.assignToPlayer(button, player);
            context.displayAssignments.assignToSharedTimer(display, playerTimerIds_[i]);

            context.numericDisplays.setFormat(display, DisplayFormat::TimeMinutesSeconds);
            context.numericDisplays.setColon(display, true);
            context.numericDisplays.setVisible(display, true);
            context.numericDisplays.setValue(display, secondsPerTurn_);

            context.players.setStatus(player, PlayerStatus::Waiting);
            context.buttonLights.setBaseState(button, LightPattern::Off);
        } else {
            playerTimerIds_[i] = TimerManager::INVALID_TIMER;
            context.buttonAssignments.clearAssignment(button);
            context.displayAssignments.clearAssignment(display);
            context.numericDisplays.setVisible(display, false);
            context.players.setStatus(player, PlayerStatus::Inactive);
            context.buttonLights.setBaseState(button, LightPattern::Off);
        }
    }

    context.buttonAssignments.assignToModeAction(ButtonId::Action, ACTION_START_ROUND);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, 500);
}

void Mode1RoundRobin::update(GameModeContext& context)
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        const long value = context.timers.currentValueSeconds(playerTimerIds_[i]);
        context.numericDisplays.setValue(static_cast<DisplayId>(i), value);
    }
}

void Mode1RoundRobin::onPause(GameModeContext& context)
{
    if (roundStarted_) {
        context.timers.stop(playerTimerIds_[activePlayerIndex_]);
    }
}

void Mode1RoundRobin::onStop(GameModeContext& context)
{
    if (roundStarted_) {
        context.timers.stop(playerTimerIds_[activePlayerIndex_]);
    }
}

void Mode1RoundRobin::onReset(GameModeContext& context)
{
    resetRoundState(context);
}

void Mode1RoundRobin::onButtonEvent(GameModeContext& context, const ButtonEvent& event)
{
    if (event.type != ButtonEventType::Pressed) {
        return;
    }

    if (!roundStarted_) {
        if (event.button == ButtonId::Action) {
            startRound(context);
        }
        return;
    }

    const ButtonId activeButton = static_cast<ButtonId>(activePlayerIndex_);
    if (event.button == activeButton) {
        advanceTurn(context);
    }
}

void Mode1RoundRobin::startRound(GameModeContext& context)
{
    if (playerCount_ == 0) {
        return;
    }

    roundStarted_ = true;
    activePlayerIndex_ = 0;

    context.timers.start(playerTimerIds_[0]);
    context.players.setStatus(PlayerId::Player1, PlayerStatus::Active);
    context.buttonLights.setBaseState(ButtonId::Player1, LightPattern::Solid);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Off);
}

void Mode1RoundRobin::advanceTurn(GameModeContext& context)
{
    const ButtonId finishedButton = static_cast<ButtonId>(activePlayerIndex_);
    const PlayerId finishedPlayer = static_cast<PlayerId>(activePlayerIndex_);

    context.timers.stop(playerTimerIds_[activePlayerIndex_]);
    context.timers.reset(playerTimerIds_[activePlayerIndex_], secondsPerTurn_);
    context.players.setStatus(finishedPlayer, PlayerStatus::Waiting);
    context.buttonLights.setBaseState(finishedButton, LightPattern::Off);

    activePlayerIndex_ = (activePlayerIndex_ + 1) % playerCount_;

    const ButtonId nextButton = static_cast<ButtonId>(activePlayerIndex_);
    const PlayerId nextPlayer = static_cast<PlayerId>(activePlayerIndex_);

    context.players.setStatus(nextPlayer, PlayerStatus::Active);
    context.buttonLights.setBaseState(nextButton, LightPattern::Solid);
    context.timers.start(playerTimerIds_[activePlayerIndex_]);
}

void Mode1RoundRobin::resetRoundState(GameModeContext& context)
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        context.timers.stop(playerTimerIds_[i]);
        context.timers.reset(playerTimerIds_[i], secondsPerTurn_);
        context.players.setStatus(static_cast<PlayerId>(i), PlayerStatus::Waiting);
        context.buttonLights.setBaseState(static_cast<ButtonId>(i), LightPattern::Off);
        context.numericDisplays.setValue(static_cast<DisplayId>(i), secondsPerTurn_);
    }

    roundStarted_ = false;
    activePlayerIndex_ = 0;
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, 500);
}
