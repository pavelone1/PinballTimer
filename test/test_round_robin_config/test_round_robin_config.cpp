#include <unity.h>
#include "modes/round_robin/RoundRobinConfig.h"

MachineCatalog catalog;
MachineId machineId;

void setUp()
{
    catalog.clear();
    catalog.add("Stars", MachineType::EM, 3, 180, true, machineId);
}

void tearDown() {}

void test_defaults_are_valid_without_machine_selection()
{
    RoundRobinConfig config;
    TEST_ASSERT_FALSE(config.hasMachineSelection());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinConfig::ValidationError::None),
        static_cast<int>(config.validate(catalog)));
    TEST_ASSERT_EQUAL_UINT8(4, config.playerCount());
    TEST_ASSERT_EQUAL_INT32(180, config.gameTimeSeconds());
    TEST_ASSERT_EQUAL_UINT8(3, config.ballCount());
}

void test_machine_selection_is_optional_and_can_be_cleared()
{
    RoundRobinConfig config;
    TEST_ASSERT_TRUE(config.selectMachine(machineId, catalog));
    TEST_ASSERT_TRUE(config.hasMachineSelection());
    config.clearMachineSelection();
    TEST_ASSERT_FALSE(config.hasMachineSelection());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinConfig::ValidationError::None),
        static_cast<int>(config.validate(catalog)));
}

void test_approved_ranges_are_enforced()
{
    RoundRobinConfig config;
    TEST_ASSERT_FALSE(config.setPlayerCount(0));
    TEST_ASSERT_FALSE(config.setGameTimeSeconds(6000));
    TEST_ASSERT_FALSE(config.setBallCount(6));
    TEST_ASSERT_TRUE(config.setPlayerCount(1));
    TEST_ASSERT_TRUE(config.setGameTimeSeconds(5999));
    TEST_ASSERT_TRUE(config.setBallCount(5));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_are_valid_without_machine_selection);
    RUN_TEST(test_machine_selection_is_optional_and_can_be_cleared);
    RUN_TEST(test_approved_ranges_are_enforced);
    return UNITY_END();
}

