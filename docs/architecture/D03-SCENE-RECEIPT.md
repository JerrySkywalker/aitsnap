# D03 scene composition and M2 review pack

Base: R04 merge `ac831964756d1ccd73457d0f13050f3ce4f617e9`.
Source/component disposition: PASS. M2 human visual acceptance: OWNER_HOLD.

The desktop now separates physical source regions from revisioned scenes. The
QGraphicsView canvas supports source reuse, move/resize, source-local crop,
centre rotation, groups, locking, visibility, z order, target-box alignment and
distribution, single-selection 10px snapping, undo/redo and four presets.
Controls expose exact geometry; dragging remains subject to human UX review.
The renderer works without an editor window and uses the R04 colour/aspect/z
ordering contract. Missing sources fail, skip or display an explicit placeholder;
incomplete output is labelled and never presented as a complete capture.

Per-profile scene/output/clipboard policies survive restart and tray switching.
A capture freezes source and scene revisions, retains lossless source images,
and can save originals, multiple composite PNGs or both. JPEG display exports
retain separate PNG evidence. Composite-only output retains sources in its
evidence subdirectory. The clipboard selects one source or composite.
Independent output failures remain visible. Originals are never overwritten.
Historical batch loading checks retained source hashes and bounded paths;
rearrangement exports a new PNG plus derivation receipt and a distinct snapshot
identity. v1 settings bytes are preserved before the first v2 write.

## Executed checks

Windows 11 build 26340 x64, v143 19.44/SDK 10.0.26100. Qt6.12 x64,
Qt5.15.2 x64 and Qt5.15.2 x86 host-preview app/scene builds passed. Scene
checks passed on all three: row/column golden pixels, source crop, aspect fit,
stretch, rotation, alpha, hidden/z ordering, missing/pinned source handling,
two scenes from one batch, JPEG plus lossless originals, historical rerender,
unchanged original SHA, group movement/lock/source reuse/undo, derived export
and exact-byte settings migration backup. Qt6 core/context checks also passed.
Runtime batch and scene/derived-snapshot JSON was checked against the public
schemas; artifact identifiers are unique within each batch.

The initial test compile errors (missing QGraphicsItem include and Qt5
QStringList assignment ambiguity) were corrected; failed logs are retained.
Native known-pixel regressions failed during this goal. The independent R01
probe also failed. Diagnosis found correctly painted own window DC pixels but
black/wallpaper screen samples, then confirmed WTS SessionFlags=LOCK on the
modern host. These samples are invalid capture-context evidence, not passes.
The backend now rejects locked/inactive/secure desktop capture before acquiring
pixels. A real locked-session negative check passed: SESSION_LOCKED, no image,
no invented acquisition time, and a FAILED manifest. It does not prove unlocked
capture. Earlier D01/D02 native results remain historical evidence only.

No UI input was attempted after the lock was confirmed. A preview deployment
race initially loaded foreign Qt libraries from PATH; that own process was
closed and the failure recorded. The final M2 preview is deployed only after
windeployqt finishes, to a separate directory. Its human launch/visual review
remains HOLD. No full CI, new SDK installation or independent read-only
acceptance was performed.

## Owner exercise

Launch the M2 preview with the desktop unlocked. Confirm five source regions,
save/copy/both hotkeys and tray switches; create row and column scenes from one
saved capture, drag/resize/crop/rotate/reuse/group layers and inspect the PNGs.
Toggle visibility/lock, align/distribute, undo, save/restart, select a composite
for clipboard, then open the same historical batch and export a new derivation.
Check original hashes, alpha, Chinese text, high-DPI sizing, multi-selection,
rotated handles and missing-source messages. Physical keys, real pointer
editing, Explorer restart, mixed-DPI/topology changes, native Win10 and native
Win7 x64/x86 are NOT_RUN/HOLD. WOW64 is not native Win7 qualification.

Next eligible software goal: D04 durable Word bridge; no human gate is waived.
