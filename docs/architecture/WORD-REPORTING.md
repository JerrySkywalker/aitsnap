# Word reporting architecture

## Initial decision
Prioritize an interactive-user **Word COM Automation** adapter for new/open/append-to-existing Word, especially Office 2010 on Win7. Evaluate but do not make a full arbitrary-OOXML mutator mandatory for v1. Word is an external dependency in COM mode; without Word, keep saved capture artifacts and report jobs pending or export a separate future format by explicit capability, never claim successful insertion.

## Process and threading
Keep WordBridge in a separate user-session process. Dedicated STA initializes and uninitializes COM on the same thread, owns Word objects, handles message pumping and busy/rejected calls with bounded retry; not a Session-0 service. Prefer binding exact document path and explicit insertion strategy (end, bookmark, chosen target), not accidentally whichever Word window is foreground.

## Document semantics
- Insert linked-to-artifact file as embedded image (not external link) by InlineShapes.AddPicture with explicit insertion Range and LinkToFile=false.
- Modes: individual originals, composed scene, or combined sequence; optionally tables/grids in Word and scene rendered as one flattened figure.
- Resize while preserving aspect to page content width/height, use inline placement by default to avoid legacy text-wrapping and clipping bugs.
- Captions and text: configurable test/run/step/command/event/time metadata and text/Word caption style. Respect existing styles/templates, do not hijack entire document formatting.
- Separate original evidence (lossless, retained) from smaller labeled Word display copies; originals remain traceable via batch manifest. Optional grouping / document volumes by stage/date/test avoids huge Word 2010 files.

## Durable queue and exactly-once *intent*
Store ReportJob with id, batch, target canonical document path, render/insert instructions and state transitions (QUEUED, RUNNING, COMMITTED, RETRYABLE, FAILED). Office updates and queue writes are not a database transaction. To avoid duplicates after crash between document save and queue ack, write an in-document idempotency marker (bookmark/content control/document variable) per reportJobId, then on recovery inspect the document before reinserting; test save/reopen/marker presence. This is a design target to validate in R02 and D04, **not an already guaranteed exactly-once implementation**.
Never automatically overwrite unrelated Word content, and never repeatedly spawn Word or interrupt screenshot when COM is blocked.

## Word 2010 qualification
Test both Word 2010 bitness variants only when available; COM server bitness may differ from app bitness and must be observed. Check template semantics, PDF-export optional, abnormal dialogs, read-only/protected document, large reports (500+ page-like workload), reopen validation, image sizes and partial failure. Log failure reasons without sensitive document bodies. Do not install Office via autonomous agent.

## Research & source
Microsoft Word COM InlineShapes.AddPicture documentation: https://learn.microsoft.com/office/vba/api/Word.InlineShapes.AddPicture
Microsoft Office automation unattended caution: https://support.microsoft.com/help/257757

## 0.7.1 daily target routing

Reporting remains opt-in. Settings distinguish automatic daily targets from explicit DOCX paths. Legacy nonempty paths migrate to explicit mode; blank paths migrate to automatic mode while retaining the enabled flag. The adapter ReportOptions default remains explicit for existing programmatic callers; defaultSettings selects automatic mode for the product.

Automatic routing uses canonical screenshot root and the batch's local calendar date. Before enqueueing the first valid saved-image job, the queue reserves `AITSnap-yyyyMMdd-HHmmsszzz.docx` directly in that root, choosing a suffix if an unrelated file exists. `report-jobs/routing/daily-targets-v1.json` records this mapping even before Word creates the document. The preview does not reserve or create files. Restart reuses the reservation; root/day changes select another mapping. Historical batches use their saved root and capture date, shown on the append action tooltip.

Queue jobs retain the resolved absolute path and existing fingerprint, create/append FIFO and recovery semantics. Changes to settings never retarget old jobs. A missing previously committed daily document requires restoration or an explicit new target; it is not silently recreated. Invalid report paths or unavailable Word do not invalidate capture configuration or discard saved screenshots. Copy-only captures do not queue reports.

Qualification: Qt6/Qt5 fake-worker routing/FIFO/restart/collision/migration tests pass. Modern Word16 synthetic daily create, queue restart and append passed on the current Win11 host, with two image nodes and two committed markers in DOCX readback and unchanged source hashes. Word2010, native Win7/Win10 and human pagination/visual acceptance remain HOLD.
