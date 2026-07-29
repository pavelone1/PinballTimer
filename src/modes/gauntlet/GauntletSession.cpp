#include "modes/gauntlet/GauntletSession.h"

#include <cstring>

void GauntletSession::clear()
{
    count_ = 0;
    currentIndex_ = -1;
    skippedPass_ = false;
    finished_ = false;
}

bool GauntletSession::addMachine(const MachineRecord& machine)
{
    if (count_ >= MAX_MACHINES || !MachineCatalog::isValid(machine)) {
        return false;
    }
    GauntletMachineInstance& instance = machines_[count_];
    instance = {};
    instance.instanceId = static_cast<uint16_t>(count_ + 1);
    instance.machineId = machine.id;
    std::strncpy(instance.machineName, machine.name, MachineRecord::NAME_CAPACITY - 1);
    instance.ballCount = machine.ballCount;
    instance.resolvedDefaultTimeSeconds = machine.resolvedPlayTimeSeconds();
    instance.state = GauntletMachineInstance::State::Pending;
    ++count_;
    return true;
}

bool GauntletSession::addCustomMachine(
    const char* name, uint8_t ballCount, uint16_t playTimeSeconds)
{
    if (count_ >= MAX_MACHINES || !name || !name[0] ||
        ballCount < MachineRecord::MIN_BALL_COUNT ||
        ballCount > MachineRecord::MAX_BALL_COUNT ||
        playTimeSeconds < MachineRecord::MIN_PLAY_TIME_SECONDS ||
        playTimeSeconds > MachineRecord::MAX_PLAY_TIME_SECONDS) {
        return false;
    }
    GauntletMachineInstance& instance = machines_[count_];
    instance = {};
    instance.instanceId = static_cast<uint16_t>(count_ + 1);
    instance.machineId = 0;
    std::strncpy(instance.machineName, name, MachineRecord::NAME_CAPACITY - 1);
    instance.ballCount = ballCount;
    instance.resolvedDefaultTimeSeconds = playTimeSeconds;
    instance.state = GauntletMachineInstance::State::Pending;
    ++count_;
    return true;
}

uint8_t GauntletSession::machineCount() const { return count_; }

const GauntletMachineInstance* GauntletSession::machineAt(uint8_t index) const
{
    return index < count_ ? &machines_[index] : nullptr;
}

const GauntletMachineInstance* GauntletSession::currentMachine() const
{
    return hasCurrentMachine() ? &machines_[currentIndex_] : nullptr;
}

long GauntletSession::startingPoolSeconds() const
{
    long total = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        total += machines_[i].resolvedDefaultTimeSeconds;
    }
    return total;
}

bool GauntletSession::start()
{
    currentIndex_ = -1;
    skippedPass_ = false;
    finished_ = false;
    return selectNext();
}

bool GauntletSession::completeCurrent()
{
    if (!hasCurrentMachine()) {
        return false;
    }
    machines_[currentIndex_].state = GauntletMachineInstance::State::Played;
    return selectNext();
}

bool GauntletSession::skipCurrent()
{
    if (!hasCurrentMachine() || isFinalMachine()) {
        return false;
    }
    machines_[currentIndex_].state = GauntletMachineInstance::State::Skipped;
    return selectNext();
}

bool GauntletSession::removeCurrent(long remainingSeconds[4], uint8_t playerCount)
{
    if (!hasCurrentMachine() || !remainingSeconds || playerCount > 4) {
        return false;
    }
    const long deduction = machines_[currentIndex_].resolvedDefaultTimeSeconds;
    machines_[currentIndex_].state = GauntletMachineInstance::State::Removed;
    for (uint8_t i = 0; i < playerCount; ++i) {
        remainingSeconds[i] -= deduction;
        if (remainingSeconds[i] < 0) {
            remainingSeconds[i] = 0;
        }
    }
    return selectNext();
}

bool GauntletSession::hasCurrentMachine() const
{
    return !finished_ && currentIndex_ >= 0 && currentIndex_ < count_;
}

bool GauntletSession::isFinalMachine() const
{
    if (!hasCurrentMachine()) {
        return false;
    }
    if (!skippedPass_) {
        return !hasPendingAfter(currentIndex_) && !hasSkipped();
    }
    for (uint8_t i = currentIndex_ + 1; i < count_; ++i) {
        if (machines_[i].state == GauntletMachineInstance::State::Skipped) {
            return false;
        }
    }
    return true;
}

bool GauntletSession::finished() const { return finished_; }
bool GauntletSession::inSkippedPass() const { return skippedPass_; }

bool GauntletSession::selectNext()
{
    if (!skippedPass_) {
        for (uint8_t i = static_cast<uint8_t>(currentIndex_ + 1); i < count_; ++i) {
            if (machines_[i].state == GauntletMachineInstance::State::Pending) {
                currentIndex_ = static_cast<int8_t>(i);
                return true;
            }
        }
        skippedPass_ = true;
        currentIndex_ = -1;
    }
    for (uint8_t i = static_cast<uint8_t>(currentIndex_ + 1); i < count_; ++i) {
        if (machines_[i].state == GauntletMachineInstance::State::Skipped) {
            currentIndex_ = static_cast<int8_t>(i);
            return true;
        }
    }
    currentIndex_ = -1;
    finished_ = true;
    return false;
}

bool GauntletSession::hasPendingAfter(uint8_t index) const
{
    for (uint8_t i = index + 1; i < count_; ++i) {
        if (machines_[i].state == GauntletMachineInstance::State::Pending) {
            return true;
        }
    }
    return false;
}

bool GauntletSession::hasSkipped() const
{
    for (uint8_t i = 0; i < count_; ++i) {
        if (machines_[i].state == GauntletMachineInstance::State::Skipped) {
            return true;
        }
    }
    return false;
}
