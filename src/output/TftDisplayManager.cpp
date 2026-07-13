#include "output/TftDisplayManager.h"

#include <Arduino.h>
#include <SPI.h>
#include <cstring>
#include "HardwarePins.h"

TftDisplayManager::TftDisplayManager()
    : tft_(HardwarePins::TFT_CS, HardwarePins::TFT_DC, HardwarePins::TFT_RST)
{
}

void TftDisplayManager::begin()
{
    SPI.begin(HardwarePins::TFT_SCLK, -1, HardwarePins::TFT_MOSI, HardwarePins::TFT_CS);
    tft_.init(240, 320);
    tft_.setRotation(1);
    tft_.fillScreen(ST77XX_BLACK);
    hasCachedScreen_ = false;
}

void TftDisplayManager::update()
{
    // Nothing periodic yet -- reserved for the App update-cycle
    // pattern (matches ButtonInput/EncoderInput/NumericDisplayManager)
    // for when animated/blinking screens are actually needed.
}

void TftDisplayManager::fillScreen(ColorId color)
{
    tft_.fillScreen(colorFor(color));
    hasCachedScreen_ = false;
}

void TftDisplayManager::drawCenteredText(
    const char* text,
    int16_t y,
    uint8_t textSize,
    ColorId color
)
{
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;

    tft_.setTextSize(textSize);
    tft_.setTextColor(colorFor(color));
    tft_.setTextWrap(false);

    tft_.getTextBounds(text, 0, y, &x1, &y1, &width, &height);

    const int16_t x =
        (static_cast<int16_t>(tft_.width()) -
         static_cast<int16_t>(width)) / 2;

    tft_.setCursor(x, y);
    tft_.print(text);

    hasCachedScreen_ = false;
}

void TftDisplayManager::showStatusScreen(
    const char* title,
    const char* const* lines,
    uint8_t lineCount,
    ColorId background,
    ColorId titleColor,
    ColorId lineColor
)
{
    const uint8_t clampedLineCount = lineCount > MAX_LINES ? MAX_LINES : lineCount;

    if (hasCachedScreen_ &&
        cachedBackground_ == background &&
        cachedTitleColor_ == titleColor &&
        cachedLineColor_ == lineColor &&
        cachedLineCount_ == clampedLineCount &&
        strncmp(cachedTitle_, title, MAX_TITLE_LENGTH) == 0) {

        bool linesMatch = true;
        for (uint8_t i = 0; i < clampedLineCount; ++i) {
            if (strncmp(cachedLines_[i], lines[i], MAX_LINE_LENGTH) != 0) {
                linesMatch = false;
                break;
            }
        }

        if (linesMatch) {
            return;
        }
    }

    tft_.fillScreen(colorFor(background));
    drawCenteredText(title, TITLE_Y, 3, titleColor);

    for (uint8_t i = 0; i < clampedLineCount; ++i) {
        drawCenteredText(lines[i], FIRST_LINE_Y + i * LINE_SPACING, 2, lineColor);
    }

    strncpy(cachedTitle_, title, MAX_TITLE_LENGTH - 1);
    cachedTitle_[MAX_TITLE_LENGTH - 1] = '\0';

    for (uint8_t i = 0; i < clampedLineCount; ++i) {
        strncpy(cachedLines_[i], lines[i], MAX_LINE_LENGTH - 1);
        cachedLines_[i][MAX_LINE_LENGTH - 1] = '\0';
    }

    cachedLineCount_ = clampedLineCount;
    cachedBackground_ = background;
    cachedTitleColor_ = titleColor;
    cachedLineColor_ = lineColor;
    hasCachedScreen_ = true;
}

uint16_t TftDisplayManager::colorFor(ColorId color) const
{
    switch (color) {
        case ColorId::Black:    return ST77XX_BLACK;
        case ColorId::White:    return ST77XX_WHITE;
        case ColorId::Red:      return ST77XX_RED;
        case ColorId::Green:    return ST77XX_GREEN;
        case ColorId::Blue:     return ST77XX_BLUE;
        case ColorId::Yellow:   return ST77XX_YELLOW;
        case ColorId::Cyan:     return ST77XX_CYAN;
        case ColorId::Magenta:  return ST77XX_MAGENTA;
        case ColorId::Orange:   return 0xFD20;
        case ColorId::Purple:   return 0x8010;
        case ColorId::DarkBlue: return 0x0010;
    }

    return ST77XX_WHITE;
}
