# D01 multi-region capture and tray preview

Base: D00 merge `c91cb680de4e63cac1cd8a274c19cad668695237`.
Source uses the existing official Qt6.12/v143 x64 and Qt5.15.2/v143 x64 kits,
Windows SDK 10.0.26100, on Windows 11 build 26340 x64.

## Implemented behavior

Native per-display top-down BGRA8 GDI capture, fingerprint/physical topology
validation, per-display UTC and monotonic acquisition intervals, and frozen
five-or-more-region profiles. Captures run in a bounded one-request worker;
an overlapping request is visibly rejected, never silently queued or dropped.
The core rejects gaps and disconnected/rearranged displays. The GDI adapter
rejects an unavailable or different input desktop; lock/secure-desktop behavior
still needs real target testing. Protected/HDR content is not qualified.

The Chinese Qt Widgets host includes precise numeric regions, multiple named
profiles, output format and filename tokens, selected clipboard image, manual
test/device/station/operator metadata, three configurable hotkeys and tray
save/copy/both/profile/pause/open-folder/quit actions. Closing hides to tray;
`--tray` and the saved startup-display preference start hidden. This preference
does not register a login startup task. Hotkey collisions are explicit; normal
capture does not show/raise the host. TaskbarCreated schedules icon recovery
in addition to Qt's tray lifecycle. A per-user QLockFile prevents a second host.

PNG, JPEG and BMP encode into same-directory temporary files and publish with
no-overwrite rename. Every trigger has a UUID directory plus persisted sequence;
source artifact digests and independent output results are recorded in batch.json.
Use PNG/BMP for lossless pixels; JPEG is an explicit lossy export choice. No
source image is subsequently edited. Default storage is local app data;
operators can choose another directory. Settings use QSaveFile and version
guards. Unreadable/future settings block captures; applying replacement settings
preserves a separate copy first. No credentials, cloud service or AIT transmitter.

The batch schema now allows an empty display list for a failed capture (for
example missing topology); successful captures still require a display. This
resolves a real failure-representation gap without changing successful v1 data.

## Executed checks

`aitsnap-desktop-tests --native --evidence <ignored-output>` passed under Qt6
and Qt5 x64 host preview. It creates its own nonactivating red/green windows on
each display, uses five 16x16 regions inside those windows, and verifies exact
pixels, five files, one selected clipboard image, foreground HWND unchanged,
hidden host, restart sequence and batch manifest. The trigger travels through
the production native hotkey event filter, worker and completion path. It posts
WM_HOTKEY; it does not prove a physical keypress. Other targeted checks cover
three image formats/decode, no-overwrite, bad output path, template traversal/
unknown tokens, config restart/version rejection and OS hotkey conflict/pause/
resume/message dispatch. Runtime batch.json validates against the public schema.

Core CTest and planning/schema checks pass. Existing D00 Qt5 x86 core regression
is reused; new desktop x86 qualification is deferred to the M2 integration pass.
No CI or dependency installs. Raw logs, synthetic PNGs and local deployment
remain ignored under out/. Historical validation files remain unchanged.

The 100-request Qt6 baseline uses steady_clock, five 16x16 PNG regions across
two real 2880x1800 displays, on local development storage. Capture+save P50
128.951 ms, P95 136.204 ms; display-start skew P50 54.5212 ms, P95 58.8494 ms.
All 500 requested files saved. This small-region baseline sets an advisory
500 ms P95 regression investigation threshold for this exact workload (over
3x observed P95, to tolerate shared-host load). It is not a large-image SLA.
Larger regions and scene/report stages have separate D03/D07 measurements.

Computer-use inspection confirmed the actual window, editable profile/region/
output controls and successful Apply action. Dark-theme status/preview text
contrast was corrected following inspection. This agent inspection is not
Owner visual signoff or independent read-only review.

## Manual M1 test pack and open gates

1. Launch the local preview, enter five regions on available monitors and apply.
2. Minimize/close to tray, focus a harmless test window, press the configured
   save hotkey, then copy and both shortcuts. Check pixel dimensions, files,
   selected clipboard image and that foreground remains the test window.
3. Switch profiles via tray, pause/resume, restart, verify retained regions and
   monotonically increasing sequence. Test an occupied shortcut and output path.
4. Observe tray recovery after a normal Explorer restart on an authorized test
   desktop; no Explorer restart was performed by this runner.

M1 human acceptance remains OWNER_HOLD. Physical keypress, real Explorer restart,
mixed-DPI/rotation/replug, locked desktop, Win10, native Win7 x64/x86/v142 and
Word2010 are NOT_RUN/HOLD. The host preview is not a Win7 or full-v1 release.
Next eligible product goal: D02 region selection/editor.
