# R01 capture and canvas feasibility

2026-10-31; Windows 11 build 26340 x64, MSVC 19.44/v143, SDK 10.0.26100.
Base: R00 merge `f2d634a45aab2e8613dc026d99978052760818c8`.

## Decisions and primary references

Use half-open physical rectangles, with 64-bit intermediate intersections.
Enumerate native display bounds and retain monitor fingerprints and topology
in the profile snapshot. A changed topology requires explicit recalibration.
Qt logical global positions cannot substitute for persisted physical pixels.
Editor pointer positions will use native physical cursor coordinates, while
preview transforms are restricted to UI adapters. Win7 DPI behavior remains a
separate native test subject.

One desktop GDI DC and one top-down 32-bit DIB per relevant display gives a
common Win7/modern backend. Assemble spanning regions from display intersections;
fail a region with uncovered pixels rather than silently filling an unobserved
desktop gap. Record wall and monotonic acquisition intervals around each BitBlt;
no atomic multi-display or HDR fidelity claim. Normal capture must never show
the editor. The overlay must close before an explicitly delayed capture.

References checked during execution:
[BitBlt](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt),
[EnumDisplayMonitors](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors),
[Windows DPI](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows),
[QGraphicsView](https://doc.qt.io/qt-6/qgraphicsview.html).
Use Graphics View for editing and a shared model-driven offscreen painter for
derived images; rendering must not depend on visible editor selection handles.

## Executed feasibility probe

Enable `AITSNAP_BUILD_RESEARCH_PROBE`, build `aitsnap-capture-scene-probe`, run
with `--native`. The probe creates only its own 64x64 synthetic red/green windows,
samples those known pixels through BitBlt, checks foreground HWND, then destroys
its own windows. It saves no screen images. Geometry and offscreen scene tests
also run without `--native`.

| Check | Qt 6.12.0 x64 | Qt 5.15.2 v143 x64 host preview |
| --- | --- | --- |
| Negative-origin crop, disjoint edge, invalid size | PASS | PASS |
| Offscreen scene red/green golden samples | PASS | PASS |
| 100 renders, total steady-clock microseconds | 5600 | 2981 |
| Display (0,-1800,2880,1800), known pixels and focus | PASS; BitBlt 31433 us | PASS; 56420 us |
| Display (0,0,2880,1800), known pixels and focus | PASS; BitBlt 38074 us | PASS; 33242 us |

These are small feasibility samples, not latency percentiles or product gates.
Raw configure/build/probe logs remain in the ignored program output directory.
No new toolchain/dependency install and no CI run. Source diff reviewed locally;
this is not independent acceptance.

## Executable benchmark protocol for D01/D07

Use steady_clock for trigger/frame/crop/save/render intervals and UTC separately.
Run known-pixel warmup, then 100 requests on the same frozen profile; report n,
P50/P95, dimensions, storage and per-display skew, output errors, process peak
RAM and idle CPU. Cover five rectangles across available displays plus synthetic
three-display negative-origin, gaps, overlap and rotation geometry. Do not
invent unavailable hardware: two 4K/mixed-DPI/three-screen/native Win7 cases stay
NOT_RUN until available. D01 derives preview thresholds from actual baseline;
human M1/M2 and no-focus-steal workflows remain OWNER_HOLD.
