# AIT command, sequence and execution context

## Critical distinction
This tool **does not** send, queue, or execute spacecraft/test commands. AIT execution software is the source of truth for commands and run outcomes. Planned AITC resource scheduling is separate and may later contribute task IDs, not replace the executor's command-run lineage.

## Static import
`CommandDefinition`: stable ID, human-readable name/description, typed parameter definitions, source catalog revision and digest.
`SequenceDefinition`: stable ID and immutable revision, zero or more `SequenceStep` definitions; each references a command or nested sequence and may include loop/condition/parallel metadata when source provides them.
CatalogImport: importer identity, source format/version, content hash, import time, validation/mapping issues, redaction profile. Support normalized JSON and simple CSV; reserve pluggable read-only importer for proprietary formats without inventing their schema.

## Dynamic execution
- SequenceRun: independent invocation of a sequence definition/revision, source-run ID and test context.
- StepExecution: a particular traversal of a step (loop iterations and nested step path); not merely static step number.
- CommandInvocation: a particular command dispatch with actual parameter snapshot (redacted/allowlisted); CommandAttempt distinguishes retry ordinal and reported status.
- ExecutionEvent: event ID, source system, source sequence number, event type (started/dispatched/acknowledged/step complete/failure/pause/resume etc), source UTC timestamp, receive UTC timestamp, optional monotonic correlation and payload metadata.
- CaptureBatch links event ID/run/step execution/attempt plus association confidence/status, and preserves catalog/scene revision at time of capture.

## Correlation
Use explicit IDs from trusted source first; do not join by command name or nearest timestamp without marking probabilistic/ambiguous/manual. Multiple concurrent runs supported. Out-of-order events preserved and reordering policy documented. Duplicate same `sourceSystem + eventId` is idempotently ignored or explicitly reprocessed with changed-state conflict. Static catalog revisions immutable for historical evidence.
User keyboard capture can inherit an *unambiguously selected* active run; otherwise mark unlinked. Export provenance and exact claimed timing distinctions: source event UTC, received, triggered, acquired per display, persisted. Neither screenshot timing nor ACK implies physical command success.

## Inbound security and safety
Local transport only, allowlisted metadata fields, bounded JSON size, reject unknown command-execution intents; no raw sensitive instruction frames in public repo/Word by default. Provide synthetic catalog and nested/repeated/retry events in tests/fixtures. Never impersonate the command executor.

## Future linkage
Rule examples: SEQUENCE_STARTED -> capture overview; COMMAND_ACKNOWLEDGED for selected command -> chosen scene; SEQUENCE_FAILED -> all regions + report queue. Rules create CaptureRequest; do not call spacecraft command APIs.
