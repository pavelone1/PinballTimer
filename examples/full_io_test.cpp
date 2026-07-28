#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TM1637Display.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ESP32Encoder.h>

// Standalone full I/O diagnostic: exercises every wired input and
// output on the REV2 board, including the buzzer. REV2 pins
// confirmed -- see CLAUDE.md "REV2 hardware" and
// docs/images/rev2-pinout.svg / docs/images/uln2803-pinout.svg.
// Unlike REV1, button switches/lights are read/written through the
// MCP23017 I2C expander instead of native GPIO, and the TFT no
// longer shares pins with UART0, so the serial monitor stays fully
// usable throughout.
//
// Player buttons (switch P1-P4): pressing a button lights its own
// LED, buzzes, and increments a counter shown on that player's own
// TM1637 display, proving the switch/light/display/buzzer chain for
// that player.
//
// Action button (switch + light only, no display of its own): lights
// its LED, buzzes, and shows PRESSED/RELEASED on the TFT.
//
// Encoder: live rotation count and SW (push) state shown on the TFT,
// with a short click on every detent. Pressing the encoder resets
// all four player counts to 0.
//
// Buzzer: active piezo, plain digitalWrite HIGH/LOW (it has its own
// oscillator -- no tone()/PWM needed, see CLAUDE.md "Hardware").
// Button presses get a longer buzz, encoder rotation a short click,
// both non-blocking so the rest of loop() (debounce, TFT, displays)
// keeps running while a sound is playing.
//
// Build/upload with: pio run -e full-io-test -t upload
// Does not touch src/main.cpp or its pin assignments.

// ----------------------------------------------------
// Four TM1637 displays
// ----------------------------------------------------

constexpr uint8_t DISPLAY_1_CLK = 39;
constexpr uint8_t DISPLAY_1_DIO = 40;

constexpr uint8_t DISPLAY_2_CLK = 10;
constexpr uint8_t DISPLAY_2_DIO = 11;

constexpr uint8_t DISPLAY_3_CLK = 13;
constexpr uint8_t DISPLAY_3_DIO = 14;

constexpr uint8_t DISPLAY_4_CLK = 21;
constexpr uint8_t DISPLAY_4_DIO = 47;

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
constexpr uint8_t PLAYER_COUNT = 4;

// Indexed by button (P1, P2, P3, P4, Action); value is the GPA/GPB
// bit number that button's switch/light is actually wired to.
constexpr uint8_t GPA_BIT_FOR_BUTTON[] = {2, 0, 4, 3, 1};
constexpr uint8_t GPB_BIT_FOR_BUTTON[] = {4, 1, 3, 0, 2};

uint8_t gShadowGpioB = 0;

constexpr unsigned long DEBOUNCE_MS = 30;

bool buttonRawState[BUTTON_COUNT] = {false, false, false, false, false};
bool buttonStableState[BUTTON_COUNT] = {false, false, false, false, false};
unsigned long buttonLastChangeMs[BUTTON_COUNT] = {0, 0, 0, 0, 0};

uint16_t playerPressCount[PLAYER_COUNT] = {0, 0, 0, 0};

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
// Rotary encoder (unchanged: native GPIO/PCNT, not on the expander)
// ----------------------------------------------------

constexpr uint8_t ENCODER_CLK = 1;
constexpr uint8_t ENCODER_DT  = 2;
constexpr uint8_t ENCODER_SW  = 42;

ESP32Encoder encoder;

bool encoderSwRawState = false;
bool encoderSwStableState = false;
unsigned long encoderSwLastChangeMs = 0;

long lastEncoderCountForBuzz = 0;

// ----------------------------------------------------
// Buzzer: passive piezo, confirmed GPIO38. Has no oscillator of its
// own (REV2 switched away from the REV1-era active module), so it's
// driven via the ESP32's LEDC peripheral at an actual frequency
// instead of a plain digitalWrite -- see BuzzerManager for the real
// firmware's version of this same pattern. Non-blocking: startBuzz()
// starts a tone and remembers when to silence it; updateBuzzer()
// (called every loop tick) does the actual silence once that time
// passes.
// ----------------------------------------------------

constexpr uint8_t BUZZER_PIN = 38;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;
constexpr unsigned int BUZZ_BUTTON_HZ = 1800; // button press: clean beep
constexpr unsigned int BUZZ_CLICK_HZ  = 4000; // encoder detent: short click
constexpr unsigned long BUZZ_BUTTON_MS = 60; // button press: longer buzz
constexpr unsigned long BUZZ_CLICK_MS  = 10; // encoder detent: short click

bool buzzerOn = false;
unsigned long buzzerOffAtMs = 0;

void startBuzz(unsigned int frequencyHz, unsigned long durationMs)
{
    ledcWriteTone(BUZZER_LEDC_CHANNEL, frequencyHz);
    buzzerOn = true;
    buzzerOffAtMs = millis() + durationMs;
}

void updateBuzzer()
{
    if (buzzerOn && millis() >= buzzerOffAtMs) {
        ledcWriteTone(BUZZER_LEDC_CHANNEL, 0); // silence
        buzzerOn = false;
    }
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

    Wire.begin(I2C_SDA, I2C_SCL);
    mcpWriteRegister(REG_IODIRA, 0xFF); // bank A: all inputs (switches)
    mcpWriteRegister(REG_GPPUA, 0xFF);  // bank A: internal pull-ups on
    mcpWriteRegister(REG_IODIRB, 0x00); // bank B: all outputs (lights)
    mcpWriteRegister(REG_GPIOB, 0x00);  // bank B: start all off

    pinMode(ENCODER_SW, INPUT_PULLUP);

    ledcSetup(BUZZER_LEDC_CHANNEL, 2000, 10); // initial freq is a placeholder -- every tone overwrites it
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0); // silent at idle

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

    Serial.println("Full I/O test started (REV2 pins, MCP23017 buttons)");
}

void loop()
{
    updateBuzzer();

    // Player buttons: press -> light on, buzz, increment own display
    for (uint8_t i = 0; i < PLAYER_COUNT; ++i) {
        const bool pressed = readButtonPressEdge(i);

        mcpWriteButtonLight(i, buttonStableState[i]);

        if (pressed) {
            startBuzz(BUZZ_BUTTON_HZ, BUZZ_BUTTON_MS);
            playerPressCount[i]++;
            timerDisplays[i].showNumberDec(playerPressCount[i], false);
            Serial.printf("Player %u pressed (count %u)\n", i + 1, playerPressCount[i]);
        }
    }

    // Action button: light + buzz only, no display of its own
    if (readButtonPressEdge(4)) {
        startBuzz(BUZZ_BUTTON_HZ, BUZZ_BUTTON_MS);
        Serial.println("Action pressed");
    }
    mcpWriteButtonLight(4, buttonStableState[4]);

    // Encoder rotation: short click on every detent
    const long currentEncoderCount = encoder.getCount();
    if (currentEncoderCount != lastEncoderCountForBuzz) {
        lastEncoderCountForBuzz = currentEncoderCount;
        startBuzz(BUZZ_CLICK_HZ, BUZZ_CLICK_MS);
    }

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
