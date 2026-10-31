# Dependency notices and source

AITSnap source is Apache-2.0, copyright 2026 AITSnap contributors:
https://github.com/JerrySkywalker/aitsnap . Exact application revision and source hashes are in `build-provenance.json`.

This preview dynamically loads unmodified Qt 6.12.0 libraries/plugins, copyright The Qt Company Ltd. and contributors, under LGPLv3 (or the applicable alternative named in each module's SBOM). Qt and its use are covered by the accompanying LGPLv3/GPLv3 texts. The Qt libraries remain separate replaceable DLLs; interface-compatible modified versions may be substituted. Debugging modifications to those libraries by reverse engineering is permitted. No signing or installation lock prevents replacement. Rebuild the application using the documented CMake/Qt SDK route when an ABI change requires it.

License texts: `licenses/LGPL-3.0-only.txt`, `licenses/GPL-3.0-only.txt`; installed SDK notices: `licenses/Qt-SDK-LICENSE.txt`. Official Qt licensing overview: https://doc.qt.io/qt-6/licensing.html . Installed SDK SPDX/CycloneDX SBOMs include per-component source, version, copyright and third-party license details in `licenses/qt-sdk-sbom`. They describe the SDK modules, not a claim that every module is shipped here. Only DLLs listed in build provenance are bundled.

Matching Qt source: https://download.qt.io/official_releases/qt/6.12/6.12.0/single/ ; component source and license texts: https://github.com/qt/qtbase/tree/v6.12.0 and the corresponding module/tag named in the SDK SBOM. The package tool copies existing SDK binaries; it does not rebuild or modify Qt. Public binary redistribution/legal release readiness remains an M5 review item; this train produces a local preview only.

Microsoft Visual C++ app-local CRT DLLs come from the existing VS2022 Build Tools redist directory. Their exact hashes and paths are recorded. They retain Microsoft's licensing terms; no installer or global runtime modification is performed. Windows system DLLs and Office are not bundled. A licensed installed Word is required for report integration.
