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

- Formal platform baseline: Windows 10 (1809 or later) / Windows 11, x64.
- C++20, Qt 6.12 Widgets (`Qt6::Widgets`), and CMake 3.24 or newer.
- Preferred compiler: MSVC 2022 (v143), x64, with the matching Qt SDK.
- Qt 6.12.x is the dependency baseline; newer minor versions require platform
  review because Qt 6.12 is the last Qt version to support Windows 10.
- Windows 7 and Linux are future possibilities only: no development or testing
  for either platform is included in the current scope.
- The platform policy does not imply that every OS version has been tested.
  See the [current validation record](docs/qt6-baseline-validation.md).

## Build

Use the existing vcpkg installation and the official Qt 6.12 MSVC x64 SDK.
Set `VCPKG_ROOT` to that vcpkg root and `QT_ROOT` to the Qt kit prefix in the
current PowerShell session. Then, from the repository root:

```powershell
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

The portable preset explicitly loads `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
and uses the `x64-windows` triplet. Qt comes from the official SDK selected by
`QT_ROOT`, **not** from a vcpkg package. Manifest installation and vcpkg app-local
copying are disabled; configuration does not start a Qt source build.
Do not create another vcpkg installation or commit local paths.

See [Windows build and launch instructions](docs/build-windows.md), including
runtime DLL/plugin setup and how to verify actual toolchain loading.
The [original initialization record](docs/initial-validation.md) is retained as
history; its Qt 5 and Windows 7 statements no longer define the current baseline.

## Source layout

```text
CMakeLists.txt        Build definition
CMakePresets.json     Portable MSVC x64 / Qt SDK / vcpkg configuration
src/main.cpp          Application entry point
src/main_window.*     Minimal window placeholder
docs/                 Build instructions and validation history
```

## License

AITSnap source is licensed under [Apache-2.0](LICENSE), using the
[official Apache license text](https://www.apache.org/licenses/LICENSE-2.0.txt).
Qt is an external dependency and retains its own license; it is not relicensed
by this repository. No Qt binaries or other third-party dependencies are bundled.
