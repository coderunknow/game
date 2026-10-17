# Design log — verifiable upstream issue investigation

This file is append-only. Later entries add findings, decisions, progress, verification evidence, and limitations without rewriting earlier entries. An implemented patch is not, by itself, evidence that an upstream issue is resolved.

## 2026-10-17 — Request, boundaries, and verified starting state

### Original request and decisions

Original user request (preserved verbatim):

> T mới fork xong [https://github.com/coderunknow/game](https://github.com/coderunknow/game). Giờ t muốn m cho prompt để AI fix các issue có thể verified thử trước xem nó làm được không.

The user subsequently authorized this agent to investigate, implement, verify, and deliver justified changes, with these explicit decisions:

1. **Scope:** Investigate all currently open upstream issues, including feature requests. Implement only those with sufficiently clear requirements and acceptance criteria.
2. **Evidence standard:** A fix may be implemented when source-code investigation provides a defensible explanation of the cause and appropriate tests support it. Clearly distinguish source-level evidence, automated test results, and actual runtime verification. Never claim runtime success without runtime evidence.
3. **Agent:** Arena AI Agent.

The objective is a systematic, reviewable evaluation of the agent's actual work, not a maximum number of changes. Prioritize genuine bugs, particularly the Soak-shroom crash. Do not invent balance targets, feature requirements, or private-rewrite behavior.

### Verified baseline and repository boundaries

- Working directory and Git root: `/home/user/game`.
- Implementation repository: `coderunknow/game` (fork of `Plants-vs-Zombies-MODDED/game`).
- Starting HEAD: `86a3a4f0946368d073ae36cab512b70d1a03297f` (`Format "fix pwa" [CF-Pages-Skip]`).
- Checked-out branch: `arena/8dbc54a1-game`; local `main`, `origin/main`, fork GitHub `main`, and upstream GitHub `main` all point to the starting HEAD.
- Initial `git status --short`: empty; no pre-existing working-tree changes.
- Only configured remote: `origin`, fetch and push both `https://github.com/coderunknow/game.git`.
- `gh auth status`: authenticated as `coderunknow`; GitHub API confirms push permission on the fork. No credentials are recorded here.
- Fork issues are disabled (`has_issues: false`). Reference upstream issues with full URLs; a fork PR must not claim to close them automatically.
- Upstream is read-only for this task. Do not modify upstream issues, PRs, branches, settings, or assets.
- Arena fixes this session to `arena/8dbc54a1-game`; its higher-priority environment constraint prevents creating/using the otherwise requested descriptive branch. All changes, commits, and the sole potential PR will use this existing dedicated branch. Do not commit/push `main`.
- If a substantive change passes meaningful checks: commit, push only `origin arena/8dbc54a1-game`, and open exactly one PR with base `coderunknow/game:main`. Leave it open: no merge, auto-merge, tag, or release.

### Instructions and existing validation

Searched the repository (excluding `.git`) and ancestor instruction locations for `AGENTS.md`, instruction files, `CLAUDE.md`, `GEMINI.md`, `.cursorrules`, `.cursor/rules`, `.claude`, and `DESIGN_LOG.md`. No applicable agent instructions or existing design log were found. This file is created before application-code changes.

Read `README.md`, `package.json`, `.gitignore`, `.oxlintrc.json`, `scripts/build.sh`, and all `.github` templates/workflows. `package.json` has lint/fix/build scripts but no test script or test dependency. `oxlint` is invoked by scripts but is not a declared development dependency.

**Unsafe build identified; not executed in this working copy:** `scripts/build.sh` globally installs tools, overwrites `game/images/Zombies/CX/v.html`, formats source, bundles `CPlants.js` in place, and minifies remaining files. Its lint command suppresses failures with `|| true`. The main workflow similarly suppresses `pnpm run fix` failures and formats/commits changes. The standalone lint workflow does not suppress failures. Prefer non-mutating commands; use a disposable copy if build verification is needed. Keep generated artifacts and temporary tooling out of the implementation diff.

### Fresh upstream inventory (initial discovery; detailed classification pending)

Retrieved all open issues (`gh issue list --state open --limit 1000`), all open PRs, repository metadata, and all branch pages. The upstream has exactly six open issues and one open PR at this check:

| Issue | Current title | Initial facts to investigate |
| --- | --- | --- |
| [#61](https://github.com/Plants-vs-Zombies-MODDED/game/issues/61) | Fix Soak-shroom crashing the game | `bug`; empty body; highest-priority candidate. |
| [#53](https://github.com/Plants-vs-Zombies-MODDED/game/issues/53) | Balance late-game levels (#52) | `enhancement`; body lists only `4-4`, references closed #52. |
| [#25](https://github.com/Plants-vs-Zombies-MODDED/game/issues/25) | Add Gargantuar zombie | `enhancement`; reporter found assets but not a level appearance. |
| [#50](https://github.com/Plants-vs-Zombies-MODDED/game/issues/50) | New plant: oImitater | `enhancement`; empty body; linked draft PR #51 remains open. |
| [#9](https://github.com/Plants-vs-Zombies-MODDED/game/issues/9) | Add Crazy's Dave Store | Empty body; discussion and implementation require review. |
| [#65](https://github.com/Plants-vs-Zombies-MODDED/game/issues/65) | Rewrite with modern technologies | Body: `Rewriting in private repo`; not authorization for a rewrite here. |

Open [PR #51](https://github.com/Plants-vs-Zombies-MODDED/game/pull/51): draft, `imitater` → `main`, body `Closes #50`, last updated 2026-09-20. Public upstream branches: `main` (`86a3a4f…`), `imitater` (`3cdd043…`), `staging` (`95a0f75…`). Detailed bodies, comments, event history, related commits, PR reviews/diff, and branch differences will be examined before selecting implementation scope. No application code has been modified.

### Verification plan

Map the unbundled browser game's entry points, plant/zombie lifecycle, scheduler, and level registration; trace the actual Soak-shroom path and analogous implementations before proposing a fix. Seek a pre-fix reproduction using the real implementation, add a minimal dependency-free regression test if meaningful, and run non-destructive static checks. Attempt an Arena browser preview and actual browser automation if available. Label source reasoning, isolated automated tests, browser/runtime observations, and unverified behavior separately. Never treat suppressed errors, a green commit, or an unexercised preview as runtime success.

## 2026-10-17 — Complete issue review, selected scope, and pre-fix evidence

### Upstream discussion, history, PR, and branch review

Read each of the six open issues' full body, all paginated comments, labels, and timeline events through the REST API. Reviewed PR #51's metadata, all three commits, all nine changed files (including the source patches), discussion, inline comments, and reviews (no inline comments or reviews). Compared all public branches with `main`, searched related issue/PR titles and path histories, and inspected the relevant source. API responses and temporary tools are saved outside this repository under `/home/user/game-investigation`, not staged.

Several issues were briefly connected to and closed by [PR #76](https://github.com/Plants-vs-Zombies-MODDED/game/pull/76), then disconnected and reopened on 2026-09-22. Inspection of that merged PR's commits, files, discussion, and reviews shows **issue-intake templates only, no game fixes**. Those historical closures do not establish resolution of any gameplay issue. `staging` is 28 commits ahead / 131 behind `main`, with no Soak-shroom, Gargantuar, store, or balance implementation in its unique file changes. `imitater` is 3 ahead / 114 behind `main` and is the head of the sole open draft PR.

### Full inventory and disposition

| Issue | Classification | Concrete evidence and disposition |
| --- | --- | --- |
| [#61 — Soak-shroom](https://github.com/Plants-vs-Zombies-MODDED/game/issues/61) | **ACTIONABLE** (overlaps an unmerged draft; see scope rationale below) | Empty body and no comments. Actual `oGun.NormalAttack` assigns undeclared `j` and `d` in an ES module. A real browser reproduction now demonstrates `ReferenceError: j is not defined` on the normal zombie-trigger path, no projectile, and `canTrigger` stuck at 0. Implement only the local-binding correction and meaningful regression coverage. Do not claim every possible interpretation of “crashing the game” is covered. |
| [#53 — late-game balance](https://github.com/Plants-vs-Zombies-MODDED/game/issues/53) | **BLOCKED / AMBIGUOUS** | Body lists only level 4-4; no discussion, success metric, wave target, or agreed change. Reviewed `game/level/34.js` (125 starting sun, 30 flags, configured zombie pool/waves). PR #51 commit `3cdd043…` proposes 175 starting sun but explicitly calls its balance change temporary and undecided. Referenced #52 returns HTTP 404; no reason for its unavailability is assumed. The other search match, closed #15, is an empty bug-report template with no comments. Do not alter sun, waves, or difficulty. Unblock with maintainer-approved balance criteria and playtest expectations. |
| [#25 — Gargantuar](https://github.com/Plants-vs-Zombies-MODDED/game/issues/25) | **BLOCKED / AMBIGUOUS** | Reporter found assets, not gameplay. Maintainer comments say it is being worked on and reopen it to track progress. `game/js/CZombie.js` contains an unregistered `oGargantuar` prototype, but no levels select it; there is no complete throw-Imp/smash state machine. [Commit `b967971…`](https://github.com/Plants-vs-Zombies-MODDED/game/commit/b967971cac232f2a28bb6b68a252754328879c2e) explicitly labels it “unfinished garg” and adds the assets. Several prototype image paths do not match supplied assets. No distinct active public PR/branch completing it was found. Do not treat a prototype or unused sprites as finished requirements. Unblock with agreed mechanics, timing, asset mapping, and level/almanac integration; coordinate with existing upstream work. |
| [#50 — Imitater](https://github.com/Plants-vs-Zombies-MODDED/game/issues/50) | **IN PROGRESS / DUPLICATE** | Empty issue body; [draft PR #51](https://github.com/Plants-vs-Zombies-MODDED/game/pull/51) is still open, unmerged, and explicitly linked to #50. Its `oImitater.js` spins and hard-codes a Peashooter transformation, with placeholder description text. `main` has no Imitater registration. Do not copy, complete, or redesign this feature in this PR. Continue in the existing upstream work with its intended selection/transformation contract clarified. |
| [#9 — Crazy Dave's Store](https://github.com/Plants-vs-Zombies-MODDED/game/issues/9) | **BLOCKED / AMBIGUOUS** | Empty issue body. Substantive maintainer discussion says the store “might be possible”; later discussion has no behavior specification. History includes conversion to discussion and reopening. `StorageUtil.js` persists values, but the displayed coin bank is hard-coded to `$999,990`, zombie death has a `todo: coins drop`, and no purchase/store flow was found in menus or runtime. A currency economy, stock/prices, unlocks, persistence, and UI behavior would have to be invented. Do not implement. Unblock with an explicit product contract for those behaviors. |
| [#65 — modern rewrite](https://github.com/Plants-vs-Zombies-MODDED/game/issues/65) | **NOT APPROPRIATE FOR THIS PR** | Body explicitly says “Rewriting in private repo”; comments are browser-support banter, not an implementation or compatibility specification. No accessible rewrite contract is available. Do not attempt private access, choose a framework, or rewrite this legacy game. |

**Selected coherent scope: #61 only.** This is not a new discovery of the two-variable correction: upstream draft PR #51 already includes the same functional correction in [commit `3cdd043c4ba15b2960b5f35968743cb6590e655b`](https://github.com/Plants-vs-Zombies-MODDED/game/commit/3cdd043c4ba15b2960b5f35968743cb6590e655b), alongside an unfinished Imitater and provisional balance changes. The distinct justification for this fork PR is a minimal, independently reproduced and regression-tested crash fix against current `main`, without importing unrelated draft feature/balance work or its formatting. Explicitly acknowledge the overlap in the PR; do not claim original authorship of the discovery or duplicate #50. No product-design answer is required for this isolated bug correction, so blocked feature work will not delay it.

### Architecture and root cause (recorded before implementation)

- `game/index.html` loads classic `Cfunction.js` (engine, globals, DOM helpers, base `CPlants`), classic zombie definitions, and `CPlants.js` with `type="module"`. The latter imports individual plant ES modules and exposes constructors on `window`. `oGun` (Soak-shroom) inherits `oPuffShroom` → `oFumeShroom` → `CPlants`; ES-module method bodies execute in strict mode.
- Level 4-3 awards Soak-shroom; level 4-4 (`game/level/34.js`) and later undersea levels offer it. Base `Birth` builds the shadow/body DOM, registers `$P` and the grid, sets trigger ranges, and calls `oGun.PrivateBirth` to create an unattached bullet template. `BirthStyle` keeps this plant awake rather than using Fume-shroom's day-sleep branch.
- Normal gameplay: `oP.Monitor` / `oZ.traversalOf` → `oT.chkD0` → `CPlants.TriggerCheck` (sets `canTrigger = 0`) → `CheckLoop` → `oGun.NormalAttack`. The latter plays a sound, then assigns `j = k.id` and `d = $(j)` without any declaration. With absent globals, the first assignment throws **before cloning a projectile, starting the attack animation, or scheduling the 140-tick attack recheck**. If globals happen to exist, they can instead be overwritten. `d` is independently undeclared too; declaring only `j` would be incomplete.
- `oSym` catches and logs task exceptions rather than propagating them to the browser process. The observed defect is an interrupted attack/traversal callback and disabled attack loop, not evidence that the entire browser process crashes.
- Adjacent implementations (`oPuffShroom`, `oFumeShroom`, `oSeaAnemone`) use local DOM/plant identifiers. Keep Soak-shroom's existing movement, 20-damage collision, short trigger range, sound, and animation behavior unchanged.
- Lifecycle investigation found existing safeguards: every sprite frame checks the DOM in `ImgSpriter`; the final animation callback checks `$P[n]`; `Die`/`BoomDie` unregister plants and clear their element/template; the inherited delayed recheck looks up `$P` and `$Z` afresh; projectile movement queries `oZ.getZ0` each tick instead of retaining a target object. No separate delayed-callback defect has yet been demonstrated. Exercise these paths in regression verification rather than adding speculative guards.
- **Planned minimal correction:** initialize function-local `j` and `d` and make the existing bullet `EditEle` call a standalone statement. No level configuration, assets, tooltip translation, scheduler, or base lifecycle changes.

### Verification already executed (baseline, before the fix)

1. Non-mutating `oxlint@1.88.0 .`: **exit 1; 29 warnings, 444 errors across 339 files**. Saved the actual output externally. Scoped `oxlint game/js/plants/oGun.js`: **exit 1; 0 warnings, 2 existing `no-nested-ternary` errors**. Do not report a clean lint baseline or fix unrelated lint debt.
2. Started normal static game preview with `python3 -m http.server 8000 --bind 0.0.0.0`. No server-side host/origin allowlist or frame header blocks the Arena proxy; repository code is unchanged.
3. Installed temporary, pinned tooling outside the repository: `playwright-core@1.64.1`, `@sparticuz/chromium@153.0.0`, `esbuild@0.28.2`, `oxlint@1.88.0`, and `oxfmt@0.73.0`. Initial browser launch failed (missing `libnspr4.so` / NSS libraries). Extracting the Chromium package's bundled AL2023 libraries and setting the child-process library path fixed that environment problem; no OS/repository dependencies were added.
4. **Actual pre-fix browser runtime reproduction:** headless Chromium **153.0.8010.0** loaded the unmodified `/game/`, clicked **Continue Here** and **PLAY!**, loaded level 4-4, chose Soak-shroom and pressed **Start** through the real UI. The local domain is recognized by the game's existing demo gate; for this test page only, added level 34 to its in-memory whitelist before calling the real `SelectModal(34)`. No source/notice/access behavior was changed. Used existing `CustomSpecial(oGun, 3, 3)` and `createZombie(oZombie, 3, 5)` APIs to obtain a deterministic encounter, with the normal scheduler and zombie monitor running.
5. At tick 744, both `j` and `d` were undefined. By tick 755, the normal trigger path logged `ReferenceError: j is not defined` at `oGun.js:57` through `CheckLoop`, `TriggerCheck`, `chkD0`, `processRow`, and `runTasks`. The live plant had `canTrigger: 0`, its idle sprite, **0 projectile elements**, and a live target with **270 HP**. Evidence JSON and screenshot are external scratch files, not fabricated test output. This is a controlled runtime encounter, not an unassisted campaign playthrough.
6. Existing baseline runtime noise is also recorded: invalid meta `X-Frame-Options`, blocked external Turnstile/favicon requests, seven local missing image/audio URLs, and an audio `play()`/`pause()` promise rejection while changing levels. These are not hidden, fixed, or attributed to the pending patch. No claim of an error-free whole game is warranted.

Next: add focused dependency-free regression coverage, demonstrate failure on the old source, implement the binding correction, rerun tests and controlled real-browser lifecycle/adjacent cases, and inspect a non-mutating bundle outside the source tree. Full mutating `scripts/build.sh` remains unexecuted.

## 2026-10-17 — Regression tests fail on the original application source

Added `npm test` using Node's built-in test runner and `vm.SourceTextModule` (`--experimental-vm-modules`), with **no added dependencies or framework**. The VM module API is explicitly experimental in Node 22 and emits its normal warning. Tests load the actual complete `Cfunction.js`, `CZombie.js`, and imported plant modules, preserving ES-module strictness and real inheritance, triggers, projectile callbacks, damage, animation callbacks, and death bookkeeping. Only DOM primitives/audio are minimal unit-test fixtures; the queue is drained deterministically rather than running browser timers. These isolated tests are not a substitute for, or a claim of, actual browser rendering/gameplay verification.

Before editing `oGun.js`, ran `npm test`: **exit 1, 13 tests, 2 pass / 11 fail / 0 skipped**. Failures demonstrate (a) `j is not defined` when no binding exists, (b) independently `d is not defined` if only `j` exists, and (c) global `j` overwritten if both globals exist. The neighboring Puff-shroom test and non-attacking Soak-shroom birth/range test pass against the baseline. The remaining Soak-shroom attack/lifecycle tests fail at the same original undeclared assignment, not at a fabricated expected failure. This confirms both the defect and that the test harness can run unaffected real game behavior.

Test scope: absent/pre-existing global binding combinations, projectile reveal/miss/hit/cleanup, unchanged 20-damage repeated firing through real triggers, animation reset, plant death at ticks 0 and 107 (immediately before the final callback), target death before impact, concurrent plants, altitude filtering, adjacent Puff-shroom behavior, and awake/short-range behavior. Proceed with the previously documented local-binding correction only.

## 2026-10-17 — Implemented correction and completed verification

### Implementation

Changed only `oGun.NormalAttack` in application code: `var j = k.id; var d = $(j);` precede the same projectile `EditEle` call. The change prevents strict-mode assignment failures and accidental writes to pre-existing globals. Attack targets, timing, animation sequence, inherited recheck, damage, range, sound, costs, assets, and levels are unchanged. Functional overlap with upstream draft PR #51 is intentional and explicitly acknowledged above; its Imitater, tooltip, formatting, and balance edits are not imported.

Files intended for this PR: `game/js/plants/oGun.js`, `tests/soak-shroom.test.mjs`, the single `package.json` test-script addition, and this append-only `DESIGN_LOG.md`. No dependency, lockfile, image/audio, stamp, generated bundle/map, CI, or level changes are needed.

### Automated source-execution and static checks

| Command / check actually executed | Outcome |
| --- | --- |
| `npm test` on patched working source (Node 22.22.3) | **Exit 0: 13 tests pass, 0 fail, 0 skipped.** Node emits the expected experimental VM-modules warning. These are isolated source-execution tests with minimal DOM/audio fixtures and deterministic task draining, not a browser simulation presented as real gameplay. |
| Final unchanged test file run against source from `git archive HEAD game/js` in `/home/user/game-investigation/baseline`, via `node --experimental-vm-modules --test tests/soak-shroom.test.mjs` | **Exit 1: 2 pass, 11 fail, 0 skipped**, confirming the final regression tests detect the original bug without reverting or modifying the implementation working copy. |
| `node --check game/js/plants/oGun.js`; `node --check tests/soak-shroom.test.mjs` | Both **exit 0**. |
| `node --check` for every tracked `.js` plus the new `.mjs` test | **342 files checked, 0 failures.** This checks syntax only, not behavioral correctness. |
| `git diff --check` | **Exit 0**. |
| `oxfmt@0.73.0 --check game/js/plants/oGun.js tests/soak-shroom.test.mjs package.json` | **Exit 0: all three files already formatted.** No formatter writes were used. Historical design-log entries were not reformatted. |
| `oxlint@1.88.0 tests/soak-shroom.test.mjs` | **Exit 0: 0 warnings, 0 errors.** |
| `oxlint@1.88.0 .` after patch | **Exit 1: 29 warnings, 444 errors on 340 files** (baseline: the same 29 warnings / 444 errors on 339 files). This is **not** a passing full-repository lint. |
| `oxlint@1.88.0 game/js/plants/oGun.js` after patch | **Exit 1: 0 warnings, 2 pre-existing nested-ternary errors**, unchanged from baseline. No unrelated lint cleanup or suppression was added. |
| `esbuild@0.28.2 game/js/CPlants.js --bundle --minify --sourcemap --outfile=/home/user/game-investigation/bundle/CPlants.js` | **Exit 0**; 181.5 KB bundle and 460.2 KB map written outside Git. Four pre-existing duplicate-key warnings (`oLaserBean.SunNum`, `oTTS.beAttackedPointR`, `oTenManNut.Stature`, `oSeaAnemone.AudioArr`). |
| Same targeted esbuild command on the isolated starting-HEAD source | **Exit 0, same four duplicate-key warnings.** It also produces a baseline bundle for the real-browser red check below. |

### Actual browser/runtime verification

Used Playwright-core with real headless Chromium **153.0.8010.0** against the normal static game served on port 8000. Browser security disabling was removed from the Chromium package's default flags; only local game resources were allowed in the automation. External services were deliberately not exercised. The preview remains bound to `0.0.0.0`; no browser-facing application URL was changed to `localhost`.

**Reproduction setup:** click the existing **Continue Here** and **PLAY!** UI, wait for resource readiness, add level 34 only to that test page's in-memory demo whitelist, call `SelectModal(34)`, choose Soak-shroom, and click **Start**. The post-fix checks also click the actual seed packet and lawn tile to plant it: starting sun falls from 125 to 100, unchanged. A zombie is introduced through existing game APIs for a deterministic encounter; the normal zombie monitor, real DOM, real collision/damage code, and real scheduler/timers execute the attack. This is actual controlled gameplay, **not** a manual campaign completion or deployed-site verification.

Ran the following external verification scripts (temporary testing tools, not application dependencies):

- `node /home/user/game-investigation/reproduce-soak.mjs` before editing source: captured the original `ReferenceError`, 0 projectiles, 270 target HP, and disabled trigger, as recorded earlier.
- `node /home/user/game-investigation/reproduce-soak.mjs bundle` after serving the **isolated starting-HEAD bundle** in place of `CPlants.js`: reproduced the same `ReferenceError: j is not defined` from the minified bundle; target remained at 270 HP with 0 projectiles. Thus bundling does not remove the demonstrated defect. This capture script exits 0 after recording the expected failure; its exit code is not evidence of successful gameplay.
- `node /home/user/game-investigation/verify-soak.mjs source`: **exit 0, PASS** on patched ES modules.
- `node /home/user/game-investigation/verify-soak.mjs bundle`: **exit 0, PASS** on the patched targeted bundle, served by the test browser's resource route while the rest of the actual game is unchanged.

Observed normal-play results:

| Observation | Patched source | Patched bundle |
| --- | --- | --- |
| UI planting, unchanged cost | Row 3/column 3; sun 100, tick 734 | Row 3/column 3; sun 100, tick 900 |
| First collision | 270 → **250 HP**, shooting sprite, tick 775 | 270 → **250 HP**, shooting sprite, tick 938 |
| Idle animation restored | Idle sprite, 250 HP, tick 851 | Idle sprite, 250 HP, tick 1014 |
| Repeated automatic firing | **210 HP** after three hits, tick 1045 | **210 HP** after three hits, tick 1210 |
| Global `j` / `d` after repeated attacks | Both still undefined | Both still undefined |

For independent lifecycle cases, the browser harness then clears level/wave timers, initializes the actual no-wave `oP` configuration, and restarts its normal zombie monitor. This deliberately isolates the bug mechanics from balance/random-wave behavior; it does not mock the methods being tested. All **seven controlled engine cases passed in both source and bundle runs**:

1. Existing `window.j`/`window.d` sentinel objects remain unchanged; projectile hits for 20, is removed, and the plant returns to idle.
2. Plant dies immediately after launch: `$P`/DOM/template removed, already launched projectile still hits for 20 and is removed.
3. Plant dies at animation tick +107, just before the final callback: no invalid-element exception; target is at 250 HP; no projectile remains.
4. Target dies before impact at tick +5: no stale-target error; projectile exits; delayed recheck restores `canTrigger` to 1.
5. Actual adjacent Puff-shroom still hits for 20 and cleans up its projectile.
6. Two Soak-shrooms attack in separate rows; one is removed mid-animation; both launched projectiles hit for 20 and the surviving plant returns to its own idle sprite.
7. A missed projectile leaves the board and is removed.

Both successful runs recorded **zero console/page errors during the exercised gameplay/engine phases**. Startup still shows the previously recorded invalid meta header, blocked external resource, seven missing local assets, and audio promise rejection. This limited clean gameplay phase is not a claim that all startup errors, assets, levels, browsers, or devices are fixed.

**Verification failure retained, not hidden:** an initial post-fix browser checker reached the real first 20-damage hit, but then failed its own assertion after taking a screenshot while the game kept running: it incorrectly required the target to remain at exactly first-hit HP until checking the idle frame, eventually dereferencing the plant after it had been eaten. The failed attempt's JSON/screenshot were retained externally. Changed only that external checker to observe the live idle frame without the stale-HP assumption and pause before screenshot capture. No application or regression-test changes were made in response. The corrected source and bundle runs above passed independently.

### Build isolation, limitations, and remaining work

- **Never ran `npm run build` / `scripts/build.sh`, `npm run fix`, or a whole-repository formatter.** Only the meaningful non-mutating plant bundle stage was executed, with output paths outside the source tree. Full HTML/CSS/other-JS minification, global tool installation, stamping, and the deployment pipeline are not verified by that targeted bundle check.
- Baseline source copies, bundles/maps, browser binaries/libraries, npm tooling, API captures, runtime JSON/screenshots, and test/lint output are external scratch artifacts. None is part of the planned Git diff. No build-produced source/asset changes needed reverting or discarding.
- Remaining upstream issues retain the classifications above. No store, Gargantuar, Imitater, rewrite, balance, asset repair, or unrelated lint changes were implemented. Runtime startup/asset problems remain separately documented; do not claim broad game correctness or close upstream issues.
- Not verified: full campaign/balance, human playtesting, mobile/other browsers, external online services, and production deployment. Verified: the concrete strict-mode attack defect, the native regression cases, the targeted bundle, and the described real Chromium paths.
- Next delivery steps: final issue/baseline/diff recheck, commit the four scoped files, push only `origin arena/8dbc54a1-game`, and open one PR on `coderunknow/game:main`. Leave it open and report any delivery or CI failure honestly.

## 2026-10-17 — Final review before delivery

Fresh GitHub recheck still shows exactly the same six open upstream issues and sole open draft PR #51, with unchanged update timestamps; fork `main` remains at `86a3a4f0946368d073ae36cab512b70d1a03297f`. The fork has no existing PRs. Reconfirmed checked-out `arena/8dbc54a1-game`, fork-only push URL, configured author identity, and all remaining PR #51 registration/debugging patches. Staged exactly the four intended files (all regular `100644` files): one application function, one test script, one native test file, and this log. `git diff --cached --check` passes; no unstaged implementation changes or asset/generated files are present. Final `npm test` rerun again exits 0 with **13 pass / 0 fail / 0 skipped**. Implementation and investigation are complete within the selected #61 scope; commit/push/PR delivery remains to be confirmed by the next entry.
