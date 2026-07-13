#pragma once

#include <cstdint>
#include <Preferences.h>

// Persistent system preferences (NVS, "settings" namespace). Values
// are cached in RAM after begin() and every setter writes through to
// flash immediately -- these change rarely (unlike active timer
// values, which must never be written continuously to flash and
// aren't handled by this class at all).
class SettingsStorage {
public:
    void begin();

    void setLastSelectedMode(uint8_t modeId);
    uint8_t lastSelectedMode() const;

    void setBrightness(uint8_t brightness);
    uint8_t brightness() const;

    void setSoundEnabled(bool enabled);
    bool soundEnabled() const;

    void setWifiCredentials(const char* ssid, const char* password);
    const char* wifiSsid() const;
    const char* wifiPassword() const;

    void setDirectorControlEnabled(bool enabled);
    bool directorControlEnabled() const;

    // true = prefer remote (director) control when available, false = local priority.
    void setPreferRemoteControl(bool preferRemote);
    bool preferRemoteControl() const;

    void setStandbyTimeoutSeconds(uint32_t seconds);
    uint32_t standbyTimeoutSeconds() const;

    void setDeviceName(const char* name);
    const char* deviceName() const;

private:
    static constexpr uint8_t SSID_MAX_LENGTH = 32;
    static constexpr uint8_t PASSWORD_MAX_LENGTH = 64;
    static constexpr uint8_t DEVICE_NAME_MAX_LENGTH = 32;

    Preferences prefs_;

    uint8_t lastSelectedMode_ = 0;
    uint8_t brightness_ = 4;
    bool soundEnabled_ = true;
    char wifiSsid_[SSID_MAX_LENGTH] = "";
    char wifiPassword_[PASSWORD_MAX_LENGTH] = "";
    bool directorControlEnabled_ = false;
    bool preferRemoteControl_ = false;
    uint32_t standbyTimeoutSeconds_ = 300;
    char deviceName_[DEVICE_NAME_MAX_LENGTH] = "PinballTimer";
};
