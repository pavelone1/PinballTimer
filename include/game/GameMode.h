#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/PlayerManager.h"
#include "game/ButtonAssignmentManager.h"
#include "game/DisplayAssignmentManager.h"
#include "game/TimerManager.h"
#include "game/MachineCatalog.h"
#include "output/NumericDisplayManager.h"
#include "output/TftDisplayManager.h"
#include "output/ButtonLightManager.h"
#include "output/BuzzerManager.h"
#include "storage/GameStorage.h"

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
    BuzzerManager& buzzer;
    TimerManager& timers;
    GameStorage& gameStorage;
};

enum class ModeConfigMenuOutcome : uint8_t {
    None,
    Start,
    OpenPlayerSetup,
    Back
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

    // Mode-owned boot configuration. BootMenu delegates to these methods and
    // remains unaware of individual menu items or validation rules.
    virtual void openConfigMenu(const MachineCatalog& catalog) { (void)catalog; }
    virtual ModeConfigMenuOutcome handleConfigMenuEvent(const EncoderEvent& event) { (void)event; return ModeConfigMenuOutcome::None; }
    virtual void renderConfigMenu(TftDisplayManager& tft) { (void)tft; }
    virtual bool applyConfiguration(const MachineCatalog& catalog) { (void)catalog; return true; }
    virtual uint8_t configuredPlayerCount() const { return defaultPlayerCount(); }
    virtual MachineId configuredMachineId() const { return 0; }

    // Called once when this mode becomes active, before any start.
    // Sets up player/button/display/timer assignments for the mode.
    // playerCount has already been validated against min/maxPlayers.
    virtual void setupAssignments(GameModeContext& context, uint8_t playerCount) = 0;

    // Restores an in-progress round from GameStorage's checkpoint
    // instead of starting fresh via setupAssignments() (a mode that
    // implements this calls setupAssignments() itself internally, as
    // part of restoring -- the caller must NOT also call it, or timer
    // slots leak). Returns false if there's no valid checkpoint for
    // this mode (default: unsupported), in which case the caller
    // should fall back to the normal setupAssignments() flow. See
    // GameModeManager::restoreActiveMode() and BootMenu's "Resume
    // Game" item.
    virtual bool restoreState(GameModeContext& context) { return false; }

    virtual void onLocalStart(GameModeContext& context) {}
    virtual void onRemoteStart(GameModeContext& context) {}
    virtual void onGameStart(GameModeContext& context) {}
    virtual void onFirstTimerStart(GameModeContext& context) {}
    virtual void update(GameModeContext& context) {}
    virtual void onPause(GameModeContext& context) {}
    virtual void onResume(GameModeContext& context) {}
    virtual void onStop(GameModeContext& context) {}
    virtual void onReset(GameModeContext& context) {}

    // Returns true if handling this event just transitioned the mode
    // from not-started to started (e.g. the Action button began the
    // round) -- GameModeManager uses this to fire notifyLocalStart()/
    // notifyGameStart() itself, since it has no other way to know a
    // mode-internal round just began from routing a button event.
    // Modes that don't have a local "start" gesture just return false
    // (the default).
    virtual bool onButtonEvent(GameModeContext& context, const ButtonEvent& event) { return false; }
    virtual void onEncoderEvent(GameModeContext& context, const EncoderEvent& event) {}

    // Generic named-option setter for DirectorControl's "set mode
    // options" command. A mode overrides this to handle whichever
    // option names it defines (e.g. Mode1RoundRobin handles
    // "secondsPerTurn"). Returns false for an unrecognized key.
    virtual bool setModeOption(const char* key, long value) { return false; }

    // Read-side counterpart to setModeOption(), for StatusReporter to
    // surface a mode's live config generically (e.g. ball count)
    // without downcasting to a concrete mode type. Returns 0 for an
    // unrecognized key (default).
    virtual long modeOption(const char* key) const { (void)key; return 0; }

    // Per-player live adjustment for director tools (Mode1RoundRobin
    // recognizes "roundsRemaining" and "timerSeconds"). Needs the full
    // context -- unlike setModeOption()/modeOption() above -- because
    // adjusting these can revive an Eliminated/Finished player back
    // into rotation, which touches PlayerManager/TimerManager/
    // ButtonLightManager/TurnRotation, none of which a mode can reach
    // from its own members alone. Returns false for an unrecognized key.
    virtual bool setPlayerOption(GameModeContext& context, PlayerId player, const char* key, long value) { (void)context; (void)player; (void)key; (void)value; return false; }
    virtual long playerOption(GameModeContext& context, PlayerId player, const char* key) const { (void)context; (void)player; (void)key; return 0; }

    // Live, in-progress-game equivalent of setModeOption() -- e.g.
    // Mode1RoundRobin recognizes "ballCount" here to mean "redistribute
    // the delta to every already-assigned player's roundsRemaining
    // right now", distinct from setModeOption("ballCount", ...) which
    // only affects the NEXT game's setupAssignments(). Needs context
    // for the same reason as setPlayerOption() above.
    virtual bool setLiveModeOption(GameModeContext& context, const char* key, long value) { (void)context; (void)key; (void)value; return false; }

    // True once this mode's own end condition has been reached (e.g.
    // every player eliminated/finished). StatusReporter surfaces this
    // generically as "gameOver" without knowing mode-specific rules.
    virtual bool isRoundOver() const { return false; }

    // Network/remote hooks. commandId is a DirectorCommandType cast to
    // uint8_t (see network/RemoteCommand.h). Default: reject
    // everything, no-op on loss -- a mode opts in explicitly.
    virtual bool allowsRemoteCommand(uint8_t commandId) const { return false; }
    virtual void onNetworkLost(GameModeContext& context) {}
};
