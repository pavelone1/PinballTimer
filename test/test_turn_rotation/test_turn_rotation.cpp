#include <unity.h>
#include "game/TurnRotation.h"
#include "game/ButtonAssignmentManager.h"
#include "game/PlayerManager.h"

ButtonAssignmentManager buttons;
PlayerManager players;

void setUp(void)
{
    buttons.begin();
    players.begin();

    // Default identity assignment, all four players Waiting -- matches
    // Mode1RoundRobin::setupAssignments()'s default (pre-reassignment) state.
    buttons.assignToPlayer(ButtonId::Player1, PlayerId::Player1);
    buttons.assignToPlayer(ButtonId::Player2, PlayerId::Player2);
    buttons.assignToPlayer(ButtonId::Player3, PlayerId::Player3);
    buttons.assignToPlayer(ButtonId::Player4, PlayerId::Player4);
    players.setStatus(PlayerId::Player1, PlayerStatus::Waiting);
    players.setStatus(PlayerId::Player2, PlayerStatus::Waiting);
    players.setStatus(PlayerId::Player3, PlayerStatus::Waiting);
    players.setStatus(PlayerId::Player4, PlayerStatus::Waiting);
}

void tearDown(void)
{
}

void test_first_button_is_red_by_default(void)
{
    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::firstButton(buttons, players, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player1), static_cast<int>(out));
}

void test_first_button_is_red_even_when_reassigned(void)
{
    // Player1 (the logical player) is now sitting on the Green button;
    // Red is still whichever physical button is wired Red, occupied by
    // whoever got assigned there -- first-to-go is still that button.
    buttons.assignToPlayer(ButtonId::Player1, PlayerId::Player3);
    buttons.assignToPlayer(ButtonId::Player3, PlayerId::Player1);

    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::firstButton(buttons, players, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player1), static_cast<int>(out));
}

void test_next_button_normal_order(void)
{
    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::nextButton(buttons, players, ButtonId::Player1, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player2), static_cast<int>(out));
}

void test_next_button_wraps_past_last(void)
{
    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::nextButton(buttons, players, ButtonId::Player4, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player1), static_cast<int>(out));
}

void test_next_button_skips_eliminated(void)
{
    players.setStatus(PlayerId::Player2, PlayerStatus::Eliminated);

    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::nextButton(buttons, players, ButtonId::Player1, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player3), static_cast<int>(out));
}

void test_next_button_skips_finished(void)
{
    players.setStatus(PlayerId::Player2, PlayerStatus::Finished);
    players.setStatus(PlayerId::Player3, PlayerStatus::Finished);

    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::nextButton(buttons, players, ButtonId::Player1, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player4), static_cast<int>(out));
}

void test_next_button_returns_same_player_when_only_one_left(void)
{
    players.setStatus(PlayerId::Player1, PlayerStatus::Eliminated);
    players.setStatus(PlayerId::Player2, PlayerStatus::Eliminated);
    players.setStatus(PlayerId::Player4, PlayerStatus::Finished);
    // Player3 is the only one still Waiting.

    ButtonId out;
    TEST_ASSERT_TRUE(TurnRotation::nextButton(buttons, players, ButtonId::Player3, out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player3), static_cast<int>(out));
}

void test_next_button_false_when_all_done(void)
{
    players.setStatus(PlayerId::Player1, PlayerStatus::Eliminated);
    players.setStatus(PlayerId::Player2, PlayerStatus::Finished);
    players.setStatus(PlayerId::Player3, PlayerStatus::Eliminated);
    players.setStatus(PlayerId::Player4, PlayerStatus::Finished);

    ButtonId out;
    TEST_ASSERT_FALSE(TurnRotation::nextButton(buttons, players, ButtonId::Player1, out));
}

void test_first_button_false_when_nobody_waiting(void)
{
    players.setStatus(PlayerId::Player1, PlayerStatus::Inactive);
    players.setStatus(PlayerId::Player2, PlayerStatus::Inactive);
    players.setStatus(PlayerId::Player3, PlayerStatus::Inactive);
    players.setStatus(PlayerId::Player4, PlayerStatus::Inactive);

    ButtonId out;
    TEST_ASSERT_FALSE(TurnRotation::firstButton(buttons, players, out));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_button_is_red_by_default);
    RUN_TEST(test_first_button_is_red_even_when_reassigned);
    RUN_TEST(test_next_button_normal_order);
    RUN_TEST(test_next_button_wraps_past_last);
    RUN_TEST(test_next_button_skips_eliminated);
    RUN_TEST(test_next_button_skips_finished);
    RUN_TEST(test_next_button_returns_same_player_when_only_one_left);
    RUN_TEST(test_next_button_false_when_all_done);
    RUN_TEST(test_first_button_false_when_nobody_waiting);
    return UNITY_END();
}
