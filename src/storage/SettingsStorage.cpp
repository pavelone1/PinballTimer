#include "storage/SettingsStorage.h"

#include <cstring>

void SettingsStorage::begin()
{
    prefs_.begin("settings", false);

    lastSelectedMode_ = prefs_.getUChar("lastMode", lastSelectedMode_);
    brightness_ = prefs_.getUChar("brightness", brightness_);
    soundEnabled_ = prefs_.getBool("sound", soundEnabled_);
    prefs_.getString("wifiSsid", wifiSsid_, sizeof(wifiSsid_));
    prefs_.getString("wifiPass", wifiPassword_, sizeof(wifiPassword_));
    // Runtime-only safety/power preference: every physical boot starts
    // with modem sleep allowed. Deliberately ignore and remove values
    // written by older firmware.
    wifiKeepAlive_ = false;
    prefs_.remove("wifiKeep");
    wifiOperatingMode_ = static_cast<WifiOperatingMode>(
        prefs_.getUChar("wifiApMode", static_cast<uint8_t>(wifiOperatingMode_))
    );
    directorControlEnabled_ = prefs_.getBool("directorEn", directorControlEnabled_);
    preferRemoteControl_ = prefs_.getBool("preferRemote", preferRemoteControl_);
    standbyTimeoutSeconds_ = prefs_.getUInt("standbyTO", standbyTimeoutSeconds_);
    prefs_.getString("deviceName", deviceName_, DEVICE_NAME_MAX_LENGTH);
}

void SettingsStorage::setLastSelectedMode(uint8_t modeId)
{
    lastSelectedMode_ = modeId;
    prefs_.putUChar("lastMode", modeId);
}

uint8_t SettingsStorage::lastSelectedMode() const
{
    return lastSelectedMode_;
}

void SettingsStorage::setBrightness(uint8_t brightness)
{
    brightness_ = brightness > 7 ? 7 : brightness;
    prefs_.putUChar("brightness", brightness_);
}

uint8_t SettingsStorage::brightness() const
{
    return brightness_;
}

void SettingsStorage::setSoundEnabled(bool enabled)
{
    soundEnabled_ = enabled;
    prefs_.putBool("sound", enabled);
}

bool SettingsStorage::soundEnabled() const
{
    return soundEnabled_;
}

void SettingsStorage::setWifiCredentials(const char* ssid, const char* password)
{
    strncpy(wifiSsid_, ssid, SSID_MAX_LENGTH);
    wifiSsid_[SSID_MAX_LENGTH] = '\0';

    strncpy(wifiPassword_, password, PASSWORD_MAX_LENGTH);
    wifiPassword_[PASSWORD_MAX_LENGTH] = '\0';

    prefs_.putString("wifiSsid", wifiSsid_);
    prefs_.putString("wifiPass", wifiPassword_);
}

void SettingsStorage::clearWifiCredentials()
{
    setWifiCredentials("", "");
}

const char* SettingsStorage::wifiSsid() const
{
    return wifiSsid_;
}

const char* SettingsStorage::wifiPassword() const
{
    return wifiPassword_;
}

void SettingsStorage::setWifiKeepAlive(bool enabled)
{
    wifiKeepAlive_ = enabled;
}

bool SettingsStorage::wifiKeepAlive() const
{
    return wifiKeepAlive_;
}

void SettingsStorage::setWifiOperatingMode(WifiOperatingMode mode)
{
    wifiOperatingMode_ = mode;
    prefs_.putUChar("wifiApMode", static_cast<uint8_t>(mode));
}

WifiOperatingMode SettingsStorage::wifiOperatingMode() const
{
    return wifiOperatingMode_;
}

void SettingsStorage::setDirectorControlEnabled(bool enabled)
{
    directorControlEnabled_ = enabled;
    prefs_.putBool("directorEn", enabled);
}

bool SettingsStorage::directorControlEnabled() const
{
    return directorControlEnabled_;
}

void SettingsStorage::setPreferRemoteControl(bool preferRemote)
{
    preferRemoteControl_ = preferRemote;
    prefs_.putBool("preferRemote", preferRemote);
}

bool SettingsStorage::preferRemoteControl() const
{
    return preferRemoteControl_;
}

void SettingsStorage::setStandbyTimeoutSeconds(uint32_t seconds)
{
    standbyTimeoutSeconds_ = seconds;
    prefs_.putUInt("standbyTO", seconds);
}

uint32_t SettingsStorage::standbyTimeoutSeconds() const
{
    return standbyTimeoutSeconds_;
}

void SettingsStorage::setDeviceName(const char* name)
{
    strncpy(deviceName_, name, DEVICE_NAME_MAX_LENGTH - 1);
    deviceName_[DEVICE_NAME_MAX_LENGTH - 1] = '\0';
    prefs_.putString("deviceName", deviceName_);
}

const char* SettingsStorage::deviceName() const
{
    return deviceName_;
}
