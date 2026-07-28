#include "modes/gauntlet/GauntletConfig.h"

void GauntletConfig::reset()
{
    machineCount_ = MIN_MACHINES;
    playerCount_ = MAX_PLAYERS;
    for (uint8_t i = 0; i < MAX_MACHINES; ++i) {
        assignments_[i] = 0;
    }
}

uint8_t GauntletConfig::machineCount() const { return machineCount_; }

bool GauntletConfig::setMachineCount(uint8_t count, bool allowDiscard)
{
    if (count < MIN_MACHINES || count > MAX_MACHINES) {
        return false;
    }
    if (count < machineCount_ && decreasingWouldDiscard(count) && !allowDiscard) {
        return false;
    }
    if (count < machineCount_) {
        for (uint8_t i = count; i < machineCount_; ++i) {
            assignments_[i] = 0;
        }
    } else {
        for (uint8_t i = machineCount_; i < count; ++i) {
            assignments_[i] = 0;
        }
    }
    machineCount_ = count;
    return true;
}

bool GauntletConfig::decreasingWouldDiscard(uint8_t count) const
{
    if (count >= machineCount_) {
        return false;
    }
    for (uint8_t i = count; i < machineCount_; ++i) {
        if (assignments_[i] != 0) {
            return true;
        }
    }
    return false;
}

uint8_t GauntletConfig::playerCount() const { return playerCount_; }

bool GauntletConfig::setPlayerCount(uint8_t count)
{
    if (count < MIN_PLAYERS || count > MAX_PLAYERS) {
        return false;
    }
    playerCount_ = count;
    return true;
}

MachineId GauntletConfig::machineAssignment(uint8_t index) const
{
    return index < machineCount_ ? assignments_[index] : 0;
}

bool GauntletConfig::assignMachine(
    uint8_t index, MachineId id, const MachineCatalog& catalog)
{
    if (index >= machineCount_ || !catalog.find(id)) {
        return false;
    }
    assignments_[index] = id;
    return true;
}

bool GauntletConfig::clearMachineAssignment(uint8_t index)
{
    if (index >= machineCount_) {
        return false;
    }
    assignments_[index] = 0;
    return true;
}

GauntletConfig::ValidationResult
GauntletConfig::validate(const MachineCatalog& catalog) const
{
    if (machineCount_ < MIN_MACHINES || machineCount_ > MAX_MACHINES) {
        return {ValidationError::InvalidMachineCount, 0};
    }
    if (playerCount_ < MIN_PLAYERS || playerCount_ > MAX_PLAYERS) {
        return {ValidationError::InvalidPlayerCount, 0};
    }
    for (uint8_t i = 0; i < machineCount_; ++i) {
        if (assignments_[i] == 0) {
            return {ValidationError::UnassignedMachine, i};
        }
        if (!catalog.find(assignments_[i])) {
            return {ValidationError::MissingCatalogRecord, i};
        }
    }
    return {};
}

bool GauntletConfig::buildSession(
    const MachineCatalog& catalog, GauntletSession& outSession) const
{
    if (!validate(catalog).valid()) {
        return false;
    }
    GauntletSession candidate;
    for (uint8_t i = 0; i < machineCount_; ++i) {
        if (!candidate.addMachine(*catalog.find(assignments_[i]))) {
            return false;
        }
    }
    outSession = candidate;
    return true;
}

