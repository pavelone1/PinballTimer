#pragma once

#include "game/GameMode.h"
#include "game/RoundRobinTurnEngine.h"
#include "modes/gauntlet/GauntletSession.h"
#include "modes/gauntlet/GauntletConfig.h"
#include "modes/gauntlet/GauntletConfigMenu.h"

// Standalone Gauntlet implementation. It is deliberately not registered in
// ModeRegistry yet; configuration is supplied programmatically until the
// catalog/setup UI integration phase.
class Mode2Gauntlet : public GameMode {
public:
    static constexpr uint8_t MODE_ID = 2;

    const char* name() const override;
    uint8_t id() const override;
    uint8_t minPlayers() const override;
    uint8_t maxPlayers() const override;
    uint8_t defaultPlayerCount() const override;
    void openConfigMenu(const MachineCatalog& catalog) override;
    ModeConfigMenuOutcome handleConfigMenuEvent(const EncoderEvent& event) override;
    void renderConfigMenu(TftDisplayManager& tft) override;
    bool applyConfiguration(const MachineCatalog& catalog) override;
    uint8_t configuredPlayerCount() const override;
    MachineId configuredMachineId() const override;

    void clearMachines();
    bool addMachine(const MachineRecord& machine);
    bool configure(const GauntletConfig& config, const MachineCatalog& catalog);
    const GauntletSession& session() const;

    void setupAssignments(GameModeContext& context, uint8_t playerCount) override;
    void update(GameModeContext& context) override;
    void onPause(GameModeContext& context) override;
    void onResume(GameModeContext& context) override;
    void onStop(GameModeContext& context) override;
    void onReset(GameModeContext& context) override;
    bool onButtonEvent(GameModeContext& context, const ButtonEvent& event) override;
    void onEncoderEvent(GameModeContext& context, const EncoderEvent& event) override;
    bool isRoundOver() const override;

private:
    static constexpr uint8_t MAX_PLAYERS = 4;
    static constexpr unsigned long FLASH_INTERVAL_MS = 500;

    GauntletSession session_;
    GauntletConfig config_;
    GauntletConfigMenu configMenu_;
    RoundRobinTurnEngine turns_;
    TimerId timerIds_[MAX_PLAYERS] = {};
    uint8_t playerCount_ = 0;
    bool configured_ = false;
    bool machineRunning_ = false;
    bool handoff_ = false;
    bool gameOver_ = false;
    uint8_t handoffSelection_ = 0; // 0=start, 1=skip, 2=remove
    bool confirmRemoval_ = false;

    void beginCurrentMachine(GameModeContext& context);
    void endBall(GameModeContext& context, bool timedOut);
    void finishCurrentMachine(GameModeContext& context);
    void applyRemovedTime(GameModeContext& context);
    void render(GameModeContext& context);
    bool anyPlayerHasTime(GameModeContext& context) const;
    void syncDisplays(GameModeContext& context);
};
