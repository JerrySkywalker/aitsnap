# ADR-0004: COM-first WordBridge with durable jobs
Status: Accepted for D04; R02 modern native probe PASS, Word2010/M3 HOLD · 2026-10-31

## Decision
Interactive-user Word COM automation in separate STA bridge process, targeting Word 2010 and modern Word with real qualification. ReportJob saved/queued independently after image capture; image insertion uses embedded InlineShape and document marker to check duplicate commit on recovery. OOXML generation explored, not mandatory for first usable Word append.

## Consequence
Word busy/absent does not block screenshot; no promise of atomic transaction across Word document and queue. Record uncertain recovery state rather than silently inserting duplicate figures.
