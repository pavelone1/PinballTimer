#include "modes/gauntlet/GauntletConfigMenu.h"

#include <cstdio>
#include "output/TftDisplayManager.h"

namespace {
constexpr uint8_t MAX_VISIBLE_ROWS = 5;

uint16_t windowStart(uint16_t selected, uint16_t total)
{
    if (total <= MAX_VISIBLE_ROWS || selected < MAX_VISIBLE_ROWS) {
        return 0;
    }
    return selected - MAX_VISIBLE_ROWS + 1;
}
}

void GauntletConfigMenu::render(TftDisplayManager& tft) const
{
    char value[32];
    char rows[MAX_VISIBLE_ROWS][MachineRecord::NAME_CAPACITY + 8] = {};
    const char* lines[MAX_VISIBLE_ROWS] = {};
    uint8_t count = 0;
    switch (screen_) {
        case Screen::Root: {
            static const char* labels[] = {
                "Start Gauntlet", "Number of Machines", "Assign Machines",
                "Number of Players", "Player Setup", "Back"
            };
            const uint16_t total = static_cast<uint8_t>(Item::Count);
            const uint16_t selected = static_cast<uint8_t>(selectedItem_);
            const uint16_t start = windowStart(selected, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                std::snprintf(rows[count], sizeof(rows[count]), "%c %s",
                              i == selected ? '>' : ' ', labels[i]);
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::EditMachineCount:
            std::snprintf(value, sizeof(value), "%u Machines", pendingMachineCount_);
            lines[count++] = value; lines[count++] = "Press to Save";
            break;
        case Screen::ConfirmMachineCountDecrease:
            lines[count++] = "Assigned Machines";
            lines[count++] = "Will Be Removed";
            lines[count++] = "Press to Confirm";
            break;
        case Screen::SelectMachineSlot: {
            const uint16_t total = config_->machineCount();
            const uint16_t start = windowStart(selectedSlot_, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                const MachineId id = config_->machineAssignment(i);
                const MachineRecord* record = id ? catalog_->find(id) : nullptr;
                std::snprintf(rows[count], sizeof(rows[count]), "%c %u: %s",
                              i == selectedSlot_ ? '>' : ' ',
                              static_cast<unsigned>(i + 1),
                              record ? record->name : "Not Assigned");
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::SelectCatalogMachine: {
            const uint16_t total = catalog_->count();
            if (total == 0) {
                lines[count++] = "Database Is Empty";
                break;
            }
            const uint16_t start = windowStart(catalogIndex_, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                const MachineRecord* record = catalog_->at(i);
                std::snprintf(rows[count], sizeof(rows[count]), "%c %s",
                              i == catalogIndex_ ? '>' : ' ', record->name);
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::EditPlayerCount:
            std::snprintf(value, sizeof(value), "%u Players", pendingPlayerCount_);
            lines[count++] = value; lines[count++] = "Press to Save";
            break;
        case Screen::ValidationWarning:
            std::snprintf(value, sizeof(value), "Machine %u Not Assigned",
                          lastValidation_.machineIndex + 1);
            lines[count++] = value;
            lines[count++] = "Assign Every Machine";
            lines[count++] = "Before Starting";
            break;
    }
    tft.showStatusScreen(title(), lines, count, ColorId::Black,
                         screen_ == Screen::ValidationWarning ? ColorId::Red : ColorId::White,
                         ColorId::Cyan);
}
