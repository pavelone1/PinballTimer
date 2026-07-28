#include "network/NetworkManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

void NetworkManager::begin(const char* ssid, const char* password)
{
    strncpy(ssid_, ssid, SSID_MAX_LENGTH);
    ssid_[SSID_MAX_LENGTH] = '\0';

    strncpy(password_, password, PASSWORD_MAX_LENGTH);
    password_[PASSWORD_MAX_LENGTH] = '\0';

    hasCredentials_ = ssid_[0] != '\0';
    standby_ = false;
    reconnectIntervalMs_ = INITIAL_RECONNECT_INTERVAL_MS;

    // Own the radio exclusively. In particular, never preserve an AP
    // by selecting WIFI_AP_STA: that mode is the trigger implicated by
    // the REV2 WiFi-task heap-corruption coredump.
    if (WiFi.getMode() != WIFI_MODE_STA) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(20);
        WiFi.mode(WIFI_STA);
    }
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(20);
    WiFi.setSleep(!keepAlive_);

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
        reconnectIntervalMs_ = INITIAL_RECONNECT_INTERVAL_MS;
        return;
    }

    const unsigned long now = millis();

    if (now - lastConnectAttemptMs_ >= reconnectIntervalMs_) {
        state_ = state_ == NetworkConnectionState::Connected
            ? NetworkConnectionState::Reconnecting
            : NetworkConnectionState::Connecting;

        WiFi.begin(ssid_, password_);
        lastConnectAttemptMs_ = now;
        reconnectIntervalMs_ =
            reconnectIntervalMs_ >= MAX_RECONNECT_INTERVAL_MS / 2
                ? MAX_RECONNECT_INTERVAL_MS
                : reconnectIntervalMs_ * 2;
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
    WiFi.disconnect(false);
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
    reconnectIntervalMs_ = INITIAL_RECONNECT_INTERVAL_MS;
}

void NetworkManager::disconnect()
{
    WiFi.disconnect(false);
    state_ = NetworkConnectionState::Disconnected;
}

void NetworkManager::reconnect()
{
    if (!hasCredentials_) {
        return;
    }

    WiFi.disconnect(false);
    WiFi.begin(ssid_, password_);
    state_ = NetworkConnectionState::Connecting;
    lastConnectAttemptMs_ = millis();
    reconnectIntervalMs_ = INITIAL_RECONNECT_INTERVAL_MS;
}

void NetworkManager::setKeepAlive(bool enabled)
{
    // enabled=true disables modem sleep, keeping the HTTP interface
    // responsive while the application's displays are in standby.
    keepAlive_ = enabled;
    WiFi.setSleep(!enabled);
}
