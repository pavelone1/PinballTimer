#include <unity.h>
#include "game/RoundRobinTurnEngine.h"

PlayerManager players;
ButtonAssignmentManager assignments;
RoundRobinTurnEngine engine;

void setUp()
{
    players.begin();
    players.setActivePlayerCount(2);
    assignments.begin();
    assignments.assignToPlayer(ButtonId::Player1, PlayerId::Player1);
    assignments.assignToPlayer(ButtonId::Player2, PlayerId::Player2);
    engine.reset();
}

void tearDown() {}

void test_start_and_two_press_handoff_mechanism()
{
    TEST_ASSERT_TRUE(engine.start(assignments, players));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player1),
                          static_cast<int>(engine.activeButton()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinTurnEngine::PlayerPressResult::EndBall),
        static_cast<int>(engine.handlePlayerPress(ButtonId::Player1)));
    players.setStatus(PlayerId::Player1, PlayerStatus::Waiting);
    TEST_ASSERT_TRUE(engine.advance(assignments, players));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player2),
                          static_cast<int>(engine.activeButton()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinTurnEngine::PlayerPressResult::StartTimer),
        static_cast<int>(engine.handlePlayerPress(ButtonId::Player2)));
}

void test_pause_blocks_player_press()
{
    engine.start(assignments, players);
    TEST_ASSERT_TRUE(engine.togglePause());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinTurnEngine::PlayerPressResult::Ignored),
        static_cast<int>(engine.handlePlayerPress(ButtonId::Player1)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_start_and_two_press_handoff_mechanism);
    RUN_TEST(test_pause_blocks_player_press);
    return UNITY_END();
}
