# L00 native legacy environment qualification — BLOCKED

Base: D06 merge `0d1eff1d22ef6ff5d4137fecc56a8233ffcd2ee5`.
This is a fresh same-session environment check, not native Win7 acceptance.
Historical initialization, Qt6 validation and win7-prevalidation records remain
unchanged. Raw current results are in the ignored program ledger directory.

## Current evidence

- Native host: Windows 11 Pro Insider Preview 10.0.26340 SP0, x64.
- Installed compiler folders: MSVC 14.44.35207 and 14.51.36231. No 14.29/v142
  folder was reported by Visual Studio installation inventory.
- A new explicit `windows-qt5-legacy-x64` configure actually failed with
  **MSB8020: v142 unavailable**, exit 1. No compiler substitution or installation
  occurred. The shared missing-toolset blocker makes a second x86 configure
  unnecessary; x86 native toolchain qualification is also blocked.
- Hyper-V's command is present, but native VM enumeration was denied by the
  current session's permissions. No guest was inspected, started or changed.
  This does not prove that no Win7 guest exists. No privilege/group change,
  credential access, OS image download or licensed-media acquisition occurred.
- Existing Qt5 kits remain 5.15.2.0, msvc2019_64 and msvc2019. Current Qt5Core
  SHA-256 is `8d2ff4ce9096ddccc4f4cd62c2e41fc854cfd1b0d6e8d296645a7f5fd4ae565a`
  for x64 and `4f8dc460162407cfccb1be6ef9cce45c4449de838aeffa3fd33378f01a3f9cc4`
  for x86. These are SDK provenance, not native target receipts.
- Both current v143 host-preview capability-probe targets compiled. Invoking
  `--require-win7` returned the expected exit 2 before GUI/COM work: native OS
  10.0.26340 SP0, native architecture x64, process bits respectively 64 and 32,
  WIN7_SP1_NATIVE=NO, WIN7_RUNTIME=NOT_TESTED_ON_THIS_HOST. A 32-bit process on
  this host is WOW64, not native x86 Win7.

## Goal and release consequences

L00 is BLOCKED on qualified v142 16.11 plus legally accessible Win7 SP1 native
x64/x86 targets and known SDK/CRT/UCRT prerequisites. L01's native screenshot,
tray and editor branch cannot execute until L00 admits that exact environment.
L02's native composition/Word2010/IPC/install and human M5 branch also remains
blocked. Neither shared-source host tests nor this expected-negative guard waive
those dependencies. Modern development and D07 remain eligible independently.

Future authorized qualification must preserve these failures and record new
evidence separately: exact compiler/SDK/CRT/Qt DLLs, native workstation OS/SP1,
native OS bitness versus process bitness, then M1/M2/M3/M4 per bitness. Word2010
and human operational signoff are required before any full-v1 release. Do not
deploy the v143 host-preview binaries as a qualified Win7 package.

No full CI or independent read-only acceptance. WIN7_X64_NATIVE=HOLD;
WIN7_X86_NATIVE=HOLD; WORD2010=HOLD; full v1 remains BLOCKED.
