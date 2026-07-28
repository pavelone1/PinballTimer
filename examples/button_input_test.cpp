#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TM1637Display.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Standalone button input diagnostic: pressing any button (P1-4 or
// Action) shows which one on the TFT and lights that button's own
// LED; P1-4 additionally flash their assigned player display with
// its assigned number while held (Action has no display of its own).
// REV2 pins confirmed -- see CLAUDE.md "REV2 hardware" and
// docs/images/rev2-pinout.svg / docs/images/uln2803-pinout.svg.
// Buttons/lights go through the MCP23017 I2C expander, same confirmed
// non-sequential bank A/B wiring as examples/full_io_test.cpp.
//
// Build/upload with: pio run -e button-input-test -t upload
// Does not touch src/main.cpp or its pin assignments.

// ----------------------------------------------------
// Four TM1637 displays (index 0-3 = players 1-4)
// ----------------------------------------------------

constexpr uint8_t DISPLAY_1_CLK = 39;
constexpr uint8_t DISPLAY_1_DIO = 40;

constexpr uint8_t DISPLAY_2_CLK = 10;
constexpr uint8_t DISPLAY_2_DIO = 11;

constexpr uint8_t DISPLAY_3_CLK = 13;
constexpr uint8_t DISPLAY_3_DIO = 14;

constexpr uint8_t DISPLAY_4_CLK = 21;
constexpr uint8_t DISPLAY_4_DIO = 47;

TM1637Display playerDisplays[] = {
    TM1637Display(DISPLAY_1_CLK, DISPLAY_1_DIO),
    TM1637Display(DISPLAY_2_CLK, DISPLAY_2_DIO),
    TM1637Display(DISPLAY_3_CLK, DISPLAY_3_DIO),
    TM1637Display(DISPLAY_4_CLK, DISPLAY_4_DIO)
};

constexpr uint8_t PLAYER_COUNT = 4;
constexpr unsigned long FLASH_INTERVAL_MS = 150;

bool displayHeldLast[PLAYER_COUNT] = {false, false, false, false};
bool displayFlashOn[PLAYER_COUNT] = {false, false, false, false};
unsigned long displayLastToggleMs[PLAYER_COUNT] = {0, 0, 0, 0};

// ----------------------------------------------------
// ST7789 TFT
// ----------------------------------------------------

constexpr uint8_t TFT_SCLK = 4;
constexpr uint8_t TFT_MOSI = 5;
constexpr uint8_t TFT_RST  = 6;
constexpr uint8_t TFT_DC   = 7;
constexpr uint8_t TFT_CS   = 15;

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ----------------------------------------------------
// Buttons: switches (bank A) and lights (bank B) via MCP23017.
// Index 0-3 = players 1-4, index 4 = Action. Neither bank is in
// index order -- confirmed physical wiring (see CLAUDE.md):
// GPA0=P2, GPA1=Action, GPA2=P1, GPA3=P4, GPA4=P3
// GPB0=P4, GPB1=P2, GPB2=Action, GPB3=P3, GPB4=P1
// ----------------------------------------------------

constexpr uint8_t I2C_SDA = 17;
constexpr uint8_t I2C_SCL = 18;
constexpr uint8_t MCP23017_ADDR = 0x20;

constexpr uint8_t REG_IODIRA = 0x00;
constexpr uint8_t REG_IODIRB = 0x01;
constexpr uint8_t REG_GPPUA  = 0x0C;
constexpr uint8_t REG_GPIOA  = 0x12;
constexpr uint8_t REG_GPIOB  = 0x13;

constexpr uint8_t BUTTON_COUNT = 5;

// Indexed by button (P1, P2, P3, P4, Action); value is the GPA/GPB
// bit number that button's switch/light is actually wired to.
constexpr uint8_t GPA_BIT_FOR_BUTTON[] = {2, 0, 4, 3, 1};
constexpr uint8_t GPB_BIT_FOR_BUTTON[] = {4, 1, 3, 0, 2};

const char* const BUTTON_NAMES[BUTTON_COUNT] = {"P1", "P2", "P3", "P4", "ACTION"};

uint8_t gShadowGpioB = 0;

constexpr unsigned long DEBOUNCE_MS = 30;

bool buttonRawState[BUTTON_COUNT] = {false, false, false, false, false};
bool buttonStableState[BUTTON_COUNT] = {false, false, false, false, false};
unsigned long buttonLastChangeMs[BUTTON_COUNT] = {0, 0, 0, 0, 0};

void mcpWriteRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(MCP23017_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t mcpReadRegister(uint8_t reg)
{
    Wire.beginTransmission(MCP23017_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(static_cast<int>(MCP23017_ADDR), 1);
    return Wire.available() ? static_cast<uint8_t>(Wire.read()) : 0;
}

bool mcpReadButtonRaw(uint8_t index)
{
    const uint8_t gpioA = mcpReadRegister(REG_GPIOA);
    return (gpioA & (1 << GPA_BIT_FOR_BUTTON[index])) == 0; // active-low
}

void mcpWriteButtonLight(uint8_t index, bool on)
{
    const uint8_t bit = GPB_BIT_FOR_BUTTON[index];
    if (on) {
        gShadowGpioB |= static_cast<uint8_t>(1 << bit);
    } else {
        gShadowGpioB &= static_cast<uint8_t>(~(1 << bit));
    }
    mcpWriteRegister(REG_GPIOB, gShadowGpioB);
}

// ----------------------------------------------------
// TFT helpers
// ----------------------------------------------------

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

void showStatus(int8_t heldButton)
{
    tft.fillRect(0, 60, tft.width(), 150, ST77XX_BLACK);

    if (heldButton < 0) {
        drawCenteredText("(no button pressed)", 130, 2, ST77XX_WHITE);
        return;
    }

    drawCenteredText(BUTTON_NAMES[heldButton], 70, 5, ST77XX_YELLOW);

    char line[24];
    snprintf(line, sizeof(line), "%s PRESSED", BUTTON_NAMES[heldButton]);
    drawCenteredText(line, 150, 2, ST77XX_YELLOW);

    char pinLine[24];
    snprintf(pinLine, sizeof(pinLine), "MCP23017 GPA%u", GPA_BIT_FOR_BUTTON[heldButton]);
    drawCenteredText(pinLine, 185, 2, ST77XX_CYAN);
}

// ----------------------------------------------------
// Debounced button read; returns true on a new press edge
// ----------------------------------------------------

bool readButtonPressEdge(uint8_t index)
{
    const bool raw = mcpReadButtonRaw(index);
    const unsigned long now = millis();

    if (raw != buttonRawState[index]) {
        buttonRawState[index] = raw;
        buttonLastChangeMs[index] = now;
    }

    if (now - buttonLastChangeMs[index] >= DEBOUNCE_MS &&
        buttonStableState[index] != buttonRawState[index]) {
        buttonStableState[index] = buttonRawState[index];
        return buttonStableState[index];
    }

    return false;
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Wire.begin(I2C_SDA, I2C_SCL);
    mcpWriteRegister(REG_IODIRA, 0xFF); // bank A: all inputs (switches)
    mcpWriteRegister(REG_GPPUA, 0xFF);  // bank A: internal pull-ups on
    mcpWriteRegister(REG_IODIRB, 0x00); // bank B: all outputs (lights)
    mcpWriteRegister(REG_GPIOB, 0x00);  // bank B: start all off

    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        playerDisplays[i].setBrightness(4, true);
        playerDisplays[i].clear();
    }

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 320);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    drawCenteredText("BUTTON INPUT TEST", 20, 2, ST77XX_WHITE);
    showStatus(-1);

    Serial.println("Button input test started (REV2 pins, MCP23017 buttons)");
}

void loop()
{
    const unsigned long now = millis();
    int8_t heldButton = -1;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed = readButtonPressEdge(i);

        mcpWriteButtonLight(i, buttonStableState[i]);

        if (pressed) {
            Serial.printf("%s pressed (MCP23017 GPA%u)\n", BUTTON_NAMES[i], GPA_BIT_FOR_BUTTON[i]);
        }

        if (buttonStableState[i]) {
            heldButton = static_cast<int8_t>(i);
        }
    }

    // TFT: show whichever button is currently held (last one wins if
    // more than one somehow is), or the idle message if none are.
    static int8_t lastShownButton = -1;
    if (heldButton != lastShownButton) {
        lastShownButton = heldButton;
        showStatus(heldButton);
    }

    // Player displays: flash the assigned number while that player's
    // button is held, clear on release.
    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        if (buttonStableState[i]) {
            if (!displayHeldLast[i]) {
                displayHeldLast[i] = true;
                displayFlashOn[i] = true;
                displayLastToggleMs[i] = now;
                playerDisplays[i].showNumberDec(i + 1, false);
            } else if (now - displayLastToggleMs[i] >= FLASH_INTERVAL_MS) {
                displayLastToggleMs[i] = now;
                displayFlashOn[i] = !displayFlashOn[i];

                if (displayFlashOn[i]) {
                    playerDisplays[i].showNumberDec(i + 1, false);
                } else {
                    playerDisplays[i].clear();
                }
            }
        } else if (displayHeldLast[i]) {
            displayHeldLast[i] = false;
            playerDisplays[i].clear();
        }
    }
}
