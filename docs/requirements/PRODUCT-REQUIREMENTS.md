# AITSnap product requirements (PRD v1)

Status: APPROVED-DIRECTION / modern implementation delivered through M3 automated evidence, Windows10 installer candidate in consolidation. Date: 2026-10-31. Original baseline: main@56804c50; current execution status is in the program receipts. Native Win10/Win7/Word2010 and Owner acceptance remain HOLD.

## Mission
Open-source Apache-2.0 Windows desktop tool for satellite AIT operators: predefined screen regions across multiple displays, one-key capture and save/copy, reusable composition scenes, Word report append, and read-only association to command/sequence execution. Standalone tray application first; reusable libraries and local host APIs thereafter.

## Supported scope
- **Required:** Windows 10 (1809+) and Windows 11 x64 on Qt 6.12; Windows 7 SP1 x64 and x86 on Qt 5.15 candidate, with *native verification HOLD*.
- **Not scheduled:** Linux port, Win10/11 x86/ARM. Keep portable core interfaces without claiming platform support.
- Development host: ZenBook Duo, Windows 11, MSVC v143, Qt 6.12.0 official MSVC kit, existing vcpkg toolchain. Legacy v142, CRT/UCRT, native Win7 targets pending.
- Public repository contains only synthetic/redistributable examples. No authentic spacecraft commands, telemetry, personal data or proprietary facility details.

## Users and workflows
1. Operator names a capture profile; picks any number of rectangular regions across displays, including overlapping and cross-display rectangles; sets hotkeys, output directory/format/name template.
2. While controlling other test software, a single non-focus-stealing global hotkey creates one CaptureBatch; outputs independent originals, one or more composed images, a selected clipboard image and optional Word tasks according to policy.
3. Operator edits an OBS-like static composition scene, arranges sources with crop/scale/z-order, saves template and later rerenders from retained source frames.
4. Operator associates evidence with manually entered test context, imported command/sequence definitions and execution run/step/attempt events; external events can later request captures without controlling the test system.
5. App normally resides in Windows notification area; context menu selects profile/scene, capture, pause, open folder, edit, view report queue, diagnose and quit.

## Functional requirements
| ID | Requirement | Priority | Evidence goal |
| --- | --- | --- | --- |
| FR-01 | Hotkey save / copy / save-and-copy, conflicts explicitly reported | P0 | D01 |
| FR-02 | Multi-monitor, any number of named fixed regions, accurate physical-pixel rectangles | P0 | D01/D02 |
| FR-03 | PNG(default)/JPEG/BMP, safe directory/name templates, unique no-overwrite writes | P0 | D01 |
| FR-04 | Windows tray lifecycle, startup option, scene switch, pause, status, foreground focus preserved | P0 | D01 |
| FR-05 | Edit capture regions visually, precision coordinates, overlap and cross-monitor support | P0 | D02 |
| FR-06 | CompositionScene separate from CaptureProfile; source reuse, layer transforms, presets | P0 | D03 |
| FR-07 | Independent originals / composites / both; clipboard selects one image | P0 | D03 |
| FR-08 | Auto-append selected Word document or generate new report; images embedded, captions | P0 | D04 |
| FR-09 | Word queue durable, independent failure status and idempotent retry | P0 | D04 |
| FR-10 | Import static command catalog and sequence definitions with provenance/version | P1 | D05 |
| FR-11 | Bind capture to run, step execution, command invocation/attempt and event | P1 | D05 |
| FR-12 | CLI + local named-pipe capture/event protocol; rule-select scene and output | P1 | D06 |
| FR-13 | Reusable non-UI core, adapters and synthetic integration harness | P0 | D00/D06 |
| FR-14 | Native Win7 32/64 qualification and Win10/11 packaging | Release gate | L00–L02/D07 |

## Nonfunctional requirements
- Capture correctness and operator noninterference take precedence over throughput: no focus theft, control-command injection, DLL injection into test apps or cloud calls.
- A batch is a logical trigger group, NOT an atomic multi-monitor instant. Preserve per-screen capture timestamps and uncertainty; do not misrepresent source timestamps.
- Raw region images are immutable evidence; derived composites and Word display copies are labeled derivations.
- Save failures never masquerade as success. Image capture outcomes and Word report outcomes tracked independently.
- No overwrite, bounded job queues, duplicate event handling and durable report recovery only where needed for core correctness.
- Word 2010 on Win7 is an explicit legacy target; no blanket Office-version compatibility claim without real tests.
- Output filename variables: datetime (with milliseconds), region/scene, test ID, device, station, operator, sequence/run/step/command IDs and persistent sequence. Validate/sanitize each variable, forbid path traversal and collision.
- UI accessible by mouse/keyboard, sane text scaling at mixed DPI. Run without internet after packaging.

## Explicit exclusions through v1
Video/streaming, OCR/AI judgment, telemetry parsing, command transmission, cloud storage, Linux deployment, general plugins and arbitrary third-party document mutation via raw OOXML.

## Release policy
Modern build may be tagged preview after evidence. **Full cross-platform v1 release is HOLD while required Win7 native + toolchain + CRT/UCRT + UI/Word functional qualification remains open.** Treat manual M1–M5 sign-off as separate from automated tests.
