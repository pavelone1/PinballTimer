#include "App.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include "FeatureFlags.h"
#include "modes/ModeRegistry.h"
#include "Secrets.h"

App::App()
    : context_{
          players_,
          buttonAssignments_,
          displayAssignments_,
          numericDisplays_,
          tft_,
          buttonLights_,
          buzzer_,
          timers_,
          gameStorage_
      }
{
}

void App::begin()
{
    Serial.begin(115200);
    delay(500);

    settings_.begin();
    gameStorage_.begin();
    machineDatabase_.begin();

    buttonInput_.begin();
    encoderInput_.begin();
    numericDisplays_.begin();
    tft_.begin();
    buttonLights_.begin();
    buzzer_.begin();

    timers_.begin();
    players_.begin();
    buttonAssignments_.begin();
    displayAssignments_.begin();

    // Player names/button assignments live in GameStorage/NVS (persist
    // across reboots); PlayerManager itself is RAM-only and defaults to
    // blank names + identity button assignment on every begin(), so
    // this needs to run every boot, not just once ever. BootMenu's
    // Player Info screen and DirectorDashboard's /game-setup page both
    // write through to both when edited.
    for (uint8_t i = 0; i < static_cast<uint8_t>(PlayerId::Count); ++i) {
        const PlayerId player = static_cast<PlayerId>(i);
        players_.setName(player, gameStorage_.playerName(player));
        players_.setButtonAssignment(player, gameStorage_.playerButtonAssignment(player));
    }

    gameModeManager_.begin(context_);
    ModeRegistry::registerAllModes(gameModeManager_);

    power_.begin(context_);

    // Normal product behavior is boot-off/explicit-on. During the temporary
    // bench-powered pre-alpha phase, FeatureFlags forces the radio on and
    // disables modem sleep so the web/debug interface stays reachable.
    if (kWifiFeatureEnabled) {
        // First-boot-only bootstrap: Secrets.h (gitignored, see
        // Secrets.h.example) supplies the one fixed venue network this
        // device connects to, if set. Once SettingsStorage has a saved
        // SSID this is skipped, so editing Secrets.h after a device's
        // first boot has no effect -- that's intentional, not a bug: NVS,
        // not the compiled-in constant, is the source of truth from then
        // on.
        if (settings_.wifiSsid()[0] == '\0') {
            settings_.setWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
        }

        // Old firmware offered a simultaneous AP+STA mode. That is the
        // exact radio mode implicated by the REV2 coredump, so migrate
        // it once and keep the radio in one exclusive mode thereafter.
        if (settings_.wifiOperatingMode() == WifiOperatingMode::Both) {
            settings_.setWifiOperatingMode(WifiOperatingMode::StationOnly);
        }
        if (kPreAlphaWifiAlwaysOn) {
            settings_.setWifiKeepAlive(true);
            enableWifi();
            lastPreAlphaWifiReminderMs_ = millis();
            Serial.println(
                "[PRE-ALPHA] WiFi is forced ON with modem sleep disabled");
        } else {
            WiFi.mode(WIFI_OFF);
            wifiPowered_ = false;
        }
    }

    // BootMenu is the boot screen (see App::update()'s input routing)
    // -- picking "Start Game"/"Resume Game"/a mode from its Select
    // Game Mode submenu is now what selects/initializes/restores the
    // active GameMode; nothing here does that automatically anymore.
    bootMenu_.begin(gameModeManager_, settings_, gameStorage_, players_, tft_, wifiPortal_, machineDatabase_.catalog());
    bootMenu_.open();

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

    updateDirectorMenuHold();
    wifiSetupMenu_.update();

    // WifiSetupMenu can close itself (connect succeeded) without any
    // input event to hang the resume off of -- catch that here. Which
    // menu opened it decides what "done" means: resume an in-progress
    // game (DirectorMenu) or come back to the boot screen (BootMenu).
    const bool wifiSetupIsOpen = wifiSetupMenu_.isOpen();
    if (!wifiSetupIsOpen && wifiSetupWasOpen_) {
        if (wifiHandoffFromBootMenu_) {
            wifiHandoffFromBootMenu_ = false;
            bootMenu_.openWifiSubmenu();
        } else if (wifiHandoffFromDirectorMenu_) {
            wifiHandoffFromDirectorMenu_ = false;
            directorMenu_.open(gameModeManager_, directorControl_, players_, tft_);
        } else {
            gameModeManager_.notifyResume();
        }
    }
    wifiSetupWasOpen_ = wifiSetupIsOpen;

    wifiPortal_.update();

    // Same idea for WifiPortal. Only re-issues NetworkManager::begin()
    // if the operator didn't just choose adhoc-only (AccessPointOnly)
    // -- otherwise this would silently undo a "Use Hotspot Only"
    // choice the moment its confirmation screen is dismissed.
    const bool wifiPortalIsOpen = wifiPortal_.isOpen();
    if (!wifiPortalIsOpen && wifiPortalWasOpen_) {
        if (settings_.wifiOperatingMode() != WifiOperatingMode::AccessPointOnly) {
            network_.begin(settings_.wifiSsid(), settings_.wifiPassword());
        }

        if (wifiHandoffFromBootMenu_) {
            wifiHandoffFromBootMenu_ = false;
            bootMenu_.openWifiSubmenu();
        } else if (wifiHandoffFromDirectorMenu_) {
            wifiHandoffFromDirectorMenu_ = false;
            directorMenu_.open(gameModeManager_, directorControl_, players_, tft_);
        } else {
            gameModeManager_.notifyResume();
        }
    }
    wifiPortalWasOpen_ = wifiPortalIsOpen;

    timers_.update();

    // Skip the active mode's own update() -- which redraws the TFT via
    // Mode1RoundRobin::renderGameStatus() -- while any menu currently
    // owns the screen, otherwise it clobbers whatever the menu just
    // drew on the very next tick (e.g. DirectorMenu flashing open then
    // immediately reverting to "GAME OVER"/the ball screen underneath
    // it). Every path that opens a menu mid-game already calls
    // notifyPause() first (or no round has started yet), so the active
    // timer is already stopped either way -- nothing meaningful is
    // lost by skipping this while a menu is up.
    const bool anyMenuOwnsScreen = bootMenu_.isOpen() || directorMenu_.isOpen() || wifiSetupIsOpen || wifiPortalIsOpen;
    if (!anyMenuOwnsScreen) {
        gameModeManager_.update();
    }

    // Drain any zero-crossing events the active mode didn't consume
    // itself. Mode1RoundRobin::update() (called just above via
    // gameModeManager_.update()) already polls and acts on its own
    // active-player timer's crossing there -- this is just a safety
    // net so the shared queue can't grow unbounded if some future mode
    // creates timers without draining its own events.
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
    buzzer_.update();

    // WiFi/HTTP subsystem disabled -- see include/FeatureFlags.h and
    // App::begin(). None of these were begun, so nothing here to poll.
    if (kWifiFeatureEnabled && wifiPowered_) {
        // A menu can change AccessPointOnly back to StationOnly while
        // the AP is idle. NetworkManager was intentionally not begun
        // during an AP-only boot, so start it here on that transition.
        if (!wifiPortal_.isApActive() &&
            !wifiSetupIsOpen &&
            settings_.wifiOperatingMode() == WifiOperatingMode::StationOnly &&
            WiFi.getMode() != WIFI_MODE_STA) {
            network_.begin(settings_.wifiSsid(), settings_.wifiPassword());
        }

        // Skipped while WifiPortal owns the radio -- its own raw WiFi.*
        // calls would otherwise race NetworkManager's independent
        // reconnect-retry timer for the same interface (see WifiPortal.h).
        // Also skipped entirely in AccessPointOnly mode -- letting
        // NetworkManager keep retrying STA in the background would fight
        // the operator's explicit "adhoc only" choice.
        if (!wifiPortal_.isApActive() && settings_.wifiOperatingMode() != WifiOperatingMode::AccessPointOnly) {
            network_.update();

            const bool isConnected = network_.isConnected();
            if (isConnected && !wasConnected_) {
                Serial.print("[WiFi] Connected, IP: ");
                Serial.println(network_.localIP());
                // ArduinoOTA/mDNS allocates network resources of its
                // own. Bring it up once, only after STA has an address.
                if (!otaStarted_) {
                    ota_.begin(settings_.deviceName(), OTA_PASSWORD, tft_);
                    otaStarted_ = true;
                }
            } else if (!isConnected && wasConnected_) {
                Serial.println("[WiFi] Disconnected");
            }

            if (isConnected) {
                wifiDisconnectedSinceMs_ = 0;
            } else if (wifiDisconnectedSinceMs_ == 0) {
                wifiDisconnectedSinceMs_ = millis();
            }
            wasConnected_ = isConnected;
        }

        directorControl_.update();
        directorDashboard_.update();
        if (otaStarted_ && network_.isConnected()) {
            ota_.update();
        }

        // A blank/unconfigured unit advertises immediately. A unit
        // that loses its saved network gets a grace period for normal
        // reconnects, then becomes locally reachable again. The AP is
        // background-only and does not take over the game screen.
        const bool missingCredentials = settings_.wifiSsid()[0] == '\0';
        const bool connectionTimedOut =
            wifiDisconnectedSinceMs_ != 0 &&
            millis() - wifiDisconnectedSinceMs_ >= WIFI_FALLBACK_AP_DELAY_MS;
        if (!wifiPortal_.isApActive() &&
            !wifiSetupIsOpen &&
            settings_.wifiOperatingMode() == WifiOperatingMode::StationOnly &&
            (missingCredentials || connectionTimedOut)) {
            Serial.println("[WiFi] Starting fallback setup hotspot");
            wifiPortal_.startFallback();
        }
    }

    if (kPreAlphaWifiAlwaysOn &&
        millis() - lastPreAlphaWifiReminderMs_ >= PRE_ALPHA_WIFI_REMINDER_MS) {
        lastPreAlphaWifiReminderMs_ = millis();
        Serial.println(
            "[PRE-ALPHA REMINDER] WiFi is still forced ON; disable "
            "kPreAlphaWifiAlwaysOn at alpha");
    }

    power_.update();

    // Drain battery events. buzzer_ is now wired for button/encoder
    // input feedback (see handleButtonEvent/handleEncoderEvent above)
    // but nothing calls it for battery events yet, and no low-battery
    // screen is wired to the TFT either, so for now this just logs,
    // and the same data is available live via StatusReporter's JSON
    // (batteryAvailable/batteryVoltage/
    // batteryPercent/powerState) for a director dashboard to react to.
    BatteryEventType batteryEvent;
    while (power_.pollBatteryEvent(batteryEvent)) {
        switch (batteryEvent) {
            case BatteryEventType::Warning20:
                Serial.println("[Battery] 20% remaining");
                break;
            case BatteryEventType::Warning10:
                Serial.println("[Battery] 10% remaining");
                break;
            case BatteryEventType::Critical5:
                Serial.println("[Battery] 5% remaining -- entering Critical state");
                break;
        }
    }

    syncSystemState();
}

void App::initializeNetworkServices()
{
    if (networkServicesInitialized_) {
        return;
    }

    statusReporter_.begin(gameModeManager_, players_, displayAssignments_, timers_,
        network_, settings_, directorControl_, power_, gameStorage_);
    directorControl_.begin(gameModeManager_, context_, statusReporter_, power_);
    wifiPortal_.begin(directorControl_.server(), settings_, tft_, WIFI_PORTAL_PASSWORD);
    directorDashboard_.begin(directorControl_.server(), machineDatabase_);
    networkServicesInitialized_ = true;
}

void App::enableWifi()
{
    if (!kWifiFeatureEnabled || wifiPowered_) {
        return;
    }

    network_.setKeepAlive(settings_.wifiKeepAlive());
    const bool useAp =
        settings_.wifiOperatingMode() == WifiOperatingMode::AccessPointOnly ||
        settings_.wifiSsid()[0] == '\0';

    // A netif must exist before httpd_start() opens its listening
    // socket. Start the selected exclusive radio role first.
    if (useAp) {
        WiFi.mode(WIFI_AP);
    } else {
        network_.begin(settings_.wifiSsid(), settings_.wifiPassword());
    }
    wifiPowered_ = true;
    initializeNetworkServices();

    if (useAp) {
        if (settings_.wifiOperatingMode() == WifiOperatingMode::AccessPointOnly) {
            wifiPortal_.applyStartupMode();
        } else {
            wifiPortal_.startFallback();
        }
    }
}

void App::disableWifi()
{
    if (kPreAlphaWifiAlwaysOn) {
        Serial.println(
            "[PRE-ALPHA] WiFi OFF ignored; radio is forced ON until alpha");
        enableWifi();
        settings_.setWifiKeepAlive(true);
        network_.setKeepAlive(true);
        return;
    }

    if (!wifiPowered_) {
        return;
    }

    network_.disconnect();
    if (networkServicesInitialized_) {
        wifiPortal_.shutdownRadio();
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
    WiFi.mode(WIFI_OFF);
    wifiPowered_ = false;
    wasConnected_ = false;
    wifiDisconnectedSinceMs_ = 0;
    Serial.println("[WiFi] Radio OFF");
}

SystemState App::state() const
{
    return state_;
}

void App::handleButtonEvent(const ButtonEvent& event)
{
    power_.notifyActivity();

    if (event.type == ButtonEventType::Pressed) {
        buzzer_.beep();
    }

    // Action press is a shortcut to cancel/close whichever on-device
    // menu is open, symmetric with the hold-to-open gesture. Every
    // other button is ignored -- whichever menu is open has exclusive
    // input focus while the game stays paused for it.
    if (wifiPortal_.isOpen()) {
        if (event.button == ButtonId::Action && event.type == ButtonEventType::Pressed) {
            wifiPortal_.close();
        }
        return;
    }

    if (wifiSetupMenu_.isOpen()) {
        if (event.button == ButtonId::Action && event.type == ButtonEventType::Pressed) {
            wifiSetupMenu_.close();
        }
        return;
    }

    if (directorMenu_.isOpen()) {
        if (event.button == ButtonId::Action && event.type == ButtonEventType::Pressed) {
            directorMenu_.close(tft_);
            gameModeManager_.notifyResume();
        }
        return;
    }

    // BootMenu has no Action-button role (no "cancel" target exists
    // at the boot screen -- see BootMenu.h) -- every button is
    // ignored here, only the encoder navigates it.
    if (bootMenu_.isOpen()) {
        return;
    }

    if (directorControl_.localControlsLocked()) {
        return;
    }

    gameModeManager_.handleButtonEvent(event);
}

void App::handleEncoderEvent(const EncoderEvent& event)
{
    power_.notifyActivity();

    if (event.type == EncoderEventType::RotatedClockwise ||
        event.type == EncoderEventType::RotatedCounterClockwise) {
        buzzer_.click();
    } else if (event.type == EncoderEventType::SwShortPress) {
        buzzer_.tone();
    } else if (event.type == EncoderEventType::SwLongPress) {
        buzzer_.boop();
    }

    // TEMP DIAGNOSTIC: unconditional, independent of any menu's own
    // logic -- proves whether SwShortPress/SwLongPress ever reach
    // here at all.
    if (event.type == EncoderEventType::SwShortPress) {
        static uint16_t shortPressCount = 0;
        shortPressCount++;
        char buf[24];
        snprintf(buf, sizeof(buf), "SHORT x%u   ", shortPressCount);
        tft_.drawCenteredText(buf, 300, 2, ColorId::Magenta);
    } else if (event.type == EncoderEventType::SwLongPress) {
        static uint16_t longPressCount = 0;
        longPressCount++;
        char buf[24];
        snprintf(buf, sizeof(buf), "LONG x%u   ", longPressCount);
        tft_.drawCenteredText(buf, 300, 2, ColorId::Orange);
    }

    if (wifiPortal_.isOpen()) {
        return; // phone-driven, not encoder-driven -- nothing to route
    }

    if (wifiSetupMenu_.isOpen()) {
        wifiSetupMenu_.handleEncoderEvent(event);
        return;
    }

    if (directorMenu_.isOpen()) {
        const MenuHandoff outcome = directorMenu_.handleEncoderEvent(event);

        if (outcome == MenuHandoff::OpenWifiSetup) {
            if (!wifiPowered_) {
                return;
            }
            directorMenu_.close(tft_); // UI cleanup only -- stays paused, hands off rather than resuming
            wifiHandoffFromDirectorMenu_ = true;
            wifiSetupMenu_.open(network_, settings_, tft_);
        } else if (outcome == MenuHandoff::OpenWifiPortal) {
            if (!wifiPowered_) {
                return;
            }
            directorMenu_.close(tft_); // UI cleanup only -- stays paused, hands off rather than resuming
            wifiHandoffFromDirectorMenu_ = true;
            wifiPortal_.open();
        } else if (outcome == MenuHandoff::Close) {
            directorMenu_.close(tft_);
            gameModeManager_.notifyResume();
        } else if (outcome == MenuHandoff::EndGame) {
            // Reset already happened inside DirectorMenu's "End Game"
            // item (notifyReset()) -- no notifyResume() here, since
            // there's no game left to resume; go straight to BootMenu.
            directorMenu_.close(tft_);
            bootMenu_.open();
        }

        return;
    }

    if (bootMenu_.isOpen()) {
        const MenuHandoff outcome = bootMenu_.handleEncoderEvent(event);

        if (outcome == MenuHandoff::OpenWifiSetup) {
            if (!wifiPowered_) {
                bootMenu_.openWifiSubmenu();
                return;
            }
            bootMenu_.close();
            wifiHandoffFromBootMenu_ = true;
            wifiSetupMenu_.open(network_, settings_, tft_);
        } else if (outcome == MenuHandoff::OpenWifiPortal) {
            if (!wifiPowered_) {
                bootMenu_.openWifiSubmenu();
                return;
            }
            bootMenu_.close();
            wifiHandoffFromBootMenu_ = true;
            wifiPortal_.open();
        } else if (outcome == MenuHandoff::RevertToAdhoc) {
            if (!wifiPowered_) {
                bootMenu_.openWifiSubmenu();
                return;
            }
            bootMenu_.close();
            wifiHandoffFromBootMenu_ = true;
            wifiPortal_.revertToAdhoc();
        } else if (outcome == MenuHandoff::ToggleWifiPower) {
            if (kPreAlphaWifiAlwaysOn) {
                enableWifi();
                settings_.setWifiKeepAlive(true);
                network_.setKeepAlive(true);
                Serial.println(
                    "[PRE-ALPHA] WiFi power control is locked ON");
            } else if (wifiPowered_) {
                disableWifi();
            } else {
                enableWifi();
            }
            bootMenu_.openWifiSubmenu();
        } else if (outcome == MenuHandoff::ForgetWifiNetwork) {
            if (wifiPowered_ && networkServicesInitialized_) {
                wifiPortal_.forgetNetwork();
            } else {
                settings_.clearWifiCredentials();
            }
            bootMenu_.openWifiSubmenu();
        } else if (outcome == MenuHandoff::ToggleWifiKeepAlive) {
            const bool enabled =
                kPreAlphaWifiAlwaysOn ? true : !settings_.wifiKeepAlive();
            settings_.setWifiKeepAlive(enabled);
            network_.setKeepAlive(enabled);
            bootMenu_.openWifiSubmenu();
        } else if (outcome == MenuHandoff::TogglePersistentHotspot) {
            if (wifiPowered_ && networkServicesInitialized_) {
                wifiPortal_.setPersistentHotspot(!wifiPortal_.persistentHotspot());
            }
            bootMenu_.openWifiSubmenu();
        } else if (outcome == MenuHandoff::Close) {
            // Start Game / Resume Game / Select Game Mode already did
            // their work synchronously inside BootMenu -- the active
            // mode is now selected/initialized (or restored), so
            // closing here is enough; normal game-input routing below
            // takes over on the next event.
            bootMenu_.close();
        }

        return;
    }

    if (directorControl_.localControlsLocked()) {
        return;
    }

    gameModeManager_.handleEncoderEvent(event);
}

void App::updateDirectorMenuHold()
{
    if (directorMenu_.isOpen() || bootMenu_.isOpen()) {
        return;
    }

    if (!buttonInput_.isPressed(ButtonId::Action)) {
        directorHoldFired_ = false;
        return;
    }

    if (directorHoldFired_) {
        return;
    }

    // Deliberately NOT gated on directorControl_.localControlsLocked(),
    // unlike handleButtonEvent()/handleEncoderEvent()'s in-game input
    // routing. Local Lock is meant to stop a local PLAYER's button
    // presses from interfering with a remotely-run game -- it must
    // never also lock the DIRECTOR out of their own menu, since
    // DirectorMenu is the only local place to toggle it back off, and
    // the alternative (remote UnlockLocalControls) requires WiFi,
    // which is currently disabled entirely (see FeatureFlags.h). This
    // used to be gated the same way and it was a real bug: locking
    // local controls from inside DirectorMenu immediately closes the
    // menu (see its ToggleLocalLock handling), and with this gate in
    // place that left no way back in short of a power cycle.

    // Reachable from a running game or the idle pre-round Setup state
    // (e.g. WiFi setup is often the first thing you'd do on a fresh
    // device, before ever starting a round) -- gated on the state as
    // of the end of the previous tick's syncSystemState(). A one-tick
    // lag against a 5-second hold threshold is not perceptible.
    // notifyPause() below is a harmless no-op from Setup (Mode1
    // RoundRobin only acts on it if a round had actually started).
    if (state_ != SystemState::GameRunning && state_ != SystemState::Setup) {
        return;
    }

    if (buttonInput_.heldDurationMs(ButtonId::Action) < DIRECTOR_MENU_HOLD_MS) {
        return;
    }

    directorHoldFired_ = true;
    gameModeManager_.notifyPause();
    directorMenu_.open(gameModeManager_, directorControl_, players_, tft_);
}

void App::syncSystemState()
{
    // Critical battery is the first (and so far only) concrete
    // trigger for SystemState::Error -- see PowerManager.h's note on
    // why a literal deep-sleep shutdown wasn't used at 5% instead.
    if (power_.state() == PowerState::Critical) {
        state_ = SystemState::Error;
        return;
    }

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
