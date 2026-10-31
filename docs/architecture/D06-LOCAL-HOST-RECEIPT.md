# D06 local CLI/IPC and synthetic host integration

Base: D05 merge `7b133261e13dd565a745fc677b524385cab91634`.
Source/native synthetic M4 automation: PASS. Human/native legacy gates: HOLD.

The desktop now owns a bounded version-1 local named-pipe endpoint with an
explicit protected current-logon-SID DACL and remote-client rejection. The
client verifies the server logon before sending. Six actions reuse the desktop
capture/context services: capture, query-status, list/select profiles, catalog
import and event submission. No shell, instrument or command-transmission action
exists. The CLI and Python metadata-host example use this API. See
[LOCAL-HOST-API.md](LOCAL-HOST-API.md) for frames, keys and source SDK usage.

PENDING intent is persisted before mutation and acknowledgement after admission.
Accepted keys survive restart; changed instructions conflict. Busy/capacity
does not consume the key. Unconfirmed intent returns UNCERTAIN rather than
blindly replaying. Receipts omit raw event parameters. One matching event rule
maps source/type/sequence/static step/command metadata to a profile, multiple
scenes and save/copy/both; Word retains its separate automatic-report policy.
Rule configuration is explicit and initially empty. Event duplicates never
create extra captures. Per-request routing does not change the selected profile.

Exit stops IPC admission, drains saved capture completion before receiver
destruction, and preserves queued Word intent. Report callbacks are disconnected
before dependent widgets disappear. Word is never synchronously awaited by the
capture path. Partially acknowledged external side effects can still require
inspection; no distributed exactly-once guarantee is claimed.

## Executed checks

Qt6.12 x64 and Qt5.15.2/v143 x64 host app/CLI/pipe/Word-helper builds passed.
Both host tests passed actual Windows pipe frames, CLI child-process roundtrip,
native DACL readback containing only the current logon SID, and an actual
ACCESS_DENIED open under a deliberately restricted own-thread token (then
restored). Journal tests passed busy retry, accepted duplicate, changed-key
content conflict, restart, pending-intent hold and bounded capacity. Unknown
execution intent/fields and remote/unrelated client namespace are rejected.
This is not an actual second-account or remote-machine test; those remain HOLD.

On the unlocked Windows 11 host the final opt-in integration enumerated four
displays. A separate CLI imported the fictional catalog, switched profiles and
submitted ACK metadata. A saved rule generated five exact red/green physical
region captures plus one scene PNG and selected composite clipboard per event.
Two runs and retry attempts 2/3 with out-of-order source metadata stayed distinct.
An identical event under the same and different transport keys, then the accepted
key after desktop restart, created zero extra screenshots. Two real Word16 jobs
committed. DOCX inspection confirmed 12 embedded drawings, two committed markers
and both run IDs; batch/scene/event/report schemas and original SHA checks passed.

An additional immediate-exit integration passed with the capture manifest saved,
explicit UNLINKED status and exactly one QUEUED, unattempted report job. This
verifies software lifecycle, not human UI acceptance. The SDK example's parser
was exercised; its CLI path is also exercised by the independent native client.

## Failure retained and fixed

The first full chain reached saved images/scenes/clipboard but Word returned a
null IDispatch at the first AddPicture. The job correctly became UNCERTAIN and
blocked its same-document successor; that batch, pending document and receipts
remain. A direct helper reproduction excluded IPC. Controlled same-image tests
showed forward-slash paths containing spaces failed with inline count 0, both
at 212 and 114 characters, while the native-backslash 212-character path passed.
All Word COM file arguments and exact file monikers now normalize at the adapter
boundary; original paths, hashes, fingerprints and failed receipts stay unchanged.
The mixed inline/table native regression now uses image/document names containing
spaces and passed create/append/reopen/replay/PREPARED recovery with unchanged
sources. The original small R02/D04 successes remain limited historical evidence.

No full CI, independent read-only acceptance, OS/Office installation or real AIT
dataset access. Actual other-account/remote refusal, native Win7 x64/x86/v142,
Win10, Word2010, large reports and M1-M4 human experience/layout remain HOLD or
subsequent qualification work. M4 automated integration does not accept full v1.
