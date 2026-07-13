#pragma once

#include <cstdint>

// Handles WiFi connection lifecycle only -- it does not interpret
// game commands or know anything about DirectorControl. Connects
// using whatever SSID/password it's given (from SettingsStorage);
// "private or venue network" just means whichever single network was
// configured, there is no dual-network failover here.
enum class NetworkConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Standby
};

class NetworkManager {
public:
    void begin(const char* ssid, const char* password);
    void update();

    NetworkConnectionState connectionState() const;
    bool isConnected() const;

    // Lowers activity (disconnects) to save power without forgetting
    // the configured credentials; update()/exitStandby() resumes.
    void enterStandby();
    void exitStandby();

    void disconnect();
    void reconnect();

private:
    static constexpr uint8_t SSID_MAX_LENGTH = 32;
    static constexpr uint8_t PASSWORD_MAX_LENGTH = 64;
    static constexpr unsigned long RECONNECT_INTERVAL_MS = 5000;

    char ssid_[SSID_MAX_LENGTH] = "";
    char password_[PASSWORD_MAX_LENGTH] = "";
    NetworkConnectionState state_ = NetworkConnectionState::Disconnected;
    unsigned long lastConnectAttemptMs_ = 0;
    bool standby_ = false;
    bool hasCredentials_ = false;
};
