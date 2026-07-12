#include <Arduino.h>
#include <TM1637Display.h>

// Standalone diagnostic sketch: each display cycles its own digit
// repeated N times (display 1 -> 1,11,111,1111; display 2 ->
// 2,22,222,2222; etc.) so each can be identified and compared at a
// glance. Build/upload with: pio run -e display-test -t upload
// Does not touch src/main.cpp or its pin assignments.

constexpr uint8_t DISPLAY_1_CLK = 40;
constexpr uint8_t DISPLAY_1_DIO = 39;

constexpr uint8_t DISPLAY_2_CLK = 9;
constexpr uint8_t DISPLAY_2_DIO = 10;

constexpr uint8_t DISPLAY_3_CLK = 13;
constexpr uint8_t DISPLAY_3_DIO = 14;

constexpr uint8_t DISPLAY_4_CLK = 47;
constexpr uint8_t DISPLAY_4_DIO = 21;

TM1637Display timerDisplays[] = {
    TM1637Display(DISPLAY_1_CLK, DISPLAY_1_DIO),
    TM1637Display(DISPLAY_2_CLK, DISPLAY_2_DIO),
    TM1637Display(DISPLAY_3_CLK, DISPLAY_3_DIO),
    TM1637Display(DISPLAY_4_CLK, DISPLAY_4_DIO)
};

constexpr uint8_t DISPLAY_COUNT = 4;

constexpr uint8_t PATTERN_LENGTH = 4;
constexpr unsigned long STEP_DELAY_MS = 700;

int patternValueFor(uint8_t displayIndex, uint8_t step)
{
    const int digit = displayIndex + 1;
    int value = 0;

    for (uint8_t i = 0; i <= step; ++i) {
        value = value * 10 + digit;
    }

    return value;
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].setBrightness(4, true);
        timerDisplays[i].clear();
    }

    Serial.println("Display flash test started");
}

void loop()
{
    for (uint8_t step = 0; step < PATTERN_LENGTH; ++step) {
        for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
            const int value = patternValueFor(i, step);

            Serial.printf("Display %u showing %d\n", i + 1, value);

            timerDisplays[i].showNumberDec(value, false);
        }

        delay(STEP_DELAY_MS);
    }
}
