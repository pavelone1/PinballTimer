#include "storage/MachineDatabase.h"

#include <cstdio>

namespace {
constexpr const char* NAMESPACE_NAME = "machines";
constexpr const char* KEY_SCHEMA = "schema";
constexpr const char* KEY_NEXT_ID = "nextId";
constexpr const char* KEY_COUNT = "count";
constexpr const char* KEY_INDEX = "index";
}

bool MachineDatabase::begin()
{
    if (open_) {
        return true;
    }
    if (!prefs_.begin(NAMESPACE_NAME, false)) {
        return false;
    }
    open_ = true;
    return load() && seedDefaultsIfEmpty();
}

void MachineDatabase::end()
{
    if (open_) {
        prefs_.end();
        open_ = false;
    }
}

const MachineCatalog& MachineDatabase::catalog() const { return catalog_; }

bool MachineDatabase::add(const char* name, MachineType type, uint8_t ballCount,
                          uint16_t playTimeSeconds, bool hasPlayTime,
                          MachineId& outId)
{
    if (!open_ || !catalog_.add(name, type, ballCount, playTimeSeconds, hasPlayTime, outId)) {
        return false;
    }
    const MachineRecord* record = catalog_.find(outId);
    if (!record || !persistRecord(*record) || !persistIndex()) {
        catalog_.remove(outId);
        return false;
    }
    return true;
}

bool MachineDatabase::update(MachineId id, const char* name, MachineType type,
                             uint8_t ballCount, uint16_t playTimeSeconds,
                             bool hasPlayTime)
{
    const MachineRecord* original = catalog_.find(id);
    if (!open_ || !original) {
        return false;
    }
    const MachineRecord backup = *original;
    if (!catalog_.update(id, name, type, ballCount, playTimeSeconds, hasPlayTime)) {
        return false;
    }
    if (!persistRecord(*catalog_.find(id))) {
        catalog_.update(id, backup.name, backup.type, backup.ballCount,
                        backup.playTimeSeconds, backup.hasPlayTime);
        return false;
    }
    return true;
}

bool MachineDatabase::remove(MachineId id)
{
    if (!open_ || !catalog_.find(id)) {
        return false;
    }
    char key[16];
    recordKey(id, key, sizeof(key));
    if (!prefs_.remove(key) || !catalog_.remove(id)) {
        return false;
    }
    return persistIndex();
}

bool MachineDatabase::load()
{
    catalog_.clear();
    const uint16_t schema = prefs_.getUShort(KEY_SCHEMA, 0);
    if (schema == 0) {
        prefs_.putUShort(KEY_SCHEMA, SCHEMA_VERSION);
        return persistIndex();
    }
    if (schema != SCHEMA_VERSION) {
        return false;
    }

    const uint16_t count = prefs_.getUShort(KEY_COUNT, 0);
    if (count > MachineCatalog::MAX_RECORDS) {
        return false;
    }
    MachineId ids[MachineCatalog::MAX_RECORDS] = {};
    const size_t expected = static_cast<size_t>(count) * sizeof(MachineId);
    if (expected && prefs_.getBytes(KEY_INDEX, ids, expected) != expected) {
        return false;
    }
    for (uint16_t i = 0; i < count; ++i) {
        char key[16];
        recordKey(ids[i], key, sizeof(key));
        MachineRecord record;
        if (prefs_.getBytes(key, &record, sizeof(record)) != sizeof(record) ||
            !catalog_.importRecord(record)) {
            return false;
        }
    }
    catalog_.restoreNextId(prefs_.getULong(KEY_NEXT_ID, catalog_.nextId()));
    return true;
}

bool MachineDatabase::seedDefaultsIfEmpty()
{
    if (catalog_.count() != 0) {
        return true;
    }

    MachineId id = 0;
    return add("Stars", MachineType::SolidState, 3, 180, true, id) &&
           add("Meteor", MachineType::SolidState, 3, 180, true, id) &&
           add("Mars Trek", MachineType::EM, 5, 270, true, id) &&
           add("Scared Stiff", MachineType::DMD, 3, 240, true, id);
}

bool MachineDatabase::persistIndex()
{
    MachineId ids[MachineCatalog::MAX_RECORDS] = {};
    for (uint16_t i = 0; i < catalog_.count(); ++i) {
        ids[i] = catalog_.at(i)->id;
    }
    const size_t bytes = static_cast<size_t>(catalog_.count()) * sizeof(MachineId);
    return prefs_.putUShort(KEY_SCHEMA, SCHEMA_VERSION) == sizeof(uint16_t) &&
           prefs_.putUShort(KEY_COUNT, catalog_.count()) == sizeof(uint16_t) &&
           prefs_.putULong(KEY_NEXT_ID, catalog_.nextId()) == sizeof(uint32_t) &&
           (bytes == 0 || prefs_.putBytes(KEY_INDEX, ids, bytes) == bytes);
}

bool MachineDatabase::persistRecord(const MachineRecord& record)
{
    char key[16];
    recordKey(record.id, key, sizeof(key));
    return prefs_.putBytes(key, &record, sizeof(record)) == sizeof(record);
}

void MachineDatabase::recordKey(MachineId id, char* out, size_t outSize) const
{
    std::snprintf(out, outSize, "m%lu", static_cast<unsigned long>(id));
}
