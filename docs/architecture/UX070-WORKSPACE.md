# 0.7.0 visual workspace checkpoint

The existing capture, scene renderer/editor and Word queue remain the product core. The main window now separates five navigation pages: Workbench, Source regions, Scenes, Batches and reports, Settings. The editable region table is retained as an optional advanced panel; region cards and physical display diagrams are the default interface. Entering Sources resets to Display layout, with Region browser second. Cards and diagrams share selection and stable numbers. Double-clicking a card renames the region; its checkbox controls capture enablement.

The scene page shows bounded previews and keeps the existing editor, source model, undo/redo and export operations. Identity labels are independent of transformed images. Missing-source previews remain explicit placeholders; the application never captures merely to refresh a thumbnail. The layout baseline is still explicit per-profile state: screen-change notifications refresh the view without overwriting saved calibration or remapping regions. A mismatch still blocks capture according to the existing core contract.

The batch browser discovers up to 200 direct batch directories under the configured output directory. It reads portable artifact paths where present, rejects paths outside the selected batch, decodes bounded preview images and leaves hash verification to the existing historical-edit path. Old absolute diagnostics remain readable when the batch is in its original location. Batch labels use capture time and saved profile name, including for legacy directories named with UUIDs. Word cards represent actual queue states and related evidence, not rendered Word pages. Inspection/retry retains the existing queue rules and explicit dialog.

Settings now have General, Appearance, Saving and filenames, Hotkeys and tray, Word report, Metadata linking and About categories. Appearance provides system/Chinese/English language and system/light/dark color mode. Colors update immediately; Apply settings persists the selection, and language changes require restart. The filename example uses the current draft, next sequence number and selected explicit context. Source pixels and exported canvas colors remain independent from UI themes.

## Verification and limits

Qt6 x64 targeted appearance/desktop/scene tests pass, including default-source navigation on reentry, portable gallery paths, invalid-path exclusion, configuration migration, image aspect ratio and stable numbering. Affected Qt5 x64 checks pass on the Win11 host. English catalog coverage and substitution-token checks pass. Native Computer Use observed the dark workbench, default display layout, settings Appearance controls, immediate light-mode switching, and real encoded synthetic gallery images including a 2:1 composite. Native theme switching exposed a stale status-color bug, corrected with theme-specific status selectors.

The first gallery observation is persisted privately at `out/AITSNAP-V1-GOAL-TRAIN-001/UX070-NATIVE/gallery-aspect-observed.png`; its observed executable SHA256 was `5f98acf44ac33db9fe47603035ad969ec9fbdfb7cdba5a26a646857622620867` before the final human-readable batch-label refinement. This is development evidence, not final packaged acceptance. The current Owner installation was not terminated or rewritten; only the isolated development process was stopped for rebuilding.

Final package verification, installer Finish launch, native Win10/Win7, Word2010 and Owner visual approval remain separate. This checkpoint does not assert those gates passed.

## Integration completion

Workbench now links source cards, topology and the enumerated next outputs in both directions. Selecting a scene highlights all its visible source regions; selecting a source highlights originals and scenes that use it. Selection survives Apply by stable identity. Small topology labels avoid one another with leader lines. Palette changes refresh generated placeholders without changing image data.

Region overlays accept F2/button rename of the selected region in the edit copy; Finish commits and Cancel retains the original. Scene missing-source choices are localized labels backed by unchanged policy values. New report captions use the frozen region number/name or scene name, while the fingerprint algorithm and existing persisted instructions remain unchanged.

The selected historical batch can be appended to an explicitly chosen report through the existing queue. This adapter reads the saved profile/scene/context snapshots, checks canonical artifact containment, exact artifact identity and SHA256, and never rewrites the batch or recaptures. Duplicate batch/target association opens the existing status instead of creating another job; retry/UNCERTAIN behavior is unchanged. Qt Chinese standard-button translations are shipped for Qt6, with a small embedded fallback for the observed Qt5 kit that lacks QM files.

Hidden/offscreen Qt6/Qt5 component checks cover linked sources/outputs, selection persistence, status contrast after theme changes and Chinese standard buttons. Report tests cover frozen legacy-job roundtrip, historical enqueue/ack using a fake worker, manifest-byte preservation, duplicate prevention and changed-source rejection. Offscreen component PNGs are synthetic-fixture renderings and do not qualify native desktop, installation, Office or Owner acceptance.
