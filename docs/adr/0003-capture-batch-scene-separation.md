# ADR-0003: Capture batches independent of composition scenes
Status: Accepted for planning · 2026-10-31

## Decision
CaptureProfile defines input rectangles; CaptureBatch retains immutable original region images with per-display acquisition times; CompositionScene stores independent layout revision and may render multiple derived images. Clip/scale/align only derived outputs, not sources. Scene editor can reopen historical batch.

## Consequence
One trigger supports originals, composite, both, clipboard and Word. Batches are grouped logically but not hardware-synchronous; preserve per-display time offsets.
