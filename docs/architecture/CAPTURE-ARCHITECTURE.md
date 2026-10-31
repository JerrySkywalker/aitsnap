# Multi-monitor capture and hotkey architecture

## Input and coordinate model
Enumerate displays using Windows APIs; preserve stable-but-revalidated display fingerprint, virtual desktop origin (may be negative), physical-pixel monitor bounds, DPI and current orientation. Region is a rectangular screen-space physical-pixel crop. Qt logical coordinates are **not** the persisted truth. Validate monitor replug, scale changes, negative origin and spanning multiple displays. A source region may overlap another; multiple sources may share the same pixels.

## Capture pipeline
1. Windows RegisterHotKey (or CLI/event trigger) -> immutable CaptureRequest with stable ID and frozen profile/scene revisions.
2. Determine affected displays and rectangle intersections; minimize repeated capture per display.
3. Capture each affected display once when practical via a shared GDI/BitBlt backend with verified virtual-screen and display boundaries. Capture monitor edges separately when needed. Optional DXGI later, never required for Win7.
4. Record timestamps at *actual frame acquisition*, monitor identity, shape, pixel format, read error and per-monitor skew; a logical batch is not a hardware-synchronized snapshot.
5. Crop/assemble each region using physical pixels and checked clipping/stride. Keep original region bitmaps immutable (and unannotated).
6. Publish a batch with independently trackable outputs: original PNG/JPEG/BMP, derived scene renders, one clipboard selection, optional report job.

## Interaction
Default shortcuts should avoid test app function keys where possible, but let users set a single key when the OS accepts it. Hotkey collision is a visible rejection (no silent remap). Do not inject keys into the AIT app; no low-level keyboard hook unless a future justified ADR. Tray/menu and configured hotkeys invoke same service.
No preview overlay in a production capture frame: region-edit overlay is an editor mode; hide/suspend it and allow compositor transition before capture, with measured latency and observable errors. Normal tray capture does not show a window or steal focus.

## Correctness and storage
Clip cross-monitor regions deliberately, with deterministic output dimensions; no silent rescaling. Preserve source color space / pixel metadata when available; SDR behavior first, document HDR/color-managed limits. Protected, locked or occluded desktop content is **not** promised as an unobstructed screenshot.
File names use allowlisted tokens (test/device/run/sequence/step/region/scene/UTC-or-local-time/millis/sequence number) and sanitized path segments, no traversal, no overwrite; atomic temp+rename on same volume where supported, distinguish durable-save from queued. For network output, optional local spool only by explicit setting. Clipboard accepts one image (default composed image, or selected region), not many images implicitly.
Use bounded capture/encode/storage queues, event-to-result mapping and per-output states; no silent drops. Do not shift a stale frame's timestamp to a later trigger instant.

## Test matrix
Known-pixel test patterns, edge coordinates, two/three displays, mixed DPI 100/125/150%, negative origins, rotation, unplug/replug, overlapping regions, cross-screen regions, simultaneous presses, interrupted storage, permissions, locked screen, remote session. Record latency and skew P50/P95 separately; do not invent performance numbers.

## Win7 boundary
Common Win32 GDI and RegisterHotKey are first targets. Any Qt high-DPI and OS API differences isolated in adapters and proven native, never inferred from Qt5 WOW64 host preview.
