#include "modes/Mode1RoundRobin.h"

#include <cstring>
#include "game/ButtonColors.h"
#include "game/TurnRotation.h"

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

void Mode1RoundRobin::setBallCount(uint8_t balls)
{
    if (balls < 1) {
        balls = 1;
    } else if (balls > MAX_BALL_COUNT) {
        balls = MAX_BALL_COUNT;
    }

    ballCount_ = balls;
}

uint8_t Mode1RoundRobin::ballCount() const
{
    return ballCount_;
}

void Mode1RoundRobin::setupAssignments(GameModeContext& context, uint8_t playerCount)
{
    playerCount_ = playerCount > MAX_MODE_PLAYERS ? MAX_MODE_PLAYERS : playerCount;
    roundStarted_ = false;
    gameOver_ = false;
    activeButton_ = ButtonId::Action;
    activeTimerRunning_ = false;
    turnEngine_.reset();
    suppressNextActionRelease_ = false;

    for (uint8_t i = 0; i < MAX_MODE_PLAYERS; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);

        // Whichever button the director has this player on -- may not
        // be the identity mapping (see ButtonColors.h). Its paired
        // display shares the same physical slot index as the button.
        const ButtonId button = context.players.buttonAssignment(player);
        const DisplayId display = static_cast<DisplayId>(static_cast<uint8_t>(button));

        if (i < playerCount_) {
            playerTimerIds_[i] = context.timers.createTimer(
                CountDirection::CountDown,
                secondsPerTurn_,
                /*allowBelowZero=*/false,
                /*stopAtZero=*/true,
                /*loopEnabled=*/false
            );
            context.timers.setAssociatedPlayer(playerTimerIds_[i], player);
            context.timers.setDisplayAssignment(playerTimerIds_[i], display);

            context.players.setDisplayAssignment(player, display);
            context.players.setAssignedColor(player, ButtonColors::colorForButton(button));
            context.players.setRoundsRemaining(player, ballCount_);

            context.buttonAssignments.assignToPlayer(button, player);
            context.displayAssignments.assignToSharedTimer(display, playerTimerIds_[i]);

            context.numericDisplays.setFormat(display, DisplayFormat::TimeMinutesSeconds);
            context.numericDisplays.setColon(display, true);
            context.numericDisplays.setVisible(display, true);
            context.numericDisplays.setFlashing(display, false);
            context.numericDisplays.setValue(display, secondsPerTurn_);

            context.players.setStatus(player, PlayerStatus::Waiting);
            context.buttonLights.setBaseState(button, LightPattern::Off);
        } else {
            playerTimerIds_[i] = TimerManager::INVALID_TIMER;
            context.players.setStatus(player, PlayerStatus::Inactive);
            context.buttonAssignments.clearAssignment(button);
            context.displayAssignments.clearAssignment(display);
            context.numericDisplays.setVisible(display, false);
            context.buttonLights.setBaseState(button, LightPattern::Off);
        }
    }

    context.buttonAssignments.assignToModeAction(ButtonId::Action, ACTION_START_ROUND);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
}

bool Mode1RoundRobin::restoreState(GameModeContext& context)
{
    const SavedRoundState& saved = context.gameStorage.loadRoundState();
    if (!saved.valid || saved.modeId != MODE_ID || saved.playerCount == 0) {
        return false;
    }

    secondsPerTurn_ = saved.secondsPerTurn;
    ballCount_ = saved.ballCount;
    setupAssignments(context, saved.playerCount); // fresh timers/displays; restored below

    roundStarted_ = saved.roundStarted;
    gameOver_ = saved.gameOver;
    activeButton_ = static_cast<ButtonId>(saved.activeButtonId);
    activeTimerRunning_ = saved.activeTimerRunning;
    manuallyPaused_ = saved.manuallyPaused;
    turnEngine_.restore(activeButton_, roundStarted_, activeTimerRunning_,
                        manuallyPaused_, gameOver_);

    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        const DisplayId display = context.players.displayAssignment(player);
        const PlayerStatus status = static_cast<PlayerStatus>(saved.playerStatus[i]);

        context.timers.reset(playerTimerIds_[i], saved.playerRemainingSeconds[i]);
        context.players.setRoundsRemaining(player, saved.playerRoundsRemaining[i]);
        context.players.setStatus(player, status);
        context.numericDisplays.setValue(display, saved.playerRemainingSeconds[i]);

        if (status == PlayerStatus::Eliminated || status == PlayerStatus::Finished) {
            context.numericDisplays.setFlashing(display, true, GAME_OVER_FLASH_INTERVAL_MS);
        }
    }

    if (roundStarted_ && !gameOver_) {
        const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;

        context.players.setStatus(activePlayer, PlayerStatus::Active);

        if (manuallyPaused_) {
            // Timer was left stopped by context.timers.reset() above --
            // do NOT start it, a reboot must not silently un-pause.
            context.buttonLights.setBaseState(activeButton_, LightPattern::Off);
            context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
        } else {
            context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Off);

            if (activeTimerRunning_) {
                context.buttonLights.setBaseState(activeButton_, LightPattern::Solid);
                context.timers.start(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
            } else {
                context.buttonLights.setBaseState(activeButton_, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
            }
        }
    }
    // else: setupAssignments() already left everything in the correct
    // "press Action to begin" state, or the game had already ended.

    return true;
}

void Mode1RoundRobin::update(GameModeContext& context)
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        const DisplayId display = context.players.displayAssignment(player);
        const long value = context.timers.currentValueSeconds(playerTimerIds_[i]);
        context.numericDisplays.setValue(display, value);
    }

    if (roundStarted_ && !gameOver_ && activeTimerRunning_) {
        const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;
        const TimerId activeTimer = playerTimerIds_[static_cast<uint8_t>(activePlayer)];

        TimerId crossedTimer;
        while (context.timers.pollZeroCrossingEvent(crossedTimer)) {
            if (crossedTimer == activeTimer) {
                handleTurnEnd(context, /*timedOut=*/true);
                break; // handleTurnEnd() may have changed which timer is active
            }
        }
    }

    // After the above, in case handleTurnEnd() just changed whose turn
    // it is or ended the game.
    renderGameStatus(context);
}

void Mode1RoundRobin::onPause(GameModeContext& context)
{
    if (roundStarted_ && !gameOver_ && activeTimerRunning_) {
        const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;
        context.timers.stop(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
        checkpointRoundState(context); // capture the exact remaining time at the moment of pausing
    }
}

void Mode1RoundRobin::onResume(GameModeContext& context)
{
    // manuallyPaused_ (tap-Action pause, see togglePause()) takes
    // precedence over GameModeManager's own resume -- e.g. opening and
    // closing DirectorMenu while manually paused must not silently
    // restart the clock out from under a pause the player explicitly
    // asked for; only another Action tap should do that.
    if (roundStarted_ && !gameOver_ && activeTimerRunning_ && !manuallyPaused_) {
        const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;
        context.timers.start(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
    }
}

bool Mode1RoundRobin::setModeOption(const char* key, long value)
{
    if (strcmp(key, "secondsPerTurn") == 0) {
        setSecondsPerTurn(value);
        return true;
    }

    if (strcmp(key, "ballCount") == 0) {
        setBallCount(static_cast<uint8_t>(value));
        return true;
    }

    return false;
}

long Mode1RoundRobin::modeOption(const char* key) const
{
    if (strcmp(key, "secondsPerTurn") == 0) {
        return secondsPerTurn_;
    }

    if (strcmp(key, "ballCount") == 0) {
        return ballCount_;
    }

    return 0;
}

bool Mode1RoundRobin::setPlayerOption(GameModeContext& context, PlayerId player, const char* key, long value)
{
    if (static_cast<uint8_t>(player) >= playerCount_) {
        return false;
    }

    if (strcmp(key, "roundsRemaining") == 0) {
        const long clamped = value < 0 ? 0 : (value > static_cast<long>(ballCount_) ? static_cast<long>(ballCount_) : value);
        context.players.setRoundsRemaining(player, static_cast<uint8_t>(clamped));
        tryRevive(context, player);
        checkpointRoundState(context);
        return true;
    }

    if (strcmp(key, "timerSeconds") == 0) {
        const long clamped = value < 0 ? 0 : (value > MAX_SECONDS_PER_TURN ? MAX_SECONDS_PER_TURN : value);
        const TimerId timerId = playerTimerIds_[static_cast<uint8_t>(player)];
        context.timers.reset(timerId, clamped);
        context.numericDisplays.setValue(context.players.displayAssignment(player), clamped);
        tryRevive(context, player);
        checkpointRoundState(context);
        return true;
    }

    return false;
}

long Mode1RoundRobin::playerOption(GameModeContext& context, PlayerId player, const char* key) const
{
    if (static_cast<uint8_t>(player) >= playerCount_) {
        return 0;
    }

    if (strcmp(key, "roundsRemaining") == 0) {
        return context.players.roundsRemaining(player);
    }

    if (strcmp(key, "timerSeconds") == 0) {
        return context.timers.currentValueSeconds(playerTimerIds_[static_cast<uint8_t>(player)]);
    }

    return 0;
}

bool Mode1RoundRobin::setLiveModeOption(GameModeContext& context, const char* key, long value)
{
    if (strcmp(key, "ballCount") != 0) {
        return false;
    }

    const long clamped = value < 1 ? 1 : (value > MAX_BALL_COUNT ? MAX_BALL_COUNT : value);
    const long delta = clamped - static_cast<long>(ballCount_);
    ballCount_ = static_cast<uint8_t>(clamped);

    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        long updated = static_cast<long>(context.players.roundsRemaining(player)) + delta;
        updated = updated < 0 ? 0 : (updated > clamped ? clamped : updated);
        context.players.setRoundsRemaining(player, static_cast<uint8_t>(updated));
        tryRevive(context, player);
    }

    checkpointRoundState(context);
    return true;
}

void Mode1RoundRobin::tryRevive(GameModeContext& context, PlayerId player)
{
    const PlayerStatus status = context.players.status(player);
    if (status != PlayerStatus::Eliminated && status != PlayerStatus::Finished) {
        return; // not eligible -- already in rotation, or this slot isn't in the game
    }

    const uint8_t rounds = context.players.roundsRemaining(player);
    const long seconds = context.timers.currentValueSeconds(playerTimerIds_[static_cast<uint8_t>(player)]);
    if (rounds == 0 || seconds <= 0) {
        return; // still missing one of the two requirements to actually play a turn
    }

    const DisplayId display = context.players.displayAssignment(player);
    context.players.setStatus(player, PlayerStatus::Waiting);
    context.numericDisplays.setFlashing(display, false); // clears the Eliminated/Finished "game over" flash

    if (gameOver_) {
        ButtonId nextButton;
        if (TurnRotation::nextButton(context.buttonAssignments, context.players, activeButton_, nextButton)) {
            gameOver_ = false;
            activeButton_ = nextButton;
            activeTimerRunning_ = false;
            context.buttonLights.setBaseState(nextButton, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
        }
    }
}

bool Mode1RoundRobin::isRoundOver() const
{
    return gameOver_;
}

bool Mode1RoundRobin::allowsRemoteCommand(uint8_t commandId) const
{
    (void)commandId;
    return true;
}

void Mode1RoundRobin::onStop(GameModeContext& context)
{
    if (roundStarted_ && !gameOver_ && activeTimerRunning_) {
        const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;
        context.timers.stop(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
    }
}

void Mode1RoundRobin::onReset(GameModeContext& context)
{
    resetRoundState(context);
}

bool Mode1RoundRobin::onButtonEvent(GameModeContext& context, const ButtonEvent& event)
{
    if (event.button == ButtonId::Action) {
        // Pause-toggle (and, once a game has ended, starting a fresh
        // one) fires on Released, not Pressed -- so a press-down that
        // turns into App::updateDirectorMenuHold()'s 5-second
        // hold-to-open-DirectorMenu gesture never races with either:
        // once that gesture opens DirectorMenu (still mid-hold),
        // App::handleButtonEvent() intercepts and swallows the
        // eventual Released for this same physical press before it
        // ever reaches here, so neither fires from it. A genuine
        // short tap still reaches Released while no menu is open yet,
        // and acts normally.
        if (event.type == ButtonEventType::Pressed) {
            // Invalidate any stale flag from an earlier press whose
            // own Released was swallowed by DirectorMenu opening --
            // otherwise it would incorrectly suppress this new,
            // unrelated press's eventual release.
            suppressNextActionRelease_ = false;

            if (!roundStarted_) {
                startRound(context);
                // This same press already started the round -- its
                // own Released must not also immediately pause it.
                suppressNextActionRelease_ = true;
                return true;
            }
            return false;
        }

        if (event.type == ButtonEventType::Released) {
            const bool suppress = suppressNextActionRelease_;
            suppressNextActionRelease_ = false;

            if (suppress) {
                return false;
            }

            if (gameOver_) {
                // "Press White Button to Start a New Game" (see the
                // GAME OVER screen, renderGameStatus()) -- reset then
                // start fresh in one gesture, same net effect as
                // DirectorMenu's "Reset Round" followed by a normal
                // Action press.
                resetRoundState(context);
                startRound(context);
                return true;
            }

            if (roundStarted_) {
                togglePause(context);
            }
        }
        return false; // ShortPress/LongPress carry no meaning for Action here
    }

    if (event.type != ButtonEventType::Pressed) {
        return false;
    }

    const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;
    const TimerId activeTimer = playerTimerIds_[static_cast<uint8_t>(activePlayer)];
    turnEngine_.restore(activeButton_, roundStarted_, activeTimerRunning_,
                        manuallyPaused_, gameOver_);
    const auto pressResult = turnEngine_.handlePlayerPress(event.button);

    if (pressResult == RoundRobinTurnEngine::PlayerPressResult::StartTimer) {
        // First press on a flashing (not-yet-started) button just
        // starts that player's own clock -- it doesn't end the turn.
        context.timers.start(activeTimer);
        context.buttonLights.setBaseState(activeButton_, LightPattern::Solid);
        activeTimerRunning_ = true;
        checkpointRoundState(context);
        return false;
    }
    if (pressResult == RoundRobinTurnEngine::PlayerPressResult::EndBall) {
        handleTurnEnd(context, /*timedOut=*/false);
    }
    return false;
}

void Mode1RoundRobin::startRound(GameModeContext& context)
{
    if (playerCount_ == 0) {
        return;
    }

    turnEngine_.reset();
    if (!turnEngine_.start(context.buttonAssignments, context.players)) {
        return;
    }
    const ButtonId firstButton = turnEngine_.activeButton();

    roundStarted_ = true;
    gameOver_ = false;
    activeButton_ = firstButton;
    activeTimerRunning_ = true;

    const PlayerId firstPlayer = context.buttonAssignments.assignment(firstButton).player;
    context.timers.start(playerTimerIds_[static_cast<uint8_t>(firstPlayer)]);
    context.players.setStatus(firstPlayer, PlayerStatus::Active);
    context.buttonLights.setBaseState(firstButton, LightPattern::Solid);
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Off);

    checkpointRoundState(context);
}

void Mode1RoundRobin::togglePause(GameModeContext& context)
{
    const PlayerId activePlayer = context.buttonAssignments.assignment(activeButton_).player;

    if (manuallyPaused_) {
        manuallyPaused_ = false;
        context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Off);

        if (activeTimerRunning_) {
            context.timers.start(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
            context.buttonLights.setBaseState(activeButton_, LightPattern::Solid);
        } else {
            // Was mid-handoff (nothing running) when paused -- resume
            // the flash-until-pressed state, not a running clock.
            context.buttonLights.setBaseState(activeButton_, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
        }
    } else {
        if (activeTimerRunning_) {
            context.timers.stop(playerTimerIds_[static_cast<uint8_t>(activePlayer)]);
        }
        manuallyPaused_ = true;
        context.buttonLights.setBaseState(activeButton_, LightPattern::Off);
        context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);
    }

    checkpointRoundState(context);
}

void Mode1RoundRobin::handleTurnEnd(GameModeContext& context, bool timedOut)
{
    const ButtonId finishedButton = activeButton_;
    const PlayerId finishedPlayer = context.buttonAssignments.assignment(finishedButton).player;
    const TimerId finishedTimer = playerTimerIds_[static_cast<uint8_t>(finishedPlayer)];
    const DisplayId finishedDisplay = context.players.displayAssignment(finishedPlayer);

    context.timers.stop(finishedTimer);

    if (timedOut) {
        // Timing out ends this player's WHOLE game immediately,
        // regardless of rounds remaining -- not just this one round.
        context.players.setStatus(finishedPlayer, PlayerStatus::Eliminated);
        context.numericDisplays.setFlashing(finishedDisplay, true, GAME_OVER_FLASH_INTERVAL_MS);
        context.buttonLights.setBaseState(finishedButton, LightPattern::Off);
        context.buzzer.buzz();
    } else {
        const uint8_t roundsLeft = context.players.roundsRemaining(finishedPlayer) - 1;
        context.players.setRoundsRemaining(finishedPlayer, roundsLeft);

        if (roundsLeft == 0) {
            // Completed their last round on their own terms -- their
            // game is over even though other players may still be
            // going, so flash the frozen time left (same signal
            // Eliminated already gives) so it doesn't read as just
            // "waiting for their next turn."
            context.players.setStatus(finishedPlayer, PlayerStatus::Finished);
            context.numericDisplays.setFlashing(finishedDisplay, true, GAME_OVER_FLASH_INTERVAL_MS);
            context.buttonLights.setBaseState(finishedButton, LightPattern::Off);
        } else {
            // Chess-clock semantics: secondsPerTurn_ is this player's
            // ONE time budget for the whole game, not a per-ball
            // allowance -- pressing the button to pass PAUSES their
            // clock (already stopped above, value left untouched), it
            // does not reset it. It resumes counting down from that
            // same remaining value next time TurnRotation comes back
            // to them (Mode1RoundRobin::onButtonEvent()'s first-press
            // "start the clock" branch calls TimerManager::start(),
            // which resumes rather than restarts -- see TimerManager.h).
            context.players.setStatus(finishedPlayer, PlayerStatus::Waiting);
            context.buttonLights.setBaseState(finishedButton, LightPattern::Off);
        }
    }

    turnEngine_.restore(finishedButton, true, false, manuallyPaused_, false);
    if (!turnEngine_.advance(context.buttonAssignments, context.players)) {
        gameOver_ = true;
        context.buzzer.littleTune();
        checkpointRoundState(context);
        return;
    }

    const ButtonId nextButton = turnEngine_.activeButton();
    activeButton_ = nextButton;
    activeTimerRunning_ = false;
    context.buttonLights.setBaseState(nextButton, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);

    checkpointRoundState(context);
}

void Mode1RoundRobin::resetRoundState(GameModeContext& context)
{
    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        const DisplayId display = context.players.displayAssignment(player);
        const ButtonId button = context.players.buttonAssignment(player);

        context.timers.stop(playerTimerIds_[i]);
        context.timers.reset(playerTimerIds_[i], secondsPerTurn_);
        context.players.setStatus(player, PlayerStatus::Waiting);
        context.players.setRoundsRemaining(player, ballCount_);
        context.buttonLights.setBaseState(button, LightPattern::Off);
        context.numericDisplays.setFlashing(display, false);
        context.numericDisplays.setValue(display, secondsPerTurn_);
    }

    roundStarted_ = false;
    gameOver_ = false;
    activeButton_ = ButtonId::Action;
    activeTimerRunning_ = false;
    manuallyPaused_ = false;
    suppressNextActionRelease_ = false;
    turnEngine_.reset();
    context.buttonLights.setBaseState(ButtonId::Action, LightPattern::Blink, HANDOFF_FLASH_INTERVAL_MS);

    context.gameStorage.clearRoundState();
}

void Mode1RoundRobin::checkpointRoundState(GameModeContext& context)
{
    SavedRoundState state;
    state.valid = true;
    state.modeId = MODE_ID;
    state.playerCount = playerCount_;
    state.activeButtonId = static_cast<uint8_t>(activeButton_);
    state.roundStarted = roundStarted_;
    state.activeTimerRunning = activeTimerRunning_;
    state.gameOver = gameOver_;
    state.manuallyPaused = manuallyPaused_;
    state.secondsPerTurn = secondsPerTurn_;
    state.ballCount = ballCount_;

    for (uint8_t i = 0; i < playerCount_; ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        state.playerRemainingSeconds[i] = context.timers.currentValueSeconds(playerTimerIds_[i]);
        state.playerRoundsRemaining[i] = context.players.roundsRemaining(player);
        state.playerStatus[i] = static_cast<uint8_t>(context.players.status(player));
    }

    context.gameStorage.saveRoundState(state);
}

void Mode1RoundRobin::renderGameStatus(GameModeContext& context)
{
    if (gameOver_) {
        const char* lines[] = {"Press White Button", "to Start a New Game", "or Hold for Main Menu"};
        context.tft.showStatusScreen("GAME OVER", lines, 3, ColorId::Black, ColorId::Red, ColorId::White);
        return;
    }

    if (manuallyPaused_) {
        const char* lines[] = {"tap white button", "to resume"};
        context.tft.showStatusScreen("PAUSED", lines, 2, ColorId::Black, ColorId::Yellow, ColorId::White);
        return;
    }

    // Before the Action press that starts the first turn, preview
    // whoever's up first (TurnRotation::firstButton -- the Red-wired
    // player, see ButtonColors.h) rather than showing nothing.
    ButtonId currentButton = activeButton_;
    if (!roundStarted_ && !TurnRotation::firstButton(context.buttonAssignments, context.players, currentButton)) {
        const char* lines[] = {"no players ready"};
        context.tft.showStatusScreen("PINBALL TIMER", lines, 1, ColorId::Black, ColorId::White, ColorId::White);
        return;
    }

    const PlayerId player = context.buttonAssignments.assignment(currentButton).player;
    const char* name = context.players.name(player);

    // roundsRemaining counts DOWN from ballCount_ to 0 (see
    // PlayerManager.h) -- ball number counts UP from 1 for display.
    const uint8_t ballNumber = ballCount_ - context.players.roundsRemaining(player) + 1;
    const ColorId playerColor = context.players.assignedColor(player);

    context.tft.showBallScreen(
        name[0] != '\0' ? name : "PLAYER",
        ballNumber,
        ColorId::Black,
        playerColor,
        playerColor,
        playerColor
    );
}
