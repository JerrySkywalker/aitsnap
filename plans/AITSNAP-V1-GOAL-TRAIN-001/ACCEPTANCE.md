# Acceptance gates and receipts

Primary matrix: docs/requirements/ACCEPTANCE-MATRIX.md.

**Goal PASS** requires files and specific executed checks with genuine evidence. **PARTIAL** = incomplete but retained source; **BLOCKED** = missing environment/authorization; **OWNER_HOLD** = human-visible test not performed; **NOT_STARTED/ACTIVE** for planning/running. These are not interchangeable.

**M1 (D01)**: human tests one-key multiple source areas, save/copy, tray action and no focus theft. **M2 (D03)**: custom layout rerender, raw originals untouched. **M3 (D04)**: Word synthetic report append/reopen, ordering, no duplicate retry. **M4 (D06)**: synthetic sequence event auto-selects scene and report, command transmitter absent. **M5 (D07+L02)**: native Win7 x64+x86, Win10/11 as claimed, offline install and human signoff.

PRs must be scoped, diff-reviewed, targeted-tested, compliant with branch protection, merged only when eligible. Preserve earlier failed probe records and distinguish reused tests from newly executed. No force pushes, bypassing required reviews or writing real AIT information to public repo.

**Receipt fields:** GOAL_ID, BASE_SHA, DISPOSITION, PR, MERGE_SHA, CHANGED_FILES, EXECUTED_TESTS, REUSED_TESTS, NOT_RUN_TESTS, HOST_OS/ARCH, QT, COMPILER, ELAPSED_H, TOTAL_REMAINING_H, MILESTONE, WIN7_NATIVE_STATUS, OWNER_HOLD_REASON, NEXT_ELIGIBLE_GOAL.

A modern preview may advance with honest limitations. Full v1 release is BLOCKED without L02/Word2010 native evidence and required visual signoffs.
