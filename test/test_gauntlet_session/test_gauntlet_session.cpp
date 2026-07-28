#include <unity.h>
#include <cstring>
#include "modes/gauntlet/GauntletSession.h"

void setUp() {}
void tearDown() {}

MachineRecord machine(MachineId id, const char* name, uint8_t balls, uint16_t time)
{
    MachineRecord record;
    record.id = id;
    std::strncpy(record.name, name, MachineRecord::NAME_CAPACITY - 1);
    record.type = MachineType::EM;
    record.ballCount = balls;
    record.playTimeSeconds = time;
    record.hasPlayTime = true;
    return record;
}

void test_pool_is_sum_of_selected_instances()
{
    GauntletSession session;
    TEST_ASSERT_TRUE(session.addMachine(machine(1, "Stars", 3, 180)));
    TEST_ASSERT_TRUE(session.addMachine(machine(1, "Stars", 3, 180)));
    TEST_ASSERT_TRUE(session.addMachine(machine(2, "Space Time", 5, 240)));
    TEST_ASSERT_EQUAL_INT32(600, session.startingPoolSeconds());
}

void test_skipped_machines_return_in_original_order()
{
    GauntletSession session;
    session.addMachine(machine(1, "One", 3, 180));
    session.addMachine(machine(2, "Two", 3, 180));
    session.addMachine(machine(3, "Three", 3, 180));
    TEST_ASSERT_TRUE(session.start());
    TEST_ASSERT_TRUE(session.skipCurrent());
    TEST_ASSERT_EQUAL_UINT32(2, session.currentMachine()->machineId);
    TEST_ASSERT_TRUE(session.completeCurrent());
    TEST_ASSERT_TRUE(session.skipCurrent());
    TEST_ASSERT_EQUAL_UINT32(1, session.currentMachine()->machineId);
    TEST_ASSERT_TRUE(session.completeCurrent());
    TEST_ASSERT_EQUAL_UINT32(3, session.currentMachine()->machineId);
    TEST_ASSERT_TRUE(session.isFinalMachine());
}

void test_removal_deducts_default_and_clamps_at_zero()
{
    GauntletSession session;
    session.addMachine(machine(1, "One", 3, 180));
    session.addMachine(machine(2, "Two", 3, 240));
    session.start();
    long remaining[4] = {200, 180, 100, 500};
    TEST_ASSERT_TRUE(session.removeCurrent(remaining, 4));
    TEST_ASSERT_EQUAL_INT32(20, remaining[0]);
    TEST_ASSERT_EQUAL_INT32(0, remaining[1]);
    TEST_ASSERT_EQUAL_INT32(0, remaining[2]);
    TEST_ASSERT_EQUAL_INT32(320, remaining[3]);
    TEST_ASSERT_EQUAL_UINT32(2, session.currentMachine()->machineId);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_pool_is_sum_of_selected_instances);
    RUN_TEST(test_skipped_machines_return_in_original_order);
    RUN_TEST(test_removal_deducts_default_and_clamps_at_zero);
    return UNITY_END();
}
