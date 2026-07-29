#include "modes/gauntlet/GauntletConfigMenu.h"

#include <cstdio>
#include "output/TftDisplayManager.h"

namespace {
constexpr uint8_t MAX_VISIBLE_ROWS = 5;
const char* const RANDOM_CATEGORY_LABELS[] = {
    "EM", "Solid State", "DMD", "Modern", "ANY"
};

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
                const auto kind = config_->assignmentKind(i);
                const char* assignmentName = record ? record->name : "Not Assigned";
                char specialName[24] = {};
                if (kind == GauntletConfig::AssignmentKind::PlayersChoice) {
                    assignmentName = "Player's Choice";
                } else if (kind == GauntletConfig::AssignmentKind::RandomChoice) {
                    std::snprintf(
                        specialName, sizeof(specialName), "Random: %s",
                        RANDOM_CATEGORY_LABELS[static_cast<uint8_t>(
                            config_->randomCategory(i))]);
                    assignmentName = specialName;
                }
                std::snprintf(rows[count], sizeof(rows[count]), "%c %u: %s",
                              i == selectedSlot_ ? '>' : ' ',
                              static_cast<unsigned>(i + 1),
                              assignmentName);
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::SelectCatalogMachine: {
            const uint16_t total = catalog_->count() + 2;
            const uint16_t start = windowStart(catalogIndex_, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                const char* label = i == 0 ? "Player's Choice 3B 3:00" :
                                    i == 1 ? "Random Choice" :
                                    catalog_->at(i - 2)->name;
                std::snprintf(rows[count], sizeof(rows[count]), "%c %s",
                              i == catalogIndex_ ? '>' : ' ', label);
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::SelectRandomCategory: {
            const uint16_t total =
                static_cast<uint8_t>(GauntletConfig::RandomCategory::Count);
            const uint16_t selected = static_cast<uint8_t>(randomCategory_);
            const uint16_t start = windowStart(selected, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                std::snprintf(
                    rows[count], sizeof(rows[count]), "%c %s",
                    i == selected ? '>' : ' ', RANDOM_CATEGORY_LABELS[i]);
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
            if (lastValidation_.error ==
                GauntletConfig::ValidationError::NoRandomCandidates) {
                std::snprintf(value, sizeof(value), "Machine %u Random Empty",
                              lastValidation_.machineIndex + 1);
                lines[count++] = value;
                lines[count++] = "Add a Matching Game";
                lines[count++] = "or Change the Type";
            } else {
                std::snprintf(value, sizeof(value), "Machine %u Not Assigned",
                              lastValidation_.machineIndex + 1);
                lines[count++] = value;
                lines[count++] = "Assign Every Machine";
                lines[count++] = "Before Starting";
            }
            break;
    }
    tft.showStatusScreen(title(), lines, count, ColorId::Black,
                         screen_ == Screen::ValidationWarning ? ColorId::Red : ColorId::White,
                         ColorId::Cyan);
}
