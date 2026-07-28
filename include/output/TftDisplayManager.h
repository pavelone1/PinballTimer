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

    // Puts the panel into sleep + display-off mode (real ST7789
    // commands, not just a black fill) for power saving. wake()
    // reverses it and forces the next showStatusScreen() to redraw.
    void sleep();
    void wake();

    void showStatusScreen(
        const char* title,
        const char* const* lines,
        uint8_t lineCount,
        ColorId background = ColorId::Black,
        ColorId titleColor = ColorId::White,
        ColorId lineColor = ColorId::White
    );

    // Fixed layout for "whose turn + which ball" (Mode1RoundRobin):
    // title (e.g. player name, same size/position as showStatusScreen()'s
    // title) with a "Ball" label directly beneath it at the same size,
    // then the ball number itself in a very large font filling most of
    // the rest of the screen. A dedicated method rather than another
    // showStatusScreen() line since the number needs a much bigger,
    // fixed text size than the generic title+lines layout supports.
    void showBallScreen(
        const char* title,
        uint8_t ballNumber,
        ColorId background = ColorId::Black,
        ColorId titleColor = ColorId::White,
        ColorId labelColor = ColorId::White,
        ColorId numberColor = ColorId::White
    );

private:
    static constexpr uint8_t MAX_LINES = 5;
    static constexpr uint8_t MAX_LINE_LENGTH = 32;
    static constexpr uint8_t MAX_TITLE_LENGTH = 32;
    static constexpr int16_t TITLE_Y = 30;
    static constexpr int16_t FIRST_LINE_Y = 90;
    static constexpr int16_t LINE_SPACING = 35;

    static constexpr int16_t BALL_LABEL_Y = 70;
    static constexpr int16_t BALL_NUMBER_Y = 110;
    static constexpr uint8_t BALL_NUMBER_TEXT_SIZE = 16;

    Adafruit_ST7789 tft_;

    // Which of showStatusScreen()/showBallScreen() last actually drew
    // the physical screen -- each method's cache-hit check also
    // requires this to match its own kind, so switching between the
    // two always redraws even if the other kind's stale cached content
    // happens to still match (they track completely separate fields).
    enum class ScreenKind : uint8_t { None, Status, Ball };
    ScreenKind lastScreenKind_ = ScreenKind::None;

    bool hasCachedScreen_ = false;
    char cachedTitle_[MAX_TITLE_LENGTH] = "";
    char cachedLines_[MAX_LINES][MAX_LINE_LENGTH] = {};
    uint8_t cachedLineCount_ = 0;
    ColorId cachedBackground_ = ColorId::Black;
    ColorId cachedTitleColor_ = ColorId::White;
    ColorId cachedLineColor_ = ColorId::White;

    bool hasCachedBallScreen_ = false;
    char cachedBallTitle_[MAX_TITLE_LENGTH] = "";
    uint8_t cachedBallNumber_ = 0;
    ColorId cachedBallBackground_ = ColorId::Black;
    ColorId cachedBallTitleColor_ = ColorId::White;
    ColorId cachedBallLabelColor_ = ColorId::White;
    ColorId cachedBallNumberColor_ = ColorId::White;

    uint16_t colorFor(ColorId color) const;
};
