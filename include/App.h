#pragma once

#include "SystemTypes.h"
#include "input/ButtonInput.h"
#include "input/EncoderInput.h"
#include "output/NumericDisplayManager.h"
#include "output/TftDisplayManager.h"
#include "output/ButtonLightManager.h"
#include "output/BuzzerManager.h"
#include "game/TimerManager.h"
#include "game/PlayerManager.h"
#include "game/ButtonAssignmentManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/GameModeManager.h"
#include "storage/SettingsStorage.h"
#include "storage/GameStorage.h"
#include "storage/MachineDatabase.h"
#include "network/NetworkManager.h"
#include "network/DirectorControl.h"
#include "network/StatusReporter.h"
#include "network/OtaManager.h"
#include "network/WifiPortal.h"
#include "network/DirectorDashboard.h"
#include "power/PowerManager.h"
#include "ui/DirectorMenu.h"
#include "ui/WifiSetupMenu.h"
#include "ui/BootMenu.h"

// Central coordinator. Owns every subsystem, handles startup order,
// calls each subsystem's update() every loop, routes input events to
// the active game mode, and tracks overall SystemState. Contains no
// subsystem-internal logic itself -- that lives in the subsystems.
//
// Error state's only current trigger is PowerManager reporting
// PowerState::Critical (battery at/below 5%, see PowerManager.h) --
// no other error condition is defined anywhere yet, so nothing else
// invents what "error" means here. Standby/GameRunning/Paused/Error
// are all driven by polling PowerManager/GameModeManager each tick
// rather than App deciding those transitions itself, so they stay
// correct regardless of whether the trigger was local input or a
// remote director command.
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
    BuzzerManager buzzer_;

    TimerManager timers_;
    PlayerManager players_;
    ButtonAssignmentManager buttonAssignments_;
    DisplayAssignmentManager displayAssignments_;
    GameModeManager gameModeManager_;

    SettingsStorage settings_;
    GameStorage gameStorage_;
    MachineDatabase machineDatabase_;

    NetworkManager network_;
    DirectorControl directorControl_;
    StatusReporter statusReporter_;
    OtaManager ota_;
    WifiPortal wifiPortal_;
    DirectorDashboard directorDashboard_;
    PowerManager power_;
    DirectorMenu directorMenu_;
    WifiSetupMenu wifiSetupMenu_;
    BootMenu bootMenu_;

    // Declared last so all the above are already constructed when
    // this binds its references to them.
    GameModeContext context_;

    SystemState state_ = SystemState::Startup;

    // Action-button hold-to-open-menu detection. Separate from
    // ButtonInput's generic 600ms LongPress event (see
    // ButtonInput.h) -- this is a much longer, App-level gesture
    // specific to one button, not a per-button input primitive.
    static constexpr unsigned long DIRECTOR_MENU_HOLD_MS = 5000;
    bool directorHoldFired_ = false;

    // Input feedback via the buzzer -- fires for any button/encoder
    // event regardless of what's currently routing it (menu or game),
    // same as power_.notifyActivity() just above each handler's own
    // routing logic. See BuzzerManager for the actual tone/duration
    // of each named preset (beep/click/tone/boop).

    // Edge-detects the Disconnected/Connecting -> Connected
    // transition so the assigned IP is logged once, not every tick.
    bool wasConnected_ = false;

    // WifiSetupMenu can close itself autonomously (connect success),
    // not just from an input event -- this edge-detects that so
    // update() can resume the game exactly once when it happens.
    bool wifiSetupWasOpen_ = false;

    // Same idea for WifiPortal, which also needs App to (re)issue
    // NetworkManager::begin() once it closes, using whatever
    // credentials ended up saved (new ones on success, unchanged ones
    // on cancel/timeout).
    bool wifiPortalWasOpen_ = false;

    // Set when WifiSetupMenu/WifiPortal was opened from BootMenu
    // (rather than from DirectorMenu mid-game) -- read by the two
    // edge-detect blocks above to decide whether finishing WiFi setup
    // should reopen BootMenu or resume an in-progress game.
    bool wifiHandoffFromBootMenu_ = false;

    void handleButtonEvent(const ButtonEvent& event);
    void handleEncoderEvent(const EncoderEvent& event);
    void syncSystemState();
    void updateDirectorMenuHold();
};
