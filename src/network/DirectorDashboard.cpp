#include "network/DirectorDashboard.h"

#include <cstdio>
#include <cstring>

namespace {

const char* machineTypeName(MachineType type)
{
    switch (type) {
        case MachineType::EM: return "EM";
        case MachineType::SolidState: return "Solid State";
        case MachineType::DMD: return "DMD";
        case MachineType::Modern: return "Modern";
    }
    return "Unknown";
}

esp_err_t sendJsonString(httpd_req_t* req, const char* value)
{
    if (httpd_resp_send_chunk(req, "\"", 1) != ESP_OK) {
        return ESP_FAIL;
    }
    const char* segment = value;
    for (const char* cursor = value; ; ++cursor) {
        const char escaped = *cursor;
        if (escaped != '"' && escaped != '\\' && escaped != '\0') {
            continue;
        }
        if (cursor > segment &&
            httpd_resp_send_chunk(req, segment, cursor - segment) != ESP_OK) {
            return ESP_FAIL;
        }
        if (escaped == '\0') {
            break;
        }
        const char pair[2] = {'\\', escaped};
        if (httpd_resp_send_chunk(req, pair, sizeof(pair)) != ESP_OK) {
            return ESP_FAIL;
        }
        segment = cursor + 1;
    }
    return httpd_resp_send_chunk(req, "\"", 1);
}

// ESP32's .rodata is flash-mapped and directly readable (unlike AVR),
// so this doesn't need PROGMEM/send_P -- a plain const char* is fine
// (same reasoning as WifiPortal's kSetupPageHtml).
//
// No server-side templating -- every field is populated client-side
// via fetch('/status') on load, and every edit is submitted via
// fetch('/command') using DirectorCommand's existing form-encoded
// vocabulary (type/intValue/stringKey/longValue). Nothing here needs
// a new server-side route beyond serving this page.
const char kSetupPageHtml[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Game Setup</title>
<style>
body{font-family:sans-serif;max-width:480px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
h1{font-size:20px}
h2{font-size:15px;color:#8ac;margin-top:24px;border-top:1px solid #333;padding-top:12px}
label{display:block;margin-top:10px;font-size:13px;color:#aaa}
select,input,button{width:100%;padding:10px;margin:4px 0;font-size:16px;box-sizing:border-box;border-radius:6px;border:1px solid #444;background:#222;color:#eee}
button{background:#2a8a4f;color:#fff;border:none;cursor:pointer;margin-top:8px}
.player{border:1px solid #333;border-radius:8px;padding:10px;margin-top:10px}
#status{margin-top:14px;font-weight:bold;min-height:1.2em}
</style></head><body>
<h1>PinballTimer Game Setup</h1>
<p><a href="/game-live">Live game</a> &nbsp; <a href="/machines">Machines</a> &nbsp; <a href="/">WiFi</a></p>

<h2>Game</h2>
<label>Mode</label>
<select id="mode"></select>
<label>Player count</label>
<select id="playerCount"><option>1</option><option>2</option><option>3</option><option>4</option></select>
<label>Seconds per turn</label>
<input id="secondsPerTurn" type="number" min="1" max="5999">
<label>Ball count (rounds)</label>
<input id="ballCount" type="number" min="1" max="5">
<label>Machine name</label>
<input id="machineName" placeholder="e.g. Attack From Mars">
<button onclick="saveGame()">Save Game Settings</button>

<h2>Players</h2>
<p style="font-size:12px;color:#888">Each button/color can only be used by one player -- don't assign two players to the same one.</p>
<div id="players"></div>

<div id="status"></div>

<script>
var BUTTON_COLORS = ['Red', 'Yellow', 'Green', 'Blue'];

function sendCommand(type, fields) {
  var body = 'type=' + encodeURIComponent(type);
  for (var k in fields) { body += '&' + k + '=' + encodeURIComponent(fields[k]); }
  return fetch('/command', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: body})
    .then(function(r){ return r.json(); });
}

function setStatusMsg(msg) {
  document.getElementById('status').textContent = msg;
}

function findPlayerDisplay(status, playerId) {
  return (status.displays || []).filter(function(d){ return d.playerId === playerId; })[0];
}

function renderPlayers(status) {
  var container = document.getElementById('players');
  container.innerHTML = '';

  for (var i = 0; i < 4; i++) {
    var d = findPlayerDisplay(status, i) || {};
    var div = document.createElement('div');
    div.className = 'player';

    var nameLabel = document.createElement('label');
    nameLabel.textContent = 'Player ' + (i + 1) + ' name';
    var nameInput = document.createElement('input');
    nameInput.id = 'pname' + i;
    nameInput.value = d.name || '';
    div.appendChild(nameLabel);
    div.appendChild(nameInput);

    var btnLabel = document.createElement('label');
    btnLabel.textContent = 'Button / color';
    var btnSelect = document.createElement('select');
    btnSelect.id = 'pbutton' + i;
    BUTTON_COLORS.forEach(function(c, idx){
      var opt = document.createElement('option');
      opt.value = idx;
      opt.textContent = c;
      btnSelect.appendChild(opt);
    });
    div.appendChild(btnLabel);
    div.appendChild(btnSelect);

    var saveBtn = document.createElement('button');
    saveBtn.textContent = 'Save Player ' + (i + 1);
    saveBtn.onclick = (function(idx){ return function(){ savePlayer(idx); }; })(i);
    div.appendChild(saveBtn);

    container.appendChild(div);
  }
}

function savePlayer(i) {
  var name = document.getElementById('pname' + i).value;
  var button = document.getElementById('pbutton' + i).value;
  Promise.all([
    sendCommand('SetPlayerName', {intValue: i, stringKey: name}),
    sendCommand('SetPlayerButton', {intValue: i, longValue: button})
  ]).then(function(){ setStatusMsg('Player ' + (i + 1) + ' saved.'); });
}

function saveGame() {
  var mode = document.getElementById('mode').value;
  var playerCount = document.getElementById('playerCount').value;
  var secondsPerTurn = document.getElementById('secondsPerTurn').value;
  var ballCount = document.getElementById('ballCount').value;
  var machineName = document.getElementById('machineName').value;

  Promise.all([
    sendCommand('SelectMode', {intValue: mode}),
    sendCommand('SetPlayerCount', {intValue: playerCount}),
    sendCommand('SetModeOption', {stringKey: 'secondsPerTurn', longValue: secondsPerTurn}),
    sendCommand('SetModeOption', {stringKey: 'ballCount', longValue: ballCount}),
    sendCommand('SetMachineName', {stringKey: machineName})
  ]).then(function(){ setStatusMsg('Game settings saved.'); });
}

function loadStatus() {
  fetch('/status').then(function(r){ return r.json(); }).then(function(status){
    var modeSelect = document.getElementById('mode');
    modeSelect.innerHTML = '';
    (status.availableModes || []).forEach(function(m){
      var opt = document.createElement('option');
      opt.value = m.id;
      opt.textContent = m.name;
      if (m.id === status.modeId) { opt.selected = true; }
      modeSelect.appendChild(opt);
    });

    document.getElementById('playerCount').value = status.playerCount || 4;
    document.getElementById('secondsPerTurn').value = status.secondsPerTurn || 180;
    document.getElementById('ballCount').value = status.ballCount || 3;
    document.getElementById('machineName').value = status.machineName || '';

    renderPlayers(status);
  });
}

loadStatus();
</script>
</body></html>
)HTML";

// Placeholder live view -- proves the /status data contract (names,
// colors, rounds remaining, timers, machine name, gameOver) works
// end-to-end. NOT the final visual design; that's pending a reference
// photo of the physical device to model it after (see
// DirectorDashboard.h). Polls /status once a second.
const char kLivePageHtml[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Game Status</title>
<style>
body{font-family:sans-serif;max-width:480px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
h1{font-size:20px}
#machine{font-size:14px;color:#aaa;margin-bottom:16px}
.player{border:1px solid #333;border-radius:8px;padding:10px;margin-top:10px}
.player.eliminated{opacity:0.5}
.player.finished{opacity:0.7}
.swatch{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:6px;vertical-align:middle}
.timer{font-size:28px;font-weight:bold}
#gameover{color:#e55;font-weight:bold;margin-top:16px;font-size:20px}
</style></head><body>
<h1>PinballTimer -- Live</h1>
<p><a href="/game-setup">Game setup</a> &nbsp; <a href="/machines">Machines</a> &nbsp; <a href="/">WiFi</a></p>
<div id="machine"></div>
<div id="players"></div>
<div id="gameover"></div>
<script>
// ColorId values, see SystemTypes.h -- Black=0,White=1,Red=2,Green=3,Blue=4,Yellow=5,...
var COLOR_HEX = {2:'#e55', 3:'#5c5', 4:'#55e', 5:'#ee5'};

// PlayerStatus values, see PlayerManager.h -- Inactive=0,Waiting=1,Active=2,Eliminated=3,Finished=4
var ELIMINATED = 3, FINISHED = 4;

function formatTime(totalSeconds) {
  var s = Math.abs(totalSeconds || 0);
  var minutes = Math.floor(s / 60);
  var seconds = s % 60;
  return (minutes < 10 ? '0' : '') + minutes + ':' + (seconds < 10 ? '0' : '') + seconds;
}

function render(status) {
  document.getElementById('machine').textContent =
    (status.machineName ? 'Machine: ' + status.machineName : 'No machine name set') +
    ' | Ball count: ' + status.ballCount;

  var container = document.getElementById('players');
  container.innerHTML = '';

  (status.displays || []).forEach(function(d){
    if (d.playerId === undefined) { return; }

    var div = document.createElement('div');
    div.className = 'player' +
      (d.playerStatus === ELIMINATED ? ' eliminated' : d.playerStatus === FINISHED ? ' finished' : '');

    var swatch = '<span class="swatch" style="background:' + (COLOR_HEX[d.color] || '#888') + '"></span>';
    div.innerHTML = swatch + '<b>' + (d.name || ('Player ' + d.playerNumber)) + '</b>' +
      '<div class="timer">' + formatTime(d.timerSeconds) + '</div>' +
      '<div>Rounds left: ' + d.roundsRemaining + '</div>';

    container.appendChild(div);
  });

  document.getElementById('gameover').textContent = status.gameOver ? 'GAME OVER' : '';
}

function poll() {
  fetch('/status').then(function(r){ return r.json(); }).then(render).finally(function(){
    setTimeout(poll, 1000);
  });
}

poll();
</script>
</body></html>
)HTML";

const char kMachinesPageHtml[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Machine Database</title>
<style>
body{font-family:sans-serif;max-width:720px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
a{color:#8ac} table{width:100%;border-collapse:collapse;margin-top:18px}
th,td{text-align:left;padding:9px;border-bottom:1px solid #333} th{color:#8ac}
#message{color:#aaa;margin-top:18px}
</style></head><body>
<h1>Machine Database</h1>
<p><a href="/game-live">Live game</a> &nbsp; <a href="/game-setup">Game setup</a> &nbsp; <a href="/">WiFi</a></p>
<table><thead><tr><th>ID</th><th>Name</th><th>Type</th><th>Balls</th><th>Play time</th></tr></thead>
<tbody id="machines"></tbody></table>
<div id="message">Loading...</div>
<script>
fetch('/api/machines').then(function(r){return r.json();}).then(function(data){
  var body=document.getElementById('machines');
  data.machines.forEach(function(m){
    var row=document.createElement('tr');
    [m.id,m.name,m.type,m.ballCount,m.hasPlayTime ? m.playTimeSeconds+' sec' : 'Default ('+m.resolvedPlayTimeSeconds+' sec)']
      .forEach(function(value){var cell=document.createElement('td');cell.textContent=value;row.appendChild(cell);});
    body.appendChild(row);
  });
  document.getElementById('message').textContent=data.count ? data.count+' machine(s), read only' : 'No machines';
}).catch(function(){document.getElementById('message').textContent='Unable to read machine database';});
</script></body></html>
)HTML";

} // namespace

void DirectorDashboard::begin(httpd_handle_t server, const MachineCatalog& machineCatalog)
{
    machineCatalog_ = &machineCatalog;
    httpd_uri_t setupUri = {};
    setupUri.uri = "/game-setup";
    setupUri.method = HTTP_GET;
    setupUri.handler = &handleSetupPageTrampoline;
    setupUri.user_ctx = this;
    httpd_register_uri_handler(server, &setupUri);

    httpd_uri_t liveUri = {};
    liveUri.uri = "/game-live";
    liveUri.method = HTTP_GET;
    liveUri.handler = &handleLivePageTrampoline;
    liveUri.user_ctx = this;
    httpd_register_uri_handler(server, &liveUri);

    httpd_uri_t machinesPageUri = {};
    machinesPageUri.uri = "/machines";
    machinesPageUri.method = HTTP_GET;
    machinesPageUri.handler = &handleMachinesPageTrampoline;
    machinesPageUri.user_ctx = this;
    httpd_register_uri_handler(server, &machinesPageUri);

    httpd_uri_t machinesApiUri = {};
    machinesApiUri.uri = "/api/machines";
    machinesApiUri.method = HTTP_GET;
    machinesApiUri.handler = &handleMachinesApiTrampoline;
    machinesApiUri.user_ctx = this;
    httpd_register_uri_handler(server, &machinesApiUri);
}

void DirectorDashboard::update()
{
    // esp_http_server runs its own FreeRTOS task internally -- nothing
    // to poll here, same as DirectorControl::update().
}

esp_err_t DirectorDashboard::handleSetupPage(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kSetupPageHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t DirectorDashboard::handleLivePage(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kLivePageHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t DirectorDashboard::handleMachinesPage(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kMachinesPageHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t DirectorDashboard::handleMachinesApi(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    char buffer[192];
    snprintf(buffer, sizeof(buffer), "{\"count\":%u,\"machines\":[",
        machineCatalog_ ? machineCatalog_->count() : 0);
    if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        return ESP_FAIL;
    }

    if (machineCatalog_) {
        for (uint16_t i = 0; i < machineCatalog_->count(); ++i) {
            const MachineRecord* machine = machineCatalog_->at(i);
            if (!machine) {
                continue;
            }
            snprintf(buffer, sizeof(buffer),
                "%s{\"id\":%lu,\"name\":",
                i == 0 ? "" : ",",
                static_cast<unsigned long>(machine->id));
            if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK ||
                sendJsonString(req, machine->name) != ESP_OK) {
                return ESP_FAIL;
            }
            snprintf(buffer, sizeof(buffer),
                ",\"type\":\"%s\",\"ballCount\":%u,\"hasPlayTime\":%s,"
                "\"playTimeSeconds\":%u,\"resolvedPlayTimeSeconds\":%u}",
                machineTypeName(machine->type),
                machine->ballCount,
                machine->hasPlayTime ? "true" : "false",
                machine->playTimeSeconds,
                machine->resolvedPlayTimeSeconds());
            if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    }

    if (httpd_resp_send_chunk(req, "]}", 2) != ESP_OK) {
        return ESP_FAIL;
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t DirectorDashboard::handleSetupPageTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleSetupPage(req);
}

esp_err_t DirectorDashboard::handleLivePageTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleLivePage(req);
}

esp_err_t DirectorDashboard::handleMachinesPageTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleMachinesPage(req);
}

esp_err_t DirectorDashboard::handleMachinesApiTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleMachinesApi(req);
}
