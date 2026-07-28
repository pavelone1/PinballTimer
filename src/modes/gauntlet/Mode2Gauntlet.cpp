#include "modes/gauntlet/Mode2Gauntlet.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include "game/ButtonColors.h"

const char* Mode2Gauntlet::name() const { return "Gauntlet"; }
uint8_t Mode2Gauntlet::id() const { return MODE_ID; }
uint8_t Mode2Gauntlet::minPlayers() const { return 1; }
uint8_t Mode2Gauntlet::maxPlayers() const { return 4; }
uint8_t Mode2Gauntlet::defaultPlayerCount() const { return 4; }

void Mode2Gauntlet::clearMachines()
{
    session_.clear();
    configured_ = false;
}

bool Mode2Gauntlet::addMachine(const MachineRecord& machine)
{
    configured_ = session_.addMachine(machine);
    return configured_;
}

bool Mode2Gauntlet::configure(
    const GauntletConfig& config, const MachineCatalog& catalog)
{
    GauntletSession configuredSession;
    if (!config.buildSession(catalog, configuredSession)) {
        return false;
    }
    session_ = configuredSession;
    playerCount_ = config.playerCount();
    configured_ = true;
    return true;
}

const GauntletSession& Mode2Gauntlet::session() const { return session_; }

void Mode2Gauntlet::setupAssignments(GameModeContext& context, uint8_t playerCount)
{
    playerCount_ = playerCount > MAX_PLAYERS ? MAX_PLAYERS : playerCount;
    machineRunning_ = false;
    handoff_ = configured_ && session_.start();
    gameOver_ = !handoff_;
    handoffSelection_ = 0;
    confirmRemoval_ = false;
    turns_.reset();

    context.players.setActivePlayerCount(playerCount_);
    const long pool = session_.startingPoolSeconds();
    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        const ButtonId button = static_cast<ButtonId>(i);
        const DisplayId display = static_cast<DisplayId>(i);
        context.players.setButtonAssignment(player, button);
        context.players.setDisplayAssignment(player, display);
        context.players.setAssignedColor(player, ButtonColors::colorForButton(button));
        context.players.setStatus(player, PlayerStatus::Waiting);
        context.buttonAssignments.assignToPlayer(button, player);
        timerIds_[i] = context.timers.createTimer(
            CountDirection::CountDown, pool, false, true);
        context.displayAssignments.assignToSharedTimer(display, timerIds_[i]);
        context.timers.setAssociatedPlayer(timerIds_[i], player);
        context.timers.setDisplayAssignment(timerIds_[i], display);
        context.numericDisplays.setValue(display, pool);
        context.numericDisplays.setFlashing(display, false);
        context.buttonLights.setBaseState(button, LightPattern::Off);
    }
    context.buttonAssignments.assignToModeAction(ButtonId::Action, 1);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink,
                                      FLASH_INTERVAL_MS);
}

void Mode2Gauntlet::update(GameModeContext& context)
{
    if (machineRunning_ && turns_.timerRunning()) {
        const PlayerId player =
            context.buttonAssignments.assignment(turns_.activeButton()).player;
        const TimerId timer = timerIds_[static_cast<uint8_t>(player)];
        if (context.timers.currentValueSeconds(timer) <= 0) {
            endBall(context, true);
        }
    }
    syncDisplays(context);
    render(context);
}

void Mode2Gauntlet::onPause(GameModeContext& context)
{
    if (machineRunning_ && turns_.timerRunning()) {
        const PlayerId player =
            context.buttonAssignments.assignment(turns_.activeButton()).player;
        context.timers.stop(timerIds_[static_cast<uint8_t>(player)]);
    }
}

void Mode2Gauntlet::onResume(GameModeContext& context)
{
    if (machineRunning_ && turns_.timerRunning() && !turns_.paused()) {
        const PlayerId player =
            context.buttonAssignments.assignment(turns_.activeButton()).player;
        context.timers.start(timerIds_[static_cast<uint8_t>(player)]);
    }
}

void Mode2Gauntlet::onStop(GameModeContext& context) { onPause(context); }

void Mode2Gauntlet::onReset(GameModeContext& context)
{
    setupAssignments(context, playerCount_);
}

bool Mode2Gauntlet::onButtonEvent(GameModeContext& context, const ButtonEvent& event)
{
    if (event.button == ButtonId::Action) {
        if (event.type == ButtonEventType::Pressed && handoff_) {
            beginCurrentMachine(context);
            return !machineRunning_ ? false : true;
        }
        if (event.type == ButtonEventType::Released && machineRunning_ &&
            turns_.togglePause()) {
            const PlayerId player =
                context.buttonAssignments.assignment(turns_.activeButton()).player;
            const TimerId timer = timerIds_[static_cast<uint8_t>(player)];
            if (turns_.paused()) {
                context.timers.stop(timer);
            } else if (turns_.timerRunning()) {
                context.timers.start(timer);
            }
        }
        return false;
    }
    if (event.type != ButtonEventType::Pressed || !machineRunning_) {
        return false;
    }
    const auto result = turns_.handlePlayerPress(event.button);
    const PlayerId player =
        context.buttonAssignments.assignment(turns_.activeButton()).player;
    if (result == RoundRobinTurnEngine::PlayerPressResult::StartTimer) {
        context.timers.start(timerIds_[static_cast<uint8_t>(player)]);
        context.buttonLights.setBaseState(event.button, LightPattern::Solid);
    } else if (result == RoundRobinTurnEngine::PlayerPressResult::EndBall) {
        endBall(context, false);
    }
    return false;
}

void Mode2Gauntlet::onEncoderEvent(GameModeContext& context, const EncoderEvent& event)
{
    if (!handoff_) {
        return;
    }
    const bool finalMachine = session_.isFinalMachine();
    if (event.type == EncoderEventType::RotatedClockwise) {
        if (finalMachine) {
            handoffSelection_ = handoffSelection_ == 0 ? 2 : 0;
        } else {
            handoffSelection_ = handoffSelection_ < 2 ? handoffSelection_ + 1 : 0;
        }
        confirmRemoval_ = false;
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        if (finalMachine) {
            handoffSelection_ = handoffSelection_ == 0 ? 2 : 0;
        } else {
            handoffSelection_ = handoffSelection_ > 0 ? handoffSelection_ - 1 : 2;
        }
        confirmRemoval_ = false;
    } else if (event.type == EncoderEventType::SwShortPress) {
        if (handoffSelection_ == 0) {
            beginCurrentMachine(context);
        } else if (handoffSelection_ == 1 && session_.skipCurrent()) {
            handoffSelection_ = 0;
        } else if (handoffSelection_ == 2) {
            if (!confirmRemoval_) {
                confirmRemoval_ = true;
            } else {
                applyRemovedTime(context);
                confirmRemoval_ = false;
                handoffSelection_ = 0;
            }
        }
    }
}

bool Mode2Gauntlet::isRoundOver() const { return gameOver_; }

void Mode2Gauntlet::beginCurrentMachine(GameModeContext& context)
{
    const GauntletMachineInstance* machine = session_.currentMachine();
    if (!machine || !anyPlayerHasTime(context)) {
        gameOver_ = true;
        handoff_ = false;
        return;
    }
    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        if (context.timers.currentValueSeconds(timerIds_[i]) > 0) {
            context.players.setStatus(player, PlayerStatus::Waiting);
            context.players.setRoundsRemaining(player, machine->ballCount);
            context.numericDisplays.setFlashing(static_cast<DisplayId>(i), false);
        } else {
            context.players.setStatus(player, PlayerStatus::Eliminated);
        }
    }
    turns_.reset();
    if (!turns_.start(context.buttonAssignments, context.players)) {
        gameOver_ = true;
        handoff_ = false;
        return;
    }
    const PlayerId first =
        context.buttonAssignments.assignment(turns_.activeButton()).player;
    context.players.setStatus(first, PlayerStatus::Active);
    context.timers.start(timerIds_[static_cast<uint8_t>(first)]);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Off);
    context.buttonLights.setBaseState(turns_.activeButton(), LightPattern::Solid);
    machineRunning_ = true;
    handoff_ = false;
}

void Mode2Gauntlet::endBall(GameModeContext& context, bool timedOut)
{
    const ButtonId button = turns_.activeButton();
    const PlayerId player = context.buttonAssignments.assignment(button).player;
    const uint8_t playerIndex = static_cast<uint8_t>(player);
    context.timers.stop(timerIds_[playerIndex]);
    context.buttonLights.setBaseState(button, LightPattern::Off);

    if (timedOut) {
        context.players.setStatus(player, PlayerStatus::Eliminated);
        context.numericDisplays.setValue(static_cast<DisplayId>(playerIndex), 0);
        context.numericDisplays.setFlashing(static_cast<DisplayId>(playerIndex), true);
        context.buzzer.buzz();
    } else {
        const uint8_t left = context.players.roundsRemaining(player) - 1;
        context.players.setRoundsRemaining(player, left);
        context.players.setStatus(player, left ? PlayerStatus::Waiting
                                               : PlayerStatus::Finished);
    }
    if (!anyPlayerHasTime(context)) {
        gameOver_ = true;
        machineRunning_ = false;
        return;
    }
    if (!turns_.advance(context.buttonAssignments, context.players)) {
        finishCurrentMachine(context);
        return;
    }
    context.buttonLights.setBaseState(turns_.activeButton(), LightPattern::Blink,
                                      FLASH_INTERVAL_MS);
}

void Mode2Gauntlet::finishCurrentMachine(GameModeContext& context)
{
    machineRunning_ = false;
    if (!session_.completeCurrent() || !anyPlayerHasTime(context)) {
        gameOver_ = true;
        handoff_ = false;
        context.buzzer.littleTune();
        return;
    }
    handoff_ = true;
    handoffSelection_ = 0;
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink,
                                      FLASH_INTERVAL_MS);
}

void Mode2Gauntlet::applyRemovedTime(GameModeContext& context)
{
    long remaining[MAX_PLAYERS] = {};
    for (uint8_t i = 0; i < playerCount_; ++i) {
        remaining[i] = context.timers.currentValueSeconds(timerIds_[i]);
    }
    if (!session_.removeCurrent(remaining, playerCount_)) {
        return;
    }
    for (uint8_t i = 0; i < playerCount_; ++i) {
        context.timers.reset(timerIds_[i], remaining[i]);
        context.numericDisplays.setValue(static_cast<DisplayId>(i), remaining[i]);
        if (remaining[i] == 0) {
            context.players.setStatus(static_cast<PlayerId>(i), PlayerStatus::Eliminated);
        }
    }
    if (session_.finished() || !anyPlayerHasTime(context)) {
        gameOver_ = true;
        handoff_ = false;
    }
}

void Mode2Gauntlet::render(GameModeContext& context)
{
    if (gameOver_) {
        const char* lines[] = {"Gauntlet Complete"};
        context.tft.showStatusScreen("GAME OVER", lines, 1);
        return;
    }
    const GauntletMachineInstance* machine = session_.currentMachine();
    if (!machine) {
        return;
    }
    if (handoff_) {
        char timeLine[32];
        if (confirmRemoval_) {
            std::snprintf(timeLine, sizeof(timeLine), "%u:%02u Will Be Removed",
                          machine->resolvedDefaultTimeSeconds / 60,
                          machine->resolvedDefaultTimeSeconds % 60);
            const char* lines[] = {timeLine, "This May End the Game", "Press Encoder to Confirm"};
            context.tft.showStatusScreen("Remove This Machine?", lines, 3);
            return;
        }
        const char* action = handoffSelection_ == 1 ? "Skip This Machine" :
                             handoffSelection_ == 2 ? "Remove This Machine" :
                             "Press White Button to Start";
        const char* lines[] = {
            machine->machineName,
            session_.isFinalMachine() ? "Final Machine" : action,
            session_.isFinalMachine() ? action : ""
        };
        context.tft.showStatusScreen("Next Game Is", lines,
                                     session_.isFinalMachine() ? 3 : 2);
        return;
    }

    char upperName[MachineRecord::NAME_CAPACITY];
    std::strncpy(upperName, machine->machineName, sizeof(upperName) - 1);
    upperName[sizeof(upperName) - 1] = '\0';
    for (char* p = upperName; *p; ++p) {
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }
    const PlayerId player =
        context.buttonAssignments.assignment(turns_.activeButton()).player;
    char ballLine[16];
    const uint8_t ball = machine->ballCount -
        context.players.roundsRemaining(player) + 1;
    std::snprintf(ballLine, sizeof(ballLine), "Ball %u", ball);
    const char* playerName = context.players.name(player);
    const char* lines[] = {playerName[0] ? playerName : "Player", ballLine};
    context.tft.showStatusScreen(upperName, lines, 2, ColorId::Black,
                                 context.players.assignedColor(player),
                                 context.players.assignedColor(player));
}

bool Mode2Gauntlet::anyPlayerHasTime(GameModeContext& context) const
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        if (context.timers.currentValueSeconds(timerIds_[i]) > 0) {
            return true;
        }
    }
    return false;
}

void Mode2Gauntlet::syncDisplays(GameModeContext& context)
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        context.numericDisplays.setValue(
            static_cast<DisplayId>(i),
            context.timers.currentValueSeconds(timerIds_[i]));
    }
}
