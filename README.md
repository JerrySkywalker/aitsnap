# AITSnap

[中文说明](README.zh-CN.md)

AITSnap is an open-source desktop application project for assembling screenshots
and associating them with test execution records.

## Status

The 0.7.1 repair candidate adds scoped bulk object management, shared context actions,
aligned workbench panels and optional daily Word reports, and fixes the reproduced
scene-save lifetime crash. Installer-Finish white-screen root cause remains
UNPROVEN/HOLD; startup phase diagnostics are included. It retains the 0.7.0
visual workspace, source/display and scene browsers,
batch/report browsing, readable region identities and filenames, a shared multi-size
icon, and Settings → Appearance with Chinese/English and system/light/dark modes.
It retains capture, tray/hotkeys, scene editing, durable Word jobs and read-only
execution-context integration. See the
[operator manual](docs/preview/OPERATOR-MANUAL.zh-CN.md) and
[release notes](docs/preview/RELEASE-NOTES.md). Local Win11 automated evidence
does not qualify native Win10, native Win7, Word2010 or human acceptance.

## Product direction

- Capture multiple regions across multiple monitors.
- Arrange captures freely on an OBS-style composition canvas.
- Automatically archive captures and related records into Word documents.
- Associate captures with commands and sequence execution.
- Provide a Windows system tray entry point.
- Develop reusable components with boundaries between UI and future capabilities.

This repository contains public synthetic fixtures; it contains no real AIT
command payloads or organization-specific test materials.

## Technology and target systems

- Windows 7 SP1 is a **required support platform**, using a Qt 5.15 Legacy
  candidate. Native Win7 acceptance remains open for both x64 and x86.
- Windows 10 (1809+) / Windows 11 x64 retain the Qt 6.12 + MSVC 2022 main build.
- C++20 and CMake 3.24+ remain common to both routes. `AITSNAP_QT_MAJOR=5|6`
  explicitly selects Qt; there is no automatic fallback.
- One repository and shared application sources; future business logic stays
  independent of Qt major versions and native platform adapters.
- Linux remains a future possibility; no Linux development or testing.

See the [platform matrix and adaptation boundaries](docs/windows-compatibility.md)
and [current prevalidation](docs/win7-prevalidation.md). Required support is a
product requirement, not a claim of completed Win7 runtime acceptance.

## Build

Reuse the existing vcpkg root. Set `VCPKG_ROOT` to it and `QT_ROOT` to the official
Qt 6.12 MSVC x64 kit prefix, then run the main build entry points:

```powershell
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

For the Qt 5 Legacy candidate, set `QT5_ROOT` to the Qt 5.15 MSVC x64 kit and use
`windows-qt5-legacy-x64` / `windows-qt5-legacy-x64-release`. This selects v142;
the candidate is currently blocked locally because that toolset is absent.
An x86 candidate and explicitly named v143 **host-preview** presets also exist.
Host-preview binaries are not qualified Win7 releases.

Every preset loads the existing vcpkg toolchain. Qt comes from the selected SDK,
not a vcpkg package; manifest installation and app-local copying remain disabled.
No second vcpkg or Qt source build is needed. Keep machine paths local.

See [build and probe instructions](docs/build-windows.md). The
[initialization record](docs/initial-validation.md) and
[Qt 6 migration record](docs/qt6-baseline-validation.md) are preserved as history;
the current platform matrix supersedes their earlier policy statements.

## Source layout

```text
CMakeLists.txt        Build definition
CMakePresets.json     Portable Qt 5/6 / MSVC / vcpkg configurations
src/main.cpp          Application entry point
src/main_window.*     Workspace controller
src/desktop/         Workspace views, editors, appearance
src/compat/           Small Qt-version adapter
probes/               Optional compatibility probe, not product functionality
docs/                 Build instructions and validation history
```

## License

AITSnap source is licensed under [Apache-2.0](LICENSE), using the
[official Apache license text](https://www.apache.org/licenses/LICENSE-2.0.txt).
Qt is an external dependency and retains its own license; it is not relicensed
by this repository. Repository source excludes Qt binaries. Offline candidate packages include the
selected Qt runtime, app-local CRT and their license/provenance records.

## Architecture and development plan

[Architecture index](docs/ARCHITECTURE-INDEX.md) · [AITSnap V1 Goal Train](plans/AITSNAP-V1-GOAL-TRAIN-001/PROGRAM.md) · [120-hour local execution handoff](plans/AITSNAP-V1-GOAL-TRAIN-001/OWNER-HANDOFF.md).

These are planned deliverables, not implemented features or proof of native Win7 compatibility.
