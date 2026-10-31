# AITSnap

[中文说明](README.zh-CN.md)

AITSnap is an open-source desktop application project for assembling screenshots
and associating them with test execution records.

## Status

Software skeleton only. The source contains a minimal Qt Widgets window;
the product capabilities below are planned and are not implemented. There is no
release, installer, or validated platform support yet.

## Product direction

- Capture multiple regions across multiple monitors.
- Arrange captures freely on an OBS-style composition canvas.
- Automatically archive captures and related records into Word documents.
- Associate captures with commands and sequence execution.
- Provide a Windows system tray entry point.
- Develop reusable components with boundaries between UI and future capabilities.

This repository contains only the software skeleton. It contains no test data,
command files, or organization-specific materials.

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
src/main_window.*     Minimal window placeholder
src/compat/           Small Qt-version adapter
probes/               Optional compatibility probe, not product functionality
docs/                 Build instructions and validation history
```

## License

AITSnap source is licensed under [Apache-2.0](LICENSE), using the
[official Apache license text](https://www.apache.org/licenses/LICENSE-2.0.txt).
Qt is an external dependency and retains its own license; it is not relicensed
by this repository. No Qt binaries or other third-party dependencies are bundled.
