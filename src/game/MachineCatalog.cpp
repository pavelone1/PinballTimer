#include "game/MachineCatalog.h"

#include <cstring>

namespace {

bool validType(MachineType type)
{
    return static_cast<uint8_t>(type) <= static_cast<uint8_t>(MachineType::Modern);
}

void copyName(char* destination, const char* source)
{
    std::strncpy(destination, source, MachineRecord::NAME_CAPACITY - 1);
    destination[MachineRecord::NAME_CAPACITY - 1] = '\0';
}

} // namespace

uint16_t MachineRecord::resolvedPlayTimeSeconds() const
{
    return hasPlayTime ? playTimeSeconds : FALLBACK_PLAY_TIME_SECONDS;
}

void MachineCatalog::clear()
{
    count_ = 0;
    nextId_ = 1;
}

uint16_t MachineCatalog::count() const { return count_; }
MachineId MachineCatalog::nextId() const { return nextId_; }

bool MachineCatalog::add(const char* name, MachineType type, uint8_t ballCount,
                         uint16_t playTimeSeconds, bool hasPlayTime,
                         MachineId& outId)
{
    if (count_ >= MAX_RECORDS || nextId_ == 0) {
        return false;
    }

    MachineRecord record;
    record.id = nextId_;
    copyName(record.name, name ? name : "");
    record.type = type;
    record.ballCount = ballCount;
    record.playTimeSeconds = playTimeSeconds;
    record.hasPlayTime = hasPlayTime;
    if (!isValid(record)) {
        return false;
    }

    records_[count_++] = record;
    outId = nextId_++;
    return true;
}

bool MachineCatalog::update(MachineId id, const char* name, MachineType type,
                            uint8_t ballCount, uint16_t playTimeSeconds,
                            bool hasPlayTime)
{
    for (uint16_t i = 0; i < count_; ++i) {
        if (records_[i].id != id) {
            continue;
        }
        MachineRecord candidate = records_[i];
        copyName(candidate.name, name ? name : "");
        candidate.type = type;
        candidate.ballCount = ballCount;
        candidate.playTimeSeconds = playTimeSeconds;
        candidate.hasPlayTime = hasPlayTime;
        if (!isValid(candidate)) {
            return false;
        }
        records_[i] = candidate;
        return true;
    }
    return false;
}

bool MachineCatalog::remove(MachineId id)
{
    for (uint16_t i = 0; i < count_; ++i) {
        if (records_[i].id != id) {
            continue;
        }
        for (uint16_t j = i + 1; j < count_; ++j) {
            records_[j - 1] = records_[j];
        }
        --count_;
        return true;
    }
    return false;
}

const MachineRecord* MachineCatalog::find(MachineId id) const
{
    for (uint16_t i = 0; i < count_; ++i) {
        if (records_[i].id == id) {
            return &records_[i];
        }
    }
    return nullptr;
}

const MachineRecord* MachineCatalog::at(uint16_t index) const
{
    return index < count_ ? &records_[index] : nullptr;
}

bool MachineCatalog::importRecord(const MachineRecord& record)
{
    if (count_ >= MAX_RECORDS || !isValid(record) || find(record.id)) {
        return false;
    }
    records_[count_++] = record;
    if (record.id >= nextId_) {
        nextId_ = record.id + 1;
    }
    return true;
}

void MachineCatalog::restoreNextId(MachineId nextId)
{
    if (nextId > nextId_) {
        nextId_ = nextId;
    }
}

bool MachineCatalog::isValid(const MachineRecord& record)
{
    const size_t nameLength = std::strlen(record.name);
    if (record.id == 0 || nameLength == 0 || nameLength >= MachineRecord::NAME_CAPACITY ||
        !validType(record.type) ||
        record.ballCount < MachineRecord::MIN_BALL_COUNT ||
        record.ballCount > MachineRecord::MAX_BALL_COUNT) {
        return false;
    }
    return !record.hasPlayTime ||
           (record.playTimeSeconds >= MachineRecord::MIN_PLAY_TIME_SECONDS &&
            record.playTimeSeconds <= MachineRecord::MAX_PLAY_TIME_SECONDS);
}

