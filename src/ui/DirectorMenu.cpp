#include "ui/DirectorMenu.h"

#include <cstdio>
#include <cstring>
#include "FeatureFlags.h"
#include "modes/Mode1RoundRobin.h"
#include "ui/ScrollList.h"

void DirectorMenu::open(GameModeManager& modeManager, DirectorControl& directorControl, PlayerManager& players, TftDisplayManager& tft)
{
    modeManager_ = &modeManager;
    directorControl_ = &directorControl;
    players_ = &players;
    tft_ = &tft;
    state_ = State::TopMenu;
    topSelectedIndex_ = 0;
    open_ = true;
    render();
}

void DirectorMenu::close(TftDisplayManager& tft)
{
    open_ = false;
    tft.fillScreen(ColorId::Black);
}

bool DirectorMenu::isOpen() const
{
    return open_;
}

MenuHandoff DirectorMenu::handleEncoderEvent(const EncoderEvent& event)
{
    if (!open_) {
        return MenuHandoff::None;
    }

    MenuHandoff outcome = MenuHandoff::None;

    switch (state_) {
        case State::TopMenu:
            handleTopMenuEncoder(event, outcome);
            break;

        case State::BallCountAllEdit:
            handleBallCountAllEditEncoder(event, outcome);
            break;

        case State::PlayerPicker:
            handlePlayerPickerEncoder(event, outcome);
            break;

        case State::PlayerBallCountEdit:
        case State::PlayerTimeEdit:
            handlePlayerEditEncoder(event, outcome);
            break;
    }

    return outcome;
}

void DirectorMenu::handleTopMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome)
{
    if (event.type == EncoderEventType::SwLongPress) {
        outcome = MenuHandoff::Close; // cancel whole menu, resume game
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        topSelectedIndex_ = ScrollList::rotate(topSelectedIndex_, TOP_ITEM_COUNT, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        topSelectedIndex_ = ScrollList::rotate(topSelectedIndex_, TOP_ITEM_COUNT, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        switch (static_cast<TopItem>(topSelectedIndex_)) {
            case TopItem::ResumeGame:
                outcome = MenuHandoff::Close;
                break;

            case TopItem::ResetRound:
                modeManager_->notifyReset();
                outcome = MenuHandoff::Close;
                break;

            case TopItem::BallCountAll: {
                GameMode* mode = modeManager_->activeMode();
                if (mode != nullptr) {
                    ballCountAllEditValue_ = mode->modeOption("ballCount");
                    state_ = State::BallCountAllEdit;
                    render();
                }
                break;
            }

            case TopItem::PlayerBallCount:
                if (modeManager_->playerCount() > 0) {
                    pendingPlayerEdit_ = PendingPlayerEdit::BallCount;
                    playerPickerSelectedIndex_ = 0;
                    state_ = State::PlayerPicker;
                    render();
                }
                break;

            case TopItem::PlayerTime:
                if (modeManager_->playerCount() > 0) {
                    pendingPlayerEdit_ = PendingPlayerEdit::Time;
                    playerPickerSelectedIndex_ = 0;
                    state_ = State::PlayerPicker;
                    render();
                }
                break;

            case TopItem::ToggleLocalLock: {
                DirectorCommand cmd;
                cmd.type = directorControl_->localControlsLocked()
                    ? DirectorCommandType::UnlockLocalControls
                    : DirectorCommandType::LockLocalControls;
                directorControl_->execute(cmd);
                outcome = MenuHandoff::Close;
                break;
            }

            case TopItem::EndGame:
                modeManager_->notifyReset();
                outcome = MenuHandoff::EndGame;
                break;

            case TopItem::WifiEncoder:
                outcome = kWifiFeatureEnabled ? MenuHandoff::OpenWifiSetup : MenuHandoff::None;
                break;

            case TopItem::WifiWeb:
                outcome = kWifiFeatureEnabled ? MenuHandoff::OpenWifiPortal : MenuHandoff::None;
                break;
        }
    }
}

void DirectorMenu::handleBallCountAllEditEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::TopMenu; // cancel, nothing written
        render();
        return;
    }

    const long maxValue = static_cast<long>(Mode1RoundRobin::MAX_BALL_COUNT);

    if (event.type == EncoderEventType::RotatedClockwise) {
        ballCountAllEditValue_ = ballCountAllEditValue_ + BALL_COUNT_STEP > maxValue ? maxValue : ballCountAllEditValue_ + BALL_COUNT_STEP;
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        ballCountAllEditValue_ = ballCountAllEditValue_ - BALL_COUNT_STEP < 1 ? 1 : ballCountAllEditValue_ - BALL_COUNT_STEP;
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        modeManager_->setLiveModeOption("ballCount", ballCountAllEditValue_);
        state_ = State::TopMenu;
        render();
    }
}

void DirectorMenu::handlePlayerPickerEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::TopMenu;
        render();
        return;
    }

    const uint8_t count = modeManager_->playerCount();
    if (count == 0) {
        return;
    }

    if (event.type == EncoderEventType::RotatedClockwise) {
        playerPickerSelectedIndex_ = ScrollList::rotate(playerPickerSelectedIndex_, count, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        playerPickerSelectedIndex_ = ScrollList::rotate(playerPickerSelectedIndex_, count, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        pendingPlayer_ = static_cast<PlayerId>(playerPickerSelectedIndex_);
        const char* key = pendingPlayerEdit_ == PendingPlayerEdit::BallCount ? "roundsRemaining" : "timerSeconds";
        playerEditValue_ = modeManager_->playerOption(pendingPlayer_, key);
        state_ = pendingPlayerEdit_ == PendingPlayerEdit::BallCount ? State::PlayerBallCountEdit : State::PlayerTimeEdit;
        render();
    }
}

void DirectorMenu::handlePlayerEditEncoder(const EncoderEvent& event, MenuHandoff&)
{
    if (event.type == EncoderEventType::SwLongPress) {
        state_ = State::PlayerPicker; // cancel, nothing written -- back to the picker, not the top menu
        render();
        return;
    }

    const bool isBallCount = pendingPlayerEdit_ == PendingPlayerEdit::BallCount;
    const long step = isBallCount ? BALL_COUNT_STEP : PLAYER_TIME_STEP;
    const long maxValue = isBallCount
        ? static_cast<long>(Mode1RoundRobin::MAX_BALL_COUNT)
        : Mode1RoundRobin::MAX_SECONDS_PER_TURN;

    if (event.type == EncoderEventType::RotatedClockwise) {
        playerEditValue_ = playerEditValue_ + step > maxValue ? maxValue : playerEditValue_ + step;
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        playerEditValue_ = playerEditValue_ - step < 0 ? 0 : playerEditValue_ - step;
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        const char* key = isBallCount ? "roundsRemaining" : "timerSeconds";
        modeManager_->setPlayerOption(pendingPlayer_, key, playerEditValue_);
        state_ = State::PlayerPicker; // back to the picker so multiple players can be fixed up in one visit
        render();
    }
}

void DirectorMenu::render()
{
    switch (state_) {
        case State::TopMenu:
            renderTopMenu();
            break;

        case State::BallCountAllEdit:
            renderBallCountAllEdit();
            break;

        case State::PlayerPicker:
            renderPlayerPicker();
            break;

        case State::PlayerBallCountEdit:
        case State::PlayerTimeEdit:
            renderPlayerEdit();
            break;
    }
}

void DirectorMenu::renderTopMenu()
{
    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t visibleCount = TOP_ITEM_COUNT < MAX_VISIBLE_ITEMS ? TOP_ITEM_COUNT : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(topSelectedIndex_, TOP_ITEM_COUNT, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        snprintf(lineBuf[i], LINE_LENGTH, "%s%s",
            itemIndex == topSelectedIndex_ ? "> " : "  ",
            topItemLabel(static_cast<TopItem>(itemIndex)));
        linePtrs[i] = lineBuf[i];
    }

    tft_->showStatusScreen("DIRECTOR MENU", linePtrs, visibleCount, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void DirectorMenu::renderBallCountAllEdit()
{
    char valueLine[16];
    snprintf(valueLine, sizeof(valueLine), "%ld", ballCountAllEditValue_);

    const char* lines[] = {valueLine, "press to confirm", "hold knob to cancel"};
    tft_->showStatusScreen("BALL COUNT (ALL)", lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void DirectorMenu::renderPlayerPicker()
{
    const uint8_t count = modeManager_->playerCount();

    if (count == 0) {
        const char* lines[] = {"No players", "hold knob to go back"};
        tft_->showStatusScreen("SELECT PLAYER", lines, 2, ColorId::Black, ColorId::White, ColorId::Cyan);
        return;
    }

    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t visibleCount = count < MAX_VISIBLE_ITEMS ? count : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(playerPickerSelectedIndex_, count, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        const PlayerId player = static_cast<PlayerId>(itemIndex);
        snprintf(lineBuf[i], LINE_LENGTH, "%sP%u: %s %s",
            itemIndex == playerPickerSelectedIndex_ ? "> " : "  ",
            itemIndex + 1,
            players_->name(player),
            statusAbbreviation(players_->status(player)));
        linePtrs[i] = lineBuf[i];
    }

    const char* title = pendingPlayerEdit_ == PendingPlayerEdit::BallCount ? "PICK: BALL COUNT" : "PICK: TIME";
    tft_->showStatusScreen(title, linePtrs, visibleCount, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void DirectorMenu::renderPlayerEdit()
{
    const bool isBallCount = pendingPlayerEdit_ == PendingPlayerEdit::BallCount;

    char valueLine[16];
    if (isBallCount) {
        snprintf(valueLine, sizeof(valueLine), "%ld", playerEditValue_);
    } else {
        snprintf(valueLine, sizeof(valueLine), "%ld:%02ld", playerEditValue_ / 60, playerEditValue_ % 60);
    }

    const char* lines[] = {valueLine, "press to confirm", "hold knob to cancel"};
    tft_->showStatusScreen(isBallCount ? "PLAYER BALL COUNT" : "PLAYER TIME", lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

const char* DirectorMenu::topItemLabel(TopItem item)
{
    switch (item) {
        case TopItem::ResumeGame: return "Resume Game";
        case TopItem::ResetRound: return "Reset Round";
        case TopItem::BallCountAll: return "Ball Count (All)";
        case TopItem::PlayerBallCount: return "Player Ball Count";
        case TopItem::PlayerTime: return "Player Time";
        case TopItem::ToggleLocalLock: return "Toggle Local Lock";
        case TopItem::EndGame: return "End Game";
        case TopItem::WifiEncoder: return kWifiFeatureEnabled ? "Setup WiFi (Encoder)" : "Setup WiFi (Encoder) - disabled";
        case TopItem::WifiWeb: return kWifiFeatureEnabled ? "Setup WiFi (Web)" : "Setup WiFi (Web) - disabled";
    }
    return "";
}

const char* DirectorMenu::statusAbbreviation(PlayerStatus status)
{
    switch (status) {
        case PlayerStatus::Inactive: return "Inac";
        case PlayerStatus::Waiting: return "Wait";
        case PlayerStatus::Active: return "Act";
        case PlayerStatus::Eliminated: return "Elim";
        case PlayerStatus::Finished: return "Fin";
    }
    return "";
}
