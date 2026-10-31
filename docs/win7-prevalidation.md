# Windows 7 requirement restoration and compatibility prevalidation

Date: 2026-10-31 (Asia/Shanghai). Local and remote main were checked at
`8f59a789afce08e9f643bd08bb72e446d49e7536` before work. Scope: explicit Qt 5/6
selection, minimal shared-source adaptation, and local compatibility probes.
**Windows 7 support is required but remains NOT ACCEPTED.**

## Changes

- Retained Qt 6.12.x + MSVC 2022 x64 as the Windows 10/11 main route.
- Added explicit `AITSNAP_QT_MAJOR=5|6`; no Qt autodetection/fallback. Qt 5 can
  target x64 or x86, while the Qt 6 main route remains x64.
- Added v142 Qt 5 Legacy candidate presets and separately named v143 host-preview
  presets. Target/host triplets are explicit and build directories are separate.
- Both application builds share the same source. The only application change
  invokes a small Qt 5/6 DPI helper before QApplication; it is a no-op on Qt 6.
- Added an optional console probe for native OS/SP1, process bitness, a C++20
  subset, Qt version, screen count, tray availability and COM STA initialization.
  It never captures screens, creates a tray icon/canvas, activates Word, or writes
  a document. The [platform matrix](windows-compatibility.md) defines later
  adaptation boundaries without implementing those product capabilities.

## Environment and dependency audit

| Item | Observation |
| --- | --- |
| Current host | Windows 11 Pro Insider Preview 10.0.26340, x64; not Win7 |
| Existing Qt 6 | Official 6.12.0 `msvc2022_64`, preserved |
| Existing Qt 5 | Official 5.15.2 `msvc2019_64` and `msvc2019` kits, with package configs, Widgets DLLs and platform plugins. Installer component metadata records 5.15.2-0-202011130602, installed 2026-10-31 |
| Other installed kits | Qt 5 MinGW 8.1 x64/x86 present; not selected or qualified as a C++20/Win7 route |
| Available compiler | MSVC 2022 19.44.35229.0, v143; Hostx64/x64 and Hostx64/x86 |
| Proposed Legacy compiler | VS 2019 16.11 / v142 14.29 candidate; not installed and not tested |
| SDK | 10.0.26100.0 selected by the host builds. The 8.1 directory contains tools but is not a complete development SDK; no older SDK is claimed qualified |
| Host runtime observation | System32 vcruntime140.dll 14.51.36247.0; UCRT 10.0.26100.9750. This is not Win7 runtime evidence |
| Build tool | Existing CMake 3.30.5 from Qt Tools; no new compiler/SDK installed |
| vcpkg | Reused the existing `VCPKG_ROOT`, Windows CLI 2026-09-26-51bf87ca6e9bf3e622d84ff323bd202ab1ca0c0b, checkout `fcb618092467258d6d4d924ad0ebb24acfa06ebd`; tracked checkout unchanged |
| Dependency source | Both Qt majors come from installed SDKs. No Qt vcpkg package installed; manifest/app-local copying disabled. No second vcpkg or package build started |
| Integration proof | Qt 5 trace demonstrates executed vcpkg toolchain handling. Cache/project checks verify toolchain, selected Qt major, SDK library paths, target triplet, `stdcpp20`, dynamic CRT, and the Qt 5 Win7 header API floor |

No internal project material, real test data, sensitive command files or
credentials were read or added. Public files contain no machine-specific paths;
raw logs and probe output stay in ignored `out/win7-prevalidation-20261031/`.

## Executed results

All successful build routes began in new output directories. On this host:

| Route / check | Result |
| --- | --- |
| v142 Legacy x64 configure | **BLOCKED**, MSB8020: v142 build tools missing. Original failure log retained; no silent compiler substitution |
| v142 Legacy x86 | Not attempted after establishing the shared toolset blocker |
| Qt 5.15.2 + v143 x64 host preview | PASS: configure, compile, link, responsive hidden AITSnap window, matching Qt 5 DLL/plugin paths, graceful exit 0 |
| Qt 5.15.2 + v143 x86 host preview | PASS: same checks under Windows 11 x64/WOW64; PE machine 0x14C. This is not x86 Win7 execution |
| Qt 6.12.0 + v143 x64 regression | PASS: clean configure, compile, link, responsive hidden AITSnap window, matching Qt 6 DLL/plugin paths, graceful exit 0; PE machine 0x8664 |
| Capability probes, all three host routes | PASS: C++20 mode/concepts/span, expected compile/runtime Qt versions; 2 screens reported, tray available, COM STA ready. Word not activated |
| `--require-win7`, all three routes | Expected exit 2 with native OS 10.0.26340 x64 and `WIN7_SP1_NATIVE=NO`; no Win7 pass reported. Probe also distinguishes workstation from Server 2008 R2 and reports native OS architecture separately from process bits |
| PE dependency inspection | Qt 5 binaries depend on VCRUNTIME140 and UCRT API-set DLLs; x64 also lists VCRUNTIME140_1. This is dependency inventory, not complete Win7 import qualification |
| Native Win7 SP1 x64 / x86 | **NOT TESTED / HOLD** |
| Human visual acceptance | Not performed in this round; hidden startup is only automated evidence |
| Linux / full CI | Not developed/tested; no full CI suite run |

Hyper-V's service and command were present, but VM enumeration was denied by
the current session's permissions. No Win7 guest was inspected, started or
modified; the audit does not prove that no guest exists. No other VM control
CLI was found on PATH. No privilege/group changes, VM installation or OS-image
download was attempted. The previously opened user-visible Qt 6 instance and
previous build directories were left alone; probes closed only their own PIDs.

Non-blocking diagnostics retained: the existing vcpkg toolchain's CMP0156
developer warning during traced configuration and optional Vulkan headers not
found for Qt 6. Final Qt 5 presets remove the irrelevant inherited Qt6_DIR entry.
These did not prevent host compilation or startup. The original initialization
and Qt 6 validation documents remain byte-for-byte unchanged.

## Disposition and next closure

The verified change is a dual-Qt build correction and host prevalidation, not
admission of a Win7 release. The PR can merge these verified changes while
preserving explicit HOLD status for required Win7 support. Remaining blockers:

1. Qualify an exact v142 16.11 compiler/SDK/CRT/UCRT combination (or separately
   qualify another suitable compiler/runtime combination); the selected v142
   toolset is absent locally. C++20 subset results from v143 cannot qualify it.
2. Obtain authorized access to licensed Win7 SP1 x64 and x86 targets, record
   exact servicing/runtime prerequisites, and run the matching candidate probe
   with `--require-win7` plus application/DLL/exit and manual visual checks.
3. Complete native dependency/import and deployment qualification per bitness.
   Do not distribute these v143 host-preview artifacts as Win7 releases.

Screenshot, canvas, tray and Word feature acceptance remains future product
work. No Goal Train, architecture research program or additional repository was
created. Win7 is retained as a required platform despite these open blockers.
