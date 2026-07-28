#include "network/NetworkManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

namespace {

bool apCurrentlyActive()
{
    const wifi_mode_t mode = WiFi.getMode();
    return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
}

// WiFi.disconnect(true) stops the whole WiFi driver, taking any
// concurrently-running WifiPortal access point down with it --
// acceptable when there's no AP to preserve, wrong when there is (see
// WifiPortal.h's persistent-hotspot / "Both" operating mode).
void disconnectStaPreservingAp()
{
    WiFi.disconnect(!apCurrentlyActive());
}

} // namespace

void NetworkManager::begin(const char* ssid, const char* password)
{
    strncpy(ssid_, ssid, SSID_MAX_LENGTH - 1);
    ssid_[SSID_MAX_LENGTH - 1] = '\0';

    strncpy(password_, password, PASSWORD_MAX_LENGTH - 1);
    password_[PASSWORD_MAX_LENGTH - 1] = '\0';

    hasCredentials_ = ssid_[0] != '\0';
    standby_ = false;

    // Always bring the network stack (lwIP's TCP/IP task, netif, the
    // WiFi driver) up via WiFi.mode() -- even with no saved SSID yet
    // and nothing to actually connect to. DirectorControl/WifiPortal
    // start an HTTP server shortly after this returns, and opening a
    // listening socket requires that stack already running; on a
    // fresh device (no saved credentials) this WiFi.mode() call is
    // the only thing that brings it up before they call begin(). See
    // CLAUDE.md's "Firmware status" for the full story -- without
    // this, httpd_start() hits lwIP's "assert failed:
    // tcpip_send_msg_wait_sem ... (Invalid mbox)" and aborts, which
    // looked for a long time like an unrelated TFT/SPI crash because
    // of where execution happened to be when the abort fired.
    //
    // Preserve an already-active WifiPortal access point rather than
    // forcing STA-only.
    WiFi.mode(apCurrentlyActive() ? WIFI_AP_STA : WIFI_STA);

    if (!hasCredentials_) {
        state_ = NetworkConnectionState::Disconnected;
        return;
    }

    WiFi.begin(ssid_, password_);
    state_ = NetworkConnectionState::Connecting;
    lastConnectAttemptMs_ = millis();
}

void NetworkManager::update()
{
    if (standby_ || !hasCredentials_) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        state_ = NetworkConnectionState::Connected;
        return;
    }

    const unsigned long now = millis();

    if (now - lastConnectAttemptMs_ >= RECONNECT_INTERVAL_MS) {
        state_ = state_ == NetworkConnectionState::Connected
            ? NetworkConnectionState::Reconnecting
            : NetworkConnectionState::Connecting;

        WiFi.begin(ssid_, password_);
        lastConnectAttemptMs_ = now;
    } else if (state_ == NetworkConnectionState::Connected) {
        state_ = NetworkConnectionState::Reconnecting;
    }
}

NetworkConnectionState NetworkManager::connectionState() const
{
    return state_;
}

bool NetworkManager::isConnected() const
{
    return state_ == NetworkConnectionState::Connected;
}

IPAddress NetworkManager::localIP() const
{
    return WiFi.localIP();
}

void NetworkManager::enterStandby()
{
    standby_ = true;
    disconnectStaPreservingAp();
    state_ = NetworkConnectionState::Standby;
}

void NetworkManager::exitStandby()
{
    standby_ = false;

    if (!hasCredentials_) {
        state_ = NetworkConnectionState::Disconnected;
        return;
    }

    WiFi.begin(ssid_, password_);
    state_ = NetworkConnectionState::Connecting;
    lastConnectAttemptMs_ = millis();
}

void NetworkManager::disconnect()
{
    disconnectStaPreservingAp();
    state_ = NetworkConnectionState::Disconnected;
}

void NetworkManager::reconnect()
{
    if (!hasCredentials_) {
        return;
    }

    disconnectStaPreservingAp();
    WiFi.begin(ssid_, password_);
    state_ = NetworkConnectionState::Connecting;
    lastConnectAttemptMs_ = millis();
}
