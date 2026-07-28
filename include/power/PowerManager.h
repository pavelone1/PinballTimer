#pragma once

#include <cstdint>
#include "game/GameMode.h"

enum class PowerState : uint8_t {
    Active,
    Standby,
    Critical
};

// Fires once per threshold crossing (edge-triggered, not level) as the
// battery discharges. Re-arms if the level later recovers with margin
// (see PowerManager.cpp's BATTERY_REARM_HYSTERESIS_PCT) so a
// subsequent discharge cycle notifies again instead of staying
// latched forever.
enum class BatteryEventType : uint8_t {
    Warning20,
    Warning10,
    Critical5
};

// A free function that returns the current battery voltage in volts.
// Takes no arguments (and so can't be a bound member function) --
// wire this up to whatever hardware read is decided later (ADC divider,
// I2C fuel gauge, etc). Until one is installed, or a stub value is set
// via setStubBatteryVoltage(), battery monitoring reports unavailable
// rather than fabricating a reading.
using BatteryVoltageReader = float (*)();

// Idle-timeout standby: blanks the TFT (real sleep command, not just
// black fill), numeric displays, and button lights when no local
// input or remote command has occurred for standbyTimeoutMs.
// notifyActivity() (call on any ButtonInput/EncoderInput event or
// executed DirectorControl command) resets the timer and wakes it.
//
// Never puts the ESP32 itself into light/deep sleep -- per the
// architecture doc, it "cannot enter full sleep while remote Wi-Fi
// control must remain available," so this only shuts down peripherals
// while the CPU/WiFi/WebServer keep running and stay responsive.
// NetworkManager's own enterStandby()/exitStandby() (which does drop
// the WiFi connection) is therefore NOT called automatically here --
// that's a separate, opt-in tradeoff the caller can invoke directly
// if they want network standby despite losing remote reachability.
//
// Battery monitoring: percentage/threshold/notification/Critical-state
// logic below is real and always running, but the physical sensing
// source is still stubbed -- no GPIO/ADC pin or fuel-gauge IC has been
// chosen yet (every ADC1 pin on the current board is already used by
// other peripherals; see HardwarePins.h). Until setBatteryVoltageReader()
// is wired to real hardware, feed it via setStubBatteryVoltage() --
// which is also how DirectorControl's SetStubBatteryVoltage remote
// command drives this for testing without hardware.
//
// "Shut down at 5%" is implemented as a new Critical PowerState, not a
// literal ESP32 deep sleep/power-off: it blanks every output exactly
// like Standby, but (like Standby) keeps the ESP32/WiFi/WebServer
// alive so a final critical-battery status stays reachable remotely.
// It does NOT auto-recover on local activity (unlike Standby) -- only
// the battery level itself recovering above the threshold (with
// hysteresis) exits it, since the point is a real low-power condition,
// not idleness. App::syncSystemState() surfaces Critical as
// SystemState::Error -- the first and so far only concrete trigger
// for that state.
class PowerManager {
public:
    void begin(GameModeContext& context, unsigned long standbyTimeoutMs = 300000);
    void update();

    void notifyActivity();

    PowerState state() const;

    void setStandbyTimeoutMs(unsigned long timeoutMs);

    // Real hardware path: wire this once a battery-sensing GPIO/IC is chosen.
    void setBatteryVoltageReader(BatteryVoltageReader reader);

    // Debug/testing path: overrides the reader with a fixed voltage.
    // Takes priority over a reader if both are set.
    void setStubBatteryVoltage(float volts);
    void clearBatteryVoltageOverride();

    bool batteryAvailable() const;
    float batteryVoltageV() const;
    uint8_t batteryPercent() const;

    // Drains one pending event per call. Call in a loop until it
    // returns false to process everything update() generated.
    bool pollBatteryEvent(BatteryEventType& outEvent);

    // Pure conversion, no timing dependency. Not covered by the native
    // test suite despite being pure: PowerManager.h pulls in
    // GameModeContext, which transitively drags in the TM1637/Adafruit
    // Arduino libraries the native env can't build against -- same
    // reasoning TimerManager.h gives for why millis()-dependent code
    // isn't tested there either.
    static uint8_t voltageToPercent(float voltageV);

private:
    static constexpr float BATTERY_EMPTY_V = 3.0f;   // placeholder single-cell Li-ion floor, pending real calibration
    static constexpr float BATTERY_FULL_V = 4.2f;    // placeholder single-cell Li-ion ceiling, pending real calibration
    static constexpr uint8_t BATTERY_WARNING_20_PCT = 20;
    static constexpr uint8_t BATTERY_WARNING_10_PCT = 10;
    static constexpr uint8_t BATTERY_CRITICAL_PCT = 5;
    static constexpr uint8_t BATTERY_REARM_HYSTERESIS_PCT = 3;
    static constexpr uint8_t BATTERY_EVENT_QUEUE_SIZE = 4;

    GameModeContext* context_ = nullptr;
    unsigned long standbyTimeoutMs_ = 300000;
    unsigned long lastActivityMs_ = 0;
    PowerState state_ = PowerState::Active;

    BatteryVoltageReader batteryVoltageReader_ = nullptr;
    bool manualVoltageOverrideActive_ = false;
    float manualVoltageOverrideV_ = 0.0f;

    bool batteryAvailable_ = false;
    float batteryVoltageV_ = 0.0f;
    uint8_t batteryPercent_ = 100;
    bool warning20Fired_ = false;
    bool warning10Fired_ = false;
    bool critical5Fired_ = false;

    BatteryEventType batteryEventQueue_[BATTERY_EVENT_QUEUE_SIZE] = {};
    uint8_t beQueueHead_ = 0;
    uint8_t beQueueTail_ = 0;
    uint8_t beQueueCount_ = 0;

    void enterStandby();
    void exitStandby();
    void enterCritical();
    void exitCritical();
    void blankOutputs();
    void restoreOutputs();

    void updateBattery();
    void pushBatteryEvent(BatteryEventType event);
};
