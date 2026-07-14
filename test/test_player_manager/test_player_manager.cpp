#include <unity.h>
#include <cstring>
#include "game/PlayerManager.h"

PlayerManager players;

void setUp(void)
{
    players.begin();
}

void tearDown(void)
{
}

void test_begin_sets_defaults(void)
{
    TEST_ASSERT_EQUAL_UINT8(1, players.displayedNumber(PlayerId::Player1));
    TEST_ASSERT_EQUAL_UINT8(4, players.displayedNumber(PlayerId::Player4));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Inactive), static_cast<int>(players.status(PlayerId::Player1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonId::Player3), static_cast<int>(players.buttonAssignment(PlayerId::Player3)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayId::Display2), static_cast<int>(players.displayAssignment(PlayerId::Player2)));
    TEST_ASSERT_EQUAL_UINT8(0, players.activePlayerCount());
}

void test_set_active_player_count_activates_waiting(void)
{
    players.setActivePlayerCount(2);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Waiting), static_cast<int>(players.status(PlayerId::Player1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Waiting), static_cast<int>(players.status(PlayerId::Player2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Inactive), static_cast<int>(players.status(PlayerId::Player3)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Inactive), static_cast<int>(players.status(PlayerId::Player4)));
    TEST_ASSERT_EQUAL_UINT8(2, players.activePlayerCount());
}

void test_set_active_player_count_does_not_touch_active_slots(void)
{
    players.setActivePlayerCount(4);
    players.setStatus(PlayerId::Player1, PlayerStatus::Active);

    players.setActivePlayerCount(4);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Active), static_cast<int>(players.status(PlayerId::Player1)));
}

void test_set_active_player_count_deactivates_out_of_range_slots(void)
{
    players.setActivePlayerCount(4);
    players.setStatus(PlayerId::Player4, PlayerStatus::Active);

    players.setActivePlayerCount(2);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerStatus::Inactive), static_cast<int>(players.status(PlayerId::Player4)));
}

void test_set_active_player_count_clamps_above_max(void)
{
    players.setActivePlayerCount(200);
    TEST_ASSERT_EQUAL_UINT8(4, players.activePlayerCount());
}

void test_name_round_trip_and_truncation(void)
{
    players.setName(PlayerId::Player1, "Alice");
    TEST_ASSERT_EQUAL_STRING("Alice", players.name(PlayerId::Player1));

    players.setName(PlayerId::Player2, "ThisNameIsWayTooLongForTheBuffer");
    TEST_ASSERT_EQUAL_INT(15, static_cast<int>(strlen(players.name(PlayerId::Player2))));
}

void test_mode_data_bounds(void)
{
    players.setModeData(PlayerId::Player1, 0, 42);
    TEST_ASSERT_EQUAL_UINT8(42, players.modeData(PlayerId::Player1, 0));

    players.setModeData(PlayerId::Player1, 100, 99);
    TEST_ASSERT_EQUAL_UINT8(0, players.modeData(PlayerId::Player1, 100));
}

void test_permanent_id_independent_of_current_slot(void)
{
    players.setCurrentSlot(PlayerId::Player1, PlayerId::Player3);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerId::Player3), static_cast<int>(players.currentSlot(PlayerId::Player1)));
    // PlayerId::Player1 itself (the permanent id used as the lookup key) is untouched by this.
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_begin_sets_defaults);
    RUN_TEST(test_set_active_player_count_activates_waiting);
    RUN_TEST(test_set_active_player_count_does_not_touch_active_slots);
    RUN_TEST(test_set_active_player_count_deactivates_out_of_range_slots);
    RUN_TEST(test_set_active_player_count_clamps_above_max);
    RUN_TEST(test_name_round_trip_and_truncation);
    RUN_TEST(test_mode_data_bounds);
    RUN_TEST(test_permanent_id_independent_of_current_slot);
    return UNITY_END();
}
