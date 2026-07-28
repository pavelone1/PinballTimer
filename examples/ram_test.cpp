#include <Arduino.h>
#include <esp_heap_caps.h>

// Standalone diagnostic: exhaustively tests both internal SRAM and
// PSRAM for stuck-bit/addressing/data faults. Allocates as much of
// each memory type as is safely available (leaving headroom for the
// running program/heap bookkeeping) and runs several classic memory
// test patterns across the whole buffer, reporting any mismatch's
// address plus expected/actual values. Does not touch main.cpp or any
// other PlatformIO environment.
// Build/upload with: pio run -e ram-test -t upload

namespace {

constexpr size_t INTERNAL_HEADROOM_BYTES = 32 * 1024;  // leave room for stack/heap bookkeeping/Serial buffers
constexpr size_t PSRAM_HEADROOM_BYTES = 64 * 1024;
constexpr size_t CHUNK_WORDS = 4096; // progress-print granularity, in 32-bit words

struct TestResult {
    bool passed = true;
    uint32_t errorCount = 0;
    size_t firstErrorOffset = 0;
    uint32_t firstErrorExpected = 0;
    uint32_t firstErrorActual = 0;
};

void recordError(TestResult& result, size_t offset, uint32_t expected, uint32_t actual)
{
    if (result.passed) {
        result.passed = false;
        result.firstErrorOffset = offset;
        result.firstErrorExpected = expected;
        result.firstErrorActual = actual;
    }
    result.errorCount++;
}

// Fills the buffer via a caller-supplied pattern function, then reads
// it back and compares -- every mismatch is counted, not just the
// first, so errorCount reflects true fault extent.
template <typename PatternFn>
void runPattern(const char* patternName, volatile uint32_t* buf, size_t wordCount, PatternFn pattern, TestResult& result)
{
    Serial.printf("    pattern: %-18s writing...\n", patternName);
    for (size_t i = 0; i < wordCount; ++i) {
        buf[i] = pattern(i);
        if (i % (CHUNK_WORDS * 64) == 0 && i != 0) {
            Serial.printf("      ...%u / %u words\n", static_cast<unsigned>(i), static_cast<unsigned>(wordCount));
        }
    }

    Serial.printf("    pattern: %-18s verifying...\n", patternName);
    for (size_t i = 0; i < wordCount; ++i) {
        const uint32_t expected = pattern(i);
        const uint32_t actual = buf[i];
        if (actual != expected) {
            recordError(result, i * sizeof(uint32_t), expected, actual);
        }
        if (i % (CHUNK_WORDS * 64) == 0 && i != 0) {
            Serial.printf("      ...%u / %u words\n", static_cast<unsigned>(i), static_cast<unsigned>(wordCount));
        }
    }
}

uint32_t patternAllOnes(size_t) { return 0xFFFFFFFFu; }
uint32_t patternAllZeros(size_t) { return 0x00000000u; }
uint32_t patternAlternateA(size_t) { return 0xAAAAAAAAu; }
uint32_t patternAlternateB(size_t) { return 0x55555555u; }
uint32_t patternWalkingBit(size_t i) { return 1u << (i % 32); }
uint32_t patternAddressAsData(size_t i) { return static_cast<uint32_t>(i); }
uint32_t patternInvertedAddress(size_t i) { return ~static_cast<uint32_t>(i); }

TestResult testRegion(const char* label, volatile uint32_t* buf, size_t wordCount)
{
    TestResult result;

    Serial.printf("  Testing %s: %u bytes (%u words)\n", label,
        static_cast<unsigned>(wordCount * sizeof(uint32_t)), static_cast<unsigned>(wordCount));

    runPattern("all-ones", buf, wordCount, patternAllOnes, result);
    runPattern("all-zeros", buf, wordCount, patternAllZeros, result);
    runPattern("alternating-A", buf, wordCount, patternAlternateA, result);
    runPattern("alternating-B", buf, wordCount, patternAlternateB, result);
    runPattern("walking-bit", buf, wordCount, patternWalkingBit, result);
    runPattern("address-as-data", buf, wordCount, patternAddressAsData, result);
    runPattern("inverted-address", buf, wordCount, patternInvertedAddress, result);

    return result;
}

void reportResult(const char* label, const TestResult& result)
{
    if (result.passed) {
        Serial.printf("  [PASS] %s: no errors across all patterns\n", label);
        return;
    }

    Serial.printf("  [FAIL] %s: %u total mismatches\n", label, result.errorCount);
    Serial.printf("         first error at byte offset 0x%08X: expected 0x%08X, got 0x%08X\n",
        static_cast<unsigned>(result.firstErrorOffset), result.firstErrorExpected, result.firstErrorActual);
}

// Total free bytes can be spread across many small blocks -- a single
// malloc() needs one CONTIGUOUS block, so this sizes off (and
// allocates against) heap_caps_get_largest_free_block() instead, then
// backs off in 10% steps if even that optimistic estimate doesn't
// actually succeed (allocator bookkeeping overhead can eat a bit more).
void* allocateLargestBlock(uint32_t caps, size_t headroomBytes, size_t& outWordCount)
{
    size_t largest = heap_caps_get_largest_free_block(caps);
    if (largest <= headroomBytes) {
        outWordCount = 0;
        return nullptr;
    }

    size_t requestBytes = largest - headroomBytes;
    for (uint8_t attempt = 0; attempt < 10 && requestBytes > sizeof(uint32_t); ++attempt) {
        const size_t wordCount = requestBytes / sizeof(uint32_t);
        void* raw = heap_caps_malloc(wordCount * sizeof(uint32_t), caps);
        if (raw != nullptr) {
            outWordCount = wordCount;
            return raw;
        }
        requestBytes = (requestBytes * 9) / 10; // back off 10% and retry
    }

    outWordCount = 0;
    return nullptr;
}

void testInternalSram()
{
    Serial.println("=== Internal SRAM test ===");

    size_t wordCount = 0;
    void* raw = allocateLargestBlock(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, INTERNAL_HEADROOM_BYTES, wordCount);
    if (raw == nullptr) {
        Serial.println("  Allocation failed -- skipping internal SRAM test.");
        return;
    }

    const TestResult result = testRegion("internal SRAM", static_cast<volatile uint32_t*>(raw), wordCount);
    reportResult("Internal SRAM", result);

    heap_caps_free(raw);
}

void testPsram()
{
    Serial.println("=== PSRAM test ===");

    if (!psramFound()) {
        Serial.println("  psramFound() == false -- no PSRAM detected, skipping.");
        return;
    }

    Serial.printf("  ESP.getPsramSize()=%u ESP.getFreePsram()=%u largest free block=%u\n",
        static_cast<unsigned>(ESP.getPsramSize()), static_cast<unsigned>(ESP.getFreePsram()),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));

    size_t wordCount = 0;
    void* raw = allocateLargestBlock(MALLOC_CAP_SPIRAM, PSRAM_HEADROOM_BYTES, wordCount);
    if (raw == nullptr) {
        Serial.println("  Allocation failed -- skipping PSRAM test.");
        return;
    }

    const TestResult result = testRegion("PSRAM", static_cast<volatile uint32_t*>(raw), wordCount);
    reportResult("PSRAM", result);

    heap_caps_free(raw);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("################ RAM TEST START ################");
    Serial.printf("Chip: %s rev %d, %d MHz\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz());
    Serial.printf("Flash: %u bytes\n", static_cast<unsigned>(ESP.getFlashChipSize()));
    Serial.println();

    testInternalSram();
    Serial.println();
    testPsram();

    Serial.println();
    Serial.println("################ RAM TEST COMPLETE ################");
    Serial.println("(Re-run by pressing RESET -- this sketch does not loop the test automatically.)");
}

void loop()
{
    delay(1000);
}
