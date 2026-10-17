# Plants vs. Zombies: MODDED — v0.1.0-dev1 (development pre-release)

**⚠️ Development pre-release for testing only.** This is not a claim that the
whole game is production-ready. It exists so the Soak-shroom crash fix can be
tested on real Windows 10 x64 hardware.

Unofficial fan project. Not an official Electronic Arts or PopCap release.
Plants vs. Zombies: MODDED © 2025 by ROBINET13 & Clay — dual-licensed
(PolyForm Noncommercial License 1.0.0 + Changjiang River License); see
`LICENSE.md`. All Plants vs. Zombies IP remains property of PopCap/EA.

## Changes in this release

- **Fix: Soak-shroom no longer crashes the game**
  (<https://github.com/Plants-vs-Zombies-MODDED/game/issues/61>). `oGun.NormalAttack`
  assigned to undeclared `j`/`d` inside a strict-mode ES module and threw
  `ReferenceError: j is not defined` on the first shot, which killed the game
  loop (game freeze). Fixed by declaring the variables (matching the sibling
  plant `oSeaAnemone.js`). Regression test: `tests/oGun-normalattack.test.mjs`.
- **New: Windows x64 desktop build** (this release's main point). A native
  window (Microsoft Edge WebView2) loads the bundled game from local files —
  no web server, no network needed for gameplay. Build scripts and host
  source live in `desktop/` and `scripts/build-windows.sh`.

## Windows x64 assets

| Asset | What it is | How to run |
| --- | --- | --- |
| `PVZM-win64-portable.zip` | Portable package: `PVZM.exe`, `WebView2Loader.dll`, complete `game\` folder, licenses/docs (1,762 files, ~163 MiB). **Not** a standalone single exe — extract the whole zip. | Extract anywhere → run `PVZM.exe` |
| `PVZM-Setup-x64.exe` | Self-extracting per-user installer with the same content (~177 MiB). Installs to `%LOCALAPPDATA%\PVZM`, adds a Desktop shortcut. No admin rights. | Run it → "Launch now" or use the shortcut |

Requirements: Windows 10 x64 (or later), Microsoft Edge WebView2 Runtime
(preinstalled on Windows 11; updated with Microsoft Edge on Windows 10 — the
app shows a download link if missing). No dedicated GPU required. Uninstall =
delete the install folder + shortcut.

**The executables are unsigned** — SmartScreen may warn on first run
("More info" → "Run anyway").

## SHA-256 checksums

See the `SHA256SUMS.txt` asset (also printed below at publication time).

## Test coverage / known limitations

Verified before publication (Linux sandbox + headless Chromium + Node):

- Regression test `node tests/oGun-normalattack.test.mjs` — passes on the
  release commit (fails with the exact `ReferenceError` on the pre-fix code).
- Real-browser (Chromium) gameplay runs of the **exact packaged game tree**,
  Soak-shroom in both top-row and lower-row placements: bullet spawn,
  visibility, hit (20 dmg), animation cleanup, re-arm, game loop alive, zero
  console/page errors.
- Installer round-trip: extracting `PVZM-Setup-x64.exe` reproduces all 1,625
  payload files with identical SHA-256 hashes.
- PE inspection: both executables are x86-64 GUI subsystem and only import
  Windows system/UCRT DLLs plus the shipped `WebView2Loader.dll`.

Not verified at publication time (no Windows machine available in the build
environment): actually double-clicking the executables on Windows. The
repository contains `desktop/host.cpp --selftest/--smoke` exactly for that
check, and `.github/workflows/windows-release.yml` runs both on a real
Windows runner (enable GitHub Actions on the fork to use it). The game itself
is the current web build; only the crash fix and the packaging are new.
