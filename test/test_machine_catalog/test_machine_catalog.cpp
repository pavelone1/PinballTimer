#include <unity.h>
#include "game/MachineCatalog.h"

void setUp() {}
void tearDown() {}

void test_add_assigns_stable_monotonic_ids()
{
    MachineCatalog catalog;
    MachineId first = 0;
    MachineId second = 0;
    TEST_ASSERT_TRUE(catalog.add("Stars", MachineType::EM, 3, 180, true, first));
    TEST_ASSERT_TRUE(catalog.remove(first));
    TEST_ASSERT_TRUE(catalog.add("Space Time", MachineType::EM, 5, 0, false, second));
    TEST_ASSERT_EQUAL_UINT32(1, first);
    TEST_ASSERT_EQUAL_UINT32(2, second);
}

void test_validation_enforces_approved_ranges()
{
    MachineCatalog catalog;
    MachineId id = 0;
    TEST_ASSERT_FALSE(catalog.add("Bad Balls", MachineType::DMD, 7, 180, true, id));
    TEST_ASSERT_FALSE(catalog.add("Bad Time", MachineType::Modern, 3, 3601, true, id));
    TEST_ASSERT_TRUE(catalog.add("Fallback", MachineType::SolidState, 1, 0, false, id));
    TEST_ASSERT_EQUAL_UINT16(180, catalog.find(id)->resolvedPlayTimeSeconds());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_add_assigns_stable_monotonic_ids);
    RUN_TEST(test_validation_enforces_approved_ranges);
    return UNITY_END();
}
