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

void test_players_choice_builds_fixed_three_ball_three_minute_instance()
{
    GauntletConfig config;
    TEST_ASSERT_TRUE(config.assignPlayersChoice(0));
    TEST_ASSERT_TRUE(config.validate(catalog).valid());

    GauntletSession session;
    TEST_ASSERT_TRUE(config.buildSession(catalog, session));
    const GauntletMachineInstance* machine = session.machineAt(0);
    TEST_ASSERT_NOT_NULL(machine);
    TEST_ASSERT_EQUAL_STRING("Player's Choice", machine->machineName);
    TEST_ASSERT_EQUAL_UINT8(3, machine->ballCount);
    TEST_ASSERT_EQUAL_UINT16(180, machine->resolvedDefaultTimeSeconds);
    TEST_ASSERT_EQUAL_UINT32(0, machine->machineId);
}

void test_random_choice_selects_only_from_requested_category()
{
    MachineId modernId;
    TEST_ASSERT_TRUE(
        catalog.add("Godzilla", MachineType::Modern, 3, 300, true, modernId));

    GauntletConfig config;
    TEST_ASSERT_TRUE(config.assignRandomChoice(
        0, GauntletConfig::RandomCategory::Modern));
    TEST_ASSERT_TRUE(config.validate(catalog).valid());

    GauntletSession session;
    TEST_ASSERT_TRUE(config.buildSession(catalog, session, 12345));
    TEST_ASSERT_EQUAL_UINT32(modernId, session.machineAt(0)->machineId);
}

void test_random_choice_requires_a_matching_database_machine()
{
    GauntletConfig config;
    TEST_ASSERT_TRUE(config.assignRandomChoice(
        0, GauntletConfig::RandomCategory::DMD));
    const auto result = config.validate(catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::ValidationError::NoRandomCandidates),
        static_cast<int>(result.error));
    TEST_ASSERT_EQUAL_UINT8(0, result.machineIndex);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_every_required_instance_must_be_assigned);
    RUN_TEST(test_repeated_database_machine_is_valid_for_separate_instances);
    RUN_TEST(test_decrease_requires_confirmation_when_assignment_would_be_lost);
    RUN_TEST(test_players_choice_builds_fixed_three_ball_three_minute_instance);
    RUN_TEST(test_random_choice_selects_only_from_requested_category);
    RUN_TEST(test_random_choice_requires_a_matching_database_machine);
    return UNITY_END();
}
