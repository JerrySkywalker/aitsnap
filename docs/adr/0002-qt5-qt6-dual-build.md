# ADR-0002: Dual Qt build for required Win7 and modern Windows
Status: Accepted direction / Win7 native qualification HOLD · 2026-10-31

## Decision
Keep Qt 6.12 + v143 x64 as Windows 10/11 development mainline and Qt 5.15 + candidate v142 as Win7 x64/x86 legacy. One shared source tree with small adaptation boundaries and separate presets/SDK/CRT. Use existing local vcpkg toolchain; Qt packages from installed official SDK, not vcpkg-installed Qt.

## Evidence
See docs/win7-prevalidation.md. Host Win11 Qt5 x64/x86 PASS is not native Win7 PASS; v142 and Win7 native targets currently unavailable.

## Consequence
Modern development may proceed while Legacy train remains HOLD, but **full v1.0 release including Win7 cannot pass without L02 native acceptance**.
