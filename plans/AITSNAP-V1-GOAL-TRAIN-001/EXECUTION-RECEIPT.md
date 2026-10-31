# Execution receipt — local preview delivered, full v1 remains HOLD

PROGRAM_ID=AITSNAP-V1-GOAL-TRAIN-001. DISPOSITION=PARTIAL_PREVIEW_WITH_EXTERNAL_HOLDS. Start SHA e6bb5b1ad0650b3a73efcb5e12cba6913bd34b78; last product merge6260114ca000e53c15baf666d56a6f0785ee66d7 (D07). The exact final documentation/main SHA, package hashes and stopped budget are written after this receipt merges to ignored `out/AITSNAP-V1-GOAL-TRAIN-001/final-receipt.json` and `FINAL-RECEIPT.md`; a tracked document cannot contain its own future merge hash.

The initial worktree was clean. Main was fast-forwarded from56804c50 to the authoritative start SHA before executing the handoff. All scoped changes used codex branches and normal PR squash merges with expected head verification. No branch-protection bypass, force push, unrelated worktree overwrite, dependency/OS/Office install or real AIT command execution occurred.

## Goal and PR results

| Goal | Disposition / evidence | Merged PR |
| --- | --- | --- |
| R00 | PASS: requirement trace, architecture boundaries, DAG/schema validation | [4](https://github.com/JerrySkywalker/aitsnap/pull/4) |
| R01 | PASS: native pattern/crop geometry, scene feasibility, benchmark protocol | [5](https://github.com/JerrySkywalker/aitsnap/pull/5) |
| D00 | PASS: Qt-neutral C++ targets/domain, deterministic tests on three host kits | [6](https://github.com/JerrySkywalker/aitsnap/pull/6) |
| D01 | PASS software/native samples: five-region save/copy, hotkeys/tray, configuration and baseline | [7](https://github.com/JerrySkywalker/aitsnap/pull/7) |
| D02 | PASS software: physical region editor, lifecycle/capture guard and stable profile persistence | [8](https://github.com/JerrySkywalker/aitsnap/pull/8) |
| R02 | PASS research/probe: native Word16 small save/reopen; Word2010 separate | [9](https://github.com/JerrySkywalker/aitsnap/pull/9) |
| R03 | PASS: versioned event/catalog/context contracts and replay semantics | [10](https://github.com/JerrySkywalker/aitsnap/pull/10) |
| R04 | PASS: architecture/contract source freeze with explicit external gates | [11](https://github.com/JerrySkywalker/aitsnap/pull/11) |
| D03 | PASS software: free scene editor, immutable originals, revisioned multi-scene/history render; native lock guard and unlocked regression | [12](https://github.com/JerrySkywalker/aitsnap/pull/12) |
| D04 | PASS software/modern Word: durable queue, exact target, commit/replay/PREPARED recovery, mixed tables | [13](https://github.com/JerrySkywalker/aitsnap/pull/13) |
| D05 | PASS: JSON/CSV snapshots, redacted event store, explicit exact/ambiguous/unlinked; additive eventv2, v1 unchanged | [14](https://github.com/JerrySkywalker/aitsnap/pull/14) |
| D06 | PASS software/native synthetic chain: logon ACL/CLI, durable idempotency, events→capture/scene/clipboard/Word | [15](https://github.com/JerrySkywalker/aitsnap/pull/15) |
| L00 | BLOCKED: actual v142 MSB8020, current Win11 guard rejects native claim, VM enumeration denied | [16](https://github.com/JerrySkywalker/aitsnap/pull/16) documents hold |
| D07 | PARTIAL overall: Win11 offline preview, measurements, operator/install/review pack PASS; external gates open | [17](https://github.com/JerrySkywalker/aitsnap/pull/17) |
| L01 | BLOCKED: L00 unqualified, no native Win7 x64/x86 screenshot/tray/editor acceptance | This closeout's native-hold receipt |
| L02 | BLOCKED: L01 plus native Word2010/full Win7/humanM5 absent | This closeout's native-hold receipt |

GOALS_PASS=R00–R04,D00–D06 (12). GOALS_PARTIAL=D07 (1). GOALS_BLOCKED=L00,L01,L02 (3). Goal PASS means its recorded software/research checks, never automatic human/native release acceptance. Each goal's detailed changed scope, test/reuse/failure notes remain in docs/architecture/*-RECEIPT.md and research notes. Raw measurements and the append-only history remain ignored locally.

## Milestones and acceptance

| Milestone | Automated evidence | Actual acceptance |
| --- | --- | --- |
| M1 | Real multi-display pattern pixels, selected clipboard, dispatch/focus/editor lifecycle, usable tray application | OWNER_HOLD for physical keys/tray interaction, Explorer recovery and manual topology checks |
| M2 | Two layouts, offscreen golden geometry, transforms, persistence/history rerender and unchanged source hashes; Qt5/6 builds | OWNER_VISUAL_HOLD for full pointer/canvas experience |
| M3 | Native Word16 embedded images, ordering/table/reopen, bounded queue, replay without duplicates, PREPARED remainsUNCERTAIN | Word2010/large document and human layout HOLD |
| M4 | Four-display synthetic runs/retry/out-of-order/duplicate events, source/run/step/attempt linkage, scene+Word; restart and immediate exit | OWNER_HOLD; actual cross-account/remote-host native refusal not exercised |
| M5 | Local offline copy/install/relocation/uninstall, exact DLL paths/hashes, synthetic stress and review pack | BLOCKED / OWNER_HOLD; native Win7/Word2010/Win10 clean target and signoff absent |

WIN7_X64_NATIVE=HOLD. WIN7_X86_NATIVE=HOLD. WORD2010=HOLD. Qt5x86 on WOW64 and modern Word16 are not replacements. User explicitly unlocked the desktop; later native samples passed. A later agent UI inspection successfully opened the current preview's free scene editor, dragged a layer (X/Y86.618/66.441) and undid it (16/16), then cancelled the test scene. This limited agent check does not provide Owner acceptance. Earlier input guards, locked samples and failures remain preserved.

## Verification summary

The final shared-source local matrix built Qt6.12x64 and Qt5.15.2x64/x86 using existing v143;7/7 component CTests passed per kit. Changed desktop targets reran after actual changes, and application-only final smoke changes compiled/ran on all three. No unchanged full CI was rerun. Same-session diff review and normal merge eligibility checks were performed; independent read-only acceptance was not claimed.

D07 stress saved620images from20single+100five-source requests. Capture+scene P50/P95 was461.36/526.94ms for the measured sizes; scene-only4.00/5.27ms; capture peak184049664bytes, GDI4→4. Ten six-picture Word jobs committed. Installed native flow and independent schema/hash/XML readback verified123batches/638artifact hashes/13jobs,60/12Word drawings and10/2markers. Corrected Word peak memory, idle sample, installed DLL paths and exact scope are in docs/architecture/D07-PREVIEW-RECEIPT.md and docs/preview/TEST-MATRIX.md.

Existing initial-validation.md, qt6-baseline-validation.md and win7-prevalidation.md remain historical records and are checked unchanged at finalization. Failed probes, LOCK evidence, queue failure, invalid event fixtures, UNCERTAIN Word path cases, configure failures and wrapper mistakes remain; successful later regressions do not erase them.

## Delivery, remaining route and budget

Local final package target: `out/AITSnap-0.5.0-preview-win11-x64/`, with `Launch-Preview.ps1`, application/CLI/Word helper, operator/owner review docs, public synthetic demo, licenses/SBOMs and complete source/file/DLL hash manifests. A new-folder install and preservation-aware uninstall are available. No public binary release/tag is created. A fresh final package/readback follows the clean main closeout merge, reusing unchanged tested product code.

The hard cap is120cumulative execution hours across sessions/waits/repairs/tests, not a promise to consume120hours. Approximately7.0hours had been measured at this receipt's preparation; the exact stopped total and remaining hours are recorded in the final local ledger/receipt, without resetting start. The initial240-second allowance and conservative active gaps are included. Per-goal consumption is retained.

Stop condition: once final clean-main package/provenance/readback and budget checkpoint finish, no safe eligible development goal remains under the existing environment. Outstanding native/platform/human gates require qualified external state; repeating CI or inventing unsupported PASS would not advance them. This is a partial program delivery, not completed full v1 acceptance.

NEXT_OWNER_ACTION: experience M1/M2 using the isolated launcher and human checklist; supply legitimately qualified Win7SP1x64/native x86/v142/CRT targets and licensed Word2010 for L00→L01→L02, plus Win10/clean-machine and M5 review. Resume from the same ignored ledger and exact final main; never restart the120-hour clock. Ordinary technical choices do not need new approval, but missing external authority/evidence cannot be fabricated.
