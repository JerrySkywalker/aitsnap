# AITSnap planning contracts (draft 2020-12 JSON Schema)

This directory defines **pre-implementation** public schema v1 for request/batch/scene/sequence/event/report. R00/R03/R04 must review semantics and contract test them before any code treats them as stable. These are not proofs of actual product behavior.

- capture-request.schema.json: one requested acquisition with profile revision, region IDs, scene IDs, trigger and outputs.
- capture-batch.schema.json: immutable acquired-source metadata and independently tracked image outputs, referenced report jobs.
- scene-template.schema.json: reusable composition geometry, independent of acquisition.
- sequence-catalog.schema.json: static command/sequence descriptions from an imported source, not executor control.
- execution-event.schema.json: read-only runtime sequence/command event; run/attempt identity and provenance.
- execution-event-v2.schema.json: explicit revision for UNLINKED events with no source sequence; v1 remains unchanged and requires a complete sequence identity when admitted.
- report-job.schema.json: durable Word append intent and state machine.

All IDs are illustrative strings; implementation can adopt UUID/ULID while retaining stable external refs. JSON Schema validates *shape*, not semantic invariants: D00/R03 must also enforce uniqueness, ID resolution, geometry bounds, source revision consistency, event deduplication, valid status transitions and time-order nuances. If field semantics change, version schema; never rewrite accepted historical evidence. `extensions` is the explicit namespaced escape hatch.

Fixtures at `tests/fixtures/synthetic-ait/` are invented, non-operational data. Their SHA digest (zeroes) is a **dummy schema-shape placeholder, not a cryptographic validation**, and no sample claims to represent actual spacecraft/test instructions.

D05 found that v1's mandatory sequence/run cannot represent its declared
UNLINKED state. The adapter accepts v1 and v2 explicitly. Existing v1 receipts
stay v1 with no rewrite; newly received events without a sequence must declare
v2. There is no implicit run-ID invention or downgrade. A source may emit a new
fully identified event later; it must use a new event ID rather than mutate an
accepted event's version/content. The six contract families now include this
separate event revision and its own fixture.

Schema files are UTF-8 and use the JSON Schema draft 2020-12 dialect. Validate with a pinned/offline validator of choice during R03; do not add network dependencies solely to validate docs.
