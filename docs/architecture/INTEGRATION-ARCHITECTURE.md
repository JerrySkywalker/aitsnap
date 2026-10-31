# Host integration, CLI and local IPC

## Three integration modes
A. **Default** standalone Windows tray host. The host owns interactive user session, hotkeys, persistent profiles and notification state.
B. **Primary future integration** out-of-process local IPC with Windows Named Pipe, permissions restricted to same authorized user/test process; useful from legacy AIT (C++, C#, Python). Do not open TCP listener or remote control endpoint by default.
C. **Optional** in-process C++ source SDK facade. No cross-MSVC/Qt stable binary ABI guarantee; only offer C ABI DLL on separately justified ADR, and Qt editor widget embedding only into a compatible Qt-major host.

## Contract and APIs
`CaptureRequest` is a typed data contract with requestId, capture profile/scene revision, output mode and optional context/event refs. `CaptureBatch` stores source/destination outcomes. Commands: `capture`, `query-status`, `list-profiles`, `select-profile`, `submit-event`, `import-catalog` (last two after D05). No arbitrary filesystem traversal or command execution accepted.
CLI example: `aitsnap-cli capture --profile power --scene eps-overview --output originals-and-scene`. IPC JSON request/response uses schemaVersion, correlationId, error type, idempotency key, bounded sizes and explicit status. Version handshake/unknown-field policy must be documented.

## Event handler
External execution events are read-only context, not control instructions. Deduplicate by source+event ID, map event to run/step/attempt, evaluate user-approved capture rules, enqueue CaptureRequest. Rejected/ambiguous events are recorded as such; never invent missing IDs. Avoid unbounded capture storms from repeated telemetry changes: capacity and cooldown policies visible.

## Local security
OS ACL restrict pipe and artifact root. No admin privilege by default. No cloud uploads, app telemetry or embedded credentials. Sanitized logs avoid raw command payloads. Win7 IPC compatibility separately proven on native target.

## Reuse acceptance (D06)
A small synthetic host process without Qt UI calls a public core API or local IPC; dispatches synthetic repeated-run/command-retry event, captures selected source regions, renders a scene, saves originals and creates a Word ReportJob. Inspect stable IDs, statuses and output chain; no real instrument commands.
