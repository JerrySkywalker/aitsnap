# Composition scene architecture

## Separate source capture from layout
`CaptureProfile` declares physical capture regions, selected displays, hotkeys and output policy. `CompositionScene` declares a pixel-dimension canvas with zero or more layers referencing region IDs (or immutable historical artifact IDs). Many scenes may use the same region; a scene may reuse a region more than once. The profile snapshot and scene revision are pinned in each CaptureBatch. Raw originals must not be overwritten by edits.

## Editor
Qt Widgets + QGraphicsView / QGraphicsScene / QGraphicsItem for source dragging, resize handles, crop, rotation, alignment, distribution, z-order, visibility, grouping and common presets (row, column, grids, hero+auxiliary, Word-page aspect). Store transforms in a platform-neutral SceneTemplate JSON contract. Editor may show still preview frames and supports zoom, snap-to-grid, undo/redo as capability matures.

## Renderer
Headless offscreen render (no open editor required) with same scene model. Specify mapping of region source pixel rectangle -> transformed canvas destination; crop is non-destructive, preserve aspect unless explicitly set. Define background/transparent/color-space and deterministic layer ordering. Render derived PNG at user-chosen dimensions, retaining source region files for later rerender.

## Sources and missing data
Per-source required/optional flag and explicit missing-source policy (fail scene, placeholder + mark incomplete, or skip). Never silently reuse a stale screenshot as though acquired in current batch. Re-open a historical batch by artifact IDs, label the result derived and retain scene revision / render timestamp.

## MVP vs later
- D02: edit actual physical capture regions, separate from scene editing.
- D03: scene visual editor, source duplication, geometric transforms, alignment, presets, revisioned save/reload, offline render, all output modes. Editable group features may be staged internally but must be complete for M2 acceptance.
- Future only: animation, live video sources, multi-stream composition, OCR annotation.

## Verification
Pure-geometry unit tests plus pixel-golden synthetic patterns for crop, scale, overlap, layering, mixed sizes, repeat-renders Qt5/Qt6. Human M2 inspects composition on true multi-monitor desktop. Avoid screenshot-of-editor feedback loops.
