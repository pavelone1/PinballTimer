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
    static constexpr uint8_t PLAYERS_CHOICE_BALLS = 3;
    static constexpr uint16_t PLAYERS_CHOICE_TIME_SECONDS = 180;

    enum class AssignmentKind : uint8_t {
        Unassigned,
        CatalogMachine,
        PlayersChoice,
        RandomChoice
    };

    enum class RandomCategory : uint8_t {
        EM,
        SolidState,
        DMD,
        Modern,
        Any,
        Count
    };

    enum class ValidationError : uint8_t {
        None,
        InvalidMachineCount,
        InvalidPlayerCount,
        UnassignedMachine,
        MissingCatalogRecord,
        NoRandomCandidates
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
    AssignmentKind assignmentKind(uint8_t index) const;
    RandomCategory randomCategory(uint8_t index) const;
    bool assignMachine(uint8_t index, MachineId id, const MachineCatalog& catalog);
    bool assignPlayersChoice(uint8_t index);
    bool assignRandomChoice(uint8_t index, RandomCategory category);
    bool clearMachineAssignment(uint8_t index);

    ValidationResult validate(const MachineCatalog& catalog) const;
    bool buildSession(const MachineCatalog& catalog, GauntletSession& outSession,
                      uint32_t randomSeed = 0) const;

private:
    struct Assignment {
        AssignmentKind kind = AssignmentKind::Unassigned;
        MachineId machineId = 0;
        RandomCategory category = RandomCategory::Any;
    };

    uint8_t machineCount_ = MIN_MACHINES;
    uint8_t playerCount_ = MAX_PLAYERS;
    Assignment assignments_[MAX_MACHINES] = {};

    static bool matchesCategory(
        const MachineRecord& machine, RandomCategory category);
    static uint16_t randomCandidateCount(
        const MachineCatalog& catalog, RandomCategory category);
};
