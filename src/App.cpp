#include "App.h"

#include <Arduino.h>
#include "modes/ModeRegistry.h"

App::App()
    : context_{
          players_,
          buttonAssignments_,
          displayAssignments_,
          numericDisplays_,
          tft_,
          buttonLights_,
          timers_
      }
{
}

void App::begin()
{
    Serial.begin(115200);
    delay(500);

    settings_.begin();
    gameStorage_.begin();

    buttonInput_.begin();
    encoderInput_.begin();
    numericDisplays_.begin();
    tft_.begin();
    buttonLights_.begin();

    timers_.begin();
    players_.begin();
    buttonAssignments_.begin();
    displayAssignments_.begin();

    gameModeManager_.begin(context_);
    ModeRegistry::registerAllModes(gameModeManager_);

    network_.begin(settings_.wifiSsid(), settings_.wifiPassword());
    power_.begin(context_);
    statusReporter_.begin(gameModeManager_, players_, displayAssignments_, timers_, network_, settings_, directorControl_);
    directorControl_.begin(gameModeManager_, context_, statusReporter_, power_);

    // Resume the last-used mode (if any) with its default player
    // count. There is no config UI yet to pick a mode/count
    // otherwise, so this is the only selection path until one exists.
    const uint8_t lastMode = settings_.lastSelectedMode();
    if (lastMode != 0 && gameModeManager_.selectMode(lastMode)) {
        gameModeManager_.setPlayerCount(gameModeManager_.activeMode()->defaultPlayerCount());
    }

    state_ = SystemState::Setup;
}

void App::update()
{
    buttonInput_.update();
    encoderInput_.update();

    ButtonEvent buttonEvent;
    while (buttonInput_.pollEvent(buttonEvent)) {
        handleButtonEvent(buttonEvent);
    }

    EncoderEvent encoderEvent;
    while (encoderInput_.pollEvent(encoderEvent)) {
        handleEncoderEvent(encoderEvent);
    }

    timers_.update();
    gameModeManager_.update();

    // Drain timer events. No concrete handler is wired yet beyond
    // NumericDisplayManager's own automatic negative-time flash --
    // see Mode1RoundRobin.h's note on the pending zero-crossing rule
    // change (stop-at-zero + buzzer) still to be discussed/implemented.
    TimerId crossedTimerId;
    while (timers_.pollZeroCrossingEvent(crossedTimerId)) {
        (void)crossedTimerId;
    }

    TimerId warnedTimerId;
    while (timers_.pollWarningEvent(warnedTimerId)) {
        (void)warnedTimerId;
    }

    numericDisplays_.update();
    tft_.update();
    buttonLights_.update();

    network_.update();
    directorControl_.update();

    power_.update();

    syncSystemState();
}

SystemState App::state() const
{
    return state_;
}

void App::handleButtonEvent(const ButtonEvent& event)
{
    power_.notifyActivity();

    if (directorControl_.localControlsLocked()) {
        return;
    }

    gameModeManager_.handleButtonEvent(event);
}

void App::handleEncoderEvent(const EncoderEvent& event)
{
    power_.notifyActivity();

    if (directorControl_.localControlsLocked()) {
        return;
    }

    gameModeManager_.handleEncoderEvent(event);
}

void App::syncSystemState()
{
    if (power_.state() == PowerState::Standby) {
        state_ = SystemState::Standby;
        return;
    }

    // Re-derived fresh every tick from GameModeManager's own state,
    // so it stays correct regardless of whether a pause/start was
    // triggered locally or by a remote director command.
    if (gameModeManager_.isPaused()) {
        state_ = SystemState::Paused;
    } else if (gameModeManager_.isGameStarted()) {
        state_ = SystemState::GameRunning;
    } else {
        state_ = SystemState::Setup;
    }
}
