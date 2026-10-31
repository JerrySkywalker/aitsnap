# D04 durable Word reporting

Base: D03 merge `750fa6c0cda49c8eee49c3d3c326b8f9aa0835af`.
Source and small modern-Word integration: PASS. M3 human/Word2010: HOLD.

Saved captures may enqueue originals, composites or both, with sequential inline
captions or a two-column table. The user selects an exact DOCX path; automatic
append is off by default. The desktop exposes queue states and explicit marker
inspection. Report metadata includes test/device/station/operator/run/step/
command/event and capture time, alongside stable artifact IDs and source hashes.
Settings v3 reads v1/v2 and preserves the previous bytes before migration.
The settings pane scrolls as report controls expand it.

`aitsnap-word-bridge.exe` runs COM STA in a separate user-session process.
The desktop atomically persists QUEUED and RUNNING before dispatch; screenshot
workers do not wait for Office. Durable enqueue sequence resolves same-millisecond
jobs. Pre-mutation errors retry at most three times, five seconds apart. A
60-second worker deadline terminates only the owned helper and records UNCERTAIN;
no broad Word kill or service is used. Invalid job files are preserved and surfaced.
On desktop restart, RUNNING becomes UNCERTAIN. That state blocks further jobs
for the same document while independent document targets can continue.

The helper verifies every source hash before mutation. An exact running-document
moniker is preferred; otherwise it opens the exact canonical path in an owned
Word instance. It never uses ActiveDocument. It refuses read-only/protected or
unsaved user targets, preserves other open documents, and closes only documents
it opened. Owned-instance macros are disabled before opening a document.
Images are embedded, aspect-preserving and bounded to the insertion section's
page content area. Existing styles and document contents remain in place.

The document variable records PREPARED plus an immutable instruction fingerprint
before content mutation and COMMITTED with the final save. A matching, saved
COMMITTED marker acknowledges replay without insertion. PREPARED, contradictory
or missing recovery markers remain UNCERTAIN. New documents are saved to a
job-specific `.aitsnap-<jobId>.pending.docx` in the selected directory, then
published by no-overwrite rename. Interrupted staging files remain for review;
marker inspection never silently publishes them or blindly retries insertion.
This is recovery evidence, not a distributed exactly-once guarantee.

## Executed evidence

Windows 11 build 26340 x64, Qt6.12/v143. App, helper and queue tests built.
Queue checks passed for JSON persistence, immutable instruction fingerprint,
asynchronous ack, absent helper, crash, timeout, retry limit, restart and explicit
marker inspection. A delayed create cannot be overtaken by its append jobs;
an independent document still progresses during that retry delay. The initial
test run crashed because a test retained a
QJsonValueRef from a temporary job list; converting it immediately to an integer
fixed the lifetime error. The original failing log/exit code is retained.
Scene/migration regression passed with settings v3.

Actual Word 16.0 x64: two jobs queued before the target existed created an inline
report then appended a two-column table. Reopen found exactly four pictures and
one table, captions in order, and two committed markers. Replaying the first job
acknowledged its marker and still left four pictures. A deliberately PREPARED
third synthetic job remained UNCERTAIN on inspection. All original source hashes
remained unchanged. Independent DOCX ZIP/XML checks confirmed embedded image
relationships, four drawings, one table, two COMMITTED/one PREPARED variables and
page-size bounds. Only new synthetic documents in ignored output were touched.
Native Word tests are opt-in and are not registered as ordinary CTests.

Qt5.15.2/v143 x64 host-preview app/helper/queue build and queue fault tests are
the shared-code compatibility check; this does not qualify native Win7 or Office
2010. No full CI, installation or independent read-only acceptance was run.

## Operator follow-up and limits

Select a new DOCX filename in an existing directory or explicitly select an
existing saved report, choose items/layout, then enable automatic append and
apply. Captures remain saved when Word fails. Review queue errors separately.
UNCERTAIN requires checking the exact report and any pending document; the
marker-inspection action can acknowledge a committed marker, never force retry.
FAILED jobs retain the bounded-attempt history. A new explicitly chosen report
path can proceed independently; no existing report is replaced.

Human page/layout review, open-target binding across real user Word instances,
busy/protected target dialogs, large 500-page workload, Word2010 x86/x64, native
Win7 and Win10 remain NOT_RUN/HOLD. Optional report-volume splitting is deferred;
this source supports one explicitly selected document per queue job. No M3 or
full-v1 human acceptance is claimed.

Primary API checks: [Range.Sections](https://learn.microsoft.com/en-us/office/vba/api/word.range.sections),
[Section.PageSetup](https://learn.microsoft.com/en-us/office/vba/api/word.section.pagesetup),
[InlineShape.LockAspectRatio](https://learn.microsoft.com/en-us/office/vba/api/word.inlineshape.lockaspectratio).
