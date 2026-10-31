# Required platforms and small adaptation boundaries

## Platform matrix

| Platform / architecture | Requirement and route | Current evidence / acceptance |
| --- | --- | --- |
| Windows 10 (1809+) / 11 x64 | Main build: Qt 6.12.x + MSVC 2022 v143, C++20 | Windows 11 build/link/startup passed; Windows 10 not run |
| Windows 7 SP1 x64 | Required support platform, primary Qt 5.15 Legacy candidate | Qt 5.15.2 x64 SDK exists; v143 host preview passed on Windows 11. v142 candidate blocked; native acceptance OPEN |
| Windows 7 SP1 x86 | 32-bit Legacy candidate with separate Qt/CRT/dependencies | Qt 5.15.2 x86 SDK exists; Win32 host preview passed under Windows 11 x64. v142/native x86 acceptance OPEN |
| Windows 10/11 x86 or ARM | No main-build commitment | Qt 6 preset remains x64; no additional development/testing |
| Linux | Future possibility only | No development or testing |

Qt's [target-platform explanation](https://www.qt.io/blog/qt6-development-hosts-and-targets)
identifies 32-bit and 64-bit Win7 targets in Qt 5.15 and the removal of Win7 from
Qt 6. That motivates two builds; it does not qualify this application's binary.
Qt 6.12 remains the modern minor line; review platform support before changing it.

## Compiler, SDK and runtime

Both routes retain C++20 mode. Shared code must use a subset compiled by both
toolsets, rather than assuming complete library conformance. The probe exercises
concepts and `std::span`. [Microsoft's conformance table](https://learn.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance)
places `/std:c++20` in VS 2019 16.11: the proposed v142 qualification starting
point, not an installed/tested toolset here. Formatting and advanced ranges need
separate compatibility decisions. Installed Qt 5 MinGW 8.1 and modern UCRT MinGW
are not adopted as C++20/Win7 solutions merely because they are present.

[MSVC binary compatibility](https://learn.microsoft.com/en-us/cpp/porting/binary-compat-2015-2017)
allows newer compilers to consume older MSVC Qt libraries under its conditions;
it does not permit replacing a binary's required newer CRT with an older one.
The [redistributable guidance](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
requires a sufficiently recent runtime and matching architecture; the latest
package now documents Windows 10/11 targets. Qualify an exact Legacy CRT/UCRT
and its prerequisites on Win7; do not copy current host runtime DLLs there.

The [SDK guidance](https://learn.microsoft.com/en-us/cpp/windows/how-to-use-the-windows-10-sdk-in-a-windows-desktop-application)
distinguishes SDK targeting, UCRT and compiler runtime. A Windows 10 SDK can
target Win7; its version and the header API floor do not prove deployed runtime
compatibility. [Microsoft's target table](https://learn.microsoft.com/en-us/cpp/overview/supported-platforms-visual-cpp)
also distinguishes earlier MSVC targeting from VS 2026's removal of Win7.
The v143 host preview is unqualified here; this is not a claim that every
v143/compiler/runtime combination is incapable of targeting Win7.

Before admission, record the exact compiler/SDK/CRT, native OS/SP1, servicing
and UCRT state, imported dependencies, Qt plugin origin and process architecture.
Native x64 and x86 results are separate; WOW64 is not x86 OS proof.

## Qt 5 acquisition and licensing

Reused installed official Qt 5.15.2 MSVC x64/x86 SDKs; no download, account login
or new license acceptance was performed. Qt provides the
[5.15.2 source archive](https://download.qt.io/archive/qt/5.15/5.15.2/).
Later LTS binary availability/rights must be checked for the exact package. The
[5.15.3 announcement](https://www.qt.io/blog/commercial-lts-qt-5.15.3-released)
describes its commercial-only release at that time, not every later patch or
community build today.

The application remains Apache-2.0; Qt retains its own terms. The installed
Qt 5 essential-module headers include LGPLv3 and GPL alternatives. Follow
[Qt's LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
for actual redistribution: notices/licenses, required corresponding Qt source
and modifications, library replacement/relinking rights and third-party terms.
Dynamic linking alone is not complete compliance. Do not add GPL-only modules
based on their presence in an SDK. No Qt DLLs or release packages are published.

## Boundaries for later capabilities

| Concern | Shared boundary / later adapter responsibility | This round |
| --- | --- | --- |
| Business core | Command/sequence and capture/archive records remain independent of Qt major checks, native handles and COM objects; adapters exchange explicit values | No business model or feature invented |
| Screens / capture | Capture adapter owns monitor enumeration, logical/physical pixel conversion, per-screen scale and backend availability; Win7 cannot assume newer Windows capture APIs | Screen count only; no capture |
| Canvas | Share QWidget/QGraphicsView/QGraphicsScene/QPainter composition where possible; contain Qt event/coordinate differences in helpers and preserve pixel scale | Shared placeholder only; no canvas |
| DPI | Configure before QApplication; Qt 5 enables scaling/pixmaps explicitly, Qt 6 keeps its defaults; mixed-DPI acceptance remains native/manual work | Small shared `qt_widgets_compat.h` helper |
| Tray | UI adapter owns QSystemTrayIcon availability/lifecycle/menu actions; business execution must not require a tray implementation | Availability query only; no icon/menu |
| Word COM | Windows document adapter owns an STA thread, COM lifetime, Office version/registration, errors and document writes. Keep COM out of core; test bitness/out-of-process automation explicitly | CoInitializeEx/CoUninitialize only; Word never activated |

Qt-major branches belong in adapters, not duplicated business logic. These
boundaries do not implement or approve product features. No Linux adapter work.
