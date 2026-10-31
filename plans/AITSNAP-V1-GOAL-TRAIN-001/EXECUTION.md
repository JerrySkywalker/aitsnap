# Local Codex execution runbook — cumulative 120h cap

## Startup
Open the user-authorized checkout of JerrySkywalker/aitsnap on ZenBook Duo using ChatGPT desktop/Codex. Do not hardcode host paths in public artifacts. Fetch main, inspect the current worktree and existing open PRs; never overwrite unrelated changes. Read OWNER-HANDOFF, PROGRAM, ROADMAP, DEPENDENCIES, ACCEPTANCE, GOAL-MANIFEST, goal pages, docs/requirements and existing Win7 prevalidation.

## Ledger
Create an ignored local ledger under the checkout out/ with program ID, start and last-resume timestamps, aggregate elapsed hours, per-goal usage, PR/merge SHAs, last clean main and holds. Retain cumulative consumption across session/context restart; do not reset the 120h budget. A client may stop before 120 hours; run in resumable segments without promising uninterrupted work.

## Work loop
1. Select highest-value eligible goal under DAG/per-goal budgets; favor R00/R01/D00/D01 to get M1, run R02/R03 as independent research.
2. Perform minimum justified research/implementation, synthetic unit/integration tests and native tests when authorized. Reuse local official Qt kits, existing VCPKG_ROOT and current Windows build presets. Avoid unnecessary CI and unrelated tool installs.
3. Open a scoped PR, inspect diff/test receipts and protected branch rules. Merge only if verified and allowed; reconcile local/main and preserve unrelated worktrees.
4. Record source-test PASS separate from manual visual/Word2010/Win7 native OWNER_HOLD/BLOCKED. Continue independent eligible goals without routine mid-run questions.
5. Checkpoint after each Goal. Stop when cumulative elapsed 120h, no safe eligible tasks, or an authorization/safety gate; return honest consolidated receipt.

## Never
Do not bypass Hyper-V/OS access restrictions, download unlicensed legacy OS/Office media, run any real satellite command, publish sensitive records or private paths, fabricate benchmark numbers or Win7 acceptance, force-merge, or delete unrelated user data.
