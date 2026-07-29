#pragma once

#include <cstdint>
#include "SystemTypes.h"
#include "game/MachineCatalog.h"
#include "modes/gauntlet/GauntletConfig.h"
class TftDisplayManager;

// Mode-owned, renderer-independent menu controller. A future BootMenu handoff
// can render the exposed state and route EncoderEvent values without knowing
// Gauntlet's menu shape or validation rules.
class GauntletConfigMenu {
public:
    enum class Item : uint8_t {
        StartGauntlet,
        NumberOfMachines,
        AssignMachines,
        NumberOfPlayers,
        PlayerSetup,
        Back,
        Count
    };

    enum class Screen : uint8_t {
        Root,
        EditMachineCount,
        ConfirmMachineCountDecrease,
        SelectMachineSlot,
        SelectCatalogMachine,
        SelectRandomCategory,
        EditPlayerCount,
        ValidationWarning
    };

    enum class Outcome : uint8_t {
        None,
        StartGauntlet,
        OpenPlayerSetup,
        Back
    };

    void begin(GauntletConfig& config, const MachineCatalog& catalog);
    Outcome handleEncoderEvent(const EncoderEvent& event);
    void render(TftDisplayManager& tft) const;

    Screen screen() const;
    Item selectedItem() const;
    uint8_t selectedMachineSlot() const;
    uint16_t selectedCatalogIndex() const;
    GauntletConfig::RandomCategory selectedRandomCategory() const;
    uint8_t pendingMachineCount() const;
    uint8_t pendingPlayerCount() const;
    GauntletConfig::ValidationResult lastValidation() const;

    const char* title() const;
    const char* selectedItemLabel() const;

private:
    GauntletConfig* config_ = nullptr;
    const MachineCatalog* catalog_ = nullptr;
    Screen screen_ = Screen::Root;
    Item selectedItem_ = Item::StartGauntlet;
    uint8_t selectedSlot_ = 0;
    uint16_t catalogIndex_ = 0;
    GauntletConfig::RandomCategory randomCategory_ =
        GauntletConfig::RandomCategory::EM;
    uint8_t pendingMachineCount_ = 1;
    uint8_t pendingPlayerCount_ = 4;
    GauntletConfig::ValidationResult lastValidation_;

    void rotate(bool clockwise);
    Outcome select();
};
