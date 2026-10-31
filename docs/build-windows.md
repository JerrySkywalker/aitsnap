# Windows builds and compatibility probes

Windows 7 SP1 is required, using a Qt 5.15 Legacy candidate. Windows 10 (1809+)
and Windows 11 x64 retain the Qt 6.12 + MSVC 2022 main build. See the
[platform matrix](windows-compatibility.md) for qualification limits.

## Common setup

Use CMake 3.24+ and the existing vcpkg root. Presets use the Visual Studio 2022
generator and x64 compiler host; Ninja is not required. Set local paths in the
current PowerShell session (or ignored `CMakeUserPresets.json`):

```powershell
$env:VCPKG_ROOT = '<existing-vcpkg-root>'
$env:QT_ROOT = '<qt-6.12-msvc2022-x64-kit>'
$env:QT5_ROOT = '<qt-5.15-msvc2019-x64-kit>'
$env:QT5_X86_ROOT = '<qt-5.15-msvc2019-x86-kit>'
cmake --list-presets
```

Only set the Qt kit variables needed by the chosen route. Prefixes are kit
roots, not `bin` directories. Never commit local paths or create another vcpkg.
All presets load `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`; manifest
installation and app-local copying remain disabled. Qt comes directly from the
SDK, not a vcpkg package. No package manifest/baseline is needed for this route.

## Main build: Qt 6 / Windows 10 and 11 x64

```powershell
cmake --preset windows-msvc-qt-sdk
cmake --build --preset windows-msvc-qt-sdk-release
```

This retains v143, host/target x64, Qt 6.12.x and `x64-windows` host/target
triplets. Qt 6 is not the Win7 route. Separate regression output can use
`-B out/build/qt6-regression`, followed by
`cmake --build out/build/qt6-regression --config Release`.

## Legacy candidates: Qt 5 / Windows 7 SP1

```powershell
cmake --preset windows-qt5-legacy-x64
cmake --build --preset windows-qt5-legacy-x64-release
cmake --preset windows-qt5-legacy-x86
cmake --build --preset windows-qt5-legacy-x86-release
```

Provision and qualify MSVC 2019 16.11 (v142 / 14.29) for C++20 mode before using
these presets. The current machine lacks v142: the x64 candidate configure
returned MSB8020. Do not silently substitute v143 and call that a Win7-qualified
build. Select/record the Windows SDK locally, using
`-DCMAKE_SYSTEM_VERSION=<installed-sdk-version>` if necessary. The SDK version
and `WINVER=0x0601` / `_WIN32_WINNT=0x0601` header API floor do not establish
runtime compatibility; CRT/UCRT and native testing remain separate requirements.

The x86 preset targets Win32 and `x86-windows`, keeping `x64-windows` as the
build-host triplet. Use separate 32-bit Qt/CRT/dependency sets. A 32-bit app on
Windows 11 x64 does not test a 32-bit Win7 OS. Never mix build directories.

## Available host-only prevalidation

The explicitly named v143 host presets check shared-source compatibility with
the installed Qt 5 SDKs on the current host, without requiring absent v142:

```powershell
cmake --preset windows-qt5-host-preview-x64
cmake --build --preset windows-qt5-host-preview-x64-release
cmake --preset windows-qt5-host-preview-x86
cmake --build --preset windows-qt5-host-preview-x86-release
```

These are **host previews, not Win7 deployment artifacts**. Do not copy current
host runtime DLLs to Win7 or downgrade a modern binary's required CRT because
Qt itself was built with MSVC 2019.

## Optional probe and launch

Legacy/host-preview presets enable `AITSNAP_BUILD_COMPAT_PROBE`. For Qt 6, enable
it with `-DAITSNAP_BUILD_COMPAT_PROBE=ON`. This adds a console probe target,
without adding product capabilities. Example using the Qt 5 x64 host preview:

```powershell
$kit = $env:QT5_ROOT
$output = './out/build/windows-qt5-host-preview-x64/Release'
$env:PATH = "$kit/bin;$env:PATH"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$kit/plugins/platforms"
$env:QT_QPA_PLATFORM = 'windows'
& "$output/aitsnap-compat-probe.exe"
& "$output/aitsnap.exe"
```

For the native check, deploy the qualified candidate, matching Qt DLLs and
`qwindows.dll`, plus validated CRT/UCRT, to the intended Win7 SP1 image. Record
its servicing prerequisites and run the application and:

```powershell
& ./aitsnap-compat-probe.exe --require-win7
```

Probe exits: 0 = minimal checks completed; 2 = not native Win7 SP1; 3 = native OS
query failed; 4 = C++20 subset failed; 5 = COM STA failed; 64 = invalid arguments.
`--require-win7` rejects Windows 10/11 before declaring a Win7 result. Without
that flag, code 0 on Windows 11 is only a host probe. Even native code 0 is not
complete product or visual acceptance.

The probe checks dynamically resolved `RtlGetVersion`, native OS/process bitness, a C++20
subset, compiled/runtime Qt versions, screen count, tray availability and COM
STA initialization. It does not capture screens, create tray icons, compose a
canvas, activate Word or write documents.

## Evidence and history

Inspect `CMakeCache.txt` and the generated Release project for compiler,
architecture, explicit Qt major, SDK libraries, vcpkg toolchain and triplets.
For actual execution evidence, configure with
`"--trace-source=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"` and
`--trace-redirect=out/vcpkg-trace.log`. Raw logs/artifacts contain local paths
and belong in ignored `out/`. This is not standalone deployment.

[Current prevalidation](win7-prevalidation.md) records actual results.
[Initialization](initial-validation.md) and [Qt 6 migration](qt6-baseline-validation.md)
remain unchanged historical evidence. Linux is not developed for or tested.
