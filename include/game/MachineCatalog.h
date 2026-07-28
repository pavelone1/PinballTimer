#pragma once

#include <cstdint>

using MachineId = uint32_t;

enum class MachineType : uint8_t {
    EM,
    SolidState,
    DMD,
    Modern
};

struct MachineRecord {
    static constexpr uint8_t NAME_CAPACITY = 33;
    static constexpr uint8_t MIN_BALL_COUNT = 1;
    static constexpr uint8_t MAX_BALL_COUNT = 6;
    static constexpr uint16_t MIN_PLAY_TIME_SECONDS = 1;
    static constexpr uint16_t MAX_PLAY_TIME_SECONDS = 3600;
    static constexpr uint16_t FALLBACK_PLAY_TIME_SECONDS = 180;

    MachineId id = 0;
    char name[NAME_CAPACITY] = {};
    MachineType type = MachineType::EM;
    uint8_t ballCount = 3;
    uint16_t playTimeSeconds = FALLBACK_PLAY_TIME_SECONDS;
    bool hasPlayTime = false;

    uint16_t resolvedPlayTimeSeconds() const;
};

class MachineCatalog {
public:
    static constexpr uint16_t MAX_RECORDS = 100;

    void clear();
    uint16_t count() const;
    MachineId nextId() const;

    bool add(const char* name, MachineType type, uint8_t ballCount,
             uint16_t playTimeSeconds, bool hasPlayTime,
             MachineId& outId);
    bool update(MachineId id, const char* name, MachineType type,
                uint8_t ballCount, uint16_t playTimeSeconds,
                bool hasPlayTime);
    bool remove(MachineId id);

    const MachineRecord* find(MachineId id) const;
    const MachineRecord* at(uint16_t index) const;

    // Persistence-only import. IDs must be nonzero and unique.
    bool importRecord(const MachineRecord& record);
    void restoreNextId(MachineId nextId);

    static bool isValid(const MachineRecord& record);

private:
    MachineRecord records_[MAX_RECORDS] = {};
    uint16_t count_ = 0;
    MachineId nextId_ = 1;
};

