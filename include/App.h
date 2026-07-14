#pragma once

#include "SystemTypes.h"
#include "input/ButtonInput.h"
#include "input/EncoderInput.h"
#include "output/NumericDisplayManager.h"
#include "output/TftDisplayManager.h"
#include "output/ButtonLightManager.h"
#include "game/TimerManager.h"
#include "game/PlayerManager.h"
#include "game/ButtonAssignmentManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/GameModeManager.h"
#include "storage/SettingsStorage.h"
#include "storage/GameStorage.h"
#include "network/NetworkManager.h"
#include "network/DirectorControl.h"
#include "network/StatusReporter.h"
#include "power/PowerManager.h"

// Central coordinator. Owns every subsystem, handles startup order,
// calls each subsystem's update() every loop, routes input events to
// the active game mode, and tracks overall SystemState. Contains no
// subsystem-internal logic itself -- that lives in the subsystems.
//
// Error state is never entered: no concrete error conditions have
// been defined anywhere yet, so nothing invents what "error" means
// here. Standby/GameRunning/Paused are driven by polling
// PowerManager/GameModeManager each tick rather than App deciding
// those transitions itself, so they stay correct regardless of
// whether the trigger was local input or a remote director command.
class App {
public:
    App();

    void begin();
    void update();

    SystemState state() const;

private:
    ButtonInput buttonInput_;
    EncoderInput encoderInput_;
    NumericDisplayManager numericDisplays_;
    TftDisplayManager tft_;
    ButtonLightManager buttonLights_;

    TimerManager timers_;
    PlayerManager players_;
    ButtonAssignmentManager buttonAssignments_;
    DisplayAssignmentManager displayAssignments_;
    GameModeManager gameModeManager_;

    SettingsStorage settings_;
    GameStorage gameStorage_;

    NetworkManager network_;
    StatusReporter statusReporter_;
    DirectorControl directorControl_;

    PowerManager power_;

    // Declared last so all the above are already constructed when
    // this binds its references to them.
    GameModeContext context_;

    SystemState state_ = SystemState::Startup;

    void handleButtonEvent(const ButtonEvent& event);
    void handleEncoderEvent(const EncoderEvent& event);
    void syncSystemState();
};
