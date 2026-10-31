# R03 event and catalog contract decisions

Base: R02 merge `f9667da11c8294cd31de724b364e0b53cce6b8cc`.
Public examples remain fictional; this is a read-only evidence association API.

## Identity and immutable provenance

Static catalog identity is catalog ID + revision + source system + computed
import digest. Commands have stable IDs; sequence identity includes revision.
Step IDs are unique within a sequence revision. A nested reference now requires
subsequence ID and revision, and command-kind steps require commandId. R03
schema/semantic checks reject missing and unresolved references. Loops, condition
and parallel metadata describe the source definition; AITSnap never executes it.
Historical snapshots retain definitions rather than resolving against a later
catalog. Recursive references must not be blindly expanded by a viewer.

Dynamic execution identity is `(sourceSystem, sequenceRunId, stepExecutionId,
commandInvocationId, attempt)`. Nested paths and repeated loop traversals are
source-authored; do not invent an invocation or merge on command display name.
Source time, first host receive time and source sequence number remain distinct.
Out-of-order source events are stored; arrival order does not redefine command
success or global active state. Keyboard capture inherits only an explicitly
selected unambiguous context. Missing run => UNLINKED; incomplete level-specific
identity => AMBIGUOUS; complete source IDs/revision => EXACT association only,
which does not imply physical command success.

The executable Qt-neutral EventRegistry proof stores immutable source events,
deduplicated by the pair `(sourceSystem,eventId)`. A duplicate with identical
source content but a new transport receive timestamp returns the first receipt.
Changed immutable content under the same ID is CONFLICT. Receiver-computed
correlation replaces caller-claimed EXACT. Capacity is bounded; full returns an
explicit rejection without evicting evidence, while known duplicates remain
idempotent. Persistence/reload and import adapters are D05 work.

For D05, canonical content digest excludes host receive time and computed
correlation status, includes supported source data/metadata, and is computed by
the adapter rather than trusted from a client. Source-reported digests and the
dummy fixture digest are not proof of actual imported bytes. An import envelope
records the measured SHA-256 and original source digest separately. Raw command
frames are not accepted or exported by the normalized adapter.

## Local host protocol for D06

Use a version-1 length-prefixed UTF-8 JSON request/response on a per-user/session
Windows named pipe. Four-byte little-endian byte count, maximum 1 MiB; bounded
connection/read/write deadlines and explicit capacity errors. Request fields:
schemaVersion, correlationId, action, idempotencyKey (for mutations), payload.
Response: schemaVersion, correlationId, status, result or error {code,message}.
Allowlisted actions: capture, query-status, list-profiles, select-profile,
submit-event, import-catalog. Unknown fields/actions/versions are rejected.
There is no shell, external program, instrument or command-transmission action.

Create the server with the current user SID ACL and PIPE_REJECT_REMOTE_CLIENTS;
do not rely on the default pipe ACL, expose TCP, grant Everyone, or widen local
privileges. Client uses only the local pipe name. A remote-client and mismatched
user rejection need actual platform checks; absence of another account is a
NOT_RUN gate, not an invented PASS. Duplicate accepted events must not trigger a
second capture; a busy/capacity result must remain retriable rather than consume
the idempotency key as success. Durable receipts preserve keys across restarts.

Primary references reviewed:
[Named pipe security](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights),
[CreateNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipew).
No endpoint is started in this research goal.

## Executed checks and gates

`python tools/validate_plan.py` passes all six draft-2020-12 schema fixtures.
`python tools/validate_contract_semantics.py` checks uniqueness, command/nested
revision references, request/batch/scene/report linkage, relative artifact paths
and deliberate invalid cases. `aitsnap-context-tests`/CTest `context` passes
two-run/retry/nested/out-of-order/duplicate/conflict/ambiguous/unlinked/capacity
and forbidden command-intent cases without Qt or COM.

Only affected native C++ context target compiled under v143 on Windows 11;
full desktop/CI runs were unnecessary. Diff and semantic rules reviewed locally.
Proprietary importer formats, native Win7, another-user IPC, actual event-driven
capture/report output and human M4 remain future/HOLD; research tests do not
claim those product gates. Next R04 freeze can admit D03 source implementation.
