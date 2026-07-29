#include "ui/BootMenu.h"

#include <cstdio>
#include <cstring>
#include <WiFi.h>
#include "FeatureFlags.h"
#include "ui/ScrollList.h"
#include "modes/round_robin/Mode1RoundRobin.h"

void BootMenu::begin(
    GameModeManager& modeManager,
    SettingsStorage& settings,
    GameStorage& gameStorage,
    PlayerManager& players,
    TftDisplayManager& tft,
    WifiPortal& wifiPortal,
    const MachineCatalog& machineCatalog
)
{
    modeManager_ = &modeManager;
    settings_ = &settings;
    gameStorage_ = &gameStorage;
    players_ = &players;
    tft_ = &tft;
    wifiPortal_ = &wifiPortal;
    machineCatalog_ = &machineCatalog;
}

void BootMenu::open()
{
    open_ = true;
    state_ = State::TopMenu;
    topSelectedIndex_ = 0;
    refreshTopItems();
    render();
}

void BootMenu::openWifiSubmenu()
{
    open_ = true;
    state_ = State::WifiSubmenu;
    wifiSelectedIndex_ = 0;
    render();
}

void BootMenu::close()
{
    open_ = false;
    tft_->fillScreen(ColorId::Black);
}

bool BootMenu::isOpen() const
{
    return open_;
}

void BootMenu::refreshTopItems()
{
    topItemCount_ = 0;

    topItems_[topItemCount_++] = TopItem::SelectMode; // always first
    if (gameStorage_->loadRoundState().valid) {
        topItems_[topItemCount_++] = TopItem::ResumeGame; // labeled "Continue Game"
    }
    topItems_[topItemCount_++] = TopItem::PlayerInfo;
    topItems_[topItemCount_++] = TopItem::WifiSetup;
}

void BootMenu::refreshModeMenuItems()
{
    modeMenuItemCount_ = 0;
    modeMenuItems_[modeMenuItemCount_++] = ModeMenuItem::Start;

    GameMode* mode = modeManager_->activeMode();
    // TBD: exact rule for "single-player-only" once more than one mode
    // exists to test this against -- maxPlayers() > 1 is the current
    // stand-in (see BootMenu.h's class comment).
    if (mode != nullptr && mode->maxPlayers() > 1) {
        modeMenuItems_[modeMenuItemCount_++] = ModeMenuItem::PlayerCount;
    }

    modeMenuItems_[modeMenuItemCount_++] = ModeMenuItem::SelectMachine;
    modeMenuItems_[modeMenuItemCount_++] = ModeMenuItem::Config;
    modeMenuItems_[modeMenuItemCount_++] = ModeMenuItem::ReturnToMain;
}

void BootMenu::startGameWithMode(uint8_t modeId)
{
    if (!modeManager_->selectMode(modeId)) {
        return;
    }

    // Only seed the default if nothing set it already -- ModeMenu's
    // "Number of Players" editor may have already written a director's
    // chosen count via setPlayerCount(), and this must not stomp it.
    if (modeManager_->playerCount() == 0) {
        modeManager_->setPlayerCount(modeManager_->activeMode()->defaultPlayerCount());
    }
    modeManager_->initializeActiveMode();
    settings_->setLastSelectedMode(modeId);
}

MenuHandoff BootMenu::handleEncoderEvent(const EncoderEvent& event)
{
    if (!open_) {
        return MenuHandoff::None;
    }

    MenuHandoff outcome = MenuHandoff::None;

    switch (state_) {
        case State::TopMenu:
            handleTopMenuEncoder(event, outcome);
            break;

        case State::ModeSelect:
            handleModeSelectEncoder(event, outcome);
            break;

        case State::ModeOwnedConfig:
            handleModeOwnedConfigEncoder(event, outcome);
            break;

        case State::ModeMenu:
            handleModeMenuEncoder(event, outcome);
            break;

        case State::PlayerCountEdit:
            handlePlayerCountEditEncoder(event, outcome);
            break;

        case State::MachineSelect:
            handleMachineSelectEncoder(event, outcome);
            break;

        case State::PlayerList:
            handlePlayerListEncoder(event, outcome);
            break;

        case State::PlayerNameEntry:
            handlePlayerNameEntryEncoder(event, outcome);
            break;

        case State::WifiSubmenu:
            handleWifiSubmenuEncoder(event, outcome);
            break;

        case State::ModeConfigList:
            handleModeConfigListEncoder(event, outcome);
            break;

        case State::ModeConfigEdit:
            handleModeConfigEditEncoder(event, outcome);
            break;
    }

    return outcome;
}

void BootMenu::handleTopMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome)
{
    if (event.type == EncoderEventType::RotatedClockwise) {
        topSelectedIndex_ = ScrollList::rotate(topSelectedIndex_, topItemCount_, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        topSelectedIndex_ = ScrollList::rotate(topSelectedIndex_, topItemCount_, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        switch (topItems_[topSelectedIndex_]) {
            case TopItem::ResumeGame: {
                const SavedRoundState& saved = gameStorage_->loadRoundState();
                if (saved.valid && modeManager_->selectMode(saved.modeId) && modeManager_->restoreActiveMode()) {
                    outcome = MenuHandoff::Close;
                }
                break;
            }

            case TopItem::SelectMode:
                modeSelectedIndex_ = 0;
                state_ = State::ModeSelect;
                render();
                break;

            case TopItem::PlayerInfo:
                playerSelectedIndex_ = 0;
                playerSetupReturnsToMode_ = false;
                state_ = State::PlayerList;
                render();
                break;

            case TopItem::WifiSetup:
                wifiSelectedIndex_ = 0;
                state_ = State::WifiSubmenu;
                render();
                break;
        }
    }
    // SwLongPress at TopMenu: no-op -- nothing to back out to at boot.
}

void BootMenu::handleModeSelectEncoder(const EncoderEvent& event, MenuHandoff& outcome)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::TopMenu;
        render();
        return;
    }

    const uint8_t count = modeManager_->modeCount();
    if (count == 0) {
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        modeSelectedIndex_ = ScrollList::rotate(modeSelectedIndex_, count, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        modeSelectedIndex_ = ScrollList::rotate(modeSelectedIndex_, count, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        GameMode* mode = modeManager_->modeAt(modeSelectedIndex_);
        if (mode != nullptr) {
            modeManager_->selectMode(mode->id());
            mode->openConfigMenu(*machineCatalog_);
            state_ = State::ModeOwnedConfig;
            render();
        }
    }
}

void BootMenu::handleModeOwnedConfigEncoder(
    const EncoderEvent& event, MenuHandoff& outcome)
{
    GameMode* mode = modeManager_->activeMode();
    if (mode == nullptr) {
        state_ = State::ModeSelect;
        render();
        return;
    }

    const ModeConfigMenuOutcome result = mode->handleConfigMenuEvent(event);
    switch (result) {
        case ModeConfigMenuOutcome::None:
            render();
            break;
        case ModeConfigMenuOutcome::Back:
            state_ = State::ModeSelect;
            render();
            break;
        case ModeConfigMenuOutcome::OpenPlayerSetup:
            playerSelectedIndex_ = 0;
            playerSetupReturnsToMode_ = true;
            state_ = State::PlayerList;
            render();
            break;
        case ModeConfigMenuOutcome::Start:
            if (!mode->applyConfiguration(*machineCatalog_)) {
                render();
                break;
            }
            modeManager_->setPlayerCount(mode->configuredPlayerCount());
            modeManager_->initializeActiveMode();
            settings_->setLastSelectedMode(mode->id());
            if (mode->configuredMachineId() != 0) {
                const MachineRecord* machine =
                    machineCatalog_->find(mode->configuredMachineId());
                if (machine != nullptr) {
                    gameStorage_->setMachineName(machine->name);
                }
            } else {
                gameStorage_->setMachineName("");
            }
            outcome = MenuHandoff::Close;
            break;
    }
}

void BootMenu::handleModeMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::ModeSelect; // one level back -- pick a different mode
        render();
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        modeMenuSelectedIndex_ = ScrollList::rotate(modeMenuSelectedIndex_, modeMenuItemCount_, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        modeMenuSelectedIndex_ = ScrollList::rotate(modeMenuSelectedIndex_, modeMenuItemCount_, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        switch (modeMenuItems_[modeMenuSelectedIndex_]) {
            case ModeMenuItem::Start: {
                GameMode* mode = modeManager_->activeMode();
                if (mode != nullptr) {
                    startGameWithMode(mode->id());
                    outcome = MenuHandoff::Close;
                }
                break;
            }

            case ModeMenuItem::PlayerCount: {
                GameMode* mode = modeManager_->activeMode();
                const uint8_t current = modeManager_->playerCount();
                playerCountEditValue_ = current > 0
                    ? current
                    : (mode != nullptr ? mode->defaultPlayerCount() : 1);
                state_ = State::PlayerCountEdit;
                render();
                break;
            }

            case ModeMenuItem::SelectMachine:
                machineSelectedIndex_ = 0;
                state_ = State::MachineSelect;
                render();
                break;

            case ModeMenuItem::Config:
                modeConfigSelectedIndex_ = 0;
                state_ = State::ModeConfigList;
                render();
                break;

            case ModeMenuItem::ReturnToMain:
                state_ = State::TopMenu;
                render();
                break;
        }
    }
}

void BootMenu::handlePlayerCountEditEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::ModeMenu; // cancel, nothing written
        render();
        return;
    }

    GameMode* mode = modeManager_->activeMode();
    const long minValue = mode != nullptr ? mode->minPlayers() : 1;
    const long maxValue = mode != nullptr ? mode->maxPlayers() : 1;

    if (event.type == EncoderEventType::RotatedClockwise) {
        playerCountEditValue_ = playerCountEditValue_ + 1 > maxValue ? maxValue : playerCountEditValue_ + 1;
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        playerCountEditValue_ = playerCountEditValue_ - 1 < minValue ? minValue : playerCountEditValue_ - 1;
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        modeManager_->setPlayerCount(static_cast<uint8_t>(playerCountEditValue_));
        state_ = State::ModeMenu;
        render();
    }
}

void BootMenu::handleMachineSelectEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::ModeMenu; // one level back -- only reachable via ModeMenu now
        render();
        return;
    }

    // MachineCatalog::MAX_RECORDS (100) fits comfortably in uint8_t --
    // ScrollList's index math is uint8_t-based, same as every other
    // list in this UI.
    const uint8_t count = static_cast<uint8_t>(machineCatalog_->count());
    if (count == 0) {
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        machineSelectedIndex_ = ScrollList::rotate(machineSelectedIndex_, count, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        machineSelectedIndex_ = ScrollList::rotate(machineSelectedIndex_, count, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        const MachineRecord* record = machineCatalog_->at(machineSelectedIndex_);
        if (record != nullptr) {
            // Only sets the display/status-JSON machine name for now --
            // pushing the record's ball count/play time into a mode's
            // config is intentionally not wired yet, see BootMenu.h.
            gameStorage_->setMachineName(record->name);
            state_ = State::ModeMenu;
            render();
        }
    }
}

void BootMenu::handlePlayerListEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = playerSetupReturnsToMode_ ? State::ModeOwnedConfig : State::TopMenu;
        playerSetupReturnsToMode_ = false;
        render();
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        playerSelectedIndex_ = ScrollList::rotate(playerSelectedIndex_, PLAYER_COUNT, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        playerSelectedIndex_ = ScrollList::rotate(playerSelectedIndex_, PLAYER_COUNT, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        editingPlayer_ = static_cast<PlayerId>(playerSelectedIndex_);
        textEntry_.reset(gameStorage_->playerName(editingPlayer_));
        state_ = State::PlayerNameEntry;
        render();
    }
}

void BootMenu::handlePlayerNameEntryEncoder(const EncoderEvent& event, MenuHandoff&)
{
    const TextEntry::Result result = textEntry_.handleEncoderEvent(event);

    switch (result) {
        case TextEntry::Result::None:
            render();
            break;

        case TextEntry::Result::Cancel:
            state_ = State::PlayerList;
            render();
            break;

        case TextEntry::Result::Done:
            gameStorage_->setPlayerName(editingPlayer_, textEntry_.text());
            players_->setName(editingPlayer_, textEntry_.text());
            state_ = State::PlayerList;
            render();
            break;
    }
}

void BootMenu::handleWifiSubmenuEncoder(const EncoderEvent& event, MenuHandoff& outcome)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::TopMenu;
        render();
        return;
    }

    // WiFi disabled -- renderWifiSubmenu() already shows a static
    // "disabled" screen with nothing to select, so only back-out
    // (handled above) does anything here.
    if (!kWifiFeatureEnabled) {
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        wifiSelectedIndex_ = ScrollList::rotate(wifiSelectedIndex_, WIFI_ITEM_COUNT, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        wifiSelectedIndex_ = ScrollList::rotate(wifiSelectedIndex_, WIFI_ITEM_COUNT, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        switch (static_cast<WifiItem>(wifiSelectedIndex_)) {
            case WifiItem::TogglePower:
                outcome = MenuHandoff::ToggleWifiPower;
                break;

            case WifiItem::JoinEncoder:
                outcome = MenuHandoff::OpenWifiSetup;
                break;

            case WifiItem::JoinWeb:
                outcome = MenuHandoff::OpenWifiPortal;
                break;

            case WifiItem::Adhoc:
                outcome = MenuHandoff::RevertToAdhoc;
                break;

            case WifiItem::ForgetNetwork:
                outcome = MenuHandoff::ForgetWifiNetwork;
                break;

            case WifiItem::ToggleKeepAlive:
                outcome = MenuHandoff::ToggleWifiKeepAlive;
                break;

            case WifiItem::ToggleHotspot:
                outcome = MenuHandoff::TogglePersistentHotspot;
                break;
        }
    }
}

void BootMenu::handleModeConfigListEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::ModeMenu; // one level back -- Mode Config is only reachable via ModeMenu now
        render();
        return;
    }

    GameMode* mode = modeManager_->activeMode();
    if (mode == nullptr) {
        return; // nothing to configure -- only back-out (above) does anything
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        modeConfigSelectedIndex_ = ScrollList::rotate(modeConfigSelectedIndex_, MODE_CONFIG_ITEM_COUNT, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        modeConfigSelectedIndex_ = ScrollList::rotate(modeConfigSelectedIndex_, MODE_CONFIG_ITEM_COUNT, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        // Seed the working value from the mode's current setting --
        // ModeConfigEdit adjusts this copy and only writes through via
        // setModeOption() on confirm, so cancelling leaves it untouched.
        modeConfigEditValue_ = mode->modeOption(modeConfigKey(static_cast<ModeConfigItem>(modeConfigSelectedIndex_)));
        state_ = State::ModeConfigEdit;
        render();
    }
}

void BootMenu::handleModeConfigEditEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        // Cancel -- modeConfigEditValue_ is discarded, nothing written.
        state_ = State::ModeConfigList;
        render();
        return;
    }

    const auto item = static_cast<ModeConfigItem>(modeConfigSelectedIndex_);
    const bool isSeconds = item == ModeConfigItem::SecondsPerTurn;
    const long step = isSeconds ? MODE_CONFIG_SECONDS_STEP : 1;
    const long maxValue = isSeconds
        ? Mode1RoundRobin::MAX_SECONDS_PER_TURN
        : static_cast<long>(Mode1RoundRobin::MAX_BALL_COUNT);

    if (event.type == EncoderEventType::RotatedClockwise) {
        modeConfigEditValue_ = modeConfigEditValue_ + step > maxValue ? maxValue : modeConfigEditValue_ + step;
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        modeConfigEditValue_ = modeConfigEditValue_ - step < 1 ? 1 : modeConfigEditValue_ - step;
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        GameMode* mode = modeManager_->activeMode();
        if (mode != nullptr) {
            mode->setModeOption(modeConfigKey(item), modeConfigEditValue_);
        }
        state_ = State::ModeConfigList;
        render();
    }
}

const char* BootMenu::modeConfigKey(ModeConfigItem item)
{
    switch (item) {
        case ModeConfigItem::SecondsPerTurn: return "secondsPerTurn";
        case ModeConfigItem::BallCount: return "ballCount";
    }
    return "";
}

void BootMenu::render()
{
    switch (state_) {
        case State::TopMenu:
            renderTopMenu();
            break;

        case State::ModeSelect:
            renderModeSelect();
            break;

        case State::ModeOwnedConfig:
            renderModeOwnedConfig();
            break;

        case State::ModeMenu:
            renderModeMenu();
            break;

        case State::PlayerCountEdit:
            renderPlayerCountEdit();
            break;

        case State::MachineSelect:
            renderMachineSelect();
            break;

        case State::PlayerList:
            renderPlayerList();
            break;

        case State::PlayerNameEntry:
            renderPlayerNameEntry();
            break;

        case State::WifiSubmenu:
            renderWifiSubmenu();
            break;

        case State::ModeConfigList:
            renderModeConfigList();
            break;

        case State::ModeConfigEdit:
            renderModeConfigEdit();
            break;
    }
}

void BootMenu::renderModeOwnedConfig()
{
    GameMode* mode = modeManager_->activeMode();
    if (mode != nullptr) {
        mode->renderConfigMenu(*tft_);
    }
}

void BootMenu::renderTopMenu()
{
    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t visibleCount = topItemCount_ < MAX_VISIBLE_ITEMS ? topItemCount_ : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(topSelectedIndex_, topItemCount_, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s",
            itemIndex == topSelectedIndex_ ? "> " : "  ",
            topItemLabel(topItems_[itemIndex]));
        linePtrs[i] = lineBuf[i];
    }

    tft_->showStatusScreen("PINBALL TIMER", linePtrs, visibleCount, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void BootMenu::renderModeSelect()
{
    const uint8_t count = modeManager_->modeCount();

    if (count == 0) {
        const char* lines[] = {"No modes registered", "hold knob to go back"};
        tft_->showStatusScreen("SELECT MODE", lines, 2, ColorId::Black, ColorId::White, ColorId::White);
        return;
    }

    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t visibleCount = count < MAX_VISIBLE_ITEMS ? count : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(modeSelectedIndex_, count, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        GameMode* mode = modeManager_->modeAt(itemIndex);
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s",
            itemIndex == modeSelectedIndex_ ? "> " : "  ",
            mode != nullptr ? mode->name() : "?");
        linePtrs[i] = lineBuf[i];
    }

    // Only when there's still room -- a full 5-item window has nowhere
    // left to put this without dropping a visible mode (see
    // MAX_VISIBLE_ITEMS's own MAX_LINES ceiling).
    uint8_t totalLines = visibleCount;
    if (totalLines < MAX_VISIBLE_ITEMS) {
        snprintf(lineBuf[totalLines], LINE_LENGTH, "hold knob to go back");
        linePtrs[totalLines] = lineBuf[totalLines];
        ++totalLines;
    }

    tft_->showStatusScreen("SELECT MODE", linePtrs, totalLines, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderModeMenu()
{
    static constexpr uint8_t LINE_LENGTH = 32;

    GameMode* mode = modeManager_->activeMode();
    const char* modeName = mode != nullptr ? mode->name() : "?";

    char startLabel[LINE_LENGTH];
    snprintf(startLabel, sizeof(startLabel), "Start %s", modeName);

    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    // Windowed like every other list here -- modeMenuItemCount_ can
    // reach MODE_MENU_MAX_ITEMS (5), same as MAX_VISIBLE_ITEMS, so
    // there's no guaranteed spare line for the hint below.
    const uint8_t visibleCount = modeMenuItemCount_ < MAX_VISIBLE_ITEMS ? modeMenuItemCount_ : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(modeMenuSelectedIndex_, modeMenuItemCount_, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        const char* label = "";
        switch (modeMenuItems_[itemIndex]) {
            case ModeMenuItem::Start: label = startLabel; break;
            case ModeMenuItem::PlayerCount: label = "Number of Players"; break;
            case ModeMenuItem::SelectMachine: label = "Select Machine"; break;
            case ModeMenuItem::Config: label = "Mode Config"; break;
            case ModeMenuItem::ReturnToMain: label = "Return to Main Menu"; break;
        }
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s", itemIndex == modeMenuSelectedIndex_ ? "> " : "  ", label);
        linePtrs[i] = lineBuf[i];
    }

    // Only when there's still room -- see the comment above.
    uint8_t totalLines = visibleCount;
    if (totalLines < MAX_VISIBLE_ITEMS) {
        snprintf(lineBuf[totalLines], LINE_LENGTH, "hold knob to go back");
        linePtrs[totalLines] = lineBuf[totalLines];
        ++totalLines;
    }

    tft_->showStatusScreen("GAME MODE", linePtrs, totalLines, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderPlayerCountEdit()
{
    char valueLine[16];
    snprintf(valueLine, sizeof(valueLine), "%ld", playerCountEditValue_);

    const char* lines[] = {valueLine, "press to confirm", "hold knob to cancel"};
    tft_->showStatusScreen("NUMBER OF PLAYERS", lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void BootMenu::renderMachineSelect()
{
    const uint8_t count = static_cast<uint8_t>(machineCatalog_->count());

    if (count == 0) {
        const char* lines[] = {"No machines in database", "hold knob to go back"};
        tft_->showStatusScreen("SELECT MACHINE", lines, 2, ColorId::Black, ColorId::White, ColorId::White);
        return;
    }

    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t visibleCount = count < MAX_VISIBLE_ITEMS ? count : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(machineSelectedIndex_, count, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        const MachineRecord* record = machineCatalog_->at(itemIndex);
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s",
            itemIndex == machineSelectedIndex_ ? "> " : "  ",
            record != nullptr ? record->name : "?");
        linePtrs[i] = lineBuf[i];
    }

    uint8_t totalLines = visibleCount;
    if (totalLines < MAX_VISIBLE_ITEMS) {
        snprintf(lineBuf[totalLines], LINE_LENGTH, "hold knob to go back");
        linePtrs[totalLines] = lineBuf[totalLines];
        ++totalLines;
    }

    tft_->showStatusScreen("SELECT MACHINE", linePtrs, totalLines, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderPlayerList()
{
    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[PLAYER_COUNT + 1][LINE_LENGTH];
    const char* linePtrs[PLAYER_COUNT + 1];

    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        const char* name = gameStorage_->playerName(static_cast<PlayerId>(i));
        snprintf(lineBuf[i], LINE_LENGTH, "%sP%u: %s",
            i == playerSelectedIndex_ ? "> " : "  ",
            i + 1,
            name[0] != '\0' ? name : "(unnamed)");
        linePtrs[i] = lineBuf[i];
    }

    snprintf(lineBuf[PLAYER_COUNT], LINE_LENGTH, "hold knob to go back");
    linePtrs[PLAYER_COUNT] = lineBuf[PLAYER_COUNT];

    tft_->showStatusScreen("PLAYER INFO", linePtrs, PLAYER_COUNT + 1, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderPlayerNameEntry()
{
    char title[16];
    snprintf(title, sizeof(title), "PLAYER %u NAME", static_cast<uint8_t>(editingPlayer_) + 1);

    char bufferLine[TextEntry::MAX_LENGTH + 2];
    snprintf(bufferLine, sizeof(bufferLine), "%s_", textEntry_.text());

    char pickerGlyph[8];
    textEntry_.currentPickerLabel(pickerGlyph, sizeof(pickerGlyph));
    char pickerLine[16];
    snprintf(pickerLine, sizeof(pickerLine), "> %s <", pickerGlyph);

    const char* lines[] = {bufferLine, pickerLine, "hold knob to cancel"};
    tft_->showStatusScreen(title, lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void BootMenu::renderWifiSubmenu()
{
    if (!kWifiFeatureEnabled) {
        const char* lines[] = {"WiFi is temporarily", "disabled", "hold knob to go back"};
        tft_->showStatusScreen("WIFI SETUP", lines, 3, ColorId::Black, ColorId::White, ColorId::Cyan);
        return;
    }

    static constexpr uint8_t LINE_LENGTH = 40;
    static constexpr uint8_t SCREEN_LINE_COUNT = 5;
    static constexpr uint8_t STATUS_LINE_COUNT = 2;
    static constexpr uint8_t VISIBLE_ACTION_COUNT =
        SCREEN_LINE_COUNT - STATUS_LINE_COUNT;
    char lineBuf[SCREEN_LINE_COUNT][LINE_LENGTH];
    const char* linePtrs[SCREEN_LINE_COUNT];

    if (WiFi.status() == WL_CONNECTED) {
        snprintf(lineBuf[0], LINE_LENGTH, "SSID: %s", WiFi.SSID().c_str());
        const IPAddress ip = WiFi.localIP();
        snprintf(lineBuf[1], LINE_LENGTH, "IP: %u.%u.%u.%u",
            ip[0], ip[1], ip[2], ip[3]);
    } else {
        snprintf(lineBuf[0], LINE_LENGTH, "SSID: Not connected");
        snprintf(lineBuf[1], LINE_LENGTH, "IP: --");
    }
    linePtrs[0] = lineBuf[0];
    linePtrs[1] = lineBuf[1];

    const char* labels[WIFI_ITEM_COUNT] = {
        kPreAlphaWifiAlwaysOn ? "DEV: WiFi Forced ON" :
            (WiFi.getMode() == WIFI_MODE_NULL ? "Turn WiFi ON" : "Turn WiFi OFF"),
        "Join Network (Encoder)",
        "Join Network (Web)",
        "Use Hotspot Only (Adhoc)",
        "Forget WiFi Network",
        kPreAlphaWifiAlwaysOn ? "DEV: Keep Alive ON" :
            (settings_->wifiKeepAlive() ? "Keep WiFi Alive: ON" : "Keep WiFi Alive: OFF"),
        wifiPortal_->persistentHotspot() ? "Keep Hotspot: ON" : "Keep Hotspot: OFF"
    };

    const uint8_t windowStart = ScrollList::scrollWindowStart(
        wifiSelectedIndex_, WIFI_ITEM_COUNT, VISIBLE_ACTION_COUNT);
    for (uint8_t i = 0; i < VISIBLE_ACTION_COUNT; ++i) {
        const uint8_t itemIndex = windowStart + i;
        snprintf(lineBuf[STATUS_LINE_COUNT + i], LINE_LENGTH, "%s%s",
            itemIndex == wifiSelectedIndex_ ? "> " : "  ", labels[itemIndex]);
        linePtrs[STATUS_LINE_COUNT + i] = lineBuf[STATUS_LINE_COUNT + i];
    }

    tft_->showStatusScreen("WIFI SETUP", linePtrs, SCREEN_LINE_COUNT,
        ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderModeConfigList()
{
    GameMode* mode = modeManager_->activeMode();
    if (mode == nullptr) {
        const char* lines[] = {"No mode selected", "hold knob to go back"};
        tft_->showStatusScreen("MODE CONFIG", lines, 2, ColorId::Black, ColorId::White, ColorId::Cyan);
        return;
    }

    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MODE_CONFIG_ITEM_COUNT + 1][LINE_LENGTH];
    const char* linePtrs[MODE_CONFIG_ITEM_COUNT + 1];

    const long seconds = mode->modeOption("secondsPerTurn");
    char secondsLabel[24];
    snprintf(secondsLabel, sizeof(secondsLabel), "Turn Timer: %ld:%02ld", seconds / 60, seconds % 60);

    char ballCountLabel[24];
    snprintf(ballCountLabel, sizeof(ballCountLabel), "Ball Count: %ld", mode->modeOption("ballCount"));

    const char* labels[MODE_CONFIG_ITEM_COUNT] = {secondsLabel, ballCountLabel};

    for (uint8_t i = 0; i < MODE_CONFIG_ITEM_COUNT; ++i) {
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s", i == modeConfigSelectedIndex_ ? "> " : "  ", labels[i]);
        linePtrs[i] = lineBuf[i];
    }

    snprintf(lineBuf[MODE_CONFIG_ITEM_COUNT], LINE_LENGTH, "hold knob to go back");
    linePtrs[MODE_CONFIG_ITEM_COUNT] = lineBuf[MODE_CONFIG_ITEM_COUNT];

    tft_->showStatusScreen("MODE CONFIG", linePtrs, MODE_CONFIG_ITEM_COUNT + 1, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void BootMenu::renderModeConfigEdit()
{
    const bool isSeconds = static_cast<ModeConfigItem>(modeConfigSelectedIndex_) == ModeConfigItem::SecondsPerTurn;

    char valueLine[24];
    if (isSeconds) {
        snprintf(valueLine, sizeof(valueLine), "%ld:%02ld", modeConfigEditValue_ / 60, modeConfigEditValue_ % 60);
    } else {
        snprintf(valueLine, sizeof(valueLine), "%ld", modeConfigEditValue_);
    }

    const char* lines[] = {valueLine, "press to confirm", "hold knob to cancel"};
    tft_->showStatusScreen(isSeconds ? "TURN TIMER" : "BALL COUNT", lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

const char* BootMenu::topItemLabel(TopItem item)
{
    switch (item) {
        case TopItem::SelectMode: return "Select Game Mode";
        case TopItem::ResumeGame: return "Continue Game";
        case TopItem::PlayerInfo: return "Player Info";
        case TopItem::WifiSetup: return kWifiFeatureEnabled ? "WiFi Setup" : "WiFi Setup (disabled)";
    }
    return "";
}
