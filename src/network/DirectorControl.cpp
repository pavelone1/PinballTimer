#include "network/DirectorControl.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "network/StatusReporter.h"

namespace {

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

} // namespace

void DirectorControl::begin(GameModeManager& modeManager, GameModeContext& context, StatusReporter& statusReporter, PowerManager& power)
{
    modeManager_ = &modeManager;
    context_ = &context;
    statusReporter_ = &statusReporter;
    power_ = &power;
    localControlsLocked_ = false;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // DirectorControl's own 2 handlers + WifiPortal's 5 + DirectorDashboard's
    // 2 (both registered separately onto this same instance via
    // server()) = 9 of 16 used, still headroom.
    config.max_uri_handlers = 16;
    httpd_start(&server_, &config);

    httpd_uri_t statusUri = {};
    statusUri.uri = "/status";
    statusUri.method = HTTP_GET;
    statusUri.handler = &handleStatusRouteTrampoline;
    statusUri.user_ctx = this;
    httpd_register_uri_handler(server_, &statusUri);

    httpd_uri_t commandUri = {};
    commandUri.uri = "/command";
    commandUri.method = HTTP_POST;
    commandUri.handler = &handleCommandRouteTrampoline;
    commandUri.user_ctx = this;
    httpd_register_uri_handler(server_, &commandUri);
}

void DirectorControl::update()
{
    // esp_http_server runs its own FreeRTOS task internally -- unlike
    // Arduino WebServer::handleClient(), there's nothing to poll here.
}

DirectorCommandResult DirectorControl::execute(const DirectorCommand& command)
{
    if (command.type == DirectorCommandType::RequestFullStatus) {
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::SelectMode) {
        return modeManager_->selectMode(command.intValue)
            ? DirectorCommandResult::Ok
            : DirectorCommandResult::InvalidRequest;
    }

    if (command.type == DirectorCommandType::LockLocalControls) {
        localControlsLocked_ = true;
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::UnlockLocalControls) {
        localControlsLocked_ = false;
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::SetStubBatteryVoltage) {
        power_->setStubBatteryVoltage(static_cast<float>(command.longValue) / 1000.0f);
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::ClearBatteryVoltageOverride) {
        power_->clearBatteryVoltageOverride();
        return DirectorCommandResult::Ok;
    }

    // Game-setup metadata (see DirectorDashboard's /game-setup page) --
    // always allowed, same reasoning as SelectMode: these are
    // configuration writes, not gameplay commands, so they shouldn't
    // require a mode to already be selected/active.
    if (command.type == DirectorCommandType::SetPlayerName) {
        if (command.intValue >= static_cast<uint8_t>(PlayerId::Count)) {
            return DirectorCommandResult::InvalidRequest;
        }
        const PlayerId player = static_cast<PlayerId>(command.intValue);
        context_->players.setName(player, command.stringKey);
        context_->gameStorage.setPlayerName(player, command.stringKey);
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::SetPlayerButton) {
        if (command.intValue >= static_cast<uint8_t>(PlayerId::Count) ||
            command.longValue < 0 || command.longValue >= static_cast<long>(ButtonId::Count)) {
            return DirectorCommandResult::InvalidRequest;
        }
        const PlayerId player = static_cast<PlayerId>(command.intValue);
        const ButtonId button = static_cast<ButtonId>(command.longValue);
        context_->players.setButtonAssignment(player, button);
        context_->gameStorage.setPlayerButtonAssignment(player, button);
        return DirectorCommandResult::Ok;
    }

    if (command.type == DirectorCommandType::SetMachineName) {
        context_->gameStorage.setMachineName(command.stringKey);
        return DirectorCommandResult::Ok;
    }

    if (!modeManager_->hasActiveMode()) {
        return DirectorCommandResult::NoActiveMode;
    }

    if (!modeManager_->activeMode()->allowsRemoteCommand(static_cast<uint8_t>(command.type))) {
        return DirectorCommandResult::Rejected;
    }

    switch (command.type) {
        case DirectorCommandType::SetPlayerCount:
            return modeManager_->setPlayerCount(command.intValue)
                ? DirectorCommandResult::Ok
                : DirectorCommandResult::InvalidRequest;

        case DirectorCommandType::SetModeOption:
            return modeManager_->activeMode()->setModeOption(command.stringKey, command.longValue)
                ? DirectorCommandResult::Ok
                : DirectorCommandResult::InvalidRequest;

        case DirectorCommandType::StartGame:
            modeManager_->initializeActiveMode();
            modeManager_->notifyRemoteStart();
            modeManager_->notifyGameStart();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::StartFirstTimer:
            modeManager_->notifyFirstTimerStart();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Pause:
            modeManager_->notifyPause();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Resume:
            modeManager_->notifyResume();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::Reset:
            modeManager_->notifyReset();
            return DirectorCommandResult::Ok;

        case DirectorCommandType::IdentifyTimer:
            context_->buttonLights.setTemporaryOverride(
                static_cast<ButtonId>(command.intValue),
                LightPattern::Blink,
                150,
                3000,
                255
            );
            return DirectorCommandResult::Ok;

        default:
            return DirectorCommandResult::InvalidRequest;
    }
}

bool DirectorControl::localControlsLocked() const
{
    return localControlsLocked_;
}

httpd_handle_t DirectorControl::server() const
{
    return server_;
}

esp_err_t DirectorControl::handleStatusRoute(httpd_req_t* req)
{
    char buffer[1024];
    statusReporter_->buildStatusJson(buffer, sizeof(buffer));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t DirectorControl::handleCommandRoute(httpd_req_t* req)
{
    char body[256] = "";
    const size_t toRead = req->content_len < sizeof(body) - 1 ? req->content_len : sizeof(body) - 1;
    const int received = toRead > 0 ? httpd_req_recv(req, body, toRead) : 0;
    body[received > 0 ? static_cast<size_t>(received) : 0] = '\0';

    char typeStr[32];
    char intValueStr[8];
    char longValueStr[16];
    extractFormField(body, "type", typeStr, sizeof(typeStr));
    extractFormField(body, "intValue", intValueStr, sizeof(intValueStr));
    extractFormField(body, "longValue", longValueStr, sizeof(longValueStr));

    DirectorCommand command;
    command.type = parseCommandType(typeStr);
    command.intValue = static_cast<uint8_t>(atoi(intValueStr));
    extractFormField(body, "stringKey", command.stringKey, sizeof(command.stringKey));
    command.longValue = atol(longValueStr);

    if (command.type != DirectorCommandType::RequestFullStatus) {
        power_->notifyActivity();
    }

    const DirectorCommandResult result = execute(command);

    char responseBuffer[64];
    snprintf(responseBuffer, sizeof(responseBuffer), "{\"result\":\"%s\"}", resultToString(result));
    httpd_resp_set_status(req, result == DirectorCommandResult::Ok ? "200 OK" : "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, responseBuffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t DirectorControl::handleStatusRouteTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorControl*>(req->user_ctx)->handleStatusRoute(req);
}

esp_err_t DirectorControl::handleCommandRouteTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorControl*>(req->user_ctx)->handleCommandRoute(req);
}

DirectorCommandType DirectorControl::parseCommandType(const char* name)
{
    static const struct { const char* text; DirectorCommandType type; } kMap[] = {
        {"SelectMode", DirectorCommandType::SelectMode},
        {"SetPlayerCount", DirectorCommandType::SetPlayerCount},
        {"SetModeOption", DirectorCommandType::SetModeOption},
        {"StartGame", DirectorCommandType::StartGame},
        {"StartFirstTimer", DirectorCommandType::StartFirstTimer},
        {"Pause", DirectorCommandType::Pause},
        {"Resume", DirectorCommandType::Resume},
        {"Reset", DirectorCommandType::Reset},
        {"LockLocalControls", DirectorCommandType::LockLocalControls},
        {"UnlockLocalControls", DirectorCommandType::UnlockLocalControls},
        {"IdentifyTimer", DirectorCommandType::IdentifyTimer},
        {"RequestFullStatus", DirectorCommandType::RequestFullStatus},
        {"SetStubBatteryVoltage", DirectorCommandType::SetStubBatteryVoltage},
        {"ClearBatteryVoltageOverride", DirectorCommandType::ClearBatteryVoltageOverride},
        {"SetPlayerName", DirectorCommandType::SetPlayerName},
        {"SetPlayerButton", DirectorCommandType::SetPlayerButton},
        {"SetMachineName", DirectorCommandType::SetMachineName},
    };

    for (const auto& entry : kMap) {
        if (strcmp(name, entry.text) == 0) {
            return entry.type;
        }
    }

    return DirectorCommandType::Unknown;
}

const char* DirectorControl::resultToString(DirectorCommandResult result)
{
    switch (result) {
        case DirectorCommandResult::Ok: return "ok";
        case DirectorCommandResult::Rejected: return "rejected";
        case DirectorCommandResult::InvalidRequest: return "invalid_request";
        case DirectorCommandResult::NoActiveMode: return "no_active_mode";
    }

    return "unknown";
}
