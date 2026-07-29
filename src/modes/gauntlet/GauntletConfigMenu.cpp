#include "modes/gauntlet/GauntletConfigMenu.h"


void GauntletConfigMenu::begin(
    GauntletConfig& config, const MachineCatalog& catalog)
{
    config_ = &config;
    catalog_ = &catalog;
    screen_ = Screen::Root;
    selectedItem_ = Item::StartGauntlet;
    selectedSlot_ = 0;
    catalogIndex_ = 0;
    randomCategory_ = GauntletConfig::RandomCategory::EM;
    pendingMachineCount_ = config.machineCount();
    pendingPlayerCount_ = config.playerCount();
    lastValidation_ = {};
}

GauntletConfigMenu::Outcome
GauntletConfigMenu::handleEncoderEvent(const EncoderEvent& event)
{
    if (!config_ || !catalog_) {
        return Outcome::None;
    }
    if (event.type == EncoderEventType::RotatedClockwise) {
        rotate(true);
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        rotate(false);
    } else if (event.type == EncoderEventType::SwShortPress) {
        return select();
    } else if (event.type == EncoderEventType::SwLongPress) {
        if (screen_ == Screen::Root) {
            return Outcome::Back;
        }
        screen_ = Screen::Root;
        return Outcome::None;
    }
    return Outcome::None;
}

GauntletConfigMenu::Screen GauntletConfigMenu::screen() const { return screen_; }
GauntletConfigMenu::Item GauntletConfigMenu::selectedItem() const { return selectedItem_; }
uint8_t GauntletConfigMenu::selectedMachineSlot() const { return selectedSlot_; }
uint16_t GauntletConfigMenu::selectedCatalogIndex() const { return catalogIndex_; }
GauntletConfig::RandomCategory
GauntletConfigMenu::selectedRandomCategory() const { return randomCategory_; }
uint8_t GauntletConfigMenu::pendingMachineCount() const { return pendingMachineCount_; }
uint8_t GauntletConfigMenu::pendingPlayerCount() const { return pendingPlayerCount_; }
GauntletConfig::ValidationResult GauntletConfigMenu::lastValidation() const { return lastValidation_; }

const char* GauntletConfigMenu::title() const
{
    switch (screen_) {
        case Screen::Root: return "Gauntlet Config";
        case Screen::EditMachineCount: return "Number of Machines";
        case Screen::ConfirmMachineCountDecrease: return "Remove Assignments?";
        case Screen::SelectMachineSlot: return "Assign Machines";
        case Screen::SelectCatalogMachine: return "Select Machine";
        case Screen::SelectRandomCategory: return "Random Machine Type";
        case Screen::EditPlayerCount: return "Number of Players";
        case Screen::ValidationWarning: return "Gauntlet Not Ready";
    }
    return "Gauntlet Config";
}

const char* GauntletConfigMenu::selectedItemLabel() const
{
    static const char* labels[] = {
        "Start Gauntlet",
        "Number of Machines",
        "Assign Machines",
        "Number of Players",
        "Player Setup",
        "Back"
    };
    return labels[static_cast<uint8_t>(selectedItem_)];
}

void GauntletConfigMenu::rotate(bool clockwise)
{
    if (screen_ == Screen::Root) {
        const uint8_t count = static_cast<uint8_t>(Item::Count);
        const uint8_t current = static_cast<uint8_t>(selectedItem_);
        selectedItem_ = static_cast<Item>(
            clockwise ? (current + 1) % count : (current + count - 1) % count);
    } else if (screen_ == Screen::EditMachineCount) {
        if (clockwise && pendingMachineCount_ < GauntletConfig::MAX_MACHINES) {
            ++pendingMachineCount_;
        } else if (!clockwise && pendingMachineCount_ > GauntletConfig::MIN_MACHINES) {
            --pendingMachineCount_;
        }
    } else if (screen_ == Screen::SelectMachineSlot) {
        const uint8_t count = config_->machineCount();
        selectedSlot_ = clockwise ? (selectedSlot_ + 1) % count
                                  : (selectedSlot_ + count - 1) % count;
    } else if (screen_ == Screen::SelectCatalogMachine) {
        const uint16_t count = catalog_->count() + 2;
        catalogIndex_ = clockwise ? (catalogIndex_ + 1) % count
                                  : (catalogIndex_ + count - 1) % count;
    } else if (screen_ == Screen::SelectRandomCategory) {
        const uint8_t count =
            static_cast<uint8_t>(GauntletConfig::RandomCategory::Count);
        const uint8_t current = static_cast<uint8_t>(randomCategory_);
        randomCategory_ = static_cast<GauntletConfig::RandomCategory>(
            clockwise ? (current + 1) % count
                      : (current + count - 1) % count);
    } else if (screen_ == Screen::EditPlayerCount) {
        if (clockwise && pendingPlayerCount_ < GauntletConfig::MAX_PLAYERS) {
            ++pendingPlayerCount_;
        } else if (!clockwise && pendingPlayerCount_ > GauntletConfig::MIN_PLAYERS) {
            --pendingPlayerCount_;
        }
    }
}

GauntletConfigMenu::Outcome GauntletConfigMenu::select()
{
    if (screen_ == Screen::ValidationWarning) {
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::EditMachineCount) {
        if (config_->decreasingWouldDiscard(pendingMachineCount_)) {
            screen_ = Screen::ConfirmMachineCountDecrease;
        } else {
            config_->setMachineCount(pendingMachineCount_);
            screen_ = Screen::Root;
        }
        return Outcome::None;
    }
    if (screen_ == Screen::ConfirmMachineCountDecrease) {
        config_->setMachineCount(pendingMachineCount_, true);
        screen_ = Screen::Root;
        return Outcome::None;
    }
    if (screen_ == Screen::SelectMachineSlot) {
        catalogIndex_ = 0;
        screen_ = Screen::SelectCatalogMachine;
        return Outcome::None;
    }
    if (screen_ == Screen::SelectCatalogMachine) {
        if (catalogIndex_ == 0) {
            config_->assignPlayersChoice(selectedSlot_);
            screen_ = Screen::SelectMachineSlot;
        } else if (catalogIndex_ == 1) {
            randomCategory_ = GauntletConfig::RandomCategory::EM;
            screen_ = Screen::SelectRandomCategory;
        } else {
            config_->assignMachine(
                selectedSlot_, catalog_->at(catalogIndex_ - 2)->id, *catalog_);
            screen_ = Screen::SelectMachineSlot;
        }
        return Outcome::None;
    }
    if (screen_ == Screen::SelectRandomCategory) {
        config_->assignRandomChoice(selectedSlot_, randomCategory_);
        screen_ = Screen::SelectMachineSlot;
        return Outcome::None;
    }
    if (screen_ == Screen::EditPlayerCount) {
        config_->setPlayerCount(pendingPlayerCount_);
        screen_ = Screen::Root;
        return Outcome::None;
    }

    switch (selectedItem_) {
        case Item::StartGauntlet:
            lastValidation_ = config_->validate(*catalog_);
            if (!lastValidation_.valid()) {
                screen_ = Screen::ValidationWarning;
                return Outcome::None;
            }
            return Outcome::StartGauntlet;
        case Item::NumberOfMachines:
            pendingMachineCount_ = config_->machineCount();
            screen_ = Screen::EditMachineCount;
            return Outcome::None;
        case Item::AssignMachines:
            selectedSlot_ = 0;
            screen_ = Screen::SelectMachineSlot;
            return Outcome::None;
        case Item::NumberOfPlayers:
            pendingPlayerCount_ = config_->playerCount();
            screen_ = Screen::EditPlayerCount;
            return Outcome::None;
        case Item::PlayerSetup:
            return Outcome::OpenPlayerSetup;
        case Item::Back:
            return Outcome::Back;
        case Item::Count:
            return Outcome::None;
    }
    return Outcome::None;
}
