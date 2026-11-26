# Daeyoung Ground Control (KEVA GCS) – build notes

## Editions

One source tree builds two products. Choose the edition with the CMake cache
variable `KEVA_EDITION` (Qt Creator: Projects → Build → CMake → Current
Configuration, or `-DKEVA_EDITION=COM` on the command line).

| | MIL (default) | COM |
|---|---|---|
| Product name | Daeyoung Ground Control | Daeyoung Ground Control CE |
| Executable / settings name (`QGC_APP_NAME`) | `DaeyoungGCS` | `DaeyoungGCS_CE` |
| Login, account policy, audit log | compiled in | **not compiled** |
| Compile definition | `KEVA_EDITION_MIL` | `KEVA_EDITION_COM` |

Use a separate build folder per edition. The security module is never added
to a COM build, even if the folder was configured as MIL before.

The product version comes from `KEVA_PRODUCT_VERSION` (default `1.0.1`) in
`custom/cmake/CustomOverrides.cmake`, not from the upstream QGC git tags.

## Look and feel

* Palette: `custom/src/KevaTheme.h` (UI deck slide 1) mapped onto QGC's palette in
  `CustomPlugin::paletteOverride()`. Same colours in both QGC themes; the theme
  setting is hidden. QML can use the brand colours through
  `QGroundControl.corePlugin.kevaColors` (e.g. `kevaColors.neonGreen`).
* Font: Arial for the whole application (`KEVA_FONT_FAMILY` compile definition).
* Language: English whatever the PC language (`KEVA_FORCE_ENGLISH`, hook in
  `QGCApplication::setLanguage()`); the language setting is hidden.
* Fly view tool strip: Takeoff, Return, Pause, Resume Mission (Pause appears only
  while flying, as in QGC) (`custom/src/FlyViewToolStripActionList.qml`).
* Map: Google Hybrid by default (can be changed in Settings > Maps, where offline
  map download is also available); vehicle marker is a Neon Green heading arrow
  (`custom/res/Images/KevaVehicleArrow.svg`, `CustomFirmwarePlugin::vehicleImageOpaque`).
  The ground station position is drawn with the company logo.
* Fly view bottom strip (UI deck slide 6), 30% of the Fly view height:
  video window | HUD | Status Data panel (`custom/src/FlyViewCustomLayer.qml`,
  parts in `custom/res/Custom/qml/KevaFlyView/`). The video window is QGC's
  picture-in-picture view with its default size matched to the strip. It is
  always shown, also without a video source (black "Video Transmission
  Disconnected"): click swaps it with the map, double-click (or the top-left
  button on hover) opens it in a separate window, the bottom-left button
  collapses it, the corner handle resizes it. QGC's
  bottom-right telemetry bar and instrument panel are removed
  (override of `FlyViewBottomRightRowLayout.qml`). The administrator banner
  sits above the strip.
* Video window (UI deck slide 9): override of `FlightDisplayViewVideo.qml`
  (copied from QGC Stable_V5.0, only the "no video" part changed) shows a black
  window with red "Video Transmission Disconnected" whenever no frames are
  decoded. QGC stops a stalled stream after its timeout (UDP 3 s, RTSP uses
  "RTSP Video Timeout", KEVA default 4 s), so the message appears about 5 s
  after the picture stops. When the upstream file changes, re-copy it and
  re-apply the "noVideo" block.
* HUD (UI deck slide 10, `KevaHudPanel.qml` + `KevaHud*.qml`): heading tape,
  airspeed tape, relative altitude tape with climb ribbon, pitch ladder
  (5/10 deg), bank scale (10/20/30/45/60 deg, fixed red index), red aircraft
  reference; footer GS, flight time (from takeoff, hh:mm:ss), roll/pitch,
  MSL altitude, climb rate. No armed/disarmed text.
* Status Data panel (UI deck slides 11-12, `KevaStatusDataPanel.qml`), "--" before
  connection:
  * Alt. (Rel), Air Spd., Gnd Spd.
  * Comm.: good MAVLink packets over the last 5 s (from the vehicle's received /
    lost counters), bar and %, 70/50/30 % colour steps.
  * Thr%: FW and M1-M4 in % with 60/80 % colour steps. Values come from
    SERVO_OUTPUT_RAW main outputs (M1-M4 = outputs 1-4, FW = output 5,
    PWM 1000-2000 us = 0-100 %; constants at the top of `CustomPlugin.cc`).
    Without that message FW falls back to the VFR_HUD throttle in forward flight.
  * Position: Lat./Lon. 7 decimals, MGRS 1 m (`CustomPlugin::kevaMgrs`).
  * Wind dir./spd. with 8 / 12 m/s colour steps.
  * Dist. to home; Dist. to GCS only with an NMEA GPS configured for the ground
    station (AutoConnect NMEA port), otherwise "--".
  * Target dials (click Alt. (Rel) / Air Spd. when flying; `KevaSetpointPopup.qml`,
    `KevaSetpointDial.qml`): multi-turn dial, 15 deg per step, mouse wheel / arrow
    keys one step, tap the number to type it, slide to confirm.
    Altitude 10-3000 m in 10 m steps (`CustomPlugin::kevaSetTargetAltitude`):
    pauses (Hold) and changes altitude, so a running mission / takeoff stops,
    as the confirm text says. Airspeed in 1 m/s steps within FW_AIRSPD_MIN..MAX,
    fixed-wing flight only (`kevaSetTargetAirspeed`). Both are written to the
    audit log (ALTITUDE_COMMAND / AIRSPEED_COMMAND) in the MIL edition and are
    unavailable to administrator accounts.
* Fixed layout: the main view (map or video) ends above the bottom strip
  (`PipView.fullItemBottomInset`, set from `FlyViewCustomLayer.mainViewBottomInset`;
  0 in full screen video). The strip is black; map insets no longer reserve it.
  Video default fit is "No Crop" (whole picture, black bars).
* Map labels in English (`KEVA_FORCE_ENGLISH` in `MapProvider.cpp`, Google `hl=en`).
  Tiles already in the cache keep their old labels until the cache is cleared.

### Fly view toolbar (UI deck slides 7-8)

* `custom/res/Custom/qml/QGroundControl/Controls/FlyViewToolBar.qml` replaces QGC's
  toolbar through the resource override (`/Custom/qml/...`): logo + "KEVA" (main
  menu), coloured status box, then the indicators.
* `custom/res/Custom/qml/KevaToolbar/KevaMainStatusIndicator.qml` is a copy of QGC's MainStatusIndicator with the
  text colour and MR/FW label changed (marked `KEVA`); keep it in step on upstream merges.
* An overriding QML file is loaded under QGC's original URL, so it cannot use
  files placed next to it as types. Import shared parts by absolute path
  (`import "qrc:/Custom/qml/KevaToolbar"`).
* Indicators (`custom/res/Custom/qml/KevaToolbar/`) are listed by
  `CustomFirmwarePlugin::toolIndicators()`: flight mode (white, PX4 "Return" shown
  as "Safe Recovery"), MR/FW, GPS (worst of satellites / lock / HDOP), batteries.
  `modeIndicators()` keeps only the multi‑vehicle selector.
* Selectable flight modes: `kevaModeSettable()` in `CustomFirmwarePlugin.cc`.

## Security module (MIL)

Sources: `custom/src/Security/`, login window: `custom/res/Security/LoginWindow.qml`.

* `CustomPlugin::init()` shows the login window before QGC creates its main
  window, so no MAVLink link, video or map starts before authentication.
* Data folder: `%ProgramData%\DaeyoungMnS\DaeyoungGCS\Security\`
  * `accounts.dat` – accounts, DPAPI (machine scope) encrypted, passwords as
    salted PBKDF2‑HMAC‑SHA256 (600,000 iterations, Windows CNG)
  * `audit.log` – one DPAPI encrypted, hash chained record per line
* Every audit record is also written to the Windows Application event log
  (source "Daeyoung Ground Control").
* A registry marker (`HKCU\Software\DAEYOUNG_MnS\DaeyoungGCS\KevaSecurity`)
  records that setup was completed. If `accounts.dat` disappears afterwards the
  application refuses to start instead of offering setup again.
* Policy values are in `custom/src/Security/SecurityConfig.h`.

### Roles (SR-15)

| | Administrator | User |
|---|---|---|
| Fly view | monitoring only – banner shown, guided actions hidden | full |
| Flight commands (arm, mode, takeoff, land, RTL, goto, altitude/speed, VTOL transition, mission start …) | blocked | allowed |
| Joystick / virtual joystick | blocked | allowed |
| Plan Flight | hidden | allowed |
| Vehicle Configuration | allowed | hidden |
| Application Settings | all pages | General, Fly View, Plan View, Video, Comm Links, Maps, Remote ID, Help |
| Account management, audit log | login window → Account Management | – |

Blocked commands are refused in C++ at the single point where QGC sends
MAVLink commands (`Vehicle::_sendMavCommandWorker`), so no QML path can
bypass them; each refusal is written to the audit log (`COMMAND_BLOCKED`).
Rules live in `custom/src/Security/AccessPolicy.cc`.

### Idle lock

After 10 minutes without keyboard/mouse/touch input a full‑screen lock
asks for the current user's PW (failures count toward the lockout rules).
The lock is never applied while a vehicle is armed. "Exit Application" on
the lock screen closes the application and records the logout.

### Upstream QGC files changed

Kept to small, marked (`KEVA custom build`) hooks so upstream merges stay easy:

* `cmake/Git.cmake` – product version override
* `cmake/Install.cmake` – installer script / file name override
* `src/QmlControls/ScreenToolsController.cc` – product font (`KEVA_FONT_FAMILY`, Arial)
* `src/QGCApplication.cc` – English UI (`KEVA_FORCE_ENGLISH`)
* `src/FlightDisplay/FlyView.qml` – video window shown even without a video source;
  main view and 3D viewer end above the bottom strip
* `src/FlightDisplay/FlyViewVideo.qml` – "disconnected" display without a video source
* `src/QmlControls/PipView.qml` – click swaps (after the double-click interval),
  double-click opens a separate window; `fullItemBottomInset`
* `src/QmlControls/PipState.qml` – full view bottom margin = `fullItemBottomInset`
* `src/QtLocationPlugin/Providers/MapProvider.cpp` – map label language English
* `src/MAVLink/LibEvents/CMakeLists.txt` – libevents pinned to `077677a`
  (upstream "main" no longer builds against QGC 5.0's MAVLink headers)
* `src/API/QGCCorePlugin.h` – `vehicleControlAllowed()` virtual
* `src/Vehicle/Vehicle.cc` – calls it for commands, SET_MODE and manual control
* `src/FlightDisplay/GuidedActionsController.qml` – hide/guard guided actions
* `src/UI/MainWindow.qml` – Plan / Vehicle Configuration by role
* `src/QmlControls/AppSettings.qml` – settings pages by role

### Resetting a development PC

Delete the folder `%ProgramData%\DaeyoungMnS\DaeyoungGCS` and the registry key
`HKCU\Software\DAEYOUNG_MnS\DaeyoungGCS\KevaSecurity`. Never do this on a
delivered system – it erases the audit trail.

### Unit tests and login preview

`custom/tests/security` is a standalone CMake project (not part of the QGC build):

```
cmake -S custom/tests/security -B build-security-tests
cmake --build build-security-tests --config Release
ctest --test-dir build-security-tests -C Release --output-on-failure
```

On Windows these tests exercise the real CNG and DPAPI code paths.
`login_preview --interactive` opens the login window with a throw‑away account
store; `login_preview <folder>` saves a screenshot of every stage.

## Windows installer

`custom/deploy/windows/keva_installer.nsi.in` is turned into
`<build>/keva_installer.nsi` for the selected edition and used instead of QGC's
script (hook in `cmake/Install.cmake`). The output is
`DaeyoungGCS-<version>-setup.exe` (MIL) or `DaeyoungGCS_CE-<version>-setup.exe` (COM).

Both editions: product name in the window/installer/start menu, install folder
`C:\Program Files\<product name>`, version and publisher in "Apps & features",
and uninstall removes only that edition's settings (the two editions share the
`DAEYOUNG_MnS` settings folder).

MIL only:

* Creates `%ProgramData%\DaeyoungMnS\DaeyoungGCS\Security` with this ACL (SR-08):
  * SYSTEM, Administrators: full control
  * Users and OWNER RIGHTS: read, create, write, append – **no delete, no
    permission change** (OWNER RIGHTS stops the operator who created a file
    from changing its permissions and then deleting it)
* Registers the event log source "Daeyoung Ground Control" so Event Viewer
  shows the audit messages as text.
* Uninstall keeps the security folder and tells the user only a Windows
  administrator can remove it.

Because users cannot delete files there, the account database is kept as two
copies (`accounts.dat`, `accounts.alt`) overwritten alternately in place
instead of write‑then‑rename. If the newest copy is damaged the other is used.
If the account files are replaced by an older copy (e.g. to undo a lockout),
the gap to the audit log is detected, recorded as `INTEGRITY_ALERT` and shown
to administrators.

### Checking the installed system

1. Install `DaeyoungGCS-1.0.1-setup.exe`, sign in to Windows as a standard user.
2. Start the application and log in: accounts are created and saved.
3. In Explorer, try to delete a file in `C:\ProgramData\DaeyoungMnS\DaeyoungGCS\Security`:
   access is denied (an administrator account gets a UAC prompt instead).
4. Event Viewer → Windows Logs → Application → source "Daeyoung Ground Control":
   login records are readable text.
