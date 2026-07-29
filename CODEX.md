# Codex Development Notes

This file records requirements and design decisions discussed with Codex. It is
kept separate from `CLAUDE.md` so changes and decisions originating in Codex
sessions are identifiable. Codex must ask for permission before creating,
editing, deleting, formatting, or otherwise changing project files.

## Supported Hardware Baseline

As of 2026-07-27, all new development targets REV2 hardware and later
revisions.

- REV2 pin assignments and MCP23017 I/O are authoritative for new work.
- New firmware behavior is verified primarily with the `app-rev2` PlatformIO
  environment.
- Codex does not need to preserve or test REV1 compatibility unless the user
  specifically requests it.
- Existing REV1 code and build environments are not removed merely because
  they are no longer the development baseline.

### REV2 TFT Observation

- On the physical REV2 unit, the bottom six pixel rows of the TFT are not
  visible. UI layout and clipping must treat those rows as unavailable unless
  later hardware testing establishes that this is an ST7789 offset/init issue
  that can be corrected safely.
- `TftDisplayManager::drawCenteredText()` now enforces a shared six-pixel
  bottom safe inset, moving any text whose glyph box would enter those rows
  upward. This applies automatically to status screens, ball screens, menus,
  and one-off centered text. `pio run -e app-rev2` passes with the change.

No Gauntlet firmware has been implemented yet. This document is the agreed
working specification and architecture direction.

## Gauntlet Mode

Gauntlet is a game mode for one to four players playing an ordered selection of
one to nine pinball machines.

Victory conditions, rankings, and pinball scores are determined and recorded
outside this timer. The timer manages player order, balls, time, machine
progression, and player eligibility.

### Terminology

- Player-facing UI uses **Machine** and **Machines** for the selected pinball
  machines.
- Pinball players sometimes use "game" and "machine" interchangeably. The
  machine-handoff prompt intentionally says **Game** as specified below.
- The physical `Action` button is the **White Button**. Existing internal names
  such as `ButtonId::Action` may remain, but all player-facing instructions and
  documentation must call it the White Button. If requirements say "Action" or
  "White," they refer to the same button.
- Titles and messages use consistent, pleasing title/message capitalization.
  Machine names on the active-game status screen remain all caps.

### Player Configuration

- One to four players may participate.
- Each player has a configurable name.
- Each player has one of the four physical colors: red, yellow, green, or blue.
- A player's color remains fixed throughout the Gauntlet.
- Players whose timers expire remain visible at `00:00` and are skipped.
- At a new machine, only players with time remaining participate.

### Machine Selection

- A Gauntlet contains one to nine selected machine instances.
- Machines are selected from a machine database.
- Machines are selected in play order during mode setup.
- A database machine may be selected more than once.
- Every selection is an independent instance. Overrides, skipping, removal, and
  progress affect only that selected instance, even when the same database
  machine appears elsewhere in the Gauntlet.
- Once the Gauntlet starts, its selected machines and order cannot be edited.
- Changing the selection or order requires starting the mode again and
  reconfiguring it.

### Machine Database Values

Each machine database record will provide at least:

- A stable machine identity.
- A machine name.
- A default ball count.
- A default time contribution.

The current default time is `3:00`. If a database entry has no default time,
`3:00` is used as the fallback.

The database design, source, editing workflow, and initial contents remain to be
discussed.

### Player Time Pool

Each player receives one time pool for the entire Gauntlet. It does not reset
between balls or machines.

The starting pool is the sum of the resolved database-default time for every
selected machine instance. For example, while all entries use `3:00`, selecting
four machine instances gives every player `12:00`.

Unused time carries between every ball and machine until either:

- The player expends all remaining time, or
- The player completes the final ball of the final machine.

The intended pacing is that experienced players will usually retain time until
the final ball of the final machine.

All clocks are stopped during player handoffs and machine-to-machine handoffs.
If every player's time expires at any point, the Gauntlet ends immediately.

The default time pool may be adjusted during configuration. The director may
also adjust time during play using the same general facility available to Mode
1. A director time override applies only to the relevant selected machine
instance or live state; it does not change the database record.

### Ball Counts

- Every player receives the same ball count for a selected machine instance.
- The initial count comes from that machine's database entry.
- The director may override the ball count using the same general capability as
  Mode 1.
- A ball-count override applies only to that selected machine instance. It does
  not change the database record or another selection of the same machine.

### Shared Round-Robin Play

Within a machine, Gauntlet progresses using the same physical play mechanism as
Mode 1 Round Robin. This behavior must be shared rather than copied so later
changes to common round-robin behavior affect both modes unless a Gauntlet rule
explicitly differs.

Shared behavior includes:

- The White Button starts play.
- Turns follow the fixed physical player-color order.
- The next player's colored button flashes during handoff.
- The player's first press starts their clock.
- The player's second press ends their turn/ball.
- The White Button pauses and resumes play.
- Timers stop during handoffs.
- Players who are finished for the current machine or whose time has expired are
  skipped.
- Timer expiration, turn handoff, button lights, TFT feedback, and buzzer
  behavior use the common round-robin mechanism.

Gauntlet-specific machine progression and presentation must remain outside the
shared turn engine.

### Active-Machine Status Screen

During active play, the status screen shows:

1. The machine name in all caps at the top.
2. The active or upcoming player's name below it.
3. The player's current ball number below the player name.

This is a Gauntlet-specific presentation override.

### Completing a Machine

The point at which Mode 1 would normally declare the game over because all
eligible players have completed their final ball instead completes the current
Gauntlet machine.

If at least one player has time remaining and another machine remains, Gauntlet
enters a paused machine-handoff state. Players with expired time do not
participate in the next machine.

If the completed machine was the final remaining machine, the Gauntlet ends
regardless of unused player time.

### Machine Handoff

The handoff screen communicates:

```text
Next Game Is
$MACHINENAME

Press White Button
to Start
```

The exact line wrapping may be adapted to the TFT. Pressing the White Button
starts the displayed machine.

If this is the last machine that can still be played, including the resolution
of previously skipped machines, the screen also says:

```text
Final Machine
```

During handoff, the encoder can navigate to actions and the encoder switch
selects the highlighted action.

### Skipping a Machine

The handoff menu can offer:

```text
Skip This Machine
```

- Skipping does not remove time from any player.
- Skipping does not add time when the machine is later played.
- Skipped machines are offered again only after all initially non-skipped
  machines have been played.
- Skipped machines are offered again in their original selection order.
- Only skipped machines may be played during this final return pass.
- The actual final machine cannot be skipped.

Whether a returned skipped machine may be skipped a second time has not yet
been explicitly decided.

### Removing a Machine

The handoff menu can also offer:

```text
Remove This Machine
```

Removal requires a confirmation warning that states:

- How much time will be removed from the Gauntlet.
- That the action may end the game for some players.

Example content, with final layout adapted to the TFT:

```text
Remove This Machine?

3:00 Will Be Removed
from the Gauntlet.

This May End the Game
for Some Players!
```

If confirmed:

- Only the currently displayed selected machine instance is removed.
- It cannot be played later in the current Gauntlet.
- Its resolved database-default time is subtracted from every player's current
  remaining pool.
- The amount subtracted is the database default (or the `3:00` fallback), even
  if the director previously applied a time override to that instance.
- Players reduced to no remaining time expire, display `00:00`, and are
  skipped.

The final machine cannot be skipped, but it can be removed using these same
rules.

### Director Recovery

If all player time expires, the Gauntlet is over. The director may still add
time or balls and restart the Gauntlet using the director controls. Exact
restart/resume state transitions will be specified with the director and
persistence design.

## Shared Architecture Direction

### Shared Round-Robin Engine

Gauntlet should not inherit directly from `Mode1RoundRobin`, and its turn logic
should not be a copied version of Mode 1.

The intended structure is:

```text
GameMode
|-- Mode1RoundRobin
|     `-- RoundRobinTurnEngine
`-- Mode2Gauntlet
      `-- RoundRobinTurnEngine
```

The shared engine owns common physical turn behavior and state, including:

- Active player/button.
- Whether the active player's timer is running.
- Manual pause state.
- Turn-order selection.
- Player-button press sequencing.
- Timer start/stop and zero detection.
- Player handoff.
- Shared light and buzzer transitions.
- Detection that all eligible players completed the current round-robin
  session.

The engine should report semantic outcomes such as:

- Ball started.
- Ball completed.
- Player timed out.
- Player completed the current machine/session.
- All eligible players completed the current machine/session.
- Pause state changed.

The owning mode decides what those outcomes mean.

For Mode 1, completing every player's balls ends the mode. For Gauntlet, it
completes the current machine and normally enters machine handoff.

Mode-specific concerns remain outside the engine:

- Status-screen layout.
- Mode configuration.
- Mode-level completion.
- Machine selection and progression.
- Persistence/checkpoint representation.
- Gauntlet skip/removal state.

The refactor must first preserve Mode 1's existing behavior without intentional
changes. Native tests should lock down the shared engine before Gauntlet relies
on it.

### Common Machine Catalog

The machine database is common infrastructure for all modes, not a component
owned by Gauntlet. It should be exposed through shared application architecture,
eventually including a `MachineCatalog&` in `GameModeContext`.

Modes may opt into machine data without forcing every mode to use it.
Mode 1 may later select a database machine and obtain default balls/time while
retaining its round-robin rules. Gauntlet consumes an ordered list of selected
machine instances. Future modes may interpret the same catalog differently.

The architecture distinguishes:

- **Catalog defaults:** reusable values stored in a machine record.
- **Selected-instance configuration:** the machine identity and overrides for
  one occurrence in a configured mode.
- **Live director adjustments:** changes to the current instance or player
  state during play.
- **Mode state:** progress and mode-specific interpretation of those instances.

A selected instance references a stable catalog machine ID but has its own
identity and state. Editing, skipping, removing, or overriding one instance
must not affect another occurrence of the same catalog machine.

Persistence should keep these concerns separate:

- Machine catalog storage.
- Configured selected-machine instances.
- Active mode checkpoint/progress.

## Deferred Design Work

The following topics remain intentionally open:

- Machine database storage medium, schema details, initial records, and editing
  workflow.
- Exact APIs and event types for `RoundRobinTurnEngine`.
- Migration of existing Mode 1 state and checkpoint data to the shared engine.
- Gauntlet checkpoint/resume behavior.
- Detailed director-menu and configuration-menu flows.
- Behavior when a returned skipped machine is skipped again.
- Exact director recovery semantics after a Gauntlet has ended.

## Approved Implementation Work

On 2026-07-27, the user approved an isolated implementation phase with these
boundaries:

- Implement a persistent common machine database.
- Implement the shared round-robin engine.
- Refactor Mode 1 to use the shared engine without intentional behavior
  changes.
- Implement Mode 2 Gauntlet as a standalone, unregistered mode.
- Add native-testable logic and tests.
- Do not register Gauntlet or integrate it with `App`, Boot Menu, Director
  Menu, network dashboards, or other main application flows.
- Do not update `CLAUDE.md` in this phase.

### Approved Machine Database Constraints

- Initial capacity: 100 records, with an architecture that can grow beyond 200.
- Machine IDs are automatically assigned stable numeric IDs.
- IDs are monotonic and are not reused after deletion.
- Records contain:
  - Machine ID.
  - Machine Name.
  - Machine Type: EM, Solid State, DMD, or Modern.
  - Ball Count.
  - Play Time.
- Machine names have an initial maximum length of 32 characters.
- Ball Count must be from 1 through 6.
- Play Time must be from 1 through 3600 seconds.
- A missing Play Time resolves to the `3:00` (180-second) fallback.
- Persistent storage uses an ESP32 NVS adapter, while catalog rules remain
  storage-independent for native testing and future storage replacement.
- Selected Gauntlet machine instances remain separate from catalog records.

### Implementation Log

- Work started by Codex on 2026-07-27.
- Added the storage-independent `MachineCatalog` and `MachineRecord` model with
  approved validation, stable monotonic IDs, 100-record capacity, and the
  missing-time fallback.
- Added `MachineDatabase`, an ESP32 `Preferences`/NVS adapter with schema
  versioning, independent record keys, and a persisted record index/next ID.
- Added `GauntletMachineInstance` and `GauntletSession`. Repeated selections are
  independent; the session calculates the player pool, progresses in configured
  order, returns skipped instances in original order, applies removals, and
  determines the actual final machine.
- Added `RoundRobinTurnEngine` for shared color-order start/advance,
  first/second player-press semantics, pause gating, and transition state.
- Refactored Mode 1 transition decisions to use the shared engine at round
  start, player presses, and player handoff while retaining its existing
  persistence and presentation state.
- Added the standalone `Mode2Gauntlet`. It configures selected machines
  programmatically, retains player timers between machines, skips expired
  players, runs per-machine ball counts, renders Gauntlet status/handoff/removal
  screens, returns skipped machines, and supports instance removal deductions.
- Gauntlet was intentionally not added to `ModeRegistry`; no `App`, menu,
  dashboard, network, or `CLAUDE.md` integration was made.
- Added native tests for the machine catalog, Gauntlet session, and shared
  round-robin engine. Verification completed:
  - `pio test -e native`: 37/37 tests passed.
  - `pio run -e app`: passed.
  - `pio run -e app-rev2`: passed.
- NVS persistence compiles in both firmware targets but has not been exercised
  against physical flash in this isolated phase.

## Approved Gauntlet Configuration Menu Work

On 2026-07-27, the user approved a second isolated implementation phase:

- Place Gauntlet gameplay, session, configuration, and menu code together under
  a Gauntlet mode feature directory.
- Keep gameplay and menu implementation in separate classes/files while the
  mode owns both.
- Add a Gauntlet configuration model with:
  - Number of Machines from 1 through 9.
  - One independent database assignment for every configured machine instance.
  - Number of Players from 1 through 4.
  - Configuration validation and identification of the first unassigned
    instance.
- Add the mode-owned menu in this order:
  1. Start Gauntlet.
  2. Number of Machines.
  3. Assign Machines.
  4. Number of Players.
  5. Player Setup.
  6. Back.
- Keep Start Gauntlet visible at the top. Selecting it while any configured
  machine instance is unassigned must show a warning and must not start.
- Allow the machine count to change during setup. Increasing it creates empty
  assignments; decreasing it removes instances from the end and requires
  confirmation when assigned instances would be discarded.
- Use the persistent common machine catalog as the assignment source.
- Keep Gauntlet unregistered and do not connect this menu to `BootMenu` yet.
- Verify native tests and the REV2 firmware target; REV1 verification is no
  longer required.

### Configuration Menu Implementation Log

- Work started by Codex on 2026-07-27.
- Moved Gauntlet gameplay/session code into `modes/gauntlet/` and added the
  mode-owned `GauntletConfig` and `GauntletConfigMenu` alongside it.
- `GauntletConfig` separately stores the required machine-instance count,
  database assignment for every instance, and player count. It validates all
  assignments before building a `GauntletSession`.
- Decreasing the machine count refuses to discard assigned trailing instances
  until explicitly confirmed.
- `GauntletConfigMenu` is renderer-independent and owns the required menu order,
  encoder navigation, database record selection, validation warning state,
  player-setup handoff, and start/back outcomes. This lets a future `BootMenu`
  integration render and route the mode-owned menu without knowing Gauntlet's
  configuration rules.
- `Mode2Gauntlet::configure()` consumes a validated configuration and catalog
  to create its independent selected-machine instances.
- Gauntlet remains unregistered and no `BootMenu` or `App` integration was
  added.
- Verification:
  - `pio test -e native`: 43/43 tests passed.
  - `pio run -e app-rev2`: passed.

## Approved Round Robin Configuration Menu Work

On 2026-07-27, the user approved restructuring Mode 1 to match Gauntlet's
mode-owned configuration pattern:

- Group Mode 1 gameplay, configuration, and menu code under a
  `modes/round_robin/` feature directory.
- Add `RoundRobinConfig` with:
  - Number of Players from 1 through 4.
  - Optional machine selection from the common database. Mode 1 does **not**
    require a machine selection to start.
  - Game Time from 1 through 5999 seconds, default 3:00.
  - Ball Count from 1 through 5, default 3.
- Add the mode-owned menu in this order:
  1. Start Round Robin.
  2. Number of Players.
  3. Select Machine.
  4. Game Time.
  5. Ball Count.
  6. Player Setup.
  7. Back.
- Keep the existing `BootMenu` path operational until all mode-owned menus are
  integrated together.
- Preserve Mode 1 runtime behavior.
- Add native tests and verify the REV2 firmware target.

### Round Robin Configuration Implementation Log

- Work started by Codex on 2026-07-27.
- Moved Mode 1 gameplay into `modes/round_robin/` and added the mode-owned
  `RoundRobinConfig` and `RoundRobinConfigMenu` alongside it.
- `RoundRobinConfig` owns player count, optional stable machine ID, game time,
  and ball count. No machine selection is valid and does not block starting.
- `RoundRobinConfigMenu` owns the approved item order, encoder navigation,
  optional database selection (including an explicit None choice), numeric
  editors, player-setup handoff, validation state, and start/back outcomes.
- Added `Mode1RoundRobin::configure()` to consume a validated configuration
  without changing the existing runtime setup path.
- Updated source includes for the new feature directory while keeping the
  current `BootMenu` flow operational pending unified mode-menu integration.
- Verification:
  - `pio test -e native`: 49/49 tests passed.
  - `pio run -e app-rev2`: passed.

## Approved Mode-Owned Menu Integration

On 2026-07-27, the user approved integrating the mode-owned configuration
menus into the real boot flow and registering Gauntlet:

- Add a generic mode-menu contract to `GameMode`.
- Make `BootMenu` delegate configuration input, rendering, validation, and
  application to the selected mode.
- Preserve shared Player Setup and return to the active mode menu afterward.
- Register `Mode2Gauntlet` in `ModeRegistry`.
- Block Gauntlet start until all configured instances are assigned.
- Continue allowing Round Robin to start without a selected machine.
- Copy a selected catalog machine name into the existing session machine label.
- Remove reliance on `BootMenu`'s hardcoded Mode 1 configuration path.
- Verify native tests and the REV2 firmware build; upload remains a separate
  user-requested action.

### Mode-Owned Menu Integration Log

- Work started by Codex on 2026-07-27.
- Added generic mode configuration hooks/outcomes to `GameMode`; `BootMenu`
  delegates menu opening, encoder handling, rendering, validation/application,
  configured player count, and optional machine identity to the active mode.
- Added firmware-only renderer companion files for both mode menus, keeping
  their state/controllers free of Adafruit dependencies and native-testable.
- Shared Player Setup can now be entered from a mode menu and returns to that
  mode menu on long-press back.
- Registered `Mode2Gauntlet` after Mode 1 in `ModeRegistry`; it now appears in
  Select Game Mode.
- Starting a mode applies its owned configuration, sets the mode manager's
  player count, initializes the mode, persists the last selected mode, and
  mirrors an optional selected machine name into the existing game-session
  label.
- Gauntlet validation still blocks start until every configured machine
  instance is assigned. Round Robin still starts with no machine selected.
- The legacy hardcoded `BootMenu` ModeMenu/config states remain compiled for
  compatibility but are no longer entered from Select Game Mode; the live path
  is `State::ModeOwnedConfig`.
- Verification:
  - `pio test -e native`: 49/49 tests passed.
  - `pio run -e app-rev2`: passed.

## Approved Scrolling Menus and Initial Catalog

On 2026-07-27, the user approved:

- Render mode-owned root menus and machine-selection screens as scrolling
  lists, not one selected item at a time.
- Keep the selected row visible and visibly marked.
- Retain focused single-value screens for numeric editing.
- Preserve the REV2 six-pixel TFT bottom safe area.
- Seed the persistent machine database only when it is completely empty with:
  - Stars — Solid State — 3 balls — 3:00.
  - Meteor — Solid State — 3 balls — 3:00.
  - Mars Trek — EM — 5 balls — 4:30.
  - Scared Stiff — DMD — 3 balls — 4:00.
- The user confirmed `Stars` as the intended spelling for the initial record
  originally typed as `Starts`.

### Scrolling Menu and Catalog Implementation Log

- Work started by Codex on 2026-07-27.
- Round Robin and Gauntlet root menus now render up to five rows at once,
  marking the current row and scrolling the window as the selection moves.
- Gauntlet machine-instance assignment and both modes' catalog pickers use the
  same scrolling-list behavior. Round Robin retains its explicit None entry.
- Numeric value editors and warnings remain focused screens.
- A completely empty persistent machine database is seeded with Stars, Meteor,
  Mars Trek, and Scared Stiff using the approved types, ball counts, and play
  times. Existing non-empty databases are never seeded or altered.
- Verification:
  - `pio test -e native`: 49/49 tests passed.
  - `pio run -e app-rev2`: passed (811,905 bytes flash; 54,636 bytes RAM).

## Approved Gauntlet Pause Correction

On 2026-07-27, the user approved correcting Gauntlet's White Button
pause/resume behavior to match Round Robin:

- Starting a machine on White Button press must suppress that same physical
  press's release so the machine is not immediately paused.
- A later White Button tap pauses or resumes the active turn.
- Pausing stops the active clock, turns off the active player's light, flashes
  the White Button, and displays the paused status.
- Resuming restores the appropriate active-player light and clock state.
- Add regression coverage and verify native tests and the REV2 build.

## WiFi Stability, Automatic Provisioning, and Database Read API

On 2026-07-28, Codex replaced the disabled/unstable WiFi lifecycle while
preserving the existing director interface, game setup, live status, captive
portal, and OTA capabilities:

- Re-enabled the WiFi feature behind `kWifiFeatureEnabled`.
- Removed all runtime use of `WIFI_AP_STA`, which was implicated by the REV2
  WiFi-task heap-corruption coredump. The radio now operates exclusively as
  either a station or an access point.
- Migrates the legacy persisted `Both` operating mode to `StationOnly`.
- Ensures a station or AP network interface exists before starting the shared
  HTTP server, preserving the earlier lwIP `Invalid mbox` boot-crash fix.
- Disabled SDK auto-reconnect and SDK credential persistence. Application
  reconnect attempts now back off from 5 seconds to a maximum of 60 seconds.
- Defers ArduinoOTA/mDNS initialization until station mode has obtained an IP
  address and services OTA only while the station remains connected.
- With no saved credentials, automatically starts a background fallback AP.
  With saved credentials, station mode is attempted first; after 30 seconds
  disconnected, the same fallback AP starts automatically.
- The fallback AP does not take TFT/input ownership or pause a running game.
- Each device advertises `PinballTimerXXXX`, where `XXXX` is the final four
  uppercase hexadecimal digits of its WiFi station MAC address.
- The web provisioning flow saves submitted credentials, acknowledges the
  browser for four seconds, stops the AP, and then attempts station mode. It
  never tests credentials using simultaneous AP+STA operation.
- The AP uses the fixed address `10.10.10.1`. AP and LAN operation share one
  `esp_http_server` instance and the same application routes.
- Added navigation among WiFi setup, game setup, live game status, and the
  machine database.
- Added read-only machine database access:
  - `GET /machines` renders a browser table.
  - `GET /api/machines` streams JSON without allocating a catalog-sized
    response buffer.
  - Records include stable ID, name, type, ball count, configured play-time
    fields, and resolved play time.
  - No database mutation route was added.
  - This initial read-only scope was later expanded by "Web Machine Database
    CRUD and CSV Backup" below.
- Updated `CLAUDE.md` with the replacement lifecycle and endpoint summary.
- Added a cross-reference from that `CLAUDE.md` summary back to this detailed
  implementation and verification log.
- Preserved unrelated pre-existing working-tree changes.
- Verification:
  - `pio test -e native`: 50/50 tests passed.
  - `pio run -e app-rev2`: passed after the final database connector changes
    (904,953 bytes flash; 57,988 bytes RAM).
  - `git diff --check`: passed.
- The firmware was built but not flashed; sustained on-device WiFi testing is
  still required before the reset issue can be considered conclusively closed.

### 2026-07-28 REV2 Deployment

- Committed the WiFi/database work and the existing Gauntlet pause correction
  as commit `7a7e72c` and pushed it to `origin/main`.
- Re-ran verification immediately before deployment:
  - `pio test -e native`: 50/50 tests passed.
  - `pio run -e app-rev2`: passed (904,953 bytes flash; 57,988 bytes RAM).
- Uploaded the `app-rev2` firmware from commit `7a7e72c` over USB to the
  ESP32-S3 detected on `COM13`.
- The connected board reported MAC `c0:4e:30:06:f9:28`; its automatic fallback
  SSID is therefore `PinballTimerF928`.
- Esptool verified the hash of every written image region and completed with a
  hard reset via RTS. The upload command reported success.
- Upload verification confirms the image was written correctly. Runtime WiFi
  stability and fallback behavior still require observation after boot.

## WiFi Setup Usability and Connection Recovery

On 2026-07-28, Codex implemented the requested follow-up WiFi behavior:

- The on-device WiFi submenu always reserves two lines for connection status:
  the connected SSID and assigned station IP address, or explicit
  not-connected placeholders.
- A successful encoder-driven connection screen now says `Connected to
  <SSID>` and displays the assigned IP address.
- Long-press navigation now unwinds one level:
  - Password/manual text entry returns to network selection.
  - Network selection exits to the Boot Menu WiFi submenu.
  - A Director Menu WiFi handoff returns to the Director Menu instead of
    resuming directly into the game.
- Added `Forget WiFi Network` to the Boot Menu WiFi submenu. It clears saved
  SSID/password data, disconnects the station, and immediately starts the
  device's fallback AP.
- Added `Keep WiFi Alive: ON/OFF` control to disable ESP32 modem sleep while
  the HTTP interface must remain responsive in application standby. Its
  original persisted/default-ON behavior was superseded by the radio-power
  policy below.
- Added a `Show password` checkbox to the web WiFi form.
- Corrected SSID storage to support the full IEEE 802.11 32-byte SSID rather
  than truncating it to 31 bytes; password storage retains up to 64 bytes.
- A new connection attempt explicitly disconnects the prior station session
  before `WiFi.begin()`.
- The encoder connection screen now drives `NetworkManager::update()` itself,
  removing dependence on later `App::update()` ordering.
- Failed candidate credentials are no longer left persisted. The previous
  credentials are restored, the previous station connection is restarted, and
  the failure screen includes the numeric SDK `wl_status_t` value to aid
  diagnosis instead of remaining indefinitely on `CONNECTING`.
- Verification after implementation:
  - `pio test -e native`: 50/50 tests passed.
  - `pio run -e app-rev2`: passed after the final parent-menu restoration
    change (907,205 bytes flash; 58,092 bytes RAM).

## WiFi Radio Power Policy

On 2026-07-28, the user clarified that most timer use does not require WiFi
and requested true radio-off behavior:

- Every physical boot now starts with WiFi powered OFF. Saved network
  credentials remain in NVS, but WiFi is not automatically started.
- Boot Menu's WiFi submenu includes an explicit `Turn WiFi ON` /
  `Turn WiFi OFF` action.
- Network interfaces and the shared HTTP/portal/dashboard services are
  initialized lazily on the first explicit enable, preserving the required
  netif-before-HTTP startup order.
- When enabled:
  - Saved credentials select exclusive station mode.
  - No credentials select the exclusive `PinballTimerXXXX` fallback AP.
  - The existing 30-second loss-of-network fallback remains active only while
    WiFi power is enabled.
- Turning WiFi off stops captive-portal DNS, closes interactive/persistent AP
  state, disconnects AP and station interfaces, and calls
  `WiFi.mode(WIFI_OFF)` to disable the RF modem. This is distinct from modem
  sleep or application display standby. The pinned Arduino-ESP32 core's
  `WiFiGenericClass::mode(WIFI_MODE_NULL)` path calls `esp_wifi_stop()` and
  then `esp_wifi_deinit()`, so this also deinitializes the WiFi driver.
- `Keep WiFi Alive` is now runtime-only and defaults OFF on every physical
  boot. Older persisted `wifiKeep` values are removed during settings load.
  Toggling it controls modem sleep only for the current boot.
- WiFi setup/join/adhoc actions do not implicitly power on a disabled radio;
  the explicit power action must be selected first.
- Verification:
  - `pio run -e app-rev2`: passed (907,973 bytes flash; 58,100 bytes RAM).

### 2026-07-28 Radio-Power Firmware Deployment

- Committed the WiFi setup usability, connection recovery, and explicit
  radio-power policy as commit `fbd714a`; pushed it to `origin/main`.
- Pre-deployment verification:
  - `pio test -e native`: 50/50 tests passed.
  - `pio run -e app-rev2`: passed (907,973 bytes flash; 58,100 bytes RAM).
- Uploaded commit `fbd714a`'s `app-rev2` firmware over USB to the ESP32-S3 on
  `COM13`, MAC `c0:4e:30:06:f9:28`.
- Esptool verified every written image-region hash and hard-reset the board via
  RTS; PlatformIO reported upload success.
- On this and every subsequent fresh boot, the expected state is WiFi OFF and
  `Keep WiFi Alive` OFF. Saved SSID/password data remains available for an
  explicit `Turn WiFi ON`.

## PowerShell WiFi Connector

On 2026-07-28, Codex added `tools/Connect-PinballTimer.ps1` for Windows-side
WiFi and HTTP diagnostics:

- Discovers a single visible `PinballTimerXXXX` SSID automatically or accepts
  `-Ssid` when multiple timers are present.
- Accepts a `SecureString` password, prompts securely, or reads
  `WIFI_PORTAL_PASSWORD` internally from a supplied gitignored `Secrets.h`
  without printing it.
- Creates a separately named `PinballTimerConnector-<SSID>` Windows WLAN
  profile, preserving pre-existing/user/group-policy profiles with the same
  SSID name.
- Joins the timer AP, waits up to a configurable timeout, and verifies both
  `GET /status` and `GET /api/machines`.
- Returns a structured PowerShell object containing connection state, base
  URI, device/mode, machine count, and live/setup/database page URLs.
- Supports `-NoJoin -Address <hostname-or-IP>` for a timer already reachable
  on the LAN, `-InterfaceName` for multi-adapter systems, and
  `-ForgetProfile` for connector-profile cleanup.
- The temporary plaintext WLAN XML is deleted in a `finally` block and its
  transient plaintext password variable is cleared after profile import.
- Live verification against the connected REV2 board:
  - Discovered `PinballTimerF928` at 75% signal.
  - Joined it successfully using the project portal password.
  - Reached `http://10.10.10.1/status`.
  - Read four records from `http://10.10.10.1/api/machines`.
  - A later `-NoJoin` verification could not reach the timer. Windows reported
    its WiFi adapter disconnected, and two subsequent scans no longer showed
    `PinballTimerF928`. With the current boot-off policy this is consistent
    with, but does not by itself prove, a timer reset followed by a fresh boot
    with WiFi OFF. Another run with serial/coredump capture is required to
    determine why the AP disappeared.

## Web Machine Database CRUD and CSV Backup

On 2026-07-28, Codex expanded the shared `/machines` interface from read-only
access to persistent database management:

- `/machines` now provides a browser form and selectable table for adding,
  modifying, and deleting machine records.
- `POST /api/machines` accepts form-encoded `add`, `update`, and `remove`
  actions. It uses `MachineDatabase` rather than mutating `MachineCatalog`
  directly, so the existing field validation and per-record NVS persistence
  remain authoritative.
- `GET /api/machines.csv` streams the complete catalog as a downloadable CSV
  backup with the header:
  `id,name,type,ballCount,playTimeSeconds,hasPlayTime`.
- `POST /api/machines.csv?mode=add` validates the complete upload, checks
  catalog capacity, ignores source IDs, and assigns new monotonic IDs. If an
  NVS add fails, records added by that request are removed.
- `POST /api/machines.csv?mode=replace` validates all records and unique,
  nonzero IDs before touching NVS, then rebuilds the catalog while preserving
  backup IDs. `MachineDatabase::replaceAll()` makes a RAM backup and attempts
  to restore it if persistence fails.
- CSV names are quoted and embedded quotes are doubled on export. Import
  supports quoted fields, doubled quotes, CRLF/LF, and an optional UTF-8 BOM.
- Uploads are limited to 16 KiB and 100 records. The CSV body and record array
  use temporary heap allocations rather than large HTTP-task stack buffers.
- Browser import has explicit `Add Records` and confirmation-gated
  `Replace Entire Database` actions.
- The shared HTTP server now uses 14 of its configured 16 URI-handler slots.
- Verification:
  - `pio test -e native`: 50/50 tests passed.
  - `pio run -e app-rev2`: passed after final input hardening
    (918,709 bytes flash; 58,100 bytes RAM).

## Gauntlet Player's Choice and Random Machine Selection

On 2026-07-28, Codex expanded each Gauntlet machine assignment beyond
database-record selection:

- `Player's Choice 3B 3:00` is always the first machine-selection option. It
  creates a placeholder session instance with three balls and a fixed
  three-minute contribution to every player's starting time pool, allowing
  the actual pinball machine to be chosen outside the timer.
- `Random Choice` is the second option. Selecting it opens an
  `EM` / `Solid State` / `DMD` / `Modern` / `Any` category menu.
- Gauntlet configuration now stores the assignment kind and random category
  separately from database machine IDs. Existing explicit machine assignments
  remain unchanged.
- Random choices validate that at least one matching database record exists.
  An empty category produces a specific setup warning rather than starting an
  invalid session.
- Random choices are resolved into real catalog records when the Gauntlet
  session is built. Firmware supplies `esp_random()` entropy; a small
  deterministic seed path keeps the pure configuration logic unit-testable.
  The resolved first record also becomes the current game-storage machine name.
- Slot summaries display `Player's Choice` or `Random: <category>` so the
  special assignment remains visible before the game starts.
- `GauntletSession::addCustomMachine()` supports the Player's Choice
  placeholder without inventing a database ID; its session machine ID is zero.
- Verification:
  - `pio test -e native`: 55/55 tests passed, including Player's Choice
    parameters, random-category filtering, empty-category validation, option
    order, and category-menu selection.
  - `pio run -e app-rev2`: passed (920,237 bytes flash; 58,172 bytes RAM).

### 2026-07-28 Gauntlet/Database Firmware Deployment

- Uploaded the current uncommitted `app-rev2` firmware over USB to the
  ESP32-S3 detected on `COM13`, MAC `c0:4e:30:06:f9:28`.
- The image includes the Gauntlet Player's Choice/random-selection work and
  the pending web machine-database CRUD/CSV interface.
- PlatformIO reported 920,237 bytes of flash and 58,172 bytes of RAM used.
- Esptool verified the hash of every written image region and hard-reset the
  board via RTS. PlatformIO reported upload success.

## Gauntlet Large Ball Status

On 2026-07-28, Codex changed Gauntlet's active-play status screen to use the
same dedicated large-ball layout as Round Robin:

- The active player's name remains at the top in that player's assigned color.
- The `Ball` label appears below the name.
- The current ball number uses the shared 16x text size and fills most of the
  remaining TFT area.
- Gauntlet's machine name remains visible on the between-machine handoff
  screen before play begins.
- Verification and deployment:
  - `pio run -e app-rev2`: passed (920,697 bytes flash; 58,172 bytes RAM).
  - Uploaded over USB to the REV2 board on `COM13`, MAC
    `c0:4e:30:06:f9:28`.
  - Esptool verified every written image-region hash and hard-reset the board;
    PlatformIO reported upload success.

## Gauntlet Timer MM:SS Formatting

On 2026-07-28, Codex corrected Gauntlet's four-digit player timers:

- Every active Gauntlet timer display now explicitly uses
  `DisplayFormat::TimeMinutesSeconds` with its colon enabled.
- Values remain stored and updated as total seconds internally; only rendering
  changes. For example, 180 seconds is displayed as `03:00`.
- This matches Round Robin's existing timer-display setup and prevents
  Gauntlet from inheriting the numeric manager's raw-seconds default.
- Verification and deployment:
  - `pio run -e app-rev2 -t upload`: build and upload passed (920,721 bytes
    flash; 58,172 bytes RAM).
  - Uploaded to the REV2 board on `COM13`, MAC `c0:4e:30:06:f9:28`.
  - Esptool verified every written image-region hash and hard-reset the board.

## Temporary Pre-Alpha Always-On WiFi Policy

On 2026-07-28, the user confirmed that the development timer remains attached
to a bench power supply and requested continuous WiFi through the alpha
milestone:

- Added `kPreAlphaWifiAlwaysOn = true` in `FeatureFlags.h` as an explicit,
  temporary release-stage switch.
- While enabled, every physical boot automatically starts WiFi using the saved
  station network or the normal `PinballTimerXXXX` fallback AP.
- Modem sleep is disabled for the entire boot, including application standby,
  so the HTTP/debug interface remains responsive.
- Attempts to use the Boot Menu's WiFi power or keep-alive toggles cannot turn
  the radio or keep-alive policy off. The menu labels them
  `DEV: WiFi Forced ON` and `DEV: Keep Alive ON`.
- Serial prints an immediate boot warning and repeats a
  `[PRE-ALPHA REMINDER]` every 15 minutes.
- At the alpha milestone, set `kPreAlphaWifiAlwaysOn` to `false` to restore
  the normal production policy: WiFi off at boot, explicit power control, and
  runtime-only keep-alive.
- Verification and deployment:
  - `pio run -e app-rev2`: passed (923,809 bytes flash; 58,172 bytes RAM).
  - Uploaded over USB to the REV2 board on `COM13`, MAC
    `c0:4e:30:06:f9:28`; esptool verified every image-region hash and
    hard-reset the board.
  - A brief serial attachment occurred after the boot warning had already
    printed, so it captured no matching reminder line.
  - A direct `pinballtimer.local` station-mode probe timed out; the subsequent
    Windows scan found the live fallback AP `PinballTimerF928`.
  - `tools/Connect-PinballTimer.ps1` joined that AP and successfully read
    `http://10.10.10.1/status` plus four machine records from the database API,
    confirming the radio and HTTP interface remained active after reboot.

## Firmware Flash Approval Rule

On 2026-07-28, the user established a standing workflow rule:

- Codex must ask the user before every firmware flash/upload.
- A request to edit, build, test, commit, or push does not itself authorize a
  flash.
- Codex may proceed only after the user explicitly approves that specific
  flash.

### 2026-07-28 Combined Feature Commit and Push

- Committed the PowerShell WiFi connector, web database CRUD/CSV backup,
  Gauntlet Player's Choice/random selection and display improvements, and the
  temporary pre-alpha always-on WiFi policy as commit `dfbdfed`.
- Pushed `dfbdfed` to `origin/main`.
- No firmware flash was performed as part of the commit/push request.
