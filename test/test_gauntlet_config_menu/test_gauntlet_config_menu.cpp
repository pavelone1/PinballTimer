#include <unity.h>
#include "modes/gauntlet/GauntletConfigMenu.h"

MachineCatalog catalog;
GauntletConfig config;
GauntletConfigMenu menu;
MachineId machineId;

EncoderEvent press()
{
    return {EncoderEventType::SwShortPress, 0, 0};
}

EncoderEvent clockwise()
{
    return {EncoderEventType::RotatedClockwise, 0, 0};
}

void openMachineChoice()
{
    menu.handleEncoderEvent(clockwise());
    menu.handleEncoderEvent(clockwise());
    menu.handleEncoderEvent(press());
    menu.handleEncoderEvent(press());
}

void setUp()
{
    catalog.clear();
    catalog.add("Stars", MachineType::EM, 3, 180, true, machineId);
    config.reset();
    menu.begin(config, catalog);
}

void tearDown() {}

void test_start_is_first_and_warns_when_machine_unassigned()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Item::StartGauntlet),
        static_cast<int>(menu.selectedItem()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Outcome::None),
        static_cast<int>(menu.handleEncoderEvent(press())));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Screen::ValidationWarning),
        static_cast<int>(menu.screen()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::ValidationError::UnassignedMachine),
        static_cast<int>(menu.lastValidation().error));
}

void test_assigned_configuration_can_start()
{
    config.assignMachine(0, machineId, catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Outcome::StartGauntlet),
        static_cast<int>(menu.handleEncoderEvent(press())));
}

void test_root_menu_order_puts_machine_count_second()
{
    menu.handleEncoderEvent(clockwise());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Item::NumberOfMachines),
        static_cast<int>(menu.selectedItem()));
}

void test_players_choice_is_first_machine_option()
{
    openMachineChoice();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Screen::SelectCatalogMachine),
        static_cast<int>(menu.screen()));
    TEST_ASSERT_EQUAL_UINT16(0, menu.selectedCatalogIndex());

    menu.handleEncoderEvent(press());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::AssignmentKind::PlayersChoice),
        static_cast<int>(config.assignmentKind(0)));
}

void test_random_choice_opens_machine_type_selector()
{
    openMachineChoice();
    menu.handleEncoderEvent(clockwise());
    menu.handleEncoderEvent(press());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfigMenu::Screen::SelectRandomCategory),
        static_cast<int>(menu.screen()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::RandomCategory::EM),
        static_cast<int>(menu.selectedRandomCategory()));

    for (uint8_t i = 0; i < 4; ++i) {
        menu.handleEncoderEvent(clockwise());
    }
    menu.handleEncoderEvent(press());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::AssignmentKind::RandomChoice),
        static_cast<int>(config.assignmentKind(0)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(GauntletConfig::RandomCategory::Any),
        static_cast<int>(config.randomCategory(0)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_start_is_first_and_warns_when_machine_unassigned);
    RUN_TEST(test_assigned_configuration_can_start);
    RUN_TEST(test_root_menu_order_puts_machine_count_second);
    RUN_TEST(test_players_choice_is_first_machine_option);
    RUN_TEST(test_random_choice_opens_machine_type_selector);
    return UNITY_END();
}
