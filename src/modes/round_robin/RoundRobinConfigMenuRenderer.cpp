#include "modes/round_robin/RoundRobinConfigMenu.h"

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

void RoundRobinConfigMenu::render(TftDisplayManager& tft) const
{
    char value[32];
    char rows[MAX_VISIBLE_ROWS][MachineRecord::NAME_CAPACITY + 3] = {};
    const char* lines[MAX_VISIBLE_ROWS] = {};
    uint8_t count = 0;
    switch (screen_) {
        case Screen::Root: {
            static const char* labels[] = {
                "Start Round Robin", "Number of Players", "Select Machine",
                "Game Time", "Ball Count", "Player Setup", "Back"
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
        case Screen::EditPlayerCount:
            std::snprintf(value, sizeof(value), "%u Players", pendingPlayerCount_);
            lines[count++] = value; lines[count++] = "Press to Save";
            break;
        case Screen::SelectCatalogMachine: {
            const uint16_t total = catalog_->count() + 1;
            const uint16_t start = windowStart(catalogIndex_, total);
            for (uint16_t i = start; i < total && count < MAX_VISIBLE_ROWS; ++i) {
                const char* label = i == 0 ? "None" : catalog_->at(i - 1)->name;
                std::snprintf(rows[count], sizeof(rows[count]), "%c %s",
                              i == catalogIndex_ ? '>' : ' ', label);
                lines[count] = rows[count];
                ++count;
            }
            break;
        }
        case Screen::EditGameTime:
            std::snprintf(value, sizeof(value), "%ld:%02ld",
                          pendingGameTimeSeconds_ / 60,
                          pendingGameTimeSeconds_ % 60);
            lines[count++] = value; lines[count++] = "Press to Save";
            break;
        case Screen::EditBallCount:
            std::snprintf(value, sizeof(value), "%u Balls", pendingBallCount_);
            lines[count++] = value; lines[count++] = "Press to Save";
            break;
        case Screen::ValidationWarning:
            lines[count++] = "Configuration Invalid";
            lines[count++] = "Press to Continue";
            break;
    }
    tft.showStatusScreen(title(), lines, count, ColorId::Black,
                         screen_ == Screen::ValidationWarning ? ColorId::Red : ColorId::White,
                         ColorId::Cyan);
}
