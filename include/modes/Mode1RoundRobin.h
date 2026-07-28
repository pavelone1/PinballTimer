#pragma once

#include <cstdint>
#include "game/GameMode.h"
#include "game/RoundRobinTurnEngine.h"

// Mode 1: 4-Player Round-Robin Timer, per CLAUDE.md's "Mode 1 rules".
//
// - 1-4 players, each with ONE configurable time budget for the
//   WHOLE game (chess-clock style) -- not a fresh allowance per ball.
//   Ending a turn by pressing the button PAUSES that player's clock;
//   it resumes counting down from the same remaining value next time
//   TurnRotation comes back to them, it does not reset to the full
//   value. Only two things ever change a player's remaining time:
//   it ticking down while their turn is active, or the whole game
//   being reset (BootMenu/DirectorMenu "Reset Round" -> onReset() ->
//   resetRoundState(), which does restore everyone to the full
//   configured value, same as the very start of a fresh game).
// - The Action button starts the round and the first turn (whichever
//   player is assigned to the Red-wired button, see ButtonColors.h).
// - A player taps their OWN button to end their turn -- but only
//   after they've pressed it once already to *start* their own clock
//   (their button flashes in the meantime); a second press ends it.
// - Ball count ("rounds"): each player starts with a configurable
//   number of rounds (default 3, max 5) -- this counts down how many
//   TURNS a player gets before they're done, independent of their
//   (shared, non-resetting) clock. Ending a turn by pressing the
//   button costs one round; on a player's LAST round, their display
//   instead freezes at whatever time was left (PlayerStatus
//   Finished). If a player's timer reaches zero before they press
//   (timeout), their WHOLE game ends immediately regardless of rounds
//   left: display freezes and flashes at 0, their button goes dark,
//   the buzzer sounds for 3s (PlayerStatus Eliminated). Turn order
//   after any hand-off is the fixed physical color order (see
//   ButtonColors.h/TurnRotation.h), skipping Eliminated/Finished
//   players; the game ends once nobody Waiting remains.
// - secondsPerTurn defaults to 3:00 (180s); adjustable per-mode via
//   setModeOption("secondsPerTurn", ...) -- see BootMenu's "Mode
//   Config" menu (on-device) or DirectorDashboard's /game-setup page
//   (remote, currently disabled -- see FeatureFlags.h).
// - Tapping the (white) Action button once a round is in play pauses
//   the active player's clock -- their button goes dark, Action
//   flashes, the TFT says PAUSED; tapping Action again resumes. The
//   toggle fires on RELEASE, not press-down (see onButtonEvent()),
//   specifically so it can never race with holding Action 5s to open
//   DirectorMenu (App::updateDirectorMenuHold(), a separate, longer
//   poll): once that gesture opens DirectorMenu mid-hold, the eventual
//   release of that same press is swallowed by App's menu-open guard
//   before it ever reaches here, so the long-hold always ends up
//   cleanly paused with no partial un-pause blip in between.
class Mode1RoundRobin : public GameMode {
public:
    static constexpr uint8_t MODE_ID = 1;
    static constexpr long DEFAULT_SECONDS_PER_TURN = 180;
    static constexpr long MAX_SECONDS_PER_TURN = 5999;
    static constexpr uint8_t DEFAULT_BALL_COUNT = 3;
    static constexpr uint8_t MAX_BALL_COUNT = 5;

    const char* name() const override;
    uint8_t id() const override;

    uint8_t minPlayers() const override;
    uint8_t maxPlayers() const override;
    uint8_t defaultPlayerCount() const override;

    void setSecondsPerTurn(long seconds);
    long secondsPerTurn() const;

    void setBallCount(uint8_t balls);
    uint8_t ballCount() const;

    void setupAssignments(GameModeContext& context, uint8_t playerCount) override;
    bool restoreState(GameModeContext& context) override;

    void update(GameModeContext& context) override;
    void onPause(GameModeContext& context) override;
    void onResume(GameModeContext& context) override;
    void onStop(GameModeContext& context) override;
    void onReset(GameModeContext& context) override;

    bool onButtonEvent(GameModeContext& context, const ButtonEvent& event) override;

    // Recognizes "secondsPerTurn" (1-5999) and "ballCount" (1-5).
    bool setModeOption(const char* key, long value) override;
    long modeOption(const char* key) const override;

    // Director-tool live adjustment for one specific player, mid-game.
    // Recognizes "roundsRemaining" (0-ballCount_) and "timerSeconds"
    // (0-MAX_SECONDS_PER_TURN) -- either may revive an Eliminated/
    // Finished player back into rotation, see tryRevive().
    bool setPlayerOption(GameModeContext& context, PlayerId player, const char* key, long value) override;
    long playerOption(GameModeContext& context, PlayerId player, const char* key) const override;

    // Director-tool live adjustment for the whole in-progress game.
    // Recognizes "ballCount" -- redistributes the delta to every
    // already-assigned player's roundsRemaining (preserving balls
    // already used), distinct from setModeOption("ballCount", ...)
    // which only affects the NEXT game's setupAssignments().
    bool setLiveModeOption(GameModeContext& context, const char* key, long value) override;

    bool isRoundOver() const override;

    // Allows every remote command unconditionally. This is a
    // placeholder judgment call (remote/director control is in scope
    // per earlier discussion, but which specific commands Mode 1
    // should permit was never itemized) -- narrow it if needed.
    bool allowsRemoteCommand(uint8_t commandId) const override;

private:
    static constexpr uint8_t MAX_MODE_PLAYERS = 4;
    static constexpr uint8_t ACTION_START_ROUND = 1;
    static constexpr unsigned long HANDOFF_FLASH_INTERVAL_MS = 500;
    static constexpr unsigned long GAME_OVER_FLASH_INTERVAL_MS = 300; // Eliminated AND Finished both flash with this -- "this player's game is over"

    long secondsPerTurn_ = DEFAULT_SECONDS_PER_TURN;
    uint8_t ballCount_ = DEFAULT_BALL_COUNT;
    uint8_t playerCount_ = 0;

    // Whose turn it is, by ButtonId -- turn order follows the fixed
    // physical button color order (ButtonColors.h), not PlayerId,
    // since a player can be assigned to any button/color.
    ButtonId activeButton_ = ButtonId::Action;

    // False while the active player's button is flashing, waiting for
    // their own first press to start their clock; true once it's
    // actually counting down. See onButtonEvent()'s two-press split.
    bool activeTimerRunning_ = false;

    bool roundStarted_ = false;
    bool gameOver_ = false;
    TimerId playerTimerIds_[MAX_MODE_PLAYERS] = {};

    // Local "tap the white Action button to pause/resume" toggle --
    // deliberately separate from GameModeManager's own isPaused()/
    // notifyPause() (the DirectorMenu-hold pause): GameModeContext
    // doesn't give a mode a way to call back into GameModeManager, and
    // conceptually this is just "the active player's clock is
    // manually held" the same way a handoff-flash already has nothing
    // running -- it doesn't need to be the same flag DirectorMenu
    // uses to freeze the whole system. While true, the active
    // player's own button stops responding (see onButtonEvent()).
    bool manuallyPaused_ = false;

    // Transient input-gesture guard, not persisted round state: true
    // for exactly the one Released that would otherwise immediately
    // toggle-pause the round its own Pressed just started. Cleared
    // unconditionally on every subsequent Action Pressed (not only
    // when a Released consumes it), since the Released for a press
    // that turns into App::updateDirectorMenuHold()'s 5-second hold
    // never reaches onButtonEvent() at all (DirectorMenu swallows it)
    // -- leaving it set would otherwise incorrectly suppress the next,
    // unrelated tap.
    bool suppressNextActionRelease_ = false;

    // Shared physical turn state machine also used by Gauntlet. The legacy
    // scalar fields above remain the persisted/public-mode state during this
    // compatibility refactor; this engine is synchronized at transition
    // boundaries and owns first/second player-press and color-order advance
    // decisions.
    RoundRobinTurnEngine turnEngine_;

    void startRound(GameModeContext& context);

    // Toggles manuallyPaused_ -- stops/restarts the active player's
    // timer (only if activeTimerRunning_; a handoff-flash has nothing
    // running to stop) and swaps which button (the active player's vs
    // Action) is lit solid/flashing, mirroring the flash-until-pressed
    // convention already used for hand-offs.
    void togglePause(GameModeContext& context);

    // Replaces the old advanceTurn(): ends the current player's turn
    // either because they pressed their button (timedOut=false) or
    // their clock ran out untouched (timedOut=true), then hands off to
    // the next Waiting player in color order (or ends the game).
    void handleTurnEnd(GameModeContext& context, bool timedOut);

    void resetRoundState(GameModeContext& context);

    // Shared revival check used by both setPlayerOption() and
    // setLiveModeOption() -- a player becomes eligible to rejoin once
    // BOTH their roundsRemaining and timer value are positive, checked
    // fresh after whichever one a director tool just edited (an
    // Eliminated/timed-out player usually only needs a time bump; a
    // Finished/out-of-balls player usually only needs a rounds bump).
    // No-ops if the player isn't Eliminated/Finished, or isn't yet
    // eligible. If the whole game had already ended, also re-derives
    // gameOver_/activeButton_ by resuming TurnRotation from the stale
    // activeButton_ left behind when the game ended.
    void tryRevive(GameModeContext& context, PlayerId player);

    // Snapshots current round state (player count, whose turn, each
    // player's remaining seconds/rounds/status) to GameStorage. Called
    // on start/turn-advance/pause -- NOT continuously, to avoid
    // writing NVS every tick; see SavedRoundState's doc comment in
    // GameStorage.h for the accuracy tradeoff that implies.
    void checkpointRoundState(GameModeContext& context);

    // Shows the active/up-next player's name and ball number (1-based,
    // counting up to ballCount_ -- see roundsRemaining's countdown-from
    // -ballCount_ semantics) on the TFT. Called unconditionally every
    // update() tick rather than only at hand-off points: showStatusScreen()
    // no-ops when title/lines/colors are unchanged (TftDisplayManager's
    // own cache), so this is cheap, and it self-heals the screen after
    // ANY menu (BootMenu/DirectorMenu/WifiSetupMenu/WifiPortal) closes
    // without needing an explicit redraw hook in onResume()/restoreState().
    void renderGameStatus(GameModeContext& context);
};
