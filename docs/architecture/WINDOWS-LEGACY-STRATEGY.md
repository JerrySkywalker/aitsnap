# Windows 7 required-platform qualification

Status: REQUIRED / NATIVE HOLD. Do not downgrade Win7 to future-only; do not call Qt 5 host preview a supported Win7 release. See ../win7-prevalidation.md for observed facts.

## Builds
- Modern Qt 6.12 + MSVC v143 x64 for Windows 10 1809+ and 11; currently Windows 11 shell build+hidden launch passed, no product acceptance yet.
- Legacy Qt 5.15.2 x64 and x86, candidate MSVC 2019 16.11 (v142), distinct CMake presets, build trees, Qt DLL/plugins and CRT/UCRT. v142 not installed, native Win7 SP1 x64/x86 not accessed.
- Existing Qt 5 v143 host previews on Win11 show shared-source feasibility only. 32-bit WOW64 launch on Win11 is not Win7 x86 evidence.
- Keep Windows 7 x64 as first native qualification target, Win7 x86 as separate release gate already in platform matrix.

## Boundary rules
Pure C++20 core uses only features confirmed under v142, not assumed Qt6, OS APIs or stdlib. Qt5 and Qt6 UI code mostly shared; explicit compile guards in thin adapters. Windows7 capture defaults GDI and RegisterHotKey. DPI behavior installed before QApplication and tested at actual OS scaling. COM and tray lifecycle are test subjects, not things proven by current startup probe.

## L00 prerequisites and evidence
Legit Win7 SP1 environments (x64 and x86) with authorized access; exact installed patches, UCRT prerequisites, v142 + Windows SDK, selected Qt5 package/license and redistributable versions inventoried; clean program/probe build and native --require-win7 launch; imported-DLL/API inspection. Do not download unlicensed OS images or elevate permissions to bypass access.
If blocked, mark HOLD, continue modern code with Qt5 host-preview regression at shared-code milestones, reserve the full Win7 release gate.

## L01 / L02
L01 proves actual multi-region GDI capture, display/DPI, hotkeys, tray, formats/config and clipboard. L02 proves full scene composition, Office 2010 COM/report recovery, read-only sequence context, IPC, offline installer and stress/performance on native x64 and x86. Human signoff separate.
Keep separate receipts by OS bitness, Qt version, compiler, SDK, CRT/UCRT and Word version. Do not claim Qt5-only compatibility equals production Win7 compatibility.

## Security / legal
Legacy OS outside ordinary security servicing. Prefer isolated, least-privilege deployment and keep artifacts local. Qt licensing/redistribution obligations for Qt 5/6 must be recorded; Apache-2.0 on this repo does not relicense Qt. Never commit third-party SDK binaries or proprietary test documents.
