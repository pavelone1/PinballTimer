#pragma once

#include <cstdint>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "SystemTypes.h"

// Controls the ST7789 color TFT. Owns the SPI/hardware init and a
// small set of reusable drawing primitives (fill, centered text) plus
// a generic text-based status screen (title + lines). Deliberately
// does NOT hardcode specific screens (menus, player names, battery
// status, remote-control status, setup/standby screens) -- those
// depend on the game-mode and network subsystems, which don't exist
// yet. A future GameMode/App should request showStatusScreen() with
// whatever title/lines it wants; this module never decides content on
// its own, it only renders what it's given.
class TftDisplayManager {
public:
    TftDisplayManager();

    void begin();
    void update();

    void fillScreen(ColorId color);
    void drawCenteredText(const char* text, int16_t y, uint8_t textSize, ColorId color);

    void showStatusScreen(
        const char* title,
        const char* const* lines,
        uint8_t lineCount,
        ColorId background = ColorId::Black,
        ColorId titleColor = ColorId::White,
        ColorId lineColor = ColorId::White
    );

private:
    static constexpr uint8_t MAX_LINES = 5;
    static constexpr uint8_t MAX_LINE_LENGTH = 32;
    static constexpr uint8_t MAX_TITLE_LENGTH = 32;
    static constexpr int16_t TITLE_Y = 30;
    static constexpr int16_t FIRST_LINE_Y = 90;
    static constexpr int16_t LINE_SPACING = 35;

    Adafruit_ST7789 tft_;

    bool hasCachedScreen_ = false;
    char cachedTitle_[MAX_TITLE_LENGTH] = "";
    char cachedLines_[MAX_LINES][MAX_LINE_LENGTH] = {};
    uint8_t cachedLineCount_ = 0;
    ColorId cachedBackground_ = ColorId::Black;
    ColorId cachedTitleColor_ = ColorId::White;
    ColorId cachedLineColor_ = ColorId::White;

    uint16_t colorFor(ColorId color) const;
};
