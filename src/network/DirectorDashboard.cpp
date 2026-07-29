#include "network/DirectorDashboard.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
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

bool parseMachineType(const char* value, MachineType& out)
{
    if (strcmp(value, "0") == 0 || strcmp(value, "EM") == 0) {
        out = MachineType::EM;
    } else if (strcmp(value, "1") == 0 || strcmp(value, "Solid State") == 0) {
        out = MachineType::SolidState;
    } else if (strcmp(value, "2") == 0 || strcmp(value, "DMD") == 0) {
        out = MachineType::DMD;
    } else if (strcmp(value, "3") == 0 || strcmp(value, "Modern") == 0) {
        out = MachineType::Modern;
    } else {
        return false;
    }
    return true;
}

void urlDecode(char* str)
{
    char* dst = str;
    const char* src = str;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            ++src;
        } else if (src[0] == '%' &&
                   isxdigit(static_cast<unsigned char>(src[1])) &&
                   isxdigit(static_cast<unsigned char>(src[2]))) {
            const char hex[3] = {src[1], src[2], '\0'};
            *dst++ = static_cast<char>(strtol(hex, nullptr, 16));
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

bool extractFormField(const char* body, const char* key, char* out, size_t outSize)
{
    if (httpd_query_key_value(body, key, out, outSize) != ESP_OK) {
        out[0] = '\0';
        return false;
    }
    urlDecode(out);
    return true;
}

bool receiveRequestBody(httpd_req_t* req, char* out, size_t outSize)
{
    if (req->content_len >= outSize) {
        return false;
    }
    size_t receivedTotal = 0;
    while (receivedTotal < req->content_len) {
        const int received = httpd_req_recv(
            req, out + receivedTotal, req->content_len - receivedTotal);
        if (received <= 0) {
            return false;
        }
        receivedTotal += static_cast<size_t>(received);
    }
    out[receivedTotal] = '\0';
    return true;
}

esp_err_t sendResult(httpd_req_t* req, bool ok, const char* result,
                     unsigned long id = 0, uint16_t count = 0)
{
    char response[160];
    snprintf(response, sizeof(response),
        "{\"ok\":%s,\"result\":\"%s\",\"id\":%lu,\"count\":%u}",
        ok ? "true" : "false", result, id, count);
    httpd_resp_set_status(req, ok ? "200 OK" : "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t sendCsvField(httpd_req_t* req, const char* value)
{
    if (httpd_resp_send_chunk(req, "\"", 1) != ESP_OK) {
        return ESP_FAIL;
    }
    const char* segment = value;
    for (const char* cursor = value; ; ++cursor) {
        if (*cursor != '"' && *cursor != '\0') {
            continue;
        }
        if (cursor > segment &&
            httpd_resp_send_chunk(req, segment, cursor - segment) != ESP_OK) {
            return ESP_FAIL;
        }
        if (*cursor == '\0') {
            break;
        }
        if (httpd_resp_send_chunk(req, "\"\"", 2) != ESP_OK) {
            return ESP_FAIL;
        }
        segment = cursor + 1;
    }
    return httpd_resp_send_chunk(req, "\"", 1);
}

struct CsvFields {
    char id[16] = {};
    char name[MachineRecord::NAME_CAPACITY] = {};
    char type[16] = {};
    char ballCount[8] = {};
    char playTime[16] = {};
    char hasPlayTime[8] = {};
};

bool readCsvField(char*& cursor, char* out, size_t outSize, char& delimiter)
{
    size_t used = 0;
    bool quoted = *cursor == '"';
    if (quoted) {
        ++cursor;
    }

    while (*cursor) {
        if (quoted) {
            if (*cursor == '"' && cursor[1] == '"') {
                if (used + 1 >= outSize) return false;
                out[used++] = '"';
                cursor += 2;
                continue;
            }
            if (*cursor == '"') {
                ++cursor;
                quoted = false;
                break;
            }
        } else if (*cursor == ',' || *cursor == '\r' || *cursor == '\n') {
            break;
        }
        if (used + 1 >= outSize) return false;
        out[used++] = *cursor++;
    }
    if (quoted) return false;
    out[used] = '\0';

    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    delimiter = *cursor;
    if (*cursor == ',') {
        ++cursor;
    } else {
        if (*cursor == '\r') ++cursor;
        if (*cursor == '\n') ++cursor;
    }
    return delimiter == ',' || delimiter == '\r' ||
           delimiter == '\n' || delimiter == '\0';
}

bool readCsvRow(char*& cursor, CsvFields& fields)
{
    char delimiter = '\0';
    char* outputs[] = {
        fields.id, fields.name, fields.type, fields.ballCount,
        fields.playTime, fields.hasPlayTime
    };
    const size_t sizes[] = {
        sizeof(fields.id), sizeof(fields.name), sizeof(fields.type),
        sizeof(fields.ballCount), sizeof(fields.playTime),
        sizeof(fields.hasPlayTime)
    };

    for (uint8_t i = 0; i < 6; ++i) {
        if (!readCsvField(cursor, outputs[i], sizes[i], delimiter)) {
            return false;
        }
        if (i < 5 && delimiter != ',') {
            return false;
        }
        if (i == 5 && delimiter == ',') {
            return false;
        }
    }
    return true;
}

bool csvFieldsToRecord(const CsvFields& fields, bool preserveId,
                       MachineRecord& record)
{
    char* end = nullptr;
    const unsigned long parsedId = strtoul(fields.id, &end, 10);
    if (preserveId && (*fields.id == '\0' || *end != '\0' || parsedId == 0 ||
        parsedId > UINT32_MAX)) {
        return false;
    }
    record.id = preserveId ? static_cast<MachineId>(parsedId) : 1;
    strncpy(record.name, fields.name, MachineRecord::NAME_CAPACITY - 1);
    record.name[MachineRecord::NAME_CAPACITY - 1] = '\0';
    if (!parseMachineType(fields.type, record.type)) {
        return false;
    }
    const unsigned long ballCount = strtoul(fields.ballCount, &end, 10);
    if (*fields.ballCount == '\0' || *end != '\0' ||
        ballCount > MachineRecord::MAX_BALL_COUNT) return false;
    record.ballCount = static_cast<uint8_t>(ballCount);
    const unsigned long playTime = strtoul(fields.playTime, &end, 10);
    if (*fields.playTime == '\0' || *end != '\0' ||
        playTime > MachineRecord::MAX_PLAY_TIME_SECONDS) return false;
    record.playTimeSeconds = static_cast<uint16_t>(playTime);
    if (strcmp(fields.hasPlayTime, "true") == 0 ||
        strcmp(fields.hasPlayTime, "1") == 0) {
        record.hasPlayTime = true;
    } else if (strcmp(fields.hasPlayTime, "false") == 0 ||
               strcmp(fields.hasPlayTime, "0") == 0) {
        record.hasPlayTime = false;
    } else {
        return false;
    }
    return MachineCatalog::isValid(record);
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
button.danger{background:#8b3030}
.row{display:flex;gap:8px}
.row button{width:auto;flex:1}
.player{border:1px solid #333;border-radius:8px;padding:10px;margin-top:10px}
#status{margin-top:14px;font-weight:bold;min-height:1.2em}
#gameState{font-size:13px;color:#aaa;min-height:1.2em}
</style></head><body>
<h1>PinballTimer Game Setup</h1>
<p><a href="/game-live">Live game</a> &nbsp; <a href="/machines">Machines</a> &nbsp; <a href="/">WiFi</a></p>

<h2>Players</h2>
<p style="font-size:12px;color:#888">Shared across every mode -- this same roster carries over no matter which mode you play next. Each button/color can only be used by one player -- don't assign two players to the same one.</p>
<div id="players"></div>

<h2>Mode Settings</h2>
<p style="font-size:12px;color:#888">Applies to the selected mode only.</p>
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
<button onclick="saveGame()">Save Mode Settings</button>

<h2>Game Control</h2>
<div id="gameState"></div>
<div class="row">
<button onclick="controlCommand('StartGame')">Start Game</button>
<button onclick="controlCommand('StartFirstTimer')">Start First Timer</button>
</div>
<div class="row">
<button onclick="controlCommand('Pause')">Pause</button>
<button onclick="controlCommand('Resume')">Resume</button>
</div>
<button class="danger" onclick="confirmReset()">Reset Round</button>

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
  ]).then(function(){ setStatusMsg('Mode settings saved.'); return loadStatus(); });
}

// Start Game / Start First Timer / Pause / Resume / Reset -- the same
// commands DirectorControl already executes for the on-device menu's
// equivalent actions (see RemoteCommand.h). Start Game readies the
// round (initializes the mode, mirrors the physical White Button's
// first press); Start First Timer is the separate step that actually
// starts the first player's clock, matching the on-device two-press
// split -- kept as its own button rather than folded into Start Game,
// since a director running the game entirely from this page (no
// physical presses at all) needs an explicit way to do both steps.
function controlCommand(type) {
  return sendCommand(type, {}).then(function(result){
    setStatusMsg(type + ': ' + result.result);
    return loadStatus();
  });
}

function confirmReset() {
  if (confirm('Reset the current round? Every player returns to their starting time and ball count.')) {
    controlCommand('Reset');
  }
}

function loadStatus() {
  return fetch('/status').then(function(r){ return r.json(); }).then(function(status){
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

    document.getElementById('gameState').textContent =
      'Mode: ' + (status.modeName || 'none') +
      ' | Game started: ' + (status.gameStarted ? 'yes' : 'no') +
      ' | First timer started: ' + (status.firstTimerStarted ? 'yes' : 'no') +
      ' | Paused: ' + (status.paused ? 'yes' : 'no');

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
#pausedBanner{color:#ee5;font-weight:bold;font-size:18px;margin-bottom:8px}
.row{display:flex;gap:8px;margin-bottom:16px}
.row button{flex:1;padding:10px;font-size:15px;border-radius:6px;border:1px solid #444;background:#2a8a4f;color:#fff;cursor:pointer}
.row button.danger{background:#8b3030}
.player{border:1px solid #333;border-radius:8px;padding:10px;margin-top:10px}
.player.eliminated{opacity:0.5}
.player.finished{opacity:0.7}
.swatch{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:6px;vertical-align:middle}
.timer{font-size:28px;font-weight:bold}
#gameover{color:#e55;font-weight:bold;margin-top:16px;font-size:20px}
</style></head><body>
<h1>PinballTimer -- Live</h1>
<p><a href="/game-setup">Game setup</a> &nbsp; <a href="/machines">Machines</a> &nbsp; <a href="/">WiFi</a></p>
<div id="pausedBanner"></div>
<div class="row">
<button onclick="control('Pause')">Pause</button>
<button onclick="control('Resume')">Resume</button>
<button class="danger" onclick="confirmReset()">Reset</button>
</div>
<div id="machine"></div>
<div id="players"></div>
<div id="gameover"></div>
<script>
// ColorId values, see SystemTypes.h -- Black=0,White=1,Red=2,Green=3,Blue=4,Yellow=5,...
var COLOR_HEX = {2:'#e55', 3:'#5c5', 4:'#55e', 5:'#ee5'};

// PlayerStatus values, see PlayerManager.h -- Inactive=0,Waiting=1,Active=2,Eliminated=3,Finished=4
var ELIMINATED = 3, FINISHED = 4;

function control(type) {
  return fetch('/command', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body: 'type=' + encodeURIComponent(type)});
}

function confirmReset() {
  if (confirm('Reset the current round? Every player returns to their starting time and ball count.')) {
    control('Reset');
  }
}

function formatTime(totalSeconds) {
  var s = Math.abs(totalSeconds || 0);
  var minutes = Math.floor(s / 60);
  var seconds = s % 60;
  return (minutes < 10 ? '0' : '') + minutes + ':' + (seconds < 10 ? '0' : '') + seconds;
}

function render(status) {
  document.getElementById('pausedBanner').textContent = status.paused ? 'PAUSED' : '';

  document.getElementById('machine').textContent =
    'Mode: ' + (status.modeName || 'none') +
    ' | ' + (status.machineName ? 'Machine: ' + status.machineName : 'No machine name set') +
    ' | Ball count: ' + status.ballCount;

  var container = document.getElementById('players');
  container.innerHTML = '';

  (status.displays || []).forEach(function(d){
    if (d.playerId === undefined) { return; }

    var div = document.createElement('div');
    div.className = 'player' +
      (d.playerStatus === ELIMINATED ? ' eliminated' : d.playerStatus === FINISHED ? ' finished' : '');

    // Built via createElement/textContent, not innerHTML -- d.name is a
    // director-settable player name reflected back to every /game-live
    // viewer, so it must never be interpreted as markup (fixed stored-XSS,
    // 2026-07-28; see CLAUDE.md).
    var swatchEl = document.createElement('span');
    swatchEl.className = 'swatch';
    swatchEl.style.background = COLOR_HEX[d.color] || '#888';
    div.appendChild(swatchEl);

    var nameEl = document.createElement('b');
    nameEl.textContent = d.name || ('Player ' + d.playerNumber);
    div.appendChild(nameEl);

    var timerEl = document.createElement('div');
    timerEl.className = 'timer';
    timerEl.textContent = formatTime(d.timerSeconds);
    div.appendChild(timerEl);

    var roundsEl = document.createElement('div');
    roundsEl.textContent = 'Rounds left: ' + d.roundsRemaining;
    div.appendChild(roundsEl);

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
label{display:block;margin-top:9px;color:#aaa;font-size:13px}
input,select,button{padding:9px;margin:3px;box-sizing:border-box;border-radius:5px;border:1px solid #444;background:#222;color:#eee}
button{background:#286b48;cursor:pointer}.danger{background:#8b3030}.selected{background:#263846}
.form{border:1px solid #333;border-radius:8px;padding:12px;margin-top:18px}
#message{color:#aaa;margin-top:18px}
</style></head><body>
<h1>Machine Database</h1>
<p><a href="/game-live">Live game</a> &nbsp; <a href="/game-setup">Game setup</a> &nbsp; <a href="/">WiFi</a></p>
<div class="form">
<input id="id" type="hidden">
<label>Name <input id="name" maxlength="32"></label>
<label>Type <select id="type"><option>EM</option><option>Solid State</option><option>DMD</option><option>Modern</option></select></label>
<label>Ball count <input id="balls" type="number" min="1" max="6" value="3"></label>
<label><input id="hasTime" type="checkbox" onchange="syncTime()"> Custom play time</label>
<label>Play time (seconds) <input id="playTime" type="number" min="1" max="3600" value="180" disabled></label>
<div><button onclick="saveNew()">Add New</button><button id="updateBtn" onclick="updateSelected()" disabled>Update Selected</button>
<button id="deleteBtn" class="danger" onclick="deleteSelected()" disabled>Delete Selected</button>
<button onclick="clearForm()">Clear</button></div>
</div>
<div class="form">
<a href="/api/machines.csv" download="pinballtimer-machines.csv">Download CSV Backup</a>
<label>CSV file <input id="csvFile" type="file" accept=".csv,text/csv"></label>
<button onclick="uploadCsv('add')">Upload: Add Records</button>
<button class="danger" onclick="uploadCsv('replace')">Upload: Replace Entire Database</button>
</div>
<table><thead><tr><th>ID</th><th>Name</th><th>Type</th><th>Balls</th><th>Play time</th></tr></thead>
<tbody id="machines"></tbody></table>
<div id="message">Loading...</div>
<script>
var records=[];
function message(text){document.getElementById('message').textContent=text;}
function syncTime(){document.getElementById('playTime').disabled=!document.getElementById('hasTime').checked;}
function clearForm(){
  document.getElementById('id').value='';document.getElementById('name').value='';
  document.getElementById('type').value='EM';document.getElementById('balls').value=3;
  document.getElementById('hasTime').checked=false;document.getElementById('playTime').value=180;syncTime();
  document.getElementById('updateBtn').disabled=true;document.getElementById('deleteBtn').disabled=true;
}
function selectRecord(m){
  document.getElementById('id').value=m.id;document.getElementById('name').value=m.name;
  document.getElementById('type').value=m.type;document.getElementById('balls').value=m.ballCount;
  document.getElementById('hasTime').checked=m.hasPlayTime;
  document.getElementById('playTime').value=m.playTimeSeconds;syncTime();
  document.getElementById('updateBtn').disabled=false;document.getElementById('deleteBtn').disabled=false;
}
function load(){
fetch('/api/machines').then(function(r){return r.json();}).then(function(data){
  records=data.machines||[];
  var body=document.getElementById('machines');
  body.innerHTML='';
  records.forEach(function(m){
    var row=document.createElement('tr');
    [m.id,m.name,m.type,m.ballCount,m.hasPlayTime ? m.playTimeSeconds+' sec' : 'Default ('+m.resolvedPlayTimeSeconds+' sec)']
      .forEach(function(value){var cell=document.createElement('td');cell.textContent=value;row.appendChild(cell);});
    row.onclick=function(){Array.prototype.forEach.call(body.children,function(r){r.className='';});row.className='selected';selectRecord(m);};
    body.appendChild(row);
  });
  message(data.count ? data.count+' machine(s)' : 'No machines');
}).catch(function(){message('Unable to read machine database');});
}
function fields(action){
  var p=new URLSearchParams();p.set('action',action);p.set('id',document.getElementById('id').value);
  p.set('name',document.getElementById('name').value);p.set('type',document.getElementById('type').value);
  p.set('ballCount',document.getElementById('balls').value);p.set('hasPlayTime',document.getElementById('hasTime').checked?'1':'0');
  p.set('playTimeSeconds',document.getElementById('playTime').value);return p.toString();
}
function mutate(action){
  return fetch('/api/machines',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fields(action)})
    .then(function(r){return r.json().then(function(j){if(!r.ok)throw new Error(j.result);return j;});})
    .then(function(j){message(j.result);clearForm();return load();}).catch(function(e){message('Error: '+e.message);});
}
function saveNew(){mutate('add');}
function updateSelected(){mutate('update');}
function deleteSelected(){if(confirm('Delete this machine?'))mutate('remove');}
function uploadCsv(mode){
  var file=document.getElementById('csvFile').files[0];if(!file){message('Choose a CSV file first');return;}
  if(mode==='replace'&&!confirm('Replace the entire machine database?'))return;
  file.text().then(function(csv){return fetch('/api/machines.csv?mode='+mode,{method:'POST',headers:{'Content-Type':'text/csv'},body:csv});})
    .then(function(r){return r.json().then(function(j){if(!r.ok)throw new Error(j.result);return j;});})
    .then(function(j){message(j.result+': '+j.count+' record(s)');clearForm();return load();})
    .catch(function(e){message('CSV error: '+e.message);});
}
clearForm();load();
</script></body></html>
)HTML";

} // namespace

void DirectorDashboard::begin(httpd_handle_t server, MachineDatabase& machineDatabase)
{
    machineDatabase_ = &machineDatabase;
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

    httpd_uri_t machinesMutationUri = {};
    machinesMutationUri.uri = "/api/machines";
    machinesMutationUri.method = HTTP_POST;
    machinesMutationUri.handler = &handleMachinesMutationTrampoline;
    machinesMutationUri.user_ctx = this;
    httpd_register_uri_handler(server, &machinesMutationUri);

    httpd_uri_t machinesCsvDownloadUri = {};
    machinesCsvDownloadUri.uri = "/api/machines.csv";
    machinesCsvDownloadUri.method = HTTP_GET;
    machinesCsvDownloadUri.handler = &handleMachinesCsvDownloadTrampoline;
    machinesCsvDownloadUri.user_ctx = this;
    httpd_register_uri_handler(server, &machinesCsvDownloadUri);

    httpd_uri_t machinesCsvUploadUri = {};
    machinesCsvUploadUri.uri = "/api/machines.csv";
    machinesCsvUploadUri.method = HTTP_POST;
    machinesCsvUploadUri.handler = &handleMachinesCsvUploadTrampoline;
    machinesCsvUploadUri.user_ctx = this;
    httpd_register_uri_handler(server, &machinesCsvUploadUri);
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
        machineDatabase_ ? machineDatabase_->catalog().count() : 0);
    if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        return ESP_FAIL;
    }

    if (machineDatabase_) {
        const MachineCatalog& catalog = machineDatabase_->catalog();
        for (uint16_t i = 0; i < catalog.count(); ++i) {
            const MachineRecord* machine = catalog.at(i);
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

esp_err_t DirectorDashboard::handleMachinesMutation(httpd_req_t* req)
{
    if (!machineDatabase_) {
        return sendResult(req, false, "database_unavailable");
    }

    char body[256];
    if (!receiveRequestBody(req, body, sizeof(body))) {
        return sendResult(req, false, "request_too_large");
    }

    char action[12], idText[16], name[100];
    char typeText[16], ballsText[8], timeText[16], hasTimeText[8];
    extractFormField(body, "action", action, sizeof(action));
    extractFormField(body, "id", idText, sizeof(idText));
    extractFormField(body, "name", name, sizeof(name));
    extractFormField(body, "type", typeText, sizeof(typeText));
    extractFormField(body, "ballCount", ballsText, sizeof(ballsText));
    extractFormField(body, "playTimeSeconds", timeText, sizeof(timeText));
    extractFormField(body, "hasPlayTime", hasTimeText, sizeof(hasTimeText));
    if (strlen(name) >= MachineRecord::NAME_CAPACITY) {
        return sendResult(req, false, "name_too_long");
    }

    char* idEnd = nullptr;
    const unsigned long id = strtoul(idText, &idEnd, 10);
    if (strcmp(action, "remove") == 0) {
        const bool validId = *idText != '\0' && *idEnd == '\0' && id > 0;
        const bool ok = validId && machineDatabase_->remove(static_cast<MachineId>(id));
        return sendResult(req, ok, ok ? "removed" : "invalid_or_missing_id", id);
    }

    MachineType type;
    if (!parseMachineType(typeText, type)) {
        return sendResult(req, false, "invalid_type");
    }
    char* end = nullptr;
    const unsigned long balls = strtoul(ballsText, &end, 10);
    if (*ballsText == '\0' || *end != '\0' ||
        balls < MachineRecord::MIN_BALL_COUNT ||
        balls > MachineRecord::MAX_BALL_COUNT) {
        return sendResult(req, false, "invalid_ball_count");
    }
    const unsigned long playTime = strtoul(timeText, &end, 10);
    const bool hasPlayTime = strcmp(hasTimeText, "1") == 0 ||
                             strcmp(hasTimeText, "true") == 0;
    if (strcmp(hasTimeText, "0") != 0 &&
        strcmp(hasTimeText, "false") != 0 && !hasPlayTime) {
        return sendResult(req, false, "invalid_has_play_time");
    }
    if (*timeText == '\0' || *end != '\0' ||
        playTime > MachineRecord::MAX_PLAY_TIME_SECONDS ||
        (hasPlayTime && playTime < MachineRecord::MIN_PLAY_TIME_SECONDS)) {
        return sendResult(req, false, "invalid_play_time");
    }

    if (strcmp(action, "add") == 0) {
        MachineId newId = 0;
        const bool ok = machineDatabase_->add(
            name, type, static_cast<uint8_t>(balls),
            static_cast<uint16_t>(playTime), hasPlayTime, newId);
        return sendResult(req, ok, ok ? "added" : "invalid_or_database_full", newId);
    }
    if (strcmp(action, "update") == 0) {
        const bool validId = *idText != '\0' && *idEnd == '\0' && id > 0;
        const bool ok = validId && machineDatabase_->update(
            static_cast<MachineId>(id), name, type,
            static_cast<uint8_t>(balls), static_cast<uint16_t>(playTime),
            hasPlayTime);
        return sendResult(req, ok, ok ? "updated" : "invalid_or_missing_id", id);
    }
    return sendResult(req, false, "invalid_action");
}

esp_err_t DirectorDashboard::handleMachinesCsvDownload(httpd_req_t* req)
{
    if (!machineDatabase_) {
        return sendResult(req, false, "database_unavailable");
    }
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition",
        "attachment; filename=\"pinballtimer-machines.csv\"");
    if (httpd_resp_send_chunk(req,
        "id,name,type,ballCount,playTimeSeconds,hasPlayTime\r\n",
        HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        return ESP_FAIL;
    }

    const MachineCatalog& catalog = machineDatabase_->catalog();
    char buffer[96];
    for (uint16_t i = 0; i < catalog.count(); ++i) {
        const MachineRecord* machine = catalog.at(i);
        if (!machine) continue;
        snprintf(buffer, sizeof(buffer), "%lu,",
            static_cast<unsigned long>(machine->id));
        if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK ||
            sendCsvField(req, machine->name) != ESP_OK) {
            return ESP_FAIL;
        }
        snprintf(buffer, sizeof(buffer), ",\"%s\",%u,%u,%s\r\n",
            machineTypeName(machine->type), machine->ballCount,
            machine->playTimeSeconds, machine->hasPlayTime ? "true" : "false");
        if (httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t DirectorDashboard::handleMachinesCsvUpload(httpd_req_t* req)
{
    static constexpr size_t MAX_CSV_BYTES = 16384;
    if (!machineDatabase_ || req->content_len == 0 ||
        req->content_len > MAX_CSV_BYTES) {
        return sendResult(req, false, "invalid_csv_size");
    }

    char mode[12] = {};
    char query[64] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "mode", mode, sizeof(mode)) != ESP_OK ||
        (strcmp(mode, "add") != 0 && strcmp(mode, "replace") != 0)) {
        return sendResult(req, false, "mode_must_be_add_or_replace");
    }

    char* csv = static_cast<char*>(malloc(req->content_len + 1));
    MachineRecord* records = static_cast<MachineRecord*>(
        malloc(sizeof(MachineRecord) * MachineCatalog::MAX_RECORDS));
    if (!csv || !records) {
        free(csv);
        free(records);
        return sendResult(req, false, "out_of_memory");
    }

    esp_err_t result = ESP_OK;
    uint16_t count = 0;
    if (!receiveRequestBody(req, csv, req->content_len + 1)) {
        result = sendResult(req, false, "incomplete_upload");
    } else {
        char* cursor = csv;
        if (req->content_len >= 3 &&
            static_cast<unsigned char>(cursor[0]) == 0xEF &&
            static_cast<unsigned char>(cursor[1]) == 0xBB &&
            static_cast<unsigned char>(cursor[2]) == 0xBF) {
            cursor += 3; // tolerate the UTF-8 BOM emitted by spreadsheet apps
        }
        CsvFields header;
        if (!readCsvRow(cursor, header) ||
            strcmp(header.id, "id") != 0 || strcmp(header.name, "name") != 0 ||
            strcmp(header.type, "type") != 0 ||
            strcmp(header.ballCount, "ballCount") != 0 ||
            strcmp(header.playTime, "playTimeSeconds") != 0 ||
            strcmp(header.hasPlayTime, "hasPlayTime") != 0) {
            result = sendResult(req, false, "invalid_csv_header");
        } else {
            const bool replace = strcmp(mode, "replace") == 0;
            bool valid = true;
            while (*cursor && valid) {
                if (*cursor == '\r' || *cursor == '\n') {
                    ++cursor;
                    continue;
                }
                if (count >= MachineCatalog::MAX_RECORDS) {
                    valid = false;
                    break;
                }
                CsvFields fields;
                valid = readCsvRow(cursor, fields) &&
                        csvFieldsToRecord(fields, replace, records[count]);
                if (valid) ++count;
            }

            if (!valid || (!replace &&
                machineDatabase_->catalog().count() + count >
                    MachineCatalog::MAX_RECORDS)) {
                result = sendResult(req, false, "invalid_csv_or_database_full");
            } else if (replace) {
                const bool ok = machineDatabase_->replaceAll(records, count);
                result = sendResult(req, ok, ok ? "database_replaced" :
                    "replace_failed", 0, count);
            } else {
                MachineId addedIds[MachineCatalog::MAX_RECORDS] = {};
                uint16_t added = 0;
                bool ok = true;
                for (; added < count; ++added) {
                    const MachineRecord& record = records[added];
                    if (!machineDatabase_->add(record.name, record.type,
                        record.ballCount, record.playTimeSeconds,
                        record.hasPlayTime, addedIds[added])) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) {
                    while (added > 0) {
                        machineDatabase_->remove(addedIds[--added]);
                    }
                }
                result = sendResult(req, ok, ok ? "records_added" :
                    "add_failed_and_rolled_back", 0, ok ? count : 0);
            }
        }
    }

    free(csv);
    free(records);
    return result;
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

esp_err_t DirectorDashboard::handleMachinesMutationTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleMachinesMutation(req);
}

esp_err_t DirectorDashboard::handleMachinesCsvDownloadTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleMachinesCsvDownload(req);
}

esp_err_t DirectorDashboard::handleMachinesCsvUploadTrampoline(httpd_req_t* req)
{
    return static_cast<DirectorDashboard*>(req->user_ctx)->handleMachinesCsvUpload(req);
}
