#include <Arduino.h>
#include <SPI.h>
#include <TM1637Display.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ESP32Encoder.h>

// Standalone full I/O diagnostic: exercises every wired input and
// output on the assembled prototype (no buzzer yet).
//
// Player buttons (switch P1-P4): pressing a button lights its own
// LED and increments a counter shown on that player's own TM1637
// display, proving the switch/light/display chain for that player.
//
// Action button (switch + light only, no display of its own): lights
// its LED while held and shows PRESSED/RELEASED on the TFT.
//
// Encoder: live rotation count and SW (push) state shown on the TFT.
// Pressing the encoder resets all four player counts to 0.
//
// Build/upload with: pio run -e full-io-test -t upload
// Does not touch src/main.cpp or its pin assignments.

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
// Buttons: switches (inputs) and lights (outputs)
// Index 0-3 = players 1-4, index 4 = Action
// ----------------------------------------------------

constexpr uint8_t BUTTON_SWITCHES[] = {4, 6, 16, 18, 19};
constexpr uint8_t BUTTON_LIGHTS[]   = {5, 7, 15, 17, 8};
constexpr uint8_t BUTTON_COUNT = 5;
constexpr uint8_t PLAYER_COUNT = 4;

constexpr unsigned long DEBOUNCE_MS = 30;

bool buttonRawState[BUTTON_COUNT] = {false, false, false, false, false};
bool buttonStableState[BUTTON_COUNT] = {false, false, false, false, false};
unsigned long buttonLastChangeMs[BUTTON_COUNT] = {0, 0, 0, 0, 0};

uint16_t playerPressCount[PLAYER_COUNT] = {0, 0, 0, 0};

// ----------------------------------------------------
// Rotary encoder
// ----------------------------------------------------

constexpr uint8_t ENCODER_CLK = 11;
constexpr uint8_t ENCODER_DT  = 12;
constexpr uint8_t ENCODER_SW  = 41;

ESP32Encoder encoder;

bool encoderSwRawState = false;
bool encoderSwStableState = false;
unsigned long encoderSwLastChangeMs = 0;

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

// ----------------------------------------------------
// Debounced button read; returns true on a new press edge
// ----------------------------------------------------

bool readButtonPressEdge(uint8_t index)
{
    const bool raw = digitalRead(BUTTON_SWITCHES[index]) == LOW;
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

bool readEncoderSwPressEdge()
{
    const bool raw = digitalRead(ENCODER_SW) == LOW;
    const unsigned long now = millis();

    if (raw != encoderSwRawState) {
        encoderSwRawState = raw;
        encoderSwLastChangeMs = now;
    }

    if (now - encoderSwLastChangeMs >= DEBOUNCE_MS &&
        encoderSwStableState != encoderSwRawState) {
        encoderSwStableState = encoderSwRawState;
        return encoderSwStableState;
    }

    return false;
}

void resetPlayerCounts()
{
    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        playerPressCount[i] = 0;
        timerDisplays[i].showNumberDec(0, false);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(BUTTON_SWITCHES[i], INPUT_PULLUP);
        pinMode(BUTTON_LIGHTS[i], OUTPUT);
        digitalWrite(BUTTON_LIGHTS[i], LOW);
    }

    pinMode(ENCODER_SW, INPUT_PULLUP);

    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(ENCODER_CLK, ENCODER_DT);
    encoder.setCount(0);

    for (uint8_t i = 0; i < DISPLAY_COUNT; ++i) {
        timerDisplays[i].setBrightness(4, true);
        timerDisplays[i].clear();
        timerDisplays[i].showNumberDec(0, false);
    }

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 320);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    drawCenteredText("FULL I/O TEST", 20, 2, ST77XX_WHITE);

    Serial.println("Full I/O test started");
}

void loop()
{
    // Player buttons: press -> light on, increment own display
    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        const bool pressed = readButtonPressEdge(i);

        digitalWrite(BUTTON_LIGHTS[i], buttonStableState[i] ? HIGH : LOW);

        if (pressed) {
            playerPressCount[i]++;
            timerDisplays[i].showNumberDec(playerPressCount[i], false);
            Serial.printf("Player %u pressed (count %u)\n", i + 1, playerPressCount[i]);
        }
    }

    // Action button: light only, no display of its own
    readButtonPressEdge(4);
    digitalWrite(BUTTON_LIGHTS[4], buttonStableState[4] ? HIGH : LOW);

    // Encoder push resets all player counts
    if (readEncoderSwPressEdge()) {
        Serial.println("Encoder SW pressed: resetting player counts");
        resetPlayerCounts();
    }

    // TFT status: encoder count, encoder SW state, Action state
    static long lastEncoderCount = 0;
    static bool lastActionState = false;
    static bool lastEncoderSwState = false;
    static unsigned long lastTftUpdate = 0;
    const unsigned long now = millis();

    const long encoderCount = encoder.getCount();
    const bool actionState = buttonStableState[4];
    const bool encoderSwState = encoderSwStableState;

    const bool changed =
        encoderCount != lastEncoderCount ||
        actionState != lastActionState ||
        encoderSwState != lastEncoderSwState;

    if (changed && now - lastTftUpdate >= 100) {
        lastEncoderCount = encoderCount;
        lastActionState = actionState;
        lastEncoderSwState = encoderSwState;
        lastTftUpdate = now;

        tft.fillRect(0, 60, tft.width(), 140, ST77XX_BLACK);

        char line[32];

        snprintf(line, sizeof(line), "ENCODER: %ld", encoderCount);
        drawCenteredText(line, 80, 2, ST77XX_CYAN);

        drawCenteredText(
            encoderSwState ? "ENC SW: PRESSED" : "ENC SW: released",
            120,
            2,
            encoderSwState ? ST77XX_YELLOW : ST77XX_WHITE
        );

        drawCenteredText(
            actionState ? "ACTION: PRESSED" : "ACTION: released",
            160,
            2,
            actionState ? ST77XX_YELLOW : ST77XX_WHITE
        );
    }
}
