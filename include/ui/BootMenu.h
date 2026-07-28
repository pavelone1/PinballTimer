#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/GameModeManager.h"
#include "game/MachineCatalog.h"
#include "storage/SettingsStorage.h"
#include "storage/GameStorage.h"
#include "game/PlayerManager.h"
#include "output/TftDisplayManager.h"
#include "network/WifiPortal.h"
#include "ui/TextEntry.h"
#include "ui/MenuHandoff.h"

// The screen shown on every boot (see App::begin()), replacing the
// previous silent "auto-resume last mode and wait for Action"
// behavior. Top-level items (see refreshTopItems()), in fixed order:
//   - Select Game Mode -- ALWAYS first. Submenu over
//     GameModeManager::modeAt(). Picking one only selects it
//     (GameModeManager::selectMode(), nothing started yet) and drops
//     into ModeMenu (see below) -- deliberately NOT an immediate
//     start, so a mode can be configured before its first game.
//     There's no top-level "Start Game"/"Mode Config" item any more:
//     starting or configuring a mode implies one has already been
//     picked, so both only make sense inside ModeMenu.
//   - Continue Game -- only shown when GameStorage has a valid
//     in-progress-round checkpoint (see GameMode::restoreState()).
//     Internally still TopItem::ResumeGame/State-machine-wise
//     unchanged, just relabeled.
//   - Player Info -- submenu: per-player name entry via TextEntry,
//     persisted through GameStorage and mirrored into PlayerManager.
//   - WiFi Setup -- submenu: join via encoder/web, adhoc-only,
//     persistent-hotspot toggle.
//
// ModeMenu (State::ModeMenu, reached only from Select Game Mode) --
// items are computed fresh each entry (see refreshModeMenuItems()),
// same "recompute, don't hardcode" convention as the top menu:
//   - Start <ModeName> -- starts the just-selected mode (the same
//     selectMode()+setPlayerCount()+initializeActiveMode()+
//     setLastSelectedMode() sequence startGameWithMode() already did).
//   - Number of Players -- only shown when the active mode's
//     maxPlayers() > 1. This is a provisional stand-in for "every mode
//     EXCLUDING single-player-only modes" -- exactly which modes count
//     as single-player-only is still TBD (there's only ever been one
//     multi-player mode to test this against), so revisit this
//     condition once a real single-player mode exists. Numeric editor,
//     writes through GameModeManager::setPlayerCount() on confirm.
//   - Select Machine -- submenu over the common MachineCatalog (shared
//     infrastructure, not owned by any one mode -- see
//     MachineCatalog.h/CODEX.md). Lives here, not on the main menu,
//     since which machine applies is a per-mode-session concern.
//     Picking a record currently only sets GameStorage's machineName
//     (the same field DirectorControl::SetMachineName already writes)
//     so it shows up in the TFT/status JSON; it does NOT yet push the
//     record's ball count/play time into the mode's config -- that
//     deeper catalog<->mode integration is explicitly still undecided
//     (see CODEX.md's "Common Machine Catalog" section), so this is
//     deliberately scoped to selection/display only for now.
//   - Mode Config -- submenu: adjust the active mode's own options --
//     Mode 1: turn timer, ball count -- via GameMode::setModeOption()/
//     modeOption(), the same generic key/value surface
//     DirectorControl/StatusReporter already use remotely.
//   - Return to Main Menu -- back to TopMenu directly, skipping
//     ModeSelect. Long-press here backs out one level to ModeSelect
//     (pick a different mode) instead, same "one level" convention as
//     everywhere else -- Return to Main Menu is the explicit
//     two-levels-at-once shortcut.
//
// Owns the encoder/TFT while open, same convention as DirectorMenu/
// WifiSetupMenu/WifiPortal -- App routes input here instead of to the
// active GameMode. Long-press backs out one level (submenu ->
// TopMenu, or ModeMenu -> ModeSelect); at TopMenu itself it's a
// no-op, since there's always something to pick at boot (no "cancel
// to nothing" state).
//
// Two WiFi items hand off to a different class's interactive flow
// (MenuHandoff::OpenWifiSetup/OpenWifiPortal/RevertToAdhoc) rather
// than acting directly, since App owns those instances -- the same
// pattern DirectorMenu already uses (see ui/MenuHandoff.h). The
// hotspot-persistence toggle is simple enough to act on directly via
// WifiPortal::setPersistentHotspot(), no handoff needed.
class BootMenu {
public:
    void begin(
        GameModeManager& modeManager,
        SettingsStorage& settings,
        GameStorage& gameStorage,
        PlayerManager& players,
        TftDisplayManager& tft,
        WifiPortal& wifiPortal,
        const MachineCatalog& machineCatalog
    );

    void open();
    void openWifiSubmenu();
    void close();
    bool isOpen() const;

    MenuHandoff handleEncoderEvent(const EncoderEvent& event);

private:
    enum class State : uint8_t {
        TopMenu,
        ModeSelect,
        ModeOwnedConfig,
        ModeMenu,
        PlayerCountEdit,
        MachineSelect,
        PlayerList,
        PlayerNameEntry,
        WifiSubmenu,
        ModeConfigList,
        ModeConfigEdit
    };

    enum class TopItem : uint8_t {
        SelectMode,
        ResumeGame, // labeled "Continue Game" -- see topItemLabel()
        PlayerInfo,
        WifiSetup
    };

    enum class ModeMenuItem : uint8_t {
        Start,
        PlayerCount,
        SelectMachine,
        Config,
        ReturnToMain
    };

    enum class WifiItem : uint8_t {
        TogglePower,
        JoinEncoder,
        JoinWeb,
        Adhoc,
        ForgetNetwork,
        ToggleKeepAlive,
        ToggleHotspot
    };

    enum class ModeConfigItem : uint8_t {
        SecondsPerTurn,
        BallCount
    };

    static constexpr uint8_t MAX_VISIBLE_ITEMS = 5; // must not exceed TftDisplayManager::MAX_LINES
    static constexpr uint8_t MAX_TOP_ITEMS = 4;
    static constexpr uint8_t MODE_MENU_MAX_ITEMS = 5; // Start, Number of Players (conditional), Select Machine, Mode Config, Return
    static constexpr uint8_t PLAYER_COUNT = static_cast<uint8_t>(PlayerId::Count);
    static constexpr uint8_t WIFI_ITEM_COUNT = 7;
    static constexpr uint8_t MODE_CONFIG_ITEM_COUNT = 2;
    static constexpr long MODE_CONFIG_SECONDS_STEP = 15;

    bool open_ = false;
    State state_ = State::TopMenu;

    GameModeManager* modeManager_ = nullptr;
    SettingsStorage* settings_ = nullptr;
    GameStorage* gameStorage_ = nullptr;
    PlayerManager* players_ = nullptr;
    TftDisplayManager* tft_ = nullptr;
    WifiPortal* wifiPortal_ = nullptr;
    const MachineCatalog* machineCatalog_ = nullptr;

    TopItem topItems_[MAX_TOP_ITEMS] = {};
    uint8_t topItemCount_ = 0;
    uint8_t topSelectedIndex_ = 0;

    uint8_t modeSelectedIndex_ = 0;

    ModeMenuItem modeMenuItems_[MODE_MENU_MAX_ITEMS] = {};
    uint8_t modeMenuItemCount_ = 0;
    uint8_t modeMenuSelectedIndex_ = 0;
    long playerCountEditValue_ = 0; // working value while in PlayerCountEdit -- not written through until confirmed

    uint8_t machineSelectedIndex_ = 0;

    uint8_t playerSelectedIndex_ = 0;
    bool playerSetupReturnsToMode_ = false;
    PlayerId editingPlayer_ = PlayerId::Player1;
    TextEntry textEntry_;

    uint8_t wifiSelectedIndex_ = 0;

    uint8_t modeConfigSelectedIndex_ = 0;
    long modeConfigEditValue_ = 0; // working value while in ModeConfigEdit -- not written through until confirmed

    void refreshTopItems();
    void refreshModeMenuItems();
    void startGameWithMode(uint8_t modeId);

    void handleTopMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleModeSelectEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleModeOwnedConfigEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleModeMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handlePlayerCountEditEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleMachineSelectEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handlePlayerListEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handlePlayerNameEntryEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleWifiSubmenuEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleModeConfigListEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleModeConfigEditEncoder(const EncoderEvent& event, MenuHandoff& outcome);

    static const char* modeConfigKey(ModeConfigItem item);

    void render();
    void renderTopMenu();
    void renderModeSelect();
    void renderModeOwnedConfig();
    void renderModeMenu();
    void renderPlayerCountEdit();
    void renderMachineSelect();
    void renderPlayerList();
    void renderPlayerNameEntry();
    void renderWifiSubmenu();
    void renderModeConfigList();
    void renderModeConfigEdit();

    static const char* topItemLabel(TopItem item);
};
