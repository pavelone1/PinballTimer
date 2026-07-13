#pragma once

#include <cstdint>
#include "game/GameMode.h"
#include "network/NetworkManager.h"

enum class PowerState : uint8_t {
    Active,
    Standby
};

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
// Battery monitoring is not implemented ("later" per the doc) since
// no ADC pin has been assigned for it. "Reduced network reporting"
// doesn't apply to the current on-demand REST design -- there is no
// periodic server-initiated status push to reduce.
class PowerManager {
public:
    void begin(GameModeContext& context, unsigned long standbyTimeoutMs = 300000);
    void update();

    void notifyActivity();

    PowerState state() const;

    void setStandbyTimeoutMs(unsigned long timeoutMs);

private:
    GameModeContext* context_ = nullptr;
    unsigned long standbyTimeoutMs_ = 300000;
    unsigned long lastActivityMs_ = 0;
    PowerState state_ = PowerState::Active;

    void enterStandby();
    void exitStandby();
};
