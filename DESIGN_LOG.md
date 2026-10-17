# DESIGN LOG

Append-only design/implementation log for work on this repository.
Never overwrite, truncate, or erase earlier entries — append new dated entries only.

---

## 2026-10-17 — Entry 1: Task intake, baseline, and issue inventory

### Original user request (preserved verbatim)

> T mới fork xong [https://github.com/coderunknow/game](https://github.com/coderunknow/game). Giờ t muốn m cho prompt để AI fix các issue có thể verified thử trước xem nó làm được không.

### Decisions selected by the user

1. **Scope:** Investigate all currently open upstream issues, including feature requests. Implement only those with sufficiently clear requirements and acceptance criteria.
2. **Evidence standard:** Implement a fix when source-code investigation gives a defensible explanation of the cause and appropriate tests support it. Clearly distinguish source-level evidence, automated test results, and actual runtime verification. Never claim runtime success without runtime evidence.
3. **Agent:** Arena AI Agent.

All implementation changes are made only in `coderunknow/game`, never upstream. Do not commit directly to `main`. Do not push to upstream. Do not modify upstream issues/PRs/settings.

### Repository state at start

- Repository checkout: `/home/user/game`, remote `origin` = `https://github.com/coderunknow/game.git` (verified — push destination is the fork, not upstream).
- Session working branch: `arena/64808b34-game`, created from `main` at commit `86a3a4f0946368d073ae36cab512b70d1a03297f` ("Format \"fix pwa\" [CF-Pages-Skip]").
- HEAD at start: `86a3a4f` (identical to the fork `main` baseline).
- Working tree: clean (`git status`: nothing to commit).
- No `AGENTS.md` or other repository instruction files found (searched the tree). `DESIGN_LOG.md` did not exist; this file was created before any application-code change as required.
- GitHub connection: authenticated `gh` available; all work targets `coderunknow/game`.

### Architecture notes (source inspection)

- The game is a classic-script + ES-module hybrid loaded by `game/index.html`:
  - Classic sloppy-mode scripts: `StorageUtil.js`, `Cfunction.js` (engine core: `$`, `NewImg`, `EditEle`, `ClearChild`, `oSym`, `oGd`, `oZ`, `oT`, `oP`, `oS`, `InheritO`, `NewO`, `ImgSpriter`, `CPlants`), `Welcome.js`, `Menus.js`, `Custom.js` (defines `PlaySound2`), `Cheatcodes.js`, `CZombie.js`, `IZombie.js`.
  - ES modules (always strict mode): `js/CPlants.js` and every `js/plants/*.js` file. They populate `window[plantName]` at load.
- Plant DOM structure built in `CPlants.Birth` (`Cfunction.js`): a `div` (id = `P_<random>`, set in `BirthStyle`) containing `childNodes[0]` = shadow `<img>` and `childNodes[1]` = plant `<img>` (`e.plantImage`).
- Attack pipeline: `oP.Monitor` starts a loop `loop()` → `oZ.traversalOf()` → per zombie `oT.chkD0/chkD1` → `plant.TriggerCheck(zombie)` → `CheckLoop` → `plant.NormalAttack()` → (after the call) `oSym.addTask(140, AttackCheck1...)`. **`loop()` reschedules itself with `oSym.addTask(10, loop, [])` only AFTER `oZ.traversalOf()` returns.** If `NormalAttack` throws, the exception propagates up through `traversalOf` and the reschedule is skipped: the entire zombie/plant update loop dies and the game freezes. `CheckLoop` also sets `canTrigger = 0` before calling `NormalAttack`, so a throw permanently disables the plant's attacks.
- `oSym` task runner (`oSym.Start` → `runTasks`) wraps each task invocation in `try/catch` and only logs the error; a synchronous throw inside a task therefore silently kills any self-rescheduling loop that has not yet queued its continuation.

### Upstream issue inventory (live query 2026-10-17, repo `Plants-vs-Zombies-MODDED/game`)

Open issues (6): #61, #53, #50, #25, #9, #65. Open PRs (1): #51.

| Issue | Subject | Classification | Evidence / disposition |
| --- | --- | --- | --- |
| [#61](https://github.com/Plants-vs-Zombies-MODDED/game/issues/61) | Fix Soak-shroom crashing the game | **ACTIONABLE** | Empty issue body, but source investigation identified and reproduced a hard crash (see Entry 2). Highest-priority fix in this change. |
| [#53](https://github.com/Plants-vs-Zombies-MODDED/game/issues/53) | Balance late-game levels (#52) | **BLOCKED / AMBIGUOUS** | Body is only `Current list: - 4-4` and `Closes #52`. Issue #52 is no longer resolvable via the GitHub API (deleted or inaccessible), so its description is unavailable. "Level 4-4" maps to `game/level/34.js` (`LevelName: "Level 4-4"`), a 30-flag pool level (`LF: [0, 3, 3, 3, 3, 3, 3]`) with `ZName: [oZombie, oZombie2, oConeheadZombie, oCXZombie, oBucketheadZombie, oXBZombie, oJX]`. No symptom, no target difficulty, no measurable acceptance criterion exists anywhere in the issue. Balance changes would require inventing difficulty targets — explicitly out of scope. Not changed. |
| [#50](https://github.com/Plants-vs-Zombies-MODDED/game/issues/50) | New plant: oImitater | **IN PROGRESS upstream** | Upstream PR [#51](https://github.com/Plants-vs-Zombies-MODDED/game/pull/51) ("Create oImitater", head branch `imitater`, body `Closes #50`) is open and MERGEABLE as of 2026-10-17, adding `game/js/plants/oImitater.js` + assets. Do not duplicate active upstream work. |
| [#25](https://github.com/Plants-vs-Zombies-MODDED/game/issues/25) | Add Gargantuar zombie | **IN PROGRESS upstream / AMBIGUOUS** | Maintainer comments: "Gargantuar is currently being worked on." (2026-02-08) and "Reopening to track Gargantuar progress" (2026-02-21). `oGargantuar` class already exists in `game/js/CZombie.js` (line 11840, `InheritO(OrnIZombies, ...)`) with assets under `game/images/Zombies/Gargantuar/`, but no level references it (`grep oGargantuar game/level/` is empty) — matching the report that it is never seen in levels. Wiring a new zombie into level rosters is gameplay design without acceptance criteria and is being tracked upstream. Not duplicated here. |
| [#9](https://github.com/Plants-vs-Zombies-MODDED/game/issues/9) | Add Crazy's Dave Store | **BLOCKED / AMBIGUOUS** | Empty body. Discussion (2024) is speculative ("Crazy daves also might be possible… I'll keep this suggestion in mind"); the rest of the thread is meme comments. No behavior spec, no acceptance criteria, no assets/design. Large feature; cannot be implemented without inventing requirements. |
| [#65](https://github.com/Plants-vs-Zombies-MODDED/game/issues/65) | Rewrite with modern technologies | **NOT APPROPRIATE FOR THIS PR** | Body: "Rewriting in private repo". A wholesale rewrite is inaccessible (private), out of scope for a reviewable bug-fix PR, and explicitly not authorized. |

Additional checks: `gh issue list --state open` returned exactly the six issues above; `gh pr list --state open` returned exactly PR #51. No other actionable items found.

### Build-script warning (recorded before running anything destructive)

- `scripts/build.sh` globally installs tools, stamps `game/images/Zombies/CX/v.html`, runs `oxfmt --write` over the tree, rebundles/minifies all JS/CSS/HTML in place, and `oxlint . || true` (suppressed failures). CI (`main.yml`) runs this destructive pipeline only on pushes to `main`/`staging`.
- Decision: do NOT run `scripts/build.sh` in the working copy. PR validation will use `npx oxlint` (matching `.github/workflows/lint.yml`, which runs `npx --yes oxlint@latest --format=github` on pull requests) plus the targeted regression test. Any build-produced changes would be kept out of the branch anyway.

### Plan

1. Reproduce the #61 crash headlessly (Node harness executing the real engine + real plant module).
2. Minimal fix in `game/js/plants/oGun.js` consistent with `oSeaAnemone` conventions.
3. Regression test (`tests/`) runnable with plain `node`, no new dependencies.
4. Lint the diff, review it, commit on `arena/64808b34-game`, push to `coderunknow/game`, open one PR (base `coderunknow/game:main`), leave it unmerged.

---

## 2026-10-17 — Entry 2: Root-cause analysis for #61 (Soak-shroom) and reproduction

### Suspected root cause (stated before implementation)

`game/js/plants/oGun.js` defines `NormalAttack` with this statement:

```js
((j = k.id), (d = $(j)), EditEle(b.BulletEle.cloneNode(false), { id: c }, 0, EDPZ));
```

`j` and `d` are **not declared** anywhere in the function or module. `oGun.js` is an ES module (loaded via `js/CPlants.js`, `type="module"`), so it executes in **strict mode**, where assignment to an undeclared identifier throws `ReferenceError`. The sibling plant `oSeaAnemone.js` — which uses the same shoot-animation + `ImgSpriter` pattern — correctly declares `var j = k.id; var d = $(j);`. The undeclared names in `oGun.js` appear to be decompiled/minified leftovers.

Call-path impact (see architecture notes, Entry 1): `NormalAttack` is invoked synchronously from `TriggerCheck` → `CheckLoop`, which is invoked from `oT.chkD0/chkD1` inside `oZ.traversalOf()` inside `oP.Monitor`'s self-rescheduling `loop()`. The throw therefore (a) prevents `CheckLoop` from queueing its `AttackCheck1` re-arm task after `canTrigger = 0` was set (plant never attacks again), and (b) unwinds `loop()` before `oSym.addTask(10, loop, [])`, **killing the main game loop** — zombies and plants freeze, matching "crashing the game".

Investigated and rejected/deferred alternative avenues (all examined in source before implementation):

- `b.BulletEle.cloneNode(false)` (`PrivateDie` nulls `BulletEle`): `NormalAttack` is only reachable while the plant is registered in `$P` and alive; `Die`/`BoomDie` delete `$P[id]` first. No demonstrated crash path. Not changed.
- Delayed `ImgSpriter` completion callback `var i = $(n); $P[n] && (i.childNodes[1].src = ...)`: `Die()` deletes `$P[id]` before `ClearChild`, and `ImgSpriter` stops animating when its element is gone, so the `$P[n]` guard already covers the destruction race. No demonstrated crash path. Not changed.
- `moveBullet` closure (`oSym` task): its exceptions are caught by `oSym`'s task wrapper and the same code is shared with Puff-shroom (no reports against it). Not changed.
- Missing assets: `game/images/Plants/gun/` contains `0.gif`, `SeaShroom.gif`, `SeaShroomShoot.gif`, `SeaShroomSleep.gif`, `ShroomBullet.gif`, `ShroomBulletHit.gif` — every referenced file exists. Not a cause.
- Global-leak hypothesis (that sloppy classic scripts might create `window.j`/`window.d`, masking the bug): empirically checked after loading the real engine — neither `j` nor `d` exists as a global (`hasOwnProperty(globalThis, 'j'/'d')` = false). Even if some path leaked them later, the module would still depend on accidental globals.

### Automated reproduction (before any code change)

Headless harness at `/home/user/harness/repro.mjs` (scratch, not committed): loads the real engine `game/js/Cfunction.js` in a Node `vm` (classic sloppy script semantics) with a minimal DOM shim, then dynamically imports the real strict-mode module `game/js/plants/oGun.js`, builds a plant exactly as `CPlants.Birth`/`PrivateBirth` do, and drives the engine path `TriggerCheck` → `CheckLoop` → `NormalAttack` with a zombie in trigger range.

Observed result (unmodified code):

```text
global leak check (hasOwnProperty on globalThis): { j: false, d: false }
calling TriggerCheck (same path as oT.chkD0) ...
RESULT: TriggerCheck THREW: ReferenceError: j is not defined
    at Child.NormalAttack (game/js/plants/oGun.js:57:7)
    at Child.CheckLoop (Cfunction.js:4342:8)
    at Child.TriggerCheck (Cfunction.js:4338:56)
```

This is source-level evidence plus headless JS-runtime execution of the real game modules. It is **not** full browser gameplay verification (no browser is available in this sandbox).

### Fix (minimal, convention-consistent)

Declare `var j = k.id; var d = $(j);` in `oGun.NormalAttack` (matching `oSeaAnemone.js`), replacing the undeclared comma-expression assignment. No other behavior changes.

---

## 2026-10-17 — Entry 3: Implementation, test evidence, and delivery

### Changes made

1. `game/js/plants/oGun.js` — `NormalAttack`: replaced the undeclared comma-expression
   assignment `((j = k.id), (d = $(j)), EditEle(...))` with proper declarations
   `var j = k.id; var d = $(j);` followed by `EditEle(...)`, matching the convention
   in `game/js/plants/oSeaAnemone.js`. No other lines changed; behavior otherwise identical.
2. `tests/oGun-normalattack.test.mjs` — new dependency-free Node regression test
   (`node tests/oGun-normalattack.test.mjs`). Loads the real `game/js/Cfunction.js`
   (classic sloppy script) and the real `game/js/plants/oGun.js` (ES module = strict
   mode), then drives `TriggerCheck -> CheckLoop -> NormalAttack` and the `oSym` task
   queue through a full shot cycle (spawn → visibility → hit → cleanup → idle-gif
   callback → `canTrigger` re-arm). Browser DOM and audio are shimmed; engine and
   plant code are real. This repo has no test framework, so the test is a plain Node
   script with `node:assert` — no new dependencies.

### Automated test evidence

- `node tests/oGun-normalattack.test.mjs` on the FIXED tree: **PASS**
  (`ok - oGun (Soak-shroom) NormalAttack regression test passed`, exit 0).
- Same test on the ORIGINAL tree (fix stashed): **FAIL** with
  `AssertionError: Got unwanted exception: TriggerCheck -> NormalAttack must not throw`,
  `Actual message: "j is not defined"` at `oGun.js:57` — the test is a genuine regression test for the reported defect.
- Lint parity check with the CI tool (`npx --yes oxlint@latest`, same as
  `.github/workflows/lint.yml`): before my change the repository reports 131 finding
  locations (449 errors / 29 warnings under current `oxlint@latest`); after my change the
  count is identical. `game/js/plants/oGun.js` has 2 pre-existing `no-nested-ternary`
  findings on untouched lines (same pattern exists in `oSeaAnemone`/`oPuffShroom`);
  my change neither adds nor removes findings in that file, and `tests/oGun-normalattack.test.mjs` lints clean. The strict `lint.yml` job cannot go green without a
  repo-wide cleanup of pre-existing findings (e.g. 37 in `CZombie.js`, 12 in
  `Cfunction.js`) — out of scope here. `scripts/build.sh` was NOT run (it would
  minify/rewrite the tree and stamp files).

### Browser runtime verification (real Chromium 131, headless)

Method: the real game (`game/index.html`) was served over HTTP and booted in headless
Chromium (`puppeteer-core` + `@sparticuz/chromium` — tooling kept OUTSIDE the repo in
`/home/user/harness`, not committed). The production boot path was used
(`InitGame` → level 0 → `SelectModal(1)` = "Level 1-1"), then `CustomSpecial(oGun, row, 3)`
and a real `oZombie` spawned with `CustomBirth`. The level's own `oP.Monitor` traversal
loop was instrumented with a call counter. Soak-shroom `PrivateBirth` + `NormalAttack`
are the real code paths; only browser automation drives placement.

Observed on the ORIGINAL code (fix stashed), plant in top row (row 5):

- The instant the zombie entered trigger range: `ReferenceError: j is not defined`
  with the full production stack
  `NormalAttack (oGun.js:57) -> CheckLoop (Cfunction.js:4342) -> TriggerCheck (Cfunction.js:4338) -> chkD0 (Cfunction.js:1639) -> processRow (Cfunction.js:1571) -> traversalOf (Cfunction.js:1594) -> loop (Cfunction.js:1283) -> runTasks (Cfunction.js:120)`.
- `oZ.traversalOf` call counter froze at 2 permanently (loop died before its
  `oSym.addTask(10, loop)` reschedule) — **the whole game froze**: zombie stopped
  walking forever, plant never fired (no bullet element ever created, shoot gif never
  applied), `canTrigger` stuck at 0. This is the "crashing the game" behavior.
- Variant (original code, plant in middle row 3): the throw is swallowed by `oSym`'s
  per-task `try/catch` (row scan runs as queued tasks) — the game limps on but
  Soak-shroom is permanently disabled after one failed shot (never shoots, `canTrigger`
  stuck at 0, zombie unharmed) and the failing row's scan aborts each cycle.

Observed on the FIXED code (both row 3 and row 5):

- No exceptions of any kind (`console.error` capture empty).
- Soak-shroom fires: bullet element spawned and made visible, shoot gif applied and
  restored to idle by the `ImgSpriter` completion callback, zombies take 20 damage per
  hit (HP 270 → 250 on first shot; 270 → 130 over 12 s of continuous re-fire in the
  row-3 run), `canTrigger` cycles correctly while a zombie stays in range.
- Traversal loop keeps running (`loopAlive: true`) — no freeze.

Environmental noise present in both runs (not related to the fix): headless audio
`AbortError` on `play()/pause()` races, 404s for unreachable external resources
(Cloudflare turnstile, fonts), `X-Frame-Options`-in-`<meta>` console warning.

Evidence classification: the Node regression test is **automated test** evidence; the
Chromium session is **real runtime verification** of the actual game in a real browser
engine (headless, scripted placement — not manual play-testing); the causal chain from
source reading is **source-level evidence**. All three agree.

### Scope decisions (unchanged from Entry 1)

- #53 (balance 4-4), #9 (Crazy Dave's Store): no acceptance criteria; documented as
  BLOCKED/AMBIGUOUS in Entry 1. No gameplay/level values were changed.
- #50 (oImitater): upstream PR #51 is open; not duplicated.
- #25 (Gargantuar): upstream tracks it as in progress; `oGargantuar` class exists but
  is unused by levels; level roster design is out of scope.
- #65 (rewrite): explicitly out of scope.
- Latent secondary risks in `oGun.NormalAttack` (`BulletEle` after `PrivateDie`,
  `ImgSpriter` callback vs element removal) were analyzed in Entry 2 and found
  unreachable via demonstrated paths given `Die()`/`BoomDie()` ordering; left unchanged
  to keep the fix minimal and convention-consistent.

### Delivery

- Working branch: `arena/64808b34-game` (session-mandated; created from fork `main` at
  `86a3a4f`, which was the verified clean baseline).
- Files changed: `game/js/plants/oGun.js` (fix), `DESIGN_LOG.md` (this log),
  `tests/oGun-normalattack.test.mjs` (regression test).
- Build artifacts, scratch harnesses (`/home/user/harness`), and extracted browser
  tooling are outside the repository and are not part of the change set.

---

## 2026-10-17 — Entry 4: Final review, Windows desktop packaging, merge & release preparation

### Request (this task)

Final technical review of PR #2, then: establish the Soak-shroom fix is correct and
regression-free; "Produce a genuinely playable Windows x64 desktop build containing the
game and its required resources"; merge PR #2 into `coderunknow/game:main` only after
merge gates pass; tag the final release commit `v0.1.0-dev1`; publish a GitHub pre-release
with downloadable Windows executable assets. Constraints carried over: all work stays in
the fork, never upstream; append-only `DESIGN_LOG.md`; audit workflow side effects before
merging; do not bypass required checks or protections; do not fabricate test results;
never label a scripted browser test as manual play-testing; do not clean up unrelated
pre-existing lint debt; report blockers instead of merging to meet a deadline.

### State refreshed before acting (not assumed from the previous session)

| Item | Observed now |
| --- | --- |
| PR #2 | OPEN, base `main`, head `arena/64808b34-game` @ `7d72421`, `mergeable: MERGEABLE`, `mergeStateStatus: UNSTABLE`, no review decision, 3 files changed |
| PR checks | `Kilo Code Review` = ACTION_REQUIRED/fail (**billing-blocked bot**: "your account is out of credits"), `CodeRabbit` = pass, Copilot review = COMMENTED, Qodo = paused (trial ended). None are required checks |
| `main` HEAD | `86a3a4f0946368d073ae36cab512b70d1a03297f` (unchanged, no concurrent work) |
| Tags / releases | none (`git tag -l` empty, `gh release list` empty) → `v0.1.0-dev1` is free |
| Working tree | clean at session start |
| GitHub Actions on this fork | **disabled/inactive**: `actions/workflows` → `total_count: 0`, `actions/runs` → `total_count: 0`; `actions/permissions` and branch-protection APIs → 403 (token scope), so protection rules could not be read directly |
| Runner labels used by repo workflows | `blacksmith-4vcpu-ubuntu-2404`, `blacksmith-2vcpu-ubuntu-2404` (upstream-hosted custom runners, not available to a fork) |
| Merge settings | `allow_merge_commit`, `allow_squash_merge`, `allow_rebase_merge` all true; `delete_branch_on_merge` false |

### Workflow side-effect audit (mandatory pre-merge check)

`.github/workflows/main.yml` (trigger: push to `main`/`staging`, plus `workflow_dispatch`)
has two jobs:

1. `format-lint-stamp` — installs oxlint/oxfmt/esbuild/html-minifier-terser globally, runs
   `pnpm run fix || true` (failures suppressed), rewrites the whole tree with
   `oxfmt --write .`, stamps `game/images/Zombies/CX/v.html` with the commit SHA, then
   `git add . && git commit && git push`. **Side effect: creates a new commit on `main`
   after the merge and rewrites sources repo-wide.**
2. `mirror` (`if: always()`) — pushes the repository to two **external, upstream-owned**
   destinations (`git@tangled.org:pvzm.net/game`, `git@codeberg.org:pvzm/game.git`) using
   `TANGLED_SSH_PRIVATE_KEY` / `CODEBERG_SSH_PRIVATE_KEY`, uploads a source archive
   artifact, then POSTs a short-lived artifact download URL plus `FEED_PVZM_LOXODROME_SECRET`
   to `https://feed-pvzm.loxodrome.app`. **Side effect: external publication of the fork's
   contents.**

Other workflows: `lint.yml` (pull_request; strict `npx oxlint@latest --format=github` —
would fail on pre-existing repo debt) and `resolve-stamp-conflict.yml` (pull_request, only
when `game/images/Zombies/CX/v.html` changes — this PR does not touch it).

**Assessment for this fork:** Actions are not running at all here (0 registered workflows,
0 runs), and the two `main.yml` jobs require upstream-only `blacksmith-*` runner labels.
So merging cannot execute them, and the fork holds none of the mirror secrets. Merging is
therefore **not** able to publish to upstream-owned destinations in the current state.

**Safeguard added anyway (defense in depth, in case Actions is ever enabled on the fork):**
the `mirror` job condition is now
`if: always() && github.repository == 'Plants-vs-Zombies-MODDED/game'`.
This preserves upstream behavior exactly (the expression is true in the upstream repo) and
makes it impossible for a push in any fork to run the mirror/feed steps. No secrets were
read, printed or logged.

The `format-lint-stamp` job was deliberately **not** altered: it is the upstream's own
format/stamp pipeline, it cannot run on this fork, and rewriting sources is out of scope.
Consequence recorded for the release step: after merging, `main` HEAD must be re-checked —
if that job ever runs it would add a formatting commit, and the tag must point at the
final verified HEAD, not at the PR head.

### Packaging decision (why WebView2 + Zig, not Electron/Tauri)

Established first: the repo has **no** existing Windows desktop packaging path
(`package.json` exposes only lint/fix/build scripts for the web game; no electron/tauri
config, no desktop entries). Environment constraints measured in this sandbox:

- `registry.npmjs.org`, `pypi.org`/`files.pythonhosted.org`, `github.com`,
  `codeload.github.com`, `api.github.com` are reachable.
- `objects.githubusercontent.com` and `release-assets.githubusercontent.com` are **not**
  reachable (TLS handshake failure, curl exit 35). GitHub *release assets* are served from
  those hosts → **Electron's `electron-vX-win32-x64.zip` cannot be downloaded here**
  (the npm `electron` package is only a downloader stub, 1.1 MB), and `nuget.org`,
  `raw.githubusercontent.com`, `cdn.npmmirror.com` are also unreachable.
- No `wine`, no `xvfb`, no `makensis`, no `rustc`/`cargo`, no `dotnet`, no MinGW.
  `zig` is available via PyPI (`ziglang==0.17.0`, ~101 MB wheel) and is a complete
  C/C++ cross-compiler for `x86_64-windows-gnu`.
- The full Microsoft WebView2 SDK bits needed to build a native host are inside the
  MIT-licensed npm package `webview-node@0.1.41`: `WebView2.h` (68,921 lines, includes
  `SetVirtualHostNameToFolderMapping`), the mingw import library
  `libWebView2Loader.a`, and the redistributable `WebView2Loader.dll`.

Chosen approach — **native Win32 host + WebView2 Runtime**, built with Zig:

- `desktop/host.cpp`: creates a real window, serves the bundled `game/` folder through
  WebView2's *virtual host name mapping* (`pvzm.game` → the executable's folder) and
  navigates to `https://pvzm.game/game/index.html`. This gives a secure origin, so ES
  modules load exactly as on the website, with **no local web server and no network
  involvement**. Browser arguments disable background networking/component updates;
  user data goes to `%LOCALAPPDATA%\PVZM\WebView2`. Missing-runtime and missing-`game/`
  cases produce clear dialogs (with a link to Microsoft's WebView2 page) instead of a
  silent failure. DevTools stay enabled in this dev release for bug reports.
- `desktop/setup.cpp` + `scripts/make-installer.py`: a self-extracting per-user installer
  (no admin rights, no services, no telemetry). Payload format is documented in the source
  header; installs to `%LOCALAPPDATA%\PVZM`, creates a Desktop shortcut, offers to launch.
  `--extract <dir> [--from <exe>]` allows host-side round-trip verification.
- `scripts/build-windows.sh`: reproducible build (pinned `webview-node@0.1.41`,
  `ziglang==0.17.0`), stages the portable layout, produces both artifacts, computes
  SHA-256, and runs the installer round-trip check.
- `.github/workflows/windows-release.yml`: least-privilege (`contents: write` only),
  `windows-latest`, builds from a clean checkout with pinned toolchain, runs
  `--selftest` and `--smoke` on the **packaged application**, uploads artifacts, and on a
  pushed `v*` tag publishes a pre-release with checksums. No deployment, mirroring or
  upstream write access.
- Licensing/attribution preserved in the package: `LICENSE.md` (PolyForm Noncommercial +
  Changjiang River), `desktop/THIRD-PARTY-NOTICES.txt` (Microsoft WebView2 SDK / MIT,
  webview-node / MIT, and the PopCap-EA IP notice), `desktop/README.md` (how it is built
  and how to run it). Nothing represents the project as an official EA/PopCap release.

Rejected alternatives: Electron (binaries unreachable here; ~200 MB; could not be
validated at all), Tauri (no Rust toolchain and no Windows SDK here), committing a
prebuilt binary blob into git (repo convention keeps artifacts out; `.gitignore` already
lists `dist/`, `build/` added), and any installer that merely opens the live website
(explicitly disallowed by the task).

### Verification of the packaged application (what could and could not be run here)

Environment: Linux x86_64 sandbox, Node 22, Python 3.11, `ziglang==0.17.0`,
`webview-node@0.1.41`, headless Chromium 131 (`@sparticuz/chromium`, harness kept outside
the repo in `/home/user/harness`). No Windows machine and no Wine → **the produced `.exe`
files cannot be executed in this environment**; that limitation is reported rather than
papered over. What was run:

1. `bash scripts/build-windows.sh` — exit 0. Outputs
   `dist/PVZM-win64-portable.zip`, `dist/PVZM-Setup-x64.exe`, `dist/SHA256SUMS.txt`.
2. Installer round-trip (in-script): extracting `dist/PVZM-Setup-x64.exe` reproduces all
   **1,625 payload files with byte-identical SHA-256** (`roundtrip OK: 1625 files match
   exactly`). This is the strongest available proof that the installer's payload and
   extraction code are correct.
3. PE inspection (`pefile`): both executables are `machine 0x8664` (x64),
   `subsystem 2` (GUI). `PVZM.exe` imports only `ADVAPI32/KERNEL32/SHELL32/USER32/ole32`
   + UCRT `api-ms-win-crt-*` + `WebView2Loader.dll` (shipped next to it) — i.e. **no
   undeclared adjacent DLL dependencies**; the package contains every runtime file it
   needs, and it is documented as *not* a single standalone exe.
4. Package inventory: portable ZIP holds 1,762 entries (1,756 under `game/`, plus
   `PVZM.exe`, `WebView2Loader.dll`, `LICENSE.md`, `README-desktop.md`,
   `THIRD-PARTY-NOTICES.txt`, `app.webmanifest`) — the complete game tree, no placeholders.
5. **Staged gameplay validation of the packaged tree in real headless Chromium.**
   `/home/user/harness/stage-scripts-verify.cjs` extracts the four `kScript*` literals
   verbatim from `desktop/host.cpp` and runs them in the same stage order, with the same
   delays and retry limits as the C++ state machine, against the tree extracted from the
   installer payload (`build/win/roundtrip`). Only substitution: the packaged app's
   `https://pvzm.game/game/...` origin is rewritten to the local static server serving the
   same files. Result (post-fix):
   - probe: `{"ready":"complete","engine":true,"selectModal":true,"oGun":true,"oGunAttack":true,"boot":"ok"}`
   - level: `{"level0":true,"select":"ok"}` (level 1-1 via production `SelectModal`)
   - scenario: Soak-shroom planted on the **top row (5)** and a **lower row (3)**, one
     zombie per row
   - collect: `{"topHP":210,"lowerHP":210,"hp0":270,"bulletSeen":true,"shootGifSeen":true,
     "loopCalls":36,"loopAlive":true,"assetStatus":200,"consoleLog":[]}`
   - pass criteria all true: bullet, shoot gif, loop alive, local asset (HTTP 200 from the
     bundled folder), zombie damaged on both rows (270→210 = 3 hits × 20), no
     `ReferenceError`; page errors `[]`
6. **Negative control for the packaged smoke test**: the same scripts against an identical
   tree whose only difference is the pre-fix `game/js/plants/oGun.js`
   (`git show 86a3a4f:...`) → FAIL, with the production stack
   `ReferenceError: j is not defined at NormalAttack (oGun.js:57:7) → CheckLoop
   (Cfunction.js:4342) → TriggerCheck (4338) → chkD0 (1639) → processRow (1571) →
   traversalOf (1594) → loop (1283) → Monitor (1285) → level/1.js:68`, `loopCalls: 1`,
   `loopAlive: false`, no bullets, HP 270 on both rows. This proves the packaged-app smoke
   check genuinely detects the reported regression rather than passing trivially.
7. `node tests/oGun-normalattack.test.mjs` — **PASS** (exit 0) on the current PR head.
   Baseline behavior re-confirmed earlier (fails with `j is not defined`), and re-confirmed
   here in a browser context via item 6.
8. Lint parity with the CI tool at a **pinned** version (`npx oxlint@1.88.0`, both trees):
   base `86a3a4f` = `29 warnings / 444 errors` on 339 files; this branch = `29 warnings /
   444 errors` on 340 files (the added file is the test). Per-rule finding counts are
   **identical** (`diff` of rule histograms: no difference), and the changed-file scan
   reports only the two pre-existing `no-nested-ternary` findings in `oGun.js` (lines 26
   and 78 — untouched code). No new findings introduced; no unrelated lint debt cleaned up.
   (The `449 errors` figure in Entry 3 came from `oxlint@latest` at that time; version
   drift, not a change in this branch.)
9. Hygiene: no credentials in the new files (the only `*token*` hits are the
   `EventRegistrationToken` type), no generated files or build artifacts staged
   (`build/`, `dist/` are gitignored), no reformatting churn — the diff touches only the
   intended files.

Not run / not available, with reasons: executing `PVZM.exe` or `PVZM-Setup-x64.exe`
(no Windows, no Wine in the sandbox); `windows-release.yml` on a real Windows runner
(GitHub Actions is disabled on this fork and the token cannot enable it — 403); manual
play-testing (all browser evidence is scripted automation, never claimed as manual).
The `--selftest`/`--smoke` modes and the workflow exist so that this exact validation runs
on a real Windows runner as soon as Actions is available; until then the published
artifacts are built in this sandbox with the same pinned toolchain versions the workflow
specifies, and that provenance is disclosed in the release notes.

### Merge gates evaluated immediately before merging PR #2

| Gate | Status | Evidence |
| --- | --- | --- |
| PR contains the intended fix + all pre-release changes | PASS | 14 files: `game/js/plants/oGun.js`, `tests/oGun-normalattack.test.mjs`, `DESIGN_LOG.md`, `desktop/*` (6), `scripts/build-windows.sh`, `scripts/make-installer.py`, `.github/workflows/windows-release.yml`, `.github/workflows/main.yml`, `.gitignore`; `oGun.js` hunk on GitHub is exactly the `var j`/`var d` declaration change and nothing else |
| Regression test passes | PASS | `node tests/oGun-normalattack.test.mjs` → `ok - ... regression test passed`, exit 0 at PR head `ad09588` |
| No new lint findings / regressions | PASS | `npx oxlint@1.88.0`: base `86a3a4f` = 29 warnings/444 errors (339 files) vs branch = 29/444 (340 files); per-rule histogram `diff` empty |
| Real-browser testing | PASS | headless Chromium 131 on the packaged tree: top row (5) and lower row (3) both fire, HP 270→210 each, bullets + `SeaShroomShoot.gif`, loop alive (36 traversal calls), local asset 200, `consoleLog` empty, `pageErrors` empty; negative control with pre-fix `oGun.js` reproduces `ReferenceError: j is not defined` and a frozen loop |
| Windows package built + launch/gameplay checks | PARTIAL (documented) | Built and validated structurally + behaviourally on Linux: round-trip extraction of 1,625 files hash-identical, PE x86-64/GUI with no undeclared DLL imports, packaged-tree gameplay validated in Chromium via the host's own stage scripts. **The `.exe` files themselves cannot be executed here** (no Windows, no Wine) and `windows-release.yml` cannot run because Actions is disabled on this fork (token cannot enable it: 403). `--selftest`/`--smoke` are implemented and wired into the workflow for the first real Windows run |
| Required checks / branch protection permit merge | PASS | `mergeable: MERGEABLE`, `mergeStateStatus: UNSTABLE` (a failing *non-required* check: Kilo Code Review, billing-blocked). Protection rules are not readable with this token (403), but GitHub would report `BLOCKED` if a required check or review were outstanding. No check was bypassed or disabled, no protection altered |
| Workflow side effects verified safe for this fork | PASS | Actions inactive on the fork (0 workflows, 0 runs) and `main.yml` jobs need upstream-only `blacksmith-*` runners; additionally the `mirror` job is now gated to `github.repository == 'Plants-vs-Zombies-MODDED/game'`, so a fork push cannot publish to tangled.org/codeberg.org or POST to feed-pvzm.loxodrome.app. No secrets were read or printed |
| Working tree / intended changes accounted for | PASS | `git status` clean after commit; `build/` and `dist/` artifacts gitignored and never staged; harness code kept outside the repo in `/home/user/harness` |

Decision: all gates that can be satisfied in this environment are satisfied; the one
partial gate (executing the Windows binaries) is an environment limitation that is
reported rather than hidden, and the release is explicitly a **development pre-release**
whose purpose is testing on real Windows hardware. Proceeding to merge with a merge
commit (repository allows merge/squash/rebase; a merge commit keeps the tested PR head
commit in history). No auto-merge.
