#include <Arduino.h>
#include <SPI.h>
#include <TM1637Display.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Standalone diagnostic sketch: each TM1637 display cycles its own
// digit repeated N times (display 1 -> 1,11,111,1111; display 2 ->
// 2,22,222,2222; etc.) so each can be identified and compared at a
// glance. The TFT cycles through colors and shows the current step
// so it can be checked alongside the numeric displays.
// Build/upload with: pio run -e display-test -t upload
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

constexpr uint8_t TFT_SCLK = 43;
constexpr uint8_t TFT_MOSI = 44;
constexpr uint8_t TFT_RST  = 1;
constexpr uint8_t TFT_DC   = 2;
constexpr uint8_t TFT_CS   = 42;

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

constexpr uint8_t PATTERN_LENGTH = 4;
constexpr unsigned long STEP_DELAY_MS = 700;

const uint16_t STEP_COLORS[PATTERN_LENGTH] = {
    ST77XX_RED,
    ST77XX_YELLOW,
    ST77XX_GREEN,
    ST77XX_CYAN
};

int patternValueFor(uint8_t displayIndex, uint8_t step)
{
    const int digit = displayIndex + 1;
    int value = 0;

    for (uint8_t i = 0; i <= step; ++i) {
        value = value * 10 + digit;
    }

    return value;
}

void drawCenteredText(
    const char* text,
    int16_t y,
    uint8_t textSize,
    uint16_t color
)
{
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;

    tft.setTextSize(textSize);
    tft.setTextColor(color);
    tft.setTextWrap(false);

    tft.getTextBounds(text, 0, y, &x1, &y1, &width, &height);

    const int16_t x =
        (static_cast<int16_t>(tft.width()) -
         static_cast<int16_t>(width)) / 2;

    tft.setCursor(x, y);
    tft.print(text);
}

void showTftStep(uint8_t step)
{
    tft.fillScreen(STEP_COLORS[step]);

    drawCenteredText("TFT TEST", 40, 3, ST77XX_BLACK);

    char stepText[16];
    snprintf(stepText, sizeof(stepText), "STEP %u/%u", step + 1, PATTERN_LENGTH);
    drawCenteredText(stepText, 100, 3, ST77XX_BLACK);
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].setBrightness(4, true);
        timerDisplays[i].clear();
    }

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 320);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    Serial.println("Display + TFT flash test started");
}

void loop()
{
    for (uint8_t step = 0; step < PATTERN_LENGTH; ++step) {
        for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
            const int value = patternValueFor(i, step);

            Serial.printf("Display %u showing %d\n", i + 1, value);

            timerDisplays[i].showNumberDec(value, false);
        }

        Serial.printf("TFT showing step %u\n", step + 1);
        showTftStep(step);

        delay(STEP_DELAY_MS);
    }
}
