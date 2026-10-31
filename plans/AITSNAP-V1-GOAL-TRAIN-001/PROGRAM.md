# AITSnap V1 Goal Train — Program

**Program ID:** AITSNAP-V1-GOAL-TRAIN-001
**Origin:** public JerrySkywalker/aitsnap main@56804c50f8b1ebeabefce7f977633ac16897a4ad
**Status:** approved roadmap for implementation; documentation is not product acceptance.
**Global budget:** 120 maximum cumulative wall-clock hours of *local agent execution*, including waits, repairs and validation. This is a cap, not an ETA or a promise of delivery.

## Product
One Apache-2.0 Windows screenshot/scene/report project, with a reusable C++20 core, Qt Widgets/Graphics View host and Windows tray. Capture arbitrary named rectangles across screens with global hotkeys, preserve original images, arrange multiple composition scenes, output file/clipboard, append Word, associate read-only command catalog and sequence-run events.

## Platform contract
- Windows 10 (1809+)/Windows 11 x64: Qt 6.12.0 SDK + MSVC 2022 v143 main development.
- **Windows 7 SP1 x64 and x86: mandatory**, Qt 5.15.2 + candidate v142 legacy; native OS and CRT/UCRT/Word2010 qualification OPEN. Qt 5 v143 host previews on Windows 11 cannot close Win7.
- Linux: future possibility only; no active development or testing.
- Reuse the existing local vcpkg toolchain, with Qt supplied from official SDKs, not a cold vcpkg Qt source build.

## Architecture decisions
Modular monolith with Ports/Adapters, Qt-neutral domain and services. CaptureProfile is not CompositionScene. CaptureBatch originals immutable; compositions are labeled derivations. Word ReportJob outcomes independent of capture. Context association is read-only, never an instruction transmitter. Single desktop app with CMake internal libraries; future local named-pipe/CLI host and source-level SDK without premature stable binary ABI.

## Deliverables and tracks
Research R00–R04 = 20h; Development D00–D07 = 82h; Legacy L00–L02 = 18h. All 16 goal pages, dependencies and allocation are in GOAL-MANIFEST.json and ROADMAP.md.
Milestones M1 hotkey+tray, M2 scene composition, M3 Word append, M4 synthetic AIT event chain, M5 qualified installed Windows7/10/11 release with human signoff.

## Boundary
No video/streaming, OCR, image inference, cloud, Linux, real satellite instruction transmission, proprietary test files, unwarranted plugin framework or irrelevant defensive features. Public fixtures must remain fictional. No unlicensed OS/Office downloads, ACL/permission bypass or fabricated validation.

## Execution exit
Track measured spent/remaining time and per-goal PR receipts. Commit eligible source work with targeted tests. If Win7/human/Word acceptance unavailable, record BLOCKED or OWNER_HOLD and continue independent work within cap. Full v1 release remains HOLD without native Win7 x64+x86 and human acceptance. Stop when cap consumed or no safe work remains; report evidence, not promises.
