# Plants vs. Zombies: MODDED — Windows desktop build (v0.1.0-dev1)

This is a **development pre-release** of the web game packaged as a native
Windows x64 desktop application. It is not an official Electronic Arts or
PopCap product; see `LICENSE.md` in the package for the PolyForm
Noncommercial + Changjiang River dual license and required attribution.

## What is in the package

| File | Purpose |
| --- | --- |
| `PVZM.exe` | Desktop host. Opens a native window (Microsoft Edge WebView2) and loads the bundled game from the local `game\` folder. |
| `WebView2Loader.dll` | Required runtime file of the Microsoft WebView2 SDK (MIT). Must stay next to `PVZM.exe`. |
| `game\` | The complete game: HTML, JavaScript modules, images, audio, fonts, levels. |
| `LICENSE.md`, `THIRD-PARTY-NOTICES.txt` | Licensing and attribution. |

The whole folder is required — `PVZM.exe` is **not** a standalone single file.

Two download flavors are provided:

- `PVZM-win64-portable.zip` — extract the **entire** zip to any folder and run
  `PVZM.exe`.
- `PVZM-Setup-x64.exe` — self-extracting installer; installs per-user to
  `%LOCALAPPDATA%\PVZM`, adds a Desktop shortcut, no administrator rights
  needed. Uninstall = delete that folder and the shortcut.

## System requirements

- Windows 10 (x64) or later.
- The [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)
  — preinstalled on Windows 11 and updated together with Microsoft Edge on
  Windows 10. `PVZM.exe` shows a download link if it is missing.
- No dedicated GPU required (works with Intel integrated graphics).
- No administrator rights, no installer services, no network access needed
  for gameplay: all game files are loaded from the local `game\` folder.

## Running

Double-click `PVZM.exe` (or the Desktop shortcut created by the installer).

Diagnostics:

- `PVZM.exe --selftest [result.json]` — checks the install layout and WebView2
  Runtime presence (no window opens). Exit code 0 = OK; the JSON result is
  written to `result.json` when given (a GUI-subsystem exe has no console
  attached, so the file is the reliable channel in CI).
- `PVZM.exe --smoke result.json` — loads the packaged game, boots level 1-1,
  plants Soak-shroom on the top row **and** a lower row, spawns a zombie in
  each row and verifies bullet spawn, shoot animation, zombie damage, game-loop
  survival and local asset loading. Writes JSON to `result.json`; exit 0 = pass.
  This is the regression check for the Soak-shroom crash, run by
  `.github/workflows/windows-release.yml` on a real Windows runner.

DevTools (F12) are enabled in this development release so testers can copy
console errors into bug reports.

## How the desktop version is built

The build is reproducible from this repository (see `scripts/build-windows.sh`):

1. `desktop/host.cpp` — native Win32 host compiled with
   [zig](https://ziglang.org) (`zig c++ -target x86_64-windows-gnu`). It
   creates a WebView2 window and serves the bundled game via WebView2's
   *virtual host name mapping* (`pvzm.game` → the application folder). This
   keeps ES-module loading working exactly like on the website, with **no**
   local web server and no network involvement.
2. `desktop/setup.cpp` + `scripts/make-installer.py` — the self-extracting
   installer (per-user install, Desktop shortcut).
3. `game\` and docs are staged, zipped into the portable package and embedded
   into the installer payload.

Toolchain used for the published artifacts: `ziglang==0.16.0` from PyPI
   (pinned to 0.16.0 because that is the newest ziglang release with both Linux
   and Windows wheels on PyPI),
`webview-node@0.1.41` from npm (source of the WebView2 SDK files), Python 3,
zip. On a machine with those installed:

```sh
bash scripts/build-windows.sh
```

## Notes and limitations (v0.1.0-dev1)

- This release exists so the Soak-shroom crash fix
  (<https://github.com/Plants-vs-Zombies-MODDED/game/issues/61>) can be tested
  on real Windows hardware. Only the packaging and that fix are new; the rest
  of the game is the current web build and is not claimed to be
  production-ready.
- The executables are **unsigned** (SmartScreen may warn on first run —
  choose "More info" → "Run anyway").
- External resources referenced by the original web page (Cloudflare
  Turnstile script, PWA manifest extras, music sites links) may not load
  offline; they are optional and core gameplay is fully local.
- The game state is kept in the browser profile of the WebView2 runtime under
  `%LOCALAPPDATA%\PVZM\WebView2`.
