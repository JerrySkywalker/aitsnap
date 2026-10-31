# Qt 6 build baseline and local vcpkg audit

Date: 2026-10-31 (Asia/Shanghai). Base revision:
`fa607a72d0ca02c96c38b4b6a4419fdea991261e`.
Scope: build baseline correction, local dependency audit, and one clean
Windows 11 Release build/startup check. No product capability was implemented.

## Current platform policy

The formal baseline is Windows 10 (1809+) / Windows 11 x64, C++20, Qt 6 Widgets,
and CMake. The selected Qt minor line is 6.12.x. Windows 7 and Linux are future
possibilities only: neither was developed for or tested in this work.
The original [initialization record](initial-validation.md) remains unchanged;
its earlier Qt 5 and Windows 7 requirements are historical, not current policy.

## vcpkg audit and dependency decision

| Item | Observation / action |
| --- | --- |
| Existing root | Reused the root selected by the existing `VCPKG_ROOT`; no second installation was created |
| Initial command resolution | PowerShell resolved the extensionless `vcpkg` file; its magic bytes were ELF (`7F-45-4C-46`). Windows `vcpkg.exe` was absent |
| CLI repair | Ran that root's own bootstrap with metrics disabled; downloaded the Windows CLI and validated its Microsoft signature. Preserved the extensionless executable and existing packages |
| Final PowerShell resolution | `vcpkg.exe` in the same root. All audit commands used the explicit Windows executable; `where.exe` also lists the older extensionless file |
| Windows CLI version | `2026-09-26-51bf87ca6e9bf3e622d84ff323bd202ab1ca0c0b` |
| vcpkg checkout HEAD | `fcb618092467258d6d4d924ad0ebb24acfa06ebd`; tracked checkout remained clean and was not updated or switched |
| Target / host triplets | Explicit `x64-windows` / `x64-windows`; target triplet specifies x64, dynamic CRT, and dynamic library linkage |
| Existing installed packages | `eigen3` 5.0.1, `vcpkg-cmake` 2024-04-23, and `vcpkg-cmake-config` 2024-05-23; all reported `x64-windows`. No Qt package installed |
| Qt port in this checkout | `qtbase` 6.11.2 was available as a port recipe, not as an installed library |
| Download cache | Existing `VCPKG_DOWNLOADS` retained; 86 top-level files, with no Qt download. No Qt entry in the root's legacy downloads or packages directories |
| Configured binary cache | Existing `VCPKG_DEFAULT_BINARY_CACHE` retained; 97 ZIP archives, 51,805,321 bytes. Read each archive's `CONTROL` package metadata: no Qt package |
| Default local archive directory | 5 ZIP archives, 2,152,897 bytes; all `CONTROL` metadata inspected, no Qt package |
| Additional cache configuration | No `VCPKG_BINARY_SOURCES` override observed in the process, user, or machine environment |
| Source-build decision | No `vcpkg install`, Qt source build, second vcpkg clone, or toolchain checkout update was started |

An existing official Qt SDK provided the compatible MSVC x64 Qt libraries.
It was selected instead of initiating a cold vcpkg Qt build. The CMake preset
loads the existing vcpkg toolchain in classic mode while selecting the SDK via
`QT_ROOT`. **Qt is not a vcpkg-managed dependency in this route.** No manifest or
`builtin-baseline` is claimed; the checkout HEAD above is an audit observation.
Machine-specific paths and raw audit data are kept only in ignored local output.

## Build environment and actual integration

| Item | Verified value |
| --- | --- |
| Host | Windows 11 Pro Insider Preview, 10.0.26340, x64 |
| MSVC 2022 | Build Tools 17.14.37827.9; compiler 19.44.35229.0, v143, Hostx64/x64; tool directory 14.44.35207 |
| Windows SDK | 10.0.26100.0 |
| Existing MinGW on PATH | GCC 13.2.0, MinGW-w64 UCRT; audited but not selected for the MSVC Qt kit |
| CMake on PATH | 3.29.2 |
| CMake used for validation | Existing Qt Tools CMake 3.30.5 |
| Ninja | PATH version 1.12.0; Qt Tools version 1.12.1. Neither was needed by the selected Visual Studio generator |
| Qt source / version | Existing official Qt SDK, `msvc2022_64`, 6.12.0; installer component `qt.qt6.6120.win64_msvc2022_64`, version `6.12.0-0-202609280346` |
| Qt kit check | `qmake6 -query` reported 6.12.0 and `win32-msvc`; Qt6 package config, Widgets DLL, and `qwindows.dll` existed |
| Original project integration | Original `build/` cache used GCC and had `Qt5_DIR-NOTFOUND`; no vcpkg toolchain/cache entries. Presence of vcpkg on the machine had not integrated it into that build |
| New configure | `cmake --preset windows-msvc-qt-sdk`, in a previously nonexistent output directory, with a trace restricted to the existing vcpkg toolchain |
| Executed toolchain evidence | Trace contained 3,065 lines, including executed target triplet and installed-prefix handling; cache resolved the existing root's toolchain and installed directory |
| New dependency evidence | `Qt6_DIR`, `Qt6Widgets_DIR`, `Qt6Gui_DIR`, and `Qt6Core_DIR` resolved inside the selected official SDK |
| Compile/link evidence | Generated Release x64 project used `stdcpp20`, `MultiThreadedDLL`, and SDK `Qt6Widgets.lib`, `Qt6Gui.lib`, `Qt6Core.lib` |

The preset disables automatic manifest installation and vcpkg app-local copying.
The observed vcpkg toolchain execution is distinct from Qt's actual SDK origin.
The source `src/` files were unchanged.

## Results and limits

| Check | Result |
| --- | --- |
| Clean Qt 6 application configuration | PASS |
| Release x64 compilation | PASS |
| Release x64 linking | PASS; executable PE machine `0x8664` (AMD64) |
| Actual process startup | PASS, using the selected SDK's DLL and plugin paths, with hidden startup |
| Qt window / event loop | PASS: native enumeration found the hidden `AITSnap` top-level window, input-idle completed, and the window responded to a timed message |
| Actual loaded Qt runtime | PASS: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, and `qwindows.dll`, all from the selected SDK |
| Graceful shutdown | PASS: posted `WM_CLOSE` to the spawned application's window; process exited with code 0 |
| Human visual acceptance | NOT PERFORMED; hidden startup does not establish appearance or interaction acceptance |
| Windows 10 execution | NOT PERFORMED; remains part of the platform baseline, not a validated machine result |
| Windows 7 / Linux | NOT PERFORMED; outside current scope |
| CI | No full CI run requested or performed; local evidence only |

The first startup probe asserted .NET `Process.MainWindowHandle`, title, and
responsiveness and did not identify the expected window. It was not counted as
a pass; the probe cleaned up only its own process. Its stdout/stderr and failure
record were preserved. A second probe enumerated windows belonging to its own
spawned PID and demonstrated that the AITSnap window existed but was hidden,
then verified message responsiveness, DLL origin, and normal shutdown. No
application source or binary was changed between probes.

Non-blocking configure diagnostics were retained: missing optional Vulkan
headers and developer policy warnings CMP0126/CMP0156 from the existing vcpkg
toolchain. Configuration, compilation, linking, and the corrected startup probe
completed successfully without installing Vulkan or changing the shared toolchain.

Local logs, environment audit, integration evidence, both startup-probe records,
and the executable remain under ignored `out/`. They contain local paths and are
not published. The original ignored `build/` evidence is also preserved.
Packaging, standalone runtime deployment, manual visual acceptance, and broader
OS verification remain outside this build-baseline result.
