#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/GameModeManager.h"
#include "game/PlayerManager.h"
#include "network/DirectorControl.h"
#include "output/TftDisplayManager.h"
#include "ui/MenuHandoff.h"

// On-device director menu: opened by App when the Action button is
// held for 5s during a running mode or idle Setup state (see
// App::updateDirectorMenuHold()), which also pauses the active mode
// first (a no-op if no round had started). While open, this owns the
// TFT and the rotary encoder -- App stops routing encoder/button
// events to the active GameMode and routes them here instead.
//
// Top-level items (see TopItem): Resume Game / Reset Round / Ball
// Count (All) / Player Ball Count / Player Time / Toggle Local Lock /
// End Game / Setup WiFi (Encoder) / Setup WiFi (Web). The last two
// stay "- disabled" and inert while WiFi is off (see FeatureFlags.h),
// same convention BootMenu already uses. "Reset Round" restarts the
// SAME game fresh and stays in gameplay; "End Game" fully ends it and
// returns to BootMenu (MenuHandoff::EndGame) -- two different things.
//
// Ball Count (All)/Player Ball Count/Player Time are stateful
// (State::BallCountAllEdit / PlayerPicker + PlayerBallCountEdit or
// PlayerTimeEdit), mirroring BootMenu's Mode Config submenu exactly:
// entering an editor seeds a local working value from the live
// source (GameModeManager::playerOption()/GameMode::modeOption()),
// rotation only adjusts that local copy, SwShortPress writes through
// (GameModeManager::setPlayerOption()/setLiveModeOption()) and steps
// back one level, SwLongPress discards and steps back one level --
// nothing is written on cancel. Editing a player's ball count or time
// may revive them from Eliminated/Finished back into rotation (see
// Mode1RoundRobin::tryRevive()) -- DirectorMenu itself has no opinion
// on this, it just calls through the generic option interface.
class DirectorMenu {
public:
    void open(GameModeManager& modeManager, DirectorControl& directorControl, PlayerManager& players, TftDisplayManager& tft);
    void close(TftDisplayManager& tft);
    bool isOpen() const;

    MenuHandoff handleEncoderEvent(const EncoderEvent& event);

private:
    enum class State : uint8_t {
        TopMenu,
        BallCountAllEdit,
        PlayerPicker,
        PlayerBallCountEdit,
        PlayerTimeEdit
    };

    enum class TopItem : uint8_t {
        ResumeGame,
        ResetRound,
        BallCountAll,
        PlayerBallCount,
        PlayerTime,
        ToggleLocalLock,
        EndGame,
        WifiEncoder,
        WifiWeb
    };

    enum class PendingPlayerEdit : uint8_t {
        BallCount,
        Time
    };

    static constexpr uint8_t MAX_VISIBLE_ITEMS = 5; // must not exceed TftDisplayManager::MAX_LINES
    static constexpr uint8_t TOP_ITEM_COUNT = 9;
    static constexpr long BALL_COUNT_STEP = 1;
    static constexpr long PLAYER_TIME_STEP = 15;

    bool open_ = false;
    State state_ = State::TopMenu;

    GameModeManager* modeManager_ = nullptr;
    DirectorControl* directorControl_ = nullptr;
    PlayerManager* players_ = nullptr;
    TftDisplayManager* tft_ = nullptr;

    uint8_t topSelectedIndex_ = 0;
    long ballCountAllEditValue_ = 0;

    uint8_t playerPickerSelectedIndex_ = 0;
    PendingPlayerEdit pendingPlayerEdit_ = PendingPlayerEdit::BallCount;
    PlayerId pendingPlayer_ = PlayerId::Player1;
    long playerEditValue_ = 0;

    void handleTopMenuEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handleBallCountAllEditEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handlePlayerPickerEncoder(const EncoderEvent& event, MenuHandoff& outcome);
    void handlePlayerEditEncoder(const EncoderEvent& event, MenuHandoff& outcome);

    void render();
    void renderTopMenu();
    void renderBallCountAllEdit();
    void renderPlayerPicker();
    void renderPlayerEdit();

    static const char* topItemLabel(TopItem item);
    static const char* statusAbbreviation(PlayerStatus status);
};
