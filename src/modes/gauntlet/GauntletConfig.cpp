#include "modes/gauntlet/GauntletConfig.h"

void GauntletConfig::reset()
{
    machineCount_ = MIN_MACHINES;
    playerCount_ = MAX_PLAYERS;
    for (uint8_t i = 0; i < MAX_MACHINES; ++i) {
        assignments_[i] = {};
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
            assignments_[i] = {};
        }
    } else {
        for (uint8_t i = machineCount_; i < count; ++i) {
            assignments_[i] = {};
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
        if (assignments_[i].kind != AssignmentKind::Unassigned) {
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
    return index < machineCount_ &&
                   assignments_[index].kind == AssignmentKind::CatalogMachine
               ? assignments_[index].machineId
               : 0;
}

GauntletConfig::AssignmentKind
GauntletConfig::assignmentKind(uint8_t index) const
{
    return index < machineCount_ ? assignments_[index].kind
                                 : AssignmentKind::Unassigned;
}

GauntletConfig::RandomCategory
GauntletConfig::randomCategory(uint8_t index) const
{
    return index < machineCount_ ? assignments_[index].category
                                 : RandomCategory::Any;
}

bool GauntletConfig::assignMachine(
    uint8_t index, MachineId id, const MachineCatalog& catalog)
{
    if (index >= machineCount_ || !catalog.find(id)) {
        return false;
    }
    assignments_[index] = {};
    assignments_[index].kind = AssignmentKind::CatalogMachine;
    assignments_[index].machineId = id;
    return true;
}

bool GauntletConfig::assignPlayersChoice(uint8_t index)
{
    if (index >= machineCount_) {
        return false;
    }
    assignments_[index] = {};
    assignments_[index].kind = AssignmentKind::PlayersChoice;
    return true;
}

bool GauntletConfig::assignRandomChoice(
    uint8_t index, RandomCategory category)
{
    if (index >= machineCount_ ||
        category >= RandomCategory::Count) {
        return false;
    }
    assignments_[index] = {};
    assignments_[index].kind = AssignmentKind::RandomChoice;
    assignments_[index].category = category;
    return true;
}

bool GauntletConfig::clearMachineAssignment(uint8_t index)
{
    if (index >= machineCount_) {
        return false;
    }
    assignments_[index] = {};
    return true;
}

bool GauntletConfig::matchesCategory(
    const MachineRecord& machine, RandomCategory category)
{
    if (category == RandomCategory::Any) {
        return true;
    }
    return static_cast<uint8_t>(machine.type) ==
           static_cast<uint8_t>(category);
}

uint16_t GauntletConfig::randomCandidateCount(
    const MachineCatalog& catalog, RandomCategory category)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < catalog.count(); ++i) {
        if (matchesCategory(*catalog.at(i), category)) {
            ++count;
        }
    }
    return count;
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
        const Assignment& assignment = assignments_[i];
        switch (assignment.kind) {
            case AssignmentKind::Unassigned:
                return {ValidationError::UnassignedMachine, i};
            case AssignmentKind::CatalogMachine:
                if (!catalog.find(assignment.machineId)) {
                    return {ValidationError::MissingCatalogRecord, i};
                }
                break;
            case AssignmentKind::PlayersChoice:
                break;
            case AssignmentKind::RandomChoice:
                if (randomCandidateCount(catalog, assignment.category) == 0) {
                    return {ValidationError::NoRandomCandidates, i};
                }
                break;
        }
    }
    return {};
}

bool GauntletConfig::buildSession(
    const MachineCatalog& catalog, GauntletSession& outSession,
    uint32_t randomSeed) const
{
    if (!validate(catalog).valid()) {
        return false;
    }
    GauntletSession candidate;
    uint32_t randomState = randomSeed;
    for (uint8_t i = 0; i < machineCount_; ++i) {
        const Assignment& assignment = assignments_[i];
        if (assignment.kind == AssignmentKind::PlayersChoice) {
            if (!candidate.addCustomMachine(
                    "Player's Choice", PLAYERS_CHOICE_BALLS,
                    PLAYERS_CHOICE_TIME_SECONDS)) {
                return false;
            }
            continue;
        }
        if (assignment.kind == AssignmentKind::CatalogMachine) {
            if (!candidate.addMachine(*catalog.find(assignment.machineId))) {
                return false;
            }
            continue;
        }

        const uint16_t candidateCount =
            randomCandidateCount(catalog, assignment.category);
        uint16_t selectedCandidate =
            static_cast<uint16_t>(randomState % candidateCount);
        randomState = randomState * 1664525u + 1013904223u;
        const MachineRecord* selected = nullptr;
        for (uint16_t catalogIndex = 0; catalogIndex < catalog.count();
             ++catalogIndex) {
            const MachineRecord* record = catalog.at(catalogIndex);
            if (!matchesCategory(*record, assignment.category)) {
                continue;
            }
            if (selectedCandidate == 0) {
                selected = record;
                break;
            }
            --selectedCandidate;
        }
        if (!selected || !candidate.addMachine(*selected)) {
            return false;
        }
    }
    outSession = candidate;
    return true;
}
