#pragma once

#include <cstdint>
#include "game/MachineCatalog.h"
#include "modes/gauntlet/GauntletSession.h"

class GauntletConfig {
public:
    static constexpr uint8_t MIN_MACHINES = 1;
    static constexpr uint8_t MAX_MACHINES = 9;
    static constexpr uint8_t MIN_PLAYERS = 1;
    static constexpr uint8_t MAX_PLAYERS = 4;

    enum class ValidationError : uint8_t {
        None,
        InvalidMachineCount,
        InvalidPlayerCount,
        UnassignedMachine,
        MissingCatalogRecord
    };

    struct ValidationResult {
        ValidationResult(
            ValidationError errorValue = ValidationError::None,
            uint8_t machineIndexValue = 0)
            : error(errorValue), machineIndex(machineIndexValue) {}

        ValidationError error;
        uint8_t machineIndex;

        bool valid() const { return error == ValidationError::None; }
    };

    void reset();

    uint8_t machineCount() const;
    // Decreasing past assigned trailing instances requires allowDiscard=true.
    bool setMachineCount(uint8_t count, bool allowDiscard = false);
    bool decreasingWouldDiscard(uint8_t count) const;

    uint8_t playerCount() const;
    bool setPlayerCount(uint8_t count);

    MachineId machineAssignment(uint8_t index) const;
    bool assignMachine(uint8_t index, MachineId id, const MachineCatalog& catalog);
    bool clearMachineAssignment(uint8_t index);

    ValidationResult validate(const MachineCatalog& catalog) const;
    bool buildSession(const MachineCatalog& catalog, GauntletSession& outSession) const;

private:
    uint8_t machineCount_ = MIN_MACHINES;
    uint8_t playerCount_ = MAX_PLAYERS;
    MachineId assignments_[MAX_MACHINES] = {};
};
