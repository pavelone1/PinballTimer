#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/PlayerManager.h"
#include "game/ButtonAssignmentManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/TimerManager.h"
#include "output/NumericDisplayManager.h"
#include "output/TftDisplayManager.h"
#include "output/ButtonLightManager.h"

// Bundles references to every shared manager a mode needs, so a mode
// doesn't have to store its own pointers/references to infrastructure
// that App/GameModeManager own for the whole program's lifetime.
struct GameModeContext {
    PlayerManager& players;
    ButtonAssignmentManager& buttonAssignments;
    DisplayAssignmentManager& displayAssignments;
    NumericDisplayManager& numericDisplays;
    TftDisplayManager& tft;
    ButtonLightManager& buttonLights;
    TimerManager& timers;
};

// Common interface every game mode implements. No central switch/case
// is needed to select mode behavior -- the active mode supplies its
// own functions via this interface, and GameModeManager just routes
// to whichever mode is active.
//
// Mode-specific settings (e.g. Mode 1's seconds-per-turn) are NOT
// part of this shared interface -- each concrete mode defines its own
// settings as its own members, since they genuinely differ per mode.
class GameMode {
public:
    virtual ~GameMode() = default;

    virtual const char* name() const = 0;
    virtual uint8_t id() const = 0;

    virtual uint8_t minPlayers() const = 0;
    virtual uint8_t maxPlayers() const = 0;
    virtual uint8_t defaultPlayerCount() const = 0;

    // Called once when this mode becomes active, before any start.
    // Sets up player/button/display/timer assignments for the mode.
    // playerCount has already been validated against min/maxPlayers.
    virtual void setupAssignments(GameModeContext& context, uint8_t playerCount) = 0;

    virtual void onLocalStart(GameModeContext& context) {}
    virtual void onRemoteStart(GameModeContext& context) {}
    virtual void onGameStart(GameModeContext& context) {}
    virtual void onFirstTimerStart(GameModeContext& context) {}
    virtual void update(GameModeContext& context) {}
    virtual void onPause(GameModeContext& context) {}
    virtual void onResume(GameModeContext& context) {}
    virtual void onStop(GameModeContext& context) {}
    virtual void onReset(GameModeContext& context) {}

    virtual void onButtonEvent(GameModeContext& context, const ButtonEvent& event) {}
    virtual void onEncoderEvent(GameModeContext& context, const EncoderEvent& event) {}

    // Generic named-option setter for DirectorControl's "set mode
    // options" command. A mode overrides this to handle whichever
    // option names it defines (e.g. Mode1RoundRobin handles
    // "secondsPerTurn"). Returns false for an unrecognized key.
    virtual bool setModeOption(const char* key, long value) { return false; }

    // Network/remote hooks. commandId is a DirectorCommandType cast to
    // uint8_t (see network/RemoteCommand.h). Default: reject
    // everything, no-op on loss -- a mode opts in explicitly.
    virtual bool allowsRemoteCommand(uint8_t commandId) const { return false; }
    virtual void onNetworkLost(GameModeContext& context) {}
};
