#include <unity.h>
#include "game/DisplayAssignmentManager.h"

DisplayAssignmentManager mgr;

void setUp(void)
{
    mgr.begin();
}

void tearDown(void)
{
}

void test_begin_sets_none(void)
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::None), static_cast<int>(mgr.assignmentType(DisplayId::Display1)));
}

void test_assign_to_shared_timer(void)
{
    mgr.assignToSharedTimer(DisplayId::Display1, 3);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::SharedTimer), static_cast<int>(mgr.assignmentType(DisplayId::Display1)));
    TEST_ASSERT_EQUAL_UINT8(3, mgr.assignment(DisplayId::Display1).timerId);
}

void test_assign_to_player_number(void)
{
    mgr.assignToPlayerNumber(DisplayId::Display2, PlayerId::Player4);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::PlayerNumber), static_cast<int>(mgr.assignmentType(DisplayId::Display2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PlayerId::Player4), static_cast<int>(mgr.assignment(DisplayId::Display2).player));
}

void test_assign_to_score_and_round_use_source_id(void)
{
    mgr.assignToScore(DisplayId::Display3, 1);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::Score), static_cast<int>(mgr.assignmentType(DisplayId::Display3)));
    TEST_ASSERT_EQUAL_UINT8(1, mgr.assignment(DisplayId::Display3).sourceId);

    mgr.assignToRound(DisplayId::Display3, 2);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::Round), static_cast<int>(mgr.assignmentType(DisplayId::Display3)));
    TEST_ASSERT_EQUAL_UINT8(2, mgr.assignment(DisplayId::Display3).sourceId);
}

void test_clear_assignment(void)
{
    mgr.assignToTeam(DisplayId::Display4, 5);
    mgr.clearAssignment(DisplayId::Display4);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::None), static_cast<int>(mgr.assignmentType(DisplayId::Display4)));
}

void test_assignments_are_independent_per_display(void)
{
    mgr.assignToPlayer(DisplayId::Display1, PlayerId::Player1);
    mgr.assignToSharedTimer(DisplayId::Display2, 0);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::SinglePlayer), static_cast<int>(mgr.assignmentType(DisplayId::Display1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::SharedTimer), static_cast<int>(mgr.assignmentType(DisplayId::Display2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DisplayAssignmentType::None), static_cast<int>(mgr.assignmentType(DisplayId::Display3)));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_begin_sets_none);
    RUN_TEST(test_assign_to_shared_timer);
    RUN_TEST(test_assign_to_player_number);
    RUN_TEST(test_assign_to_score_and_round_use_source_id);
    RUN_TEST(test_clear_assignment);
    RUN_TEST(test_assignments_are_independent_per_display);
    return UNITY_END();
}
