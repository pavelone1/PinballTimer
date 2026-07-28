#pragma once

#include <cstdint>
#include "game/MachineCatalog.h"

struct GauntletMachineInstance {
    enum class State : uint8_t { Pending, Played, Skipped, Removed };

    uint16_t instanceId = 0;
    MachineId machineId = 0;
    char machineName[MachineRecord::NAME_CAPACITY] = {};
    uint8_t ballCount = 3;
    uint16_t resolvedDefaultTimeSeconds = MachineRecord::FALLBACK_PLAY_TIME_SECONDS;
    uint16_t playTimeOverrideSeconds = 0;
    bool hasPlayTimeOverride = false;
    State state = State::Pending;
};

class GauntletSession {
public:
    static constexpr uint8_t MAX_MACHINES = 9;

    void clear();
    bool addMachine(const MachineRecord& machine);
    uint8_t machineCount() const;
    const GauntletMachineInstance* machineAt(uint8_t index) const;
    const GauntletMachineInstance* currentMachine() const;

    long startingPoolSeconds() const;
    bool start();
    bool completeCurrent();
    bool skipCurrent();
    bool removeCurrent(long remainingSeconds[4], uint8_t playerCount);

    bool hasCurrentMachine() const;
    bool isFinalMachine() const;
    bool finished() const;
    bool inSkippedPass() const;

private:
    GauntletMachineInstance machines_[MAX_MACHINES] = {};
    uint8_t count_ = 0;
    int8_t currentIndex_ = -1;
    bool skippedPass_ = false;
    bool finished_ = false;

    bool selectNext();
    bool hasPendingAfter(uint8_t index) const;
    bool hasSkipped() const;
};
