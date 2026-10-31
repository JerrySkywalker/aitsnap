# Acceptance matrix and disposition policy

Do not conflate source-only PASS, desktop launch PASS, visual acceptance, and native Windows 7 qualification. Baseline facts are in ../win7-prevalidation.md and ../windows-compatibility.md.

| Gate | Tests / real-world demonstration | Owner | Blocking result |
| --- | --- | --- | --- |
| A0: architecture | Modules/contracts consistent, public synthetic data, 16 goals traceable, docs compile conceptually | R04 | contradictions / absent provenance |
| M1 / D01 | On an actual multi-monitor workstation, one hotkey saves several exact regions, second copies one requested image, tray switches profile, foreground test window retains focus | developer + human | screenshots inaccurate or focus stolen |
| M2 / D03 | Move/resize/crop/align sources; same template reliably recomposes new batch; original region pixels preserved and rerender from historical batch | developer + human | source corruption, mismatched layout |
| M3 / D04 | With Word open, repeated captures append correctly ordered embedded images/captions to a synthetic test report; Word busy/unavailable leaves retriable tasks, no duplicate inserts | developer + human | loss/duplication or corruption |
| M4 / D06 | Synthetic sequence: repeated run + retry + event replay -> correct run/step/attempt links, scene chosen, batch and Word record; no command execution | developer + human | incorrect provenance / unintended action |
| M5 / D07 + L02 | Offline install/uninstall, DLL origin, DPI, multi-screen, 24-hour operational observation or justified reduced evidence, Word 2010 and native Win7 x64/x86 results separately | release owner | Win7 lack of native evidence blocks full v1 |

## Platform matrix
- Windows 11 Qt 6.12 x64: native synthetic capture/scene/Word16/IPC and local offline deployment PASS within the recorded scopes; human M1–M5 acceptance HOLD. See the D01–D07 receipts and preview matrix.
- Windows 10 1809+ Qt 6.12 x64: requirement, actual deployment UNTESTED.
- Win7 SP1 Qt 5.15 x64/x86: requirement; v143 host-preview Qt5 builds PASS on Win11, v142 absent, native Win7 NOT TESTED. WOW64 is not x86 OS evidence.
- Linux: no release requirement.

## Performance benchmark contract
Measure from real high-resolution monotonic clock. Label clock source and percentile sample count; never invent numbers.
- Trigger -> capture frame latency P50/P95; frame timestamp uncertainty.
- Frame -> original file flush P50/P95 and complete batch latency.
- Scene render P50/P95 per scene size; report queue latency separated from Word COM completion P50/P95.
- Per-monitor capture skew across one batch.
- Idle CPU/memory; peak RAM on 2x1080p and 2x4K plus mixed-DPI test; queue depth and failure counts.
- Repeat under Qt6 modern and Qt5 native Win7 separately. Benchmark fixture must specify hardware/software and storage type.
Initial thresholds determined at D01 using observed baseline, revised only with recorded justification.

## Test techniques
- Pure-core schemas, naming rules, scene geometry and event correlation: fast deterministic unit tests with synthetic fixtures.
- Platform: Win32 HotKey/GDI pixel-pattern validation and monitor topology tests on target desktop, including negative virtual coordinates, rotation, overlaps, unavailable monitor, notification visibility and screen lock.
- Word: synthetic DOCX, Office 2010 COM and a modern Office build; verify embedded images, dimensions, ordering, captions and reopen result; perform idempotent retry and recovery test.
- Provenance: synthetic command library only; repeats, nested steps, retry, duplicate/out-of-order event scenarios.
- Packaging: offline bundle, AppLocal DLL/plugin provenance, license notices, dependency/import inspection, clean VM check. Do not publish Win7 binaries without native qualification.

## Result states
PASS / FAIL / BLOCKED / NOT_RUN / PARTIAL / OWNER_HOLD. Each receipt includes commit SHA, OS/build, architecture, Qt major+patch, compiler, SDK/CRT, tests executed vs reused, artifact links, open holds. No automation may replace required human visual acceptance.

## Minimum evidence for each goal
Changed files, targeted test invocation/output, new assumptions, remaining issues, PR link, merge SHA when merged, and next goal eligibility. Historical failed probes preserved.
