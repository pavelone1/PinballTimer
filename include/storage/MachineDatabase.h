#pragma once

#include <Preferences.h>
#include "game/MachineCatalog.h"

// Persistent NVS adapter for the common machine catalog. Records are stored
// independently so increasing catalog capacity does not require rewriting one
// monolithic blob.
class MachineDatabase {
public:
    static constexpr uint16_t SCHEMA_VERSION = 1;

    bool begin();
    void end();

    const MachineCatalog& catalog() const;

    bool add(const char* name, MachineType type, uint8_t ballCount,
             uint16_t playTimeSeconds, bool hasPlayTime,
             MachineId& outId);
    bool update(MachineId id, const char* name, MachineType type,
                uint8_t ballCount, uint16_t playTimeSeconds,
                bool hasPlayTime);
    bool remove(MachineId id);

    // Replaces the complete persistent catalog. All records are
    // validated (including unique, nonzero IDs) before NVS is touched.
    bool replaceAll(const MachineRecord* records, uint16_t count);

private:
    Preferences prefs_;
    MachineCatalog catalog_;
    bool open_ = false;

    bool load();
    bool seedDefaultsIfEmpty();
    bool persistIndex();
    bool persistRecord(const MachineRecord& record);
    bool persistCatalog();
    void recordKey(MachineId id, char* out, size_t outSize) const;
};
