#include "network/WifiPortal.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_system.h>

namespace {

// Default adhoc/setup-hotspot address -- overrides the ESP32 SDK's
// stock softAP default (192.168.4.1) so it's a fixed, memorable
// address across every device rather than an SDK implementation
// detail. /24 is plenty for a handful of phones/laptops joining one
// device's setup hotspot.
const IPAddress kApLocalIp(10, 10, 10, 1);
const IPAddress kApGateway(10, 10, 10, 1);
const IPAddress kApSubnet(255, 255, 255, 0);

// application/x-www-form-urlencoded uses the same key=val&key2=val2
// shape as a URL query string, so httpd_query_key_value() can parse a
// POST body directly -- but (unlike Arduino's WebServer::arg(), which
// decodes automatically) it does NOT percent-decode the result, so
// that's done here by hand.
void urlDecode(char* str)
{
    char* dst = str;
    const char* src = str;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && isxdigit(static_cast<unsigned char>(src[1])) && isxdigit(static_cast<unsigned char>(src[2]))) {
            const char hex[3] = {src[1], src[2], '\0'};
            *dst++ = static_cast<char>(strtol(hex, nullptr, 16));
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

// Reads one field out of an already-decoded-shape form body (see
// urlDecode() above) -- writes "" if the key isn't present.
void extractFormField(const char* body, const char* key, char* out, size_t outSize)
{
    if (httpd_query_key_value(body, key, out, outSize) != ESP_OK) {
        out[0] = '\0';
        return;
    }

    urlDecode(out);
}

// ESP32's .rodata is flash-mapped and directly readable (unlike AVR),
// so this doesn't need PROGMEM/send_P -- a plain const char* is fine.
const char kSetupPageHtml[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Setup</title>
<style>
body{font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
h1{font-size:20px}
label{display:block;margin-top:10px;font-size:13px;color:#aaa}
select,input,button{width:100%;padding:10px;margin:4px 0;font-size:16px;box-sizing:border-box;border-radius:6px;border:1px solid #444;background:#222;color:#eee}
button{background:#2a8a4f;color:#fff;border:none;cursor:pointer;margin-top:14px}
#status{margin-top:14px;font-weight:bold;min-height:1.2em}
</style></head><body>
<h1>PinballTimer WiFi Setup</h1>
<p><a href="/game-live">Live game</a> &nbsp; <a href="/game-setup">Game setup</a> &nbsp; <a href="/machines">Machines</a></p>
<label>Network</label>
<select id="ssidSelect"><option>Scanning...</option></select>
<label>Or type SSID manually (for hidden networks)</label>
<input id="ssidManual" placeholder="Network name">
<label>Password</label>
<input id="password" type="password" placeholder="Leave blank if open network">
<button onclick="connect()">Connect</button>
<div id="status"></div>
<script>
function refreshScan(){
  fetch('/wifi-scan').then(function(r){return r.json();}).then(function(list){
    var sel = document.getElementById('ssidSelect');
    sel.innerHTML = '';
    if (list.length === 0) {
      var opt = document.createElement('option');
      opt.textContent = 'No networks found';
      sel.appendChild(opt);
      return;
    }
    list.forEach(function(n){
      var opt = document.createElement('option');
      opt.value = n.ssid;
      opt.textContent = n.ssid + (n.open ? '' : ' (locked)');
      sel.appendChild(opt);
    });
  });
}
function connect(){
  var manual = document.getElementById('ssidManual').value;
  var ssid = manual.length ? manual : document.getElementById('ssidSelect').value;
  var password = document.getElementById('password').value;
  document.getElementById('status').textContent = 'Connecting...';
  var body = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password);
  fetch('/wifi-connect', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: body})
    .then(poll);
}
function poll(){
  fetch('/wifi-status').then(function(r){return r.json();}).then(function(s){
    if (s.state === 'connecting') {
      document.getElementById('status').textContent = 'Connecting...';
      setTimeout(poll, 1000);
    } else if (s.state === 'saved') {
      document.getElementById('status').textContent = 'Saved. The setup hotspot will close while the device joins that network.';
    } else if (s.state === 'failed') {
      document.getElementById('status').textContent = 'Could not connect -- check the password and try again.';
    }
  });
}
refreshScan();
</script>
</body></html>
)HTML";

} // namespace

WifiPortal* WifiPortal::instance_ = nullptr;

void WifiPortal::begin(httpd_handle_t server, SettingsStorage& settings, TftDisplayManager& tft, const char* apPassword)
{
    server_ = server;
    settings_ = &settings;
    tft_ = &tft;
    instance_ = this;

    strncpy(apPassword_, apPassword, PASSWORD_MAX_LENGTH);
    apPassword_[PASSWORD_MAX_LENGTH] = '\0';

    registerRoutes();
}

void WifiPortal::applyStartupMode()
{
    if (settings_->wifiOperatingMode() != WifiOperatingMode::AccessPointOnly) {
        return;
    }

    persistentApActive_ = true;
    startAp();
}

void WifiPortal::open()
{
    startAp();
    interactiveOpen_ = true;
    state_ = State::Portal;
    stateEnteredMs_ = millis();
    renderTftStatus();
}

void WifiPortal::close()
{
    // Only actually tear the AP down if nothing wants it to persist
    // in the background -- either way, UI/input focus (isOpen())
    // returns to false, which is the part App relies on.
    if (!persistentApActive_) {
        dnsServer_.stop();
        WiFi.softAPdisconnect(true);
    }
    interactiveOpen_ = false;
    state_ = State::Idle;
}

bool WifiPortal::isOpen() const
{
    return interactiveOpen_;
}

void WifiPortal::startFallback()
{
    if (isApActive()) {
        return;
    }

    persistentApActive_ = false;
    interactiveOpen_ = false;
    startAp();
    state_ = State::Portal;
    stateEnteredMs_ = millis();
}

bool WifiPortal::isApActive() const
{
    const wifi_mode_t mode = WiFi.getMode();
    return mode == WIFI_MODE_AP;
}

void WifiPortal::revertToAdhoc()
{
    WiFi.disconnect(true);
    persistentApActive_ = true;
    settings_->setWifiOperatingMode(WifiOperatingMode::AccessPointOnly);
    open(); // shows the hotspot's join info rather than switching silently
}

void WifiPortal::setPersistentHotspot(bool persistent)
{
    persistentApActive_ = persistent;

    if (persistent) {
        // A persistent hotspot is now exclusive. "Both" used
        // WIFI_AP_STA and is intentionally no longer selected.
        settings_->setWifiOperatingMode(WifiOperatingMode::AccessPointOnly);
        startAp();
        return;
    }

    settings_->setWifiOperatingMode(WifiOperatingMode::StationOnly);

    // If the interactive flow is mid-flight, its own close() will
    // tear the AP down when it finishes, now that the flag above is
    // false -- only tear it down immediately here if nothing else is
    // about to.
    if (state_ == State::Idle) {
        dnsServer_.stop();
        WiFi.softAPdisconnect(true);
    }
}

bool WifiPortal::persistentHotspot() const
{
    return persistentApActive_;
}

void WifiPortal::update()
{
    if (persistentApActive_ || state_ != State::Idle) {
        dnsServer_.processNextRequest();
    }

    if (state_ == State::Idle) {
        return;
    }

    switch (state_) {
        case State::Success:
            if (millis() - stateEnteredMs_ >= SUCCESS_GRACE_MS) {
                close(); // App notices isOpen()==false and finalizes via NetworkManager::begin()
            }
            break;

        case State::Failed:
            if (millis() - stateEnteredMs_ >= FAILURE_DISPLAY_MS) {
                startAp(); // WiFi.disconnect() may have left the radio in AP_STA; rebuild clean AP-only for a retry
                state_ = State::Portal;
                stateEnteredMs_ = millis();
                renderTftStatus();
            }
            break;

        default:
            break;
    }
}

void WifiPortal::registerRoutes()
{
    httpd_uri_t rootUri = {};
    rootUri.uri = "/";
    rootUri.method = HTTP_GET;
    rootUri.handler = &handleRootTrampoline;
    rootUri.user_ctx = this;
    httpd_register_uri_handler(server_, &rootUri);

    httpd_uri_t scanUri = {};
    scanUri.uri = "/wifi-scan";
    scanUri.method = HTTP_GET;
    scanUri.handler = &handleScanTrampoline;
    scanUri.user_ctx = this;
    httpd_register_uri_handler(server_, &scanUri);

    httpd_uri_t connectUri = {};
    connectUri.uri = "/wifi-connect";
    connectUri.method = HTTP_POST;
    connectUri.handler = &handleConnectTrampoline;
    connectUri.user_ctx = this;
    httpd_register_uri_handler(server_, &connectUri);

    httpd_uri_t statusUri = {};
    statusUri.uri = "/wifi-status";
    statusUri.method = HTTP_GET;
    statusUri.handler = &handleStatusTrampoline;
    statusUri.user_ctx = this;
    httpd_register_uri_handler(server_, &statusUri);

    // Captive-portal redirect: any unmatched path (including the
    // probe URLs iOS/Android/Windows use to auto-popup a setup
    // browser, e.g. /generate_204, /hotspot-detect.html) bounces to
    // the setup page instead of a plain 404.
    httpd_register_err_handler(server_, HTTPD_404_NOT_FOUND, &handleNotFoundTrampoline);
}

esp_err_t WifiPortal::handleRoot(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kSetupPageHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WifiPortal::handleScan(httpd_req_t* req)
{
    const int16_t count = WiFi.scanNetworks(); // synchronous: this is a single deliberate operator action, not polled
    const int16_t shown = count > 0 ? (count > 20 ? 20 : count) : 0;

    char buf[768];
    size_t offset = 0;
    offset += snprintf(buf + offset, sizeof(buf) - offset, "[");

    for (int16_t i = 0; i < shown; ++i) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s{\"ssid\":\"%s\",\"open\":%s}",
            i == 0 ? "" : ",",
            WiFi.SSID(i).c_str(),
            WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
    }
    snprintf(buf + offset, sizeof(buf) - offset, "]");

    WiFi.scanDelete();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WifiPortal::handleConnect(httpd_req_t* req)
{
    if (state_ != State::Portal) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"result\":\"not_active\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    char body[192] = "";
    const size_t toRead = req->content_len < sizeof(body) - 1 ? req->content_len : sizeof(body) - 1;
    const int received = toRead > 0 ? httpd_req_recv(req, body, toRead) : 0;
    body[received > 0 ? static_cast<size_t>(received) : 0] = '\0';

    extractFormField(body, "ssid", pendingSsid_, sizeof(pendingSsid_));
    extractFormField(body, "password", pendingPassword_, sizeof(pendingPassword_));

    if (pendingSsid_[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"result\":\"invalid_request\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"result\":\"connecting\"}", HTTPD_RESP_USE_STRLEN);

    // Save first, then let App switch from AP to STA after the browser
    // has received this response. Never enable WIFI_AP_STA.
    settings_->setWifiCredentials(pendingSsid_, pendingPassword_);
    settings_->setWifiOperatingMode(WifiOperatingMode::StationOnly);
    persistentApActive_ = false;
    state_ = State::Success;
    stateEnteredMs_ = millis();
    renderTftStatus();
    return ESP_OK;
}

esp_err_t WifiPortal::handleStatus(httpd_req_t* req)
{
    char buf[96];

    switch (state_) {
        case State::TestConnect:
        case State::Success:
            snprintf(buf, sizeof(buf), "{\"state\":\"saved\"}");
            break;

        case State::Failed:
            snprintf(buf, sizeof(buf), "{\"state\":\"failed\"}");
            break;

        default:
            snprintf(buf, sizeof(buf), "{\"state\":\"idle\"}");
            break;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WifiPortal::handleNotFound(httpd_req_t* req)
{
    if (state_ == State::Idle) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Captive-portal redirect: any unmatched path (including the
    // probe URLs iOS/Android/Windows use to auto-popup a setup
    // browser, e.g. /generate_204, /hotspot-detect.html) bounces to
    // the setup page instead of a plain 404.
    IPAddress apIp = WiFi.softAPIP();
    char location[32];
    snprintf(location, sizeof(location), "http://%u.%u.%u.%u/", apIp[0], apIp[1], apIp[2], apIp[3]);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

esp_err_t WifiPortal::handleRootTrampoline(httpd_req_t* req)
{
    return static_cast<WifiPortal*>(req->user_ctx)->handleRoot(req);
}

esp_err_t WifiPortal::handleScanTrampoline(httpd_req_t* req)
{
    return static_cast<WifiPortal*>(req->user_ctx)->handleScan(req);
}

esp_err_t WifiPortal::handleConnectTrampoline(httpd_req_t* req)
{
    return static_cast<WifiPortal*>(req->user_ctx)->handleConnect(req);
}

esp_err_t WifiPortal::handleStatusTrampoline(httpd_req_t* req)
{
    return static_cast<WifiPortal*>(req->user_ctx)->handleStatus(req);
}

esp_err_t WifiPortal::handleNotFoundTrampoline(httpd_req_t* req, httpd_err_code_t /*error*/)
{
    // Error handlers (unlike regular URI handlers) have no per-request
    // user_ctx slot -- this relies on there only ever being one
    // WifiPortal instance (App owns exactly one).
    return instance_->handleNotFound(req);
}

void WifiPortal::startAp()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char apSsid[24];
    snprintf(apSsid, sizeof(apSsid), "PinballTimer%02X%02X", mac[4], mac[5]);

    // Transition cleanly to an exclusive AP. Never use WIFI_AP_STA.
    if (WiFi.getMode() != WIFI_MODE_AP) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(20);
        WiFi.mode(WIFI_AP);
    }
    WiFi.softAPConfig(kApLocalIp, kApGateway, kApSubnet);
    WiFi.softAP(apSsid, apPassword_);
    dnsServer_.start(53, "*", WiFi.softAPIP());
}

void WifiPortal::renderTftStatus()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char apSsidLine[32];
    snprintf(apSsidLine, sizeof(apSsidLine), "Join: PinballTimer%02X%02X", mac[4], mac[5]);
    char apPasswordLine[24];
    snprintf(apPasswordLine, sizeof(apPasswordLine), "Pass: %s", apPassword_);

    switch (state_) {
        case State::Portal: {
            const char* lines[] = {apSsidLine, apPasswordLine, "Then browse to 10.10.10.1"};
            tft_->showStatusScreen("WIFI SETUP", lines, 3, ColorId::Black, ColorId::Cyan, ColorId::White);
            break;
        }

        case State::Success: {
            const char* lines[] = {pendingSsid_, "Saved; joining shortly"};
            tft_->showStatusScreen("WIFI SAVED", lines, 2, ColorId::Black, ColorId::Green, ColorId::White);
            break;
        }

        case State::Failed: {
            const char* lines[] = {"Check password", "Returning to setup..."};
            tft_->showStatusScreen("CONNECT FAILED", lines, 2, ColorId::Black, ColorId::Red, ColorId::White);
            break;
        }

        default:
            break;
    }
}
