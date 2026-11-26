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
* Fly view tool strip: Takeoff, Return, Pause, Resume Mission
  (`custom/src/FlyViewToolStripActionList.qml`).

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
