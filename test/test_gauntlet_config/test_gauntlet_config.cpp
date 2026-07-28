#include <unity.h>
#include <cstring>
#include "modes/gauntlet/GauntletConfig.h"

MachineCatalog catalog;
MachineId starsId;
MachineId spaceTimeId;

void setUp()
{
    catalog.clear();
    catalog.add("Stars", MachineType::EM, 3, 180, true, starsId);
    catalog.add("Space Time", MachineType::EM, 5, 240, true, spaceTimeId);
}

void tearDown() {}

void test_every_required_instance_must_be_assigned()
{
    GauntletConfig config;
    TEST_ASSERT_TRUE(config.setMachineCount(2));
    TEST_ASSERT_TRUE(config.assignMachine(0, starsId, catalog));
    const auto result = config.validate(catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::ValidationError::UnassignedMachine),
        static_cast<int>(result.error));
    TEST_ASSERT_EQUAL_UINT8(1, result.machineIndex);
}

void test_repeated_database_machine_is_valid_for_separate_instances()
{
    GauntletConfig config;
    config.setMachineCount(2);
    config.assignMachine(0, starsId, catalog);
    config.assignMachine(1, starsId, catalog);
    TEST_ASSERT_TRUE(config.validate(catalog).valid());

    GauntletSession session;
    TEST_ASSERT_TRUE(config.buildSession(catalog, session));
    TEST_ASSERT_EQUAL_UINT16(1, session.machineAt(0)->instanceId);
    TEST_ASSERT_EQUAL_UINT16(2, session.machineAt(1)->instanceId);
    TEST_ASSERT_EQUAL_UINT32(starsId, session.machineAt(0)->machineId);
    TEST_ASSERT_EQUAL_UINT32(starsId, session.machineAt(1)->machineId);
}

void test_decrease_requires_confirmation_when_assignment_would_be_lost()
{
    GauntletConfig config;
    config.setMachineCount(2);
    config.assignMachine(1, spaceTimeId, catalog);
    TEST_ASSERT_FALSE(config.setMachineCount(1));
    TEST_ASSERT_EQUAL_UINT8(2, config.machineCount());
    TEST_ASSERT_TRUE(config.setMachineCount(1, true));
    TEST_ASSERT_EQUAL_UINT8(1, config.machineCount());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_every_required_instance_must_be_assigned);
    RUN_TEST(test_repeated_database_machine_is_valid_for_separate_instances);
    RUN_TEST(test_decrease_requires_confirmation_when_assignment_would_be_lost);
    return UNITY_END();
}

