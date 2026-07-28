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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_start_is_first_and_warns_when_machine_unassigned);
    RUN_TEST(test_assigned_configuration_can_start);
    RUN_TEST(test_root_menu_order_puts_machine_count_second);
    return UNITY_END();
}

