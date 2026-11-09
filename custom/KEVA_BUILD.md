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

## Still to do (installer)

* Create `%ProgramData%\DaeyoungMnS\DaeyoungGCS\Security` with write access for
  standard users and no delete right (SR‑08).
* Register the event log source and write the setup marker under HKLM.
* Keep the security folder on uninstall (MIL).
