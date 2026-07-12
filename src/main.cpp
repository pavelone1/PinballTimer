#include <Arduino.h>
#include <SPI.h>
#include <TM1637Display.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ----------------------------------------------------
// Four TM1637 displays
// ----------------------------------------------------

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

// ----------------------------------------------------
// ST7789 TFT
// ----------------------------------------------------

constexpr uint8_t TFT_SCLK = 43;
constexpr uint8_t TFT_MOSI = 44;
constexpr uint8_t TFT_RST  = 1;
constexpr uint8_t TFT_DC   = 2;
constexpr uint8_t TFT_CS   = 42;

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ----------------------------------------------------
// Five button lights through ULN2803
//
// GPIO HIGH = ULN channel on = button light on
// ----------------------------------------------------

constexpr uint8_t BUTTON_LIGHTS[] = {
    5,  // Player 1
    7,  // Player 2
    15, // Player 3
    17, // Player 4
    8   // Control
};

constexpr uint8_t LIGHT_COUNT = 5;

// ----------------------------------------------------
// Colors
// ----------------------------------------------------

constexpr uint16_t COLOR_ORANGE = 0xFD20;
constexpr uint16_t COLOR_PURPLE = 0x8010;
constexpr uint16_t COLOR_DARK_BLUE = 0x0010;

// ----------------------------------------------------
// Basic output functions
// ----------------------------------------------------

void setButtonLight(uint8_t lightIndex, bool on)
{
    if (lightIndex >= LIGHT_COUNT) {
        return;
    }

    digitalWrite(BUTTON_LIGHTS[lightIndex], on ? HIGH : LOW);
}

void allButtonLights(bool on)
{
    for (uint8_t i = 0; i < LIGHT_COUNT; ++i) {
        setButtonLight(i, on);
    }
}

void clearTimerDisplays()
{
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].clear();
    }
}

void showNumberOnAllDisplays(int number)
{
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].showNumberDec(number, true);
    }
}

void showPlayerNumbers()
{
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].showNumberDec(i + 1, true);
    }
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

// ----------------------------------------------------
// Demonstration sequences
// ----------------------------------------------------

void startupSplash()
{
    tft.fillScreen(ST77XX_BLACK);

    drawCenteredText(
        "PINBALL",
        70,
        4,
        ST77XX_CYAN
    );

    drawCenteredText(
        "GAME TIMER",
        120,
        3,
        ST77XX_WHITE
    );

    drawCenteredText(
        "ESP32-S3",
        175,
        2,
        COLOR_ORANGE
    );

    for (uint8_t i = 0; i < LIGHT_COUNT; ++i) {
        setButtonLight(i, true);
        delay(120);
    }

    showNumberOnAllDisplays(8888);
    delay(1200);

    allButtonLights(false);
    clearTimerDisplays();
}

void colorScreenShow()
{
    const uint16_t colors[] = {
        ST77XX_RED,
        ST77XX_GREEN,
        ST77XX_BLUE,
        ST77XX_YELLOW,
        ST77XX_MAGENTA,
        ST77XX_CYAN,
        ST77XX_WHITE
    };

    for (uint16_t color : colors) {
        tft.fillScreen(color);
        delay(250);
    }

    tft.fillScreen(ST77XX_BLACK);
}

void lightChaseShow()
{
    tft.fillScreen(ST77XX_BLACK);
    drawCenteredText(
        "BUTTON LIGHT TEST",
        25,
        2,
        ST77XX_WHITE
    );

    for (uint8_t round = 0; round < 3; ++round) {
        for (uint8_t i = 0; i < LIGHT_COUNT; ++i) {
            allButtonLights(false);
            setButtonLight(i, true);

            if (i < DISPLAY_COUNT) {
                clearTimerDisplays();
                timerDisplays[i].showNumberDec(i + 1, true);
            }

            delay(180);
        }
    }

    allButtonLights(true);
    showNumberOnAllDisplays(8888);
    delay(500);

    allButtonLights(false);
    clearTimerDisplays();
}

void playerShow()
{
    const uint16_t playerColors[] = {
        ST77XX_RED,
        ST77XX_BLUE,
        ST77XX_GREEN,
        ST77XX_YELLOW
    };

    const char* playerNames[] = {
        "PLAYER 1",
        "PLAYER 2",
        "PLAYER 3",
        "PLAYER 4"
    };

    for (uint8_t player = 0; player < 4; ++player) {
        allButtonLights(false);
        setButtonLight(player, true);

        tft.fillScreen(ST77XX_BLACK);

        drawCenteredText(
            playerNames[player],
            80,
            4,
            playerColors[player]
        );

        drawCenteredText(
            "READY",
            145,
            3,
            ST77XX_WHITE
        );

        for (uint8_t display = 0;
             display < DISPLAY_COUNT;
             ++display) {

            timerDisplays[display].clear();
        }

        timerDisplays[player].showNumberDec(
            1000 + ((player + 1) * 111),
            true
        );

        delay(700);
    }

    allButtonLights(false);
    clearTimerDisplays();
}

void timerCountdownShow()
{
    tft.fillScreen(COLOR_DARK_BLUE);

    drawCenteredText(
        "COUNTDOWN",
        30,
        3,
        ST77XX_WHITE
    );

    drawCenteredText(
        "OUTPUT TEST",
        75,
        2,
        ST77XX_CYAN
    );

    allButtonLights(true);

    for (int seconds = 10; seconds >= 0; --seconds) {
        for (uint8_t display = 0;
             display < DISPLAY_COUNT;
             ++display) {

            const int shownValue =
                (display + 1) * 100 + seconds;

            timerDisplays[display].showNumberDec(
                shownValue,
                true
            );
        }

        tft.fillRect(
            40,
            130,
            160,
            80,
            COLOR_DARK_BLUE
        );

        char countdownText[8];
        snprintf(
            countdownText,
            sizeof(countdownText),
            "%02d",
            seconds
        );

        drawCenteredText(
            countdownText,
            130,
            8,
            seconds <= 3
                ? ST77XX_RED
                : ST77XX_YELLOW
        );

        if (seconds <= 3) {
            allButtonLights(seconds % 2 == 1);
        }

        delay(500);
    }

    allButtonLights(false);
}

void flashFinale()
{
    tft.fillScreen(ST77XX_BLACK);

    drawCenteredText(
        "GAME",
        65,
        5,
        ST77XX_GREEN
    );

    drawCenteredText(
        "READY!",
        130,
        5,
        ST77XX_YELLOW
    );

    showPlayerNumbers();

    for (uint8_t flash = 0; flash < 8; ++flash) {
        const bool on = flash % 2 == 0;

        allButtonLights(on);

        if (on) {
            showPlayerNumbers();
        } else {
            clearTimerDisplays();
        }

        delay(220);
    }

    allButtonLights(false);
    clearTimerDisplays();
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    // Button-light outputs
    for (uint8_t pin : BUTTON_LIGHTS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    // Numeric displays
    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].setBrightness(4, true);
        timerDisplays[i].clear();
    }

    // TFT SPI bus
    SPI.begin(
        TFT_SCLK,
        -1,
        TFT_MOSI,
        TFT_CS
    );

    // 240 x 320 ST7789
    tft.init(240, 320);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    Serial.println("Output demonstration started");
}

void loop()
{
    startupSplash();
    colorScreenShow();
    lightChaseShow();
    playerShow();
    timerCountdownShow();
    flashFinale();

    tft.fillScreen(ST77XX_BLACK);
    drawCenteredText(
        "RESTARTING SHOW",
        135,
        2,
        ST77XX_WHITE
    );

    delay(2000);
}