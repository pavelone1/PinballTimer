#include "network/NetworkManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

void NetworkManager::begin(const char* ssid, const char* password)
{
    strncpy(ssid_, ssid, SSID_MAX_LENGTH - 1);
    ssid_[SSID_MAX_LENGTH - 1] = '\0';

    strncpy(password_, password, PASSWORD_MAX_LENGTH - 1);
    password_[PASSWORD_MAX_LENGTH - 1] = '\0';

    hasCredentials_ = ssid_[0] != '\0';
    standby_ = false;

    if (!hasCredentials_) {
        state_ = NetworkConnectionState::Disconnected;
        return;
    }

    WiFi.mode(WIFI_STA);
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

void NetworkManager::enterStandby()
{
    standby_ = true;
    WiFi.disconnect(true);
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
    WiFi.disconnect(true);
    state_ = NetworkConnectionState::Disconnected;
}

void NetworkManager::reconnect()
{
    if (!hasCredentials_) {
        return;
    }

    WiFi.disconnect(true);
    WiFi.begin(ssid_, password_);
    state_ = NetworkConnectionState::Connecting;
    lastConnectAttemptMs_ = millis();
}
