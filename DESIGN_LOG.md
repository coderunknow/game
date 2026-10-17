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
