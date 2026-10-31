# Local metadata/capture host v1

The desktop owns one local named pipe per interactive logon/session. Its current
name appears in the desktop. A process in that logon shares the user's authority;
this is not a process-level isolation boundary. The pipe has an explicit protected
current-logon-SID DACL, first-instance ownership and PIPE_REJECT_REMOTE_CLIENTS.
The client admits only `AITSnap.*` local names and checks the server logon identity
before sending metadata. There is no TCP endpoint or generic execution action.

## CLI and source SDK

With the desktop running:

```powershell
aitsnap-cli query-status
aitsnap-cli list-profiles
aitsnap-cli select-profile --profile-id <id> --key <stable-intent-id>
aitsnap-cli capture --mode both --profile-id <id> --scene-id <id> --key <stable-intent-id>
aitsnap-cli import-catalog --file <absolute-json-or-csv> --key <stable-intent-id>
aitsnap-cli submit-event --file <normalized-event-json> --key <stable-intent-id>
```

`--scene-id` may repeat. An explicit `--capture` on submit requests a capture;
otherwise the first enabled matching desktop rule decides. Rules match source,
event type, sequence definition, static step and command definition; they select
a profile, scene IDs and save/copy/both. Empty filters match any value. The first
matching rule wins; an empty scene array means originals only. No rules are
enabled by default. Word uses the separately selected automatic-report policy.
Request-specific profile/scene routing does not change the user's selected
profile; only select-profile persists that choice.

An event duplicate never creates a new capture, even under a new transport key.
If a previously received event needs another deliberate screenshot, use a new
capture intent with `--source-system <source> --event-id <event> --key <new-key>`.
Do not change instructions under an accepted key. CLI results expose the key in
result.clientIdempotencyKey. Exit codes: 0 accepted/OK/duplicate, 3 busy, 4
uncertain, 1 error, 2 invalid CLI input. ACCEPTED confirms request admission;
query status and the immutable batch/ReportJob establish actual output results.

`examples/metadata_host.py` wraps the CLI with stable keys for the fictional
catalog/event pair. It never connects to instruments, executes source commands
or includes a proprietary executor adapter. Decide first-receipt capture policy
before replay; a later duplicate remains a duplicate. For C++ integration, use
`adapters/local_host.h` and callLocalHost with the same request envelope.

## Wire format and durability

One request/response per connection. Each frame is a four-byte little-endian
UTF-8 JSON byte count followed by 1..1,048,576 bytes. The client closes after
reading the complete response. Server I/O and UI handoff have bounded deadlines;
an interrupted call may be UNCERTAIN. No unbounded FlushFileBuffers wait is used.

```json
{"schemaVersion":1,"correlationId":"caller-1","action":"capture","idempotencyKey":"intent-1","payload":{"mode":"save","profileId":"profile-id","sceneIds":["scene-id"]}}
```

Responses echo schemaVersion/correlationId and contain status with result or
error {code,message}. Allowlisted actions are capture, query-status,
list-profiles, select-profile, import-catalog and submit-event. Unknown structure,
actions and protocol versions fail. Event payload revision is independent of
the envelope revision: explicit event versions 1/2 follow D05's contracts.

Read-only actions need no key. Mutations preflight before storing a PENDING
intent, then persist the final acknowledgement. Busy/capacity does not consume
an accepted key. Accepted instructions deduplicate across host restart. Changed
instructions under a known key conflict. A restart with only PENDING/UNCERTAIN
never blindly executes again. Rejected attempt receipts remain; no silent key
eviction occurs at capacity. Receipts retain action/fingerprint/result, not raw
event parameters. Source-event first receipt is independent of transport-key
receipts. Filesystem/GUI/Office changes are not a distributed transaction.

Capture batches retain source event IDs, run/step/invocation/attempt, source UTC,
first receive UTC, trigger UTC, display acquisition times and static/scene
snapshots. AMBIGUOUS/UNLINKED stays explicit. No ACK or screenshot timing is
interpreted as physical command success.

Primary platform references: [named pipe security](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights),
[CreateNamedPipeW](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew).
Actual other-account and remote-machine refusal remain separate native gates.
