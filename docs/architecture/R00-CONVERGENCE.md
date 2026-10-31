# R00 requirements convergence and execution baseline

Reviewed 2026-10-31 against main `e6bb5b1ad0650b3a73efcb5e12cba6913bd34b78`.
The original initialization, Qt6 and Win7 prevalidation records are historical
evidence and remain unchanged. Current platform requirements are governed by
the PRD and ADR-0002, which supersede the older Qt6-only policy.

| Requirements | Owning service / adapter | Implementation goal |
| --- | --- | --- |
| FR-01, FR-04 | Capture service / Win32 hotkeys, Qt tray and settings | D01 |
| FR-02, FR-05 | Physical rectangle model / GDI and region editor | D01, D02 |
| FR-03 | Artifact store / image encoder, exclusive file creation | D01 |
| FR-06, FR-07 | Scene model / Qt renderer and Graphics View editor | D03 |
| FR-08, FR-09 | Report planner and durable queue / isolated Word STA process | D04 |
| FR-10, FR-11 | Catalog and execution-event context service | D05 |
| FR-12 | Typed facade / per-user local IPC and CLI | D06 |
| FR-13 | Qt-neutral domain, capture, scene, context and report libraries | D00, D06 |
| FR-14 | Deployment tools and native qualification receipts | D07, L00–L02 |

Core value types and ports must compile without Qt, HWND or COM. Pixel buffers
cross the boundary as owned bytes and dimensions; UI previews are adapters.
CaptureProfile holds physical desktop input rectangles. SceneTemplate holds
derived output geometry; neither edits the other. A CaptureBatch pins profile,
scene and context revisions and records actual acquisition times per display.
Source acquisition, save, clipboard and report outcomes remain independent.
Word failures cannot turn acquired images into failed acquisitions or cause
originals to be modified. Context can receive execution descriptions but has
no command-transmission port.

Public data is fictional. Apache-2.0 covers project sources, not redistributed
Qt/CRT components. DLL/license provenance belongs to D07; no SDK binaries enter
Git. The existing official SDKs and vcpkg are reused without package installs.
Native Win7 x64/x86, v142, Word2010, Win10 and human M1–M5 acceptance remain
unproven. Linux is outside execution scope.

## Executable planning verification and recovery

`python tools/validate_plan.py` verifies DAG, schema meta-validity, fixture
shapes and referenced files; semantic runtime tests follow in D00/R03.
It requires the already installed jsonschema module and performs no downloads.

`python tools/goal_train.py start` resumes the existing ignored ledger in
`out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json`; it never resets accumulated time.
Checkpoint before and after each goal, PR merge, expensive check and context
handoff. Select a goal with `goal --goal D00 --status ACTIVE`; add disposition,
PR and merge SHA on completion. `stop` closes an execution segment. If the
client crashes with an open segment, the next invocation conservatively charges
the entire gap rather than silently dropping time. Normal interruptions can be
closed with `stop` and resumed with `start`. All waits, repairs and tests within
an active segment count once. Per-goal and total limits produce explicit stops.
The startup interval is separately accounted as a conservative allowance.

R00 source review and planning checks do not validate the desktop product.
No Qt build is needed for this documentation/accounting change.
