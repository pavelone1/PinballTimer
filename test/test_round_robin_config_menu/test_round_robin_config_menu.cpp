#include <unity.h>
#include "modes/round_robin/RoundRobinConfigMenu.h"

MachineCatalog catalog;
RoundRobinConfig config;
RoundRobinConfigMenu menu;

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
    config.reset();
    menu.begin(config, catalog);
}

void tearDown() {}

void test_start_is_first_and_succeeds_without_machine()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinConfigMenu::Item::StartRoundRobin),
        static_cast<int>(menu.selectedItem()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinConfigMenu::Outcome::StartRoundRobin),
        static_cast<int>(menu.handleEncoderEvent(press())));
}

void test_menu_order_matches_mode_owned_structure()
{
    const RoundRobinConfigMenu::Item expected[] = {
        RoundRobinConfigMenu::Item::StartRoundRobin,
        RoundRobinConfigMenu::Item::NumberOfPlayers,
        RoundRobinConfigMenu::Item::SelectMachine,
        RoundRobinConfigMenu::Item::GameTime,
        RoundRobinConfigMenu::Item::BallCount,
        RoundRobinConfigMenu::Item::PlayerSetup,
        RoundRobinConfigMenu::Item::Back
    };
    for (uint8_t i = 0; i < 7; ++i) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(expected[i]),
            static_cast<int>(menu.selectedItem()));
        menu.handleEncoderEvent(clockwise());
    }
}

void test_empty_catalog_still_exposes_none_selection()
{
    menu.handleEncoderEvent(clockwise());
    menu.handleEncoderEvent(clockwise());
    menu.handleEncoderEvent(press());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RoundRobinConfigMenu::Screen::SelectCatalogMachine),
        static_cast<int>(menu.screen()));
    menu.handleEncoderEvent(press());
    TEST_ASSERT_FALSE(config.hasMachineSelection());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_start_is_first_and_succeeds_without_machine);
    RUN_TEST(test_menu_order_matches_mode_owned_structure);
    RUN_TEST(test_empty_catalog_still_exposes_none_selection);
    return UNITY_END();
}

