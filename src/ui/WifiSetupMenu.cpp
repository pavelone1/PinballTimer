#include "ui/WifiSetupMenu.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>
#include "ui/ScrollList.h"

void WifiSetupMenu::open(NetworkManager& network, SettingsStorage& settings, TftDisplayManager& tft)
{
    network_ = &network;
    settings_ = &settings;
    tft_ = &tft;
    open_ = true;
    networkCount_ = 0;
    selectedNetworkIndex_ = 0;
    pendingSsid_[0] = '\0';
    strncpy(previousSsid_, settings.wifiSsid(), SSID_MAX_LENGTH);
    previousSsid_[SSID_MAX_LENGTH] = '\0';
    strncpy(previousPassword_, settings.wifiPassword(), sizeof(previousPassword_) - 1);
    previousPassword_[sizeof(previousPassword_) - 1] = '\0';
    startScan();
}

void WifiSetupMenu::close()
{
    open_ = false;
    tft_->fillScreen(ColorId::Black);
}

bool WifiSetupMenu::isOpen() const
{
    return open_;
}

void WifiSetupMenu::update()
{
    if (!open_) {
        return;
    }

    switch (state_) {
        case State::Scanning:
            pollScan();
            break;

        case State::Connecting:
            network_->update();
            pollConnecting();
            break;

        case State::Result:
            if (millis() - stateEnteredMs_ >= RESULT_DISPLAY_MS) {
                if (connectSucceeded_) {
                    close(); // job done, hand control back to the game
                } else {
                    // Let the user retry rather than dumping them out
                    // of the whole flow on a bad password.
                    state_ = State::SelectNetwork;
                    selectedNetworkIndex_ = 0;
                    render();
                }
            }
            break;

        default:
            break;
    }
}

void WifiSetupMenu::handleEncoderEvent(const EncoderEvent& event)
{
    if (!open_) {
        return;
    }

    if (event.type == EncoderEventType::SwLongPress) {
        if (state_ == State::SelectNetwork || state_ == State::Scanning) {
            close();
        } else {
            if (state_ == State::Connecting || state_ == State::Result) {
                network_->disconnect();
                settings_->setWifiCredentials(previousSsid_, previousPassword_);
                network_->begin(previousSsid_, previousPassword_);
                network_->setKeepAlive(settings_->wifiKeepAlive());
            }
            state_ = State::SelectNetwork;
            selectedNetworkIndex_ = 0;
            render();
        }
        return;
    }

    switch (state_) {
        case State::SelectNetwork:
            handleSelectNetworkEncoder(event);
            break;

        case State::EnterSsidManual:
        case State::EnterPassword:
            handleTextEntryEncoder(event);
            break;

        default:
            break; // Scanning/Connecting/Result: input-less, ignore
    }
}

void WifiSetupMenu::startScan()
{
    state_ = State::Scanning;
    WiFi.scanNetworks(true); // async -- picked up in pollScan()
    render();
}

void WifiSetupMenu::pollScan()
{
    const int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) {
        return;
    }

    networkCount_ = 0;
    if (result > 0) {
        const uint8_t count = static_cast<uint8_t>(result) > MAX_NETWORKS
            ? MAX_NETWORKS
            : static_cast<uint8_t>(result);

        for (uint8_t i = 0; i < count; ++i) {
            strncpy(networkSsids_[i], WiFi.SSID(i).c_str(), SSID_MAX_LENGTH);
            networkSsids_[i][SSID_MAX_LENGTH] = '\0';
            networkIsOpen_[i] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        }
        networkCount_ = count;
    }

    WiFi.scanDelete();
    selectedNetworkIndex_ = 0;
    state_ = State::SelectNetwork;
    render();
}

void WifiSetupMenu::handleSelectNetworkEncoder(const EncoderEvent& event)
{
    const uint8_t totalItems = networkCount_ + 2; // + [Manual Entry] + [Rescan]

    if (event.type == EncoderEventType::RotatedClockwise) {
        selectedNetworkIndex_ = ScrollList::rotate(selectedNetworkIndex_, totalItems, true);
        render();
    } else if (event.type == EncoderEventType::RotatedCounterClockwise) {
        selectedNetworkIndex_ = ScrollList::rotate(selectedNetworkIndex_, totalItems, false);
        render();
    } else if (event.type == EncoderEventType::SwShortPress) {
        if (selectedNetworkIndex_ < networkCount_) {
            strncpy(pendingSsid_, networkSsids_[selectedNetworkIndex_], SSID_MAX_LENGTH);
            pendingSsid_[SSID_MAX_LENGTH] = '\0';
            textEntry_.reset();

            if (networkIsOpen_[selectedNetworkIndex_]) {
                beginConnect(); // no password to collect
            } else {
                state_ = State::EnterPassword;
                render();
            }
        } else if (selectedNetworkIndex_ == networkCount_) {
            textEntry_.reset();
            state_ = State::EnterSsidManual;
            render();
        } else {
            startScan();
        }
    }
}

void WifiSetupMenu::handleTextEntryEncoder(const EncoderEvent& event)
{
    const TextEntry::Result result = textEntry_.handleEncoderEvent(event);

    switch (result) {
        case TextEntry::Result::None:
            render();
            break;

        case TextEntry::Result::Cancel:
            textEntry_.reset();
            state_ = State::SelectNetwork;
            selectedNetworkIndex_ = 0;
            render();
            break;

        case TextEntry::Result::Done:
            if (state_ == State::EnterSsidManual) {
                strncpy(pendingSsid_, textEntry_.text(), SSID_MAX_LENGTH);
                pendingSsid_[SSID_MAX_LENGTH] = '\0';
                textEntry_.reset();
                state_ = State::EnterPassword;
                render();
            } else {
                beginConnect(); // finishing password entry
            }
            break;
    }
}

void WifiSetupMenu::beginConnect()
{
    settings_->setWifiCredentials(pendingSsid_, textEntry_.text());
    network_->begin(pendingSsid_, textEntry_.text()); // re-inits with the new credentials and reconnects
    network_->setKeepAlive(settings_->wifiKeepAlive());
    state_ = State::Connecting;
    stateEnteredMs_ = millis();
    render();
}

void WifiSetupMenu::pollConnecting()
{
    if (network_->isConnected()) {
        connectSucceeded_ = true;
        state_ = State::Result;
        stateEnteredMs_ = millis();
        render();
    } else if (millis() - stateEnteredMs_ >= CONNECT_TIMEOUT_MS) {
        connectSucceeded_ = false;
        failureStatus_ = static_cast<int>(WiFi.status());
        settings_->setWifiCredentials(previousSsid_, previousPassword_);
        network_->begin(previousSsid_, previousPassword_);
        network_->setKeepAlive(settings_->wifiKeepAlive());
        state_ = State::Result;
        stateEnteredMs_ = millis();
        render();
    }
}

void WifiSetupMenu::render()
{
    switch (state_) {
        case State::Scanning: {
            const char* lines[] = {"Please wait..."};
            tft_->showStatusScreen("SCANNING WIFI", lines, 1, ColorId::Black, ColorId::White, ColorId::White);
            break;
        }

        case State::SelectNetwork:
            renderSelectNetwork();
            break;

        case State::EnterSsidManual:
            renderTextEntry("ENTER SSID");
            break;

        case State::EnterPassword:
            renderTextEntry("WIFI PASSWORD");
            break;

        case State::Connecting:
            renderConnecting();
            break;

        case State::Result:
            renderResult();
            break;
    }
}

void WifiSetupMenu::renderSelectNetwork()
{
    static constexpr uint8_t LINE_LENGTH = 32;
    char lineBuf[MAX_VISIBLE_ITEMS][LINE_LENGTH];
    const char* linePtrs[MAX_VISIBLE_ITEMS];

    const uint8_t totalItems = networkCount_ + 2; // + [Manual Entry] + [Rescan]
    const uint8_t visibleCount = totalItems < MAX_VISIBLE_ITEMS ? totalItems : MAX_VISIBLE_ITEMS;
    const uint8_t windowStart = ScrollList::scrollWindowStart(selectedNetworkIndex_, totalItems, MAX_VISIBLE_ITEMS);

    for (uint8_t i = 0; i < visibleCount; ++i) {
        const uint8_t itemIndex = windowStart + i;
        const char* cursor = itemIndex == selectedNetworkIndex_ ? "> " : "  ";

        if (itemIndex < networkCount_) {
            snprintf(lineBuf[i], LINE_LENGTH, "%s%s%s",
                cursor, networkSsids_[itemIndex], networkIsOpen_[itemIndex] ? "" : " *");
        } else if (itemIndex == networkCount_) {
            snprintf(lineBuf[i], LINE_LENGTH, "%s[Manual Entry]", cursor);
        } else {
            snprintf(lineBuf[i], LINE_LENGTH, "%s[Rescan]", cursor);
        }
        linePtrs[i] = lineBuf[i];
    }

    tft_->showStatusScreen("SELECT WIFI NETWORK", linePtrs, visibleCount, ColorId::Black, ColorId::White, ColorId::Cyan);
}

void WifiSetupMenu::renderTextEntry(const char* title)
{
    char bufferLine[TextEntry::MAX_LENGTH + 2];
    snprintf(bufferLine, sizeof(bufferLine), "%s_", textEntry_.text());

    char pickerGlyph[8];
    textEntry_.currentPickerLabel(pickerGlyph, sizeof(pickerGlyph));
    char pickerLine[16];
    snprintf(pickerLine, sizeof(pickerLine), "> %s <", pickerGlyph);

    const char* lines[] = {bufferLine, pickerLine, "hold knob to cancel"};
    tft_->showStatusScreen(title, lines, 3, ColorId::Black, ColorId::White, ColorId::Yellow);
}

void WifiSetupMenu::renderConnecting()
{
    const char* lines[] = {pendingSsid_};
    tft_->showStatusScreen("CONNECTING...", lines, 1, ColorId::Black, ColorId::White, ColorId::White);
}

void WifiSetupMenu::renderResult()
{
    if (connectSucceeded_) {
        char ipLine[32];
        IPAddress ip = network_->localIP();
        snprintf(ipLine, sizeof(ipLine), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        char ssidLine[40];
        snprintf(ssidLine, sizeof(ssidLine), "Connected to %s", pendingSsid_);
        const char* lines[] = {ssidLine, ipLine};
        tft_->showStatusScreen("WIFI CONNECTED", lines, 2, ColorId::Black, ColorId::Green, ColorId::White);
    } else {
        char statusLine[24];
        snprintf(statusLine, sizeof(statusLine), "WiFi status: %d", failureStatus_);
        const char* lines[] = {"Could not connect", "Check password & retry", statusLine};
        tft_->showStatusScreen("WIFI SETUP", lines, 3, ColorId::Black, ColorId::Red, ColorId::White);
    }
}
