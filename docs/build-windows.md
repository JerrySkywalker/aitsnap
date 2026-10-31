# Windows build baseline

The formal platform baseline is Windows 10 (1809+) / Windows 11, x64, using
C++20 and Qt 6 Widgets. Windows 7 and Linux are future possibilities only and
are outside current development and testing. The tested dependency baseline is
Qt 6.12.0; CMake accepts 6.12.x. A newer Qt minor version needs platform review:
[Qt's Windows support table](https://doc.qt.io/qt-6.12/windows.html) states that
6.12 is the last version to support Windows 10.

## Prerequisites and dependency choice

- Visual Studio 2022 or its Build Tools, with the v143 x64 C++ toolchain and a
  Windows SDK. A developer prompt is not required for the Visual Studio preset.
- CMake 3.24 or newer. The preset uses the Visual Studio generator, so Ninja is
  not required. Do not accidentally combine a MinGW compiler with an MSVC Qt kit.
- An existing vcpkg root with its Windows `vcpkg.exe` and CMake toolchain.
  Reuse that root; do not clone or bootstrap another installation. If the
  existing root lacks the Windows CLI, its own `bootstrap-vcpkg.bat -disableMetrics`
  can add it without replacing the checkout or installed packages.
- The official Qt 6.12 MSVC 2022 x64 SDK, including Widgets and `qwindows.dll`.

The current route uses the vcpkg **toolchain**, but sources Qt from the official
SDK. The local audit found no installed vcpkg Qt package and no Qt package in
either inspected binary cache. Reusing the installed SDK avoids compiling Qt
from source. No `vcpkg install` is part of this route.

The preset explicitly disables `VCPKG_MANIFEST_MODE` and `VCPKG_APPLOCAL_DEPS`.
There is no vcpkg manifest or package baseline because no project dependency is
being resolved by vcpkg. The audit records the toolchain checkout's Git HEAD;
that observation is not a lock on other machines. If vcpkg-managed dependencies
are introduced later, review the install plan and caches first, then introduce
a manifest and an appropriate baseline. Do not silently trigger a full Qt build.

## Configure and build

In PowerShell, set these environment variables to existing local directories:

```powershell
$env:VCPKG_ROOT = '<existing-vcpkg-root>'
$env:QT_ROOT = '<qt-6.12-msvc2022-x64-kit-prefix>'
Test-Path "$env:VCPKG_ROOT/vcpkg.exe"
Test-Path "$env:QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake"
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

The kit prefix is not its `bin` directory. Both paths are session-local and must
not be committed. Use `CMakeUserPresets.json` (ignored) for optional local
overrides. The checked-in preset contains no machine-specific paths.

The preset selects Visual Studio 2022, v143 host x64, target x64, and the
`x64-windows` target/host triplets. The triplet uses dynamic CRT and library
linkage. It sets `Qt6_DIR` explicitly to the SDK so an unrelated installation
does not select Qt. Use a new build directory when changing compiler, toolchain,
or Qt kit. The original ignored `build/` directory is historical evidence and
must not be reused for this baseline.

## Confirm integration and actual dependency origin

Installing vcpkg or setting `VCPKG_ROOT` alone does not integrate a CMake project.
Inspect the new build's `CMakeCache.txt` for `CMAKE_TOOLCHAIN_FILE`,
`VCPKG_TARGET_TRIPLET`, `VCPKG_HOST_TRIPLET`, and `Qt6_DIR`. For evidence that the
toolchain actually executed, configure with a trace of the toolchain file:

```powershell
cmake --preset windows-msvc-qt-sdk `
    "--trace-source=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    --trace-redirect=out/vcpkg-toolchain-trace.log
```

Check the trace for executed triplet and installed-prefix handling. Check the
generated project/link command for SDK `Qt6Widgets`, `Qt6Gui`, and `Qt6Core`
libraries. In the current route, the presence of the vcpkg toolchain does not
mean that these Qt libraries came from vcpkg. Keep raw logs local: they contain
machine paths and belong in ignored `out/`.

## Launch the placeholder

For a local smoke check, use the same SDK's DLLs and platform plugin:

```powershell
$env:PATH = "$env:QT_ROOT/bin;$env:PATH"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$env:QT_ROOT/plugins/platforms"
$env:QT_QPA_PLATFORM = 'windows'
& ./out/build/windows-msvc-qt-sdk/Release/aitsnap.exe
```

This uses the SDK installation and is not a standalone deployment or release
package. A compile/link pass, a process/window startup check, and human visual
acceptance are separate results. See the [validation record](qt6-baseline-validation.md)
for which checks were actually performed. Windows 10 compatibility and manual
visual acceptance are not established by the Windows 11 startup check.

## Historical evidence

[Initial validation](initial-validation.md) is preserved unchanged. It records
the original Qt 5 configure failure and earlier platform intent; it is not the
current build contract.
