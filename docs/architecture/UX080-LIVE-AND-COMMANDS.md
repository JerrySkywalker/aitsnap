# 0.8.0 command, report text and live-view contracts

Three capture actions retain their 0.7.2 meanings. Each has a primary and optional alternate global binding plus an enable flag. Bindings map explicitly to an action; normalized duplicates within an action collapse, cross-action duplicates fail. Editing releases registration without changing user pause; applying an unpaused edit probes OS conflicts before committing. Explicit pause defers registration until resume. Old settings migrate to v5 with byte-preserving backup and unchanged primary keys.

Mini capture and independent-view capture reuse the same QAction/startCapture entry. A formal request persists one sequence and takes fresh pixels; visible mini/live windows hide before acquisition, DwmFlush precedes the worker acquisition, and windows restore afterward. Live preview never replaces lastOutcome or creates a durable request, batch, sequence, image file or report job.

LivePreviewService reuses the crop/coverage/topology engine with an anonymous temporary result, one frame in flight, generation checks, expiry, stop/cache release and pixel limits. A capture-stage mutex serializes native acquisitions; file encoding and Word do not hold it. GUI callbacks consume immutable QImages. Actual output geometry uses the same crop/rotation/aspect/render code; labels remain graphics items separate from source pixels. Historical editing never subscribes to live updates. Unrecorded live sources have no evidence paths and cannot pass historical export checks.

Visible workspace/editor/live-window subscriptions govern sampling. Hidden/minimized or paused views do not keep it alive by themselves. Own-window exclusion is applied only on supported native systems; overlap fallback holds affected sources rather than asserting unoccluded pixels. API success alone is insufficient: the Win11 native regression verifies actual red pixels beneath an excluded blue window. Win10/Win7 capability qualification remains separate. See [SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity) and [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey).

## Report text

Title, caption and context switches/templates are evaluated at job creation. New jobs freeze resolved strings and a versioned textPolicy; policy participates in their immutable fingerprint. Jobs without this extension retain the exact legacy fingerprint input and helper semantics. Visible text off does not remove hidden recovery markers, hashes, FIFO, UNCERTAIN or explicit-target/daily routing. The helper skips disabled text insertion, including its text-only paragraphs.

Templates allow literal text/newlines, `{{`/`}}` for braces, and these case-sensitive fields. Values are inserted once, never parsed again as templates or executable code. Unknown fields/unmatched braces fail; known absent values are empty. Maximum template length is4096 characters.

| Scope | Fields / values |
| --- | --- |
| Batch | `profile`: captured profile name; `datetime`: captured UTC ISO timestamp; `sequence`: captured persisted sequence (empty for older history without this extension) |
| Batch context | `test`, `device`, `station`, `operator`, `run`, `step`, `command`, `event`: frozen capture-context values |
| Caption only | `name`: region/scene name; `label`: existing readable numbered region label or scene name/revision; `number`: original region number; `kind`: ORIGINAL/COMPOSITE; `scene_revision`: composite revision |

Default title/caption/context preserve the previous visible format. Name/time/custom/image-only presets are user choices. Editing templates or renaming sources does not reinterpret an already queued job.

## Qualification

Component, native synthetic, injected input, human visual, real Word and target-OS evidence are separate. Own-window exclusion can make a WGC inspection show underlying content: that image is not credited as this application's visual result, and exclusion is not disabled to work around it. UIA interaction/output readback and synthetic native pixel tests retain their actual scopes. Real Word2010/Win7/Win10 and required Owner signoffs remain HOLD; new Word text layout/append requires its own real-Office evidence.
