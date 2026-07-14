#include <unity.h>
#include "game/ButtonAssignmentManager.h"

ButtonAssignmentManager mgr;

void setUp(void)
{
    mgr.begin();
}

void tearDown(void)
{
}

void test_begin_sets_none(void)
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::None), static_cast<int>(mgr.assignmentType(ButtonId::Player1)));
}

void test_assign_to_player(void)
{
    mgr.assignToPlayer(ButtonId::Player2, PlayerId::Player3);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::SinglePlayer), static_cast<int>(mgr.assignmentType(ButtonId::Player2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerId::Player3), static_cast<int>(mgr.assignment(ButtonId::Player2).player));
}

void test_assign_to_players_mask(void)
{
    mgr.assignToPlayers(ButtonId::Player1, 0b1010);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::MultiplePlayers), static_cast<int>(mgr.assignmentType(ButtonId::Player1)));
    TEST_ASSERT_EQUAL_UINT8(0b1010, mgr.assignment(ButtonId::Player1).playerMask);
}

void test_assign_to_mode_action(void)
{
    mgr.assignToModeAction(ButtonId::Action, 7);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::ModeAction), static_cast<int>(mgr.assignmentType(ButtonId::Action)));
    TEST_ASSERT_EQUAL_UINT8(7, mgr.assignment(ButtonId::Action).modeActionId);
}

void test_reassign_resets_other_fields_to_struct_defaults(void)
{
    mgr.assignToPlayer(ButtonId::Action, PlayerId::Player4);
    mgr.assignToModeAction(ButtonId::Action, 7);

    const ButtonAssignment& a = mgr.assignment(ButtonId::Action);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::ModeAction), static_cast<int>(a.type));
    TEST_ASSERT_EQUAL_UINT8(7, a.modeActionId);
    // player field resets to the struct's default (Player1), not left
    // over as the stale Player4 from the previous assignment.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerId::Player1), static_cast<int>(a.player));
}

void test_clear_assignment(void)
{
    mgr.assignToTeam(ButtonId::Player3, 5);
    mgr.clearAssignment(ButtonId::Player3);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::None), static_cast<int>(mgr.assignmentType(ButtonId::Player3)));
}

void test_assignments_are_independent_per_button(void)
{
    mgr.assignToPlayer(ButtonId::Player1, PlayerId::Player1);
    mgr.assignToTeam(ButtonId::Player2, 3);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::SinglePlayer), static_cast<int>(mgr.assignmentType(ButtonId::Player1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::Team), static_cast<int>(mgr.assignmentType(ButtonId::Player2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonAssignmentType::None), static_cast<int>(mgr.assignmentType(ButtonId::Player3)));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_begin_sets_none);
    RUN_TEST(test_assign_to_player);
    RUN_TEST(test_assign_to_players_mask);
    RUN_TEST(test_assign_to_mode_action);
    RUN_TEST(test_reassign_resets_other_fields_to_struct_defaults);
    RUN_TEST(test_clear_assignment);
    RUN_TEST(test_assignments_are_independent_per_button);
    return UNITY_END();
}
