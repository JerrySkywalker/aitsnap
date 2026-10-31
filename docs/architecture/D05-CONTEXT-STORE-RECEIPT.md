# D05 read-only catalog and execution context

Base: D04 merge `3e539b625e36aee41806ebd964b41875b6aed4a3`.
Source/targeted synthetic replay: PASS. Proprietary executor and human M4: HOLD.

The desktop can import normalized JSON or UTF-8 CSV, view static definitions
without recursively expanding loops/nesting, receive source-authored event JSON,
and explicitly select a complete, uniquely cataloged event for later captures.
New events never implicitly select a global latest command. Restart clears the
active keyboard selection while retaining the immutable event/history store.
Unselected captures are UNLINKED; incomplete execution IDs remain AMBIGUOUS.
EXACT means complete claimed source identity, not physical command success.

Imports preserve original bytes in the application store, record their measured
SHA-256 separately from sourceReportedDigest, and retain normalized definitions.
The importer never writes its source file. Changed bytes under the same source/
catalog/revision are rejected, and new revisions coexist. Command references,
versioned nested sequence references and duplicate IDs are checked. The JSON
limit is 4 MiB; recursive definitions are retained rather than executed.

CSV v1 uses the exact header in
`tests/fixtures/synthetic-ait/sequence-catalog-v1.csv`: schemaVersion, catalogId,
catalogRevision, sourceSystem, recordType, commandId, commandName, sequenceId,
sequenceRevision, sequenceName, stepId, order, kind, subsequenceId,
subsequenceRevision. Each row repeats the version/catalog identity. recordType
is command, sequence or step. Quoted commas/newlines/doubled quotes are supported;
formulas are text and are never evaluated. Sequence rows must be declared in
the same import. The public CSV example is entirely fictional.

Event dedup uses `(sourceSystem,eventId)` and canonical supported source content,
excluding transport receive time and caller-claimed correlation. The receiver
computes first receive UTC. Source UTC, source sequence number, run, step
execution, nested path, invocation and retry attempt stay distinct. Parameter
values must use known typed metadata definitions; raw-frame/unknown fields are
rejected. Values contribute to the conflict digest but are omitted from the
durable normalized event, batch snapshot and export. The source event and its
matching catalog snapshots freeze at first receipt, including explicit unresolved
or multiple-catalog status. Later imports cannot rebind earlier events.
CaptureBatch extensions freeze the selected snapshot; file naming and Word
metadata use that request's context. Exports create new files and can additionally
omit descriptions/static metadata. No source command is transmitted or executed.

## Version decision discovered during implementation

The frozen v1 event schema requires a sequence/run, while its declared UNLINKED
state and the core research model allow an event without a run. Runtime schema
validation caught this discrepancy; the initial invalid v1 UNLINKED sample and
failure are retained. The original v1 schema/fixture remain unchanged. A separate
`execution-event-v2.schema.json` and fixture admit a missing sequence explicitly.
The adapter accepts versions 1 and 2, rejects missing-sequence v1, and never
invents run IDs or rewrites accepted v1 receipts. See contracts README for the
explicit source migration rule. Other contract families remain v1.

## Executed evidence

Qt6.12 x64 and Qt5.15.2/v143 x64 host-preview app/context-store builds and targeted
tests passed. Tests cover JSON/quoted multiline CSV, measured import digest,
source unchanged, immutable revision conflict, unresolved references, two runs
sharing a command, attempt 2 ACK and nested retry, out-of-order arrival,
duplicate first receipt, changed-parameter conflict, raw intent rejection,
restart, later-catalog isolation, redaction, no-overwrite export and batch linkage.
The batch linkage uses an explicitly fake capture backend; it is not a native
pixel or source-clock timing acceptance. Real native pixel evidence remains D03.

The first restart test found mutable JSON indexing inserted a missing sequence
field before presence validation. Const parsing fixed the rejection and the test
now asserts acceptance separately from correlation. Qt5 also rejected a reversed
QString/QJsonValueRef comparison in CSV header identity checks; explicit string
conversion fixed it. Original logs and failed samples remain in ignored output.
All versioned public schema fixtures and plan/semantic checks passed. Runtime
catalog/event/batch snapshots were checked against their corresponding revision.

No full CI or independent read-only acceptance. Proprietary executor formats,
native Win7/Win10, real external event-driven capture and human M4 remain HOLD
or D06 work. No licensed software, real AIT dataset or instrument was accessed.
