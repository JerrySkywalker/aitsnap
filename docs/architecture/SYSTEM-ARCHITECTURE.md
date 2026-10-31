# System architecture: reusable modular monolith

## Decision
One Apache-2.0 repository; a Windows tray host containing a *reusable non-UI application core*. Use Ports & Adapters, typed commands/results, dependency injection at composition root, and versioned persistent contracts. No microservices, dynamic plugins or separate Program/Design Lab repositories.

## Dependency direction
- `aitsnap-domain`: value types, identifiers, versioned records, errors, capture/scene/report/sequence concepts; **no Qt, HWND, COM or platform API**.
- `aitsnap-capture`: CaptureService and CaptureBatch scheduler; depends on `ICaptureBackend`, `IClock`, `IArtifactStore` and domain.
- `aitsnap-scene`: composition source/scene model, transforms, non-destructive render specification; Qt Graphics View renderer/editor is an adapter.
- `aitsnap-context`: static catalog import, execution context/event correlation, provenance; no command transport/execution.
- `aitsnap-report`: ReportJob planning/queue/state and Word connector, images as linked artifacts. Word COM implementation isolated in interactive-user bridge process.
- `aitsnap-integration`: event input, CLI/named-pipe protocol, host facade and rule matching; adapters depend on app service API.
- `aitsnap-desktop`: Qt5/Qt6 Widgets shell, system tray, hotkey registration, settings, region editor, scene editor. It *calls* services, never owns batch/evidence truth.

CMake targets should follow these names as justified; defer external DLL/ABI promises. Pure-core code uses C++20 features verified under v142; Qt 5/6 differences contained within adapters and compat helpers. Config and I/O schemas do not expose Qt types.

## App services and side effects
`CaptureService.submit(request) -> requestId/jobId` validates a snapshot of profile and scene references; a platform capture worker provides immutable frames with per-monitor acquisition times; output plan separately invokes file/clipboard and report sink. The queue is bounded and publishes accurate per-output success/failure without falsely moving acquisition time.
`SceneService.render(sceneRevision, capturedFrames)` is deterministic from saved originals and transforms; never mutates source evidence.
`ContextService.associate(batchId, executionContext, eventProvenance)` tolerates missing source information with explicit unlinked/ambiguous status.
`ReportService.enqueue(reportJob)` is durable and eventually handled independently of capture; retry without duplicate insertion after crash.
`IntegrationService` accepts local-only commands/events through narrow authenticated host adapter; cannot send instrument/spacecraft control commands.

## Run topology
- Main GUI and capture service run in **interactive user session**, not a Session-0 Windows Service.
- Optional separate WordBridge process uses a dedicated COM STA and a durable queue; isolate Word busy, dialogs and crash from screenshot process.
- Single instance per Windows user session; tray registration/Explorer restart and hotkey registration errors visible.
- Default zero cloud dependency. No network listener; named pipe secured to user/local authorized principals.
- Settings and scene definitions in user-writable config path; evidence output root user-selected. Keep runtime caches/credentials out of public repo.

## Failure boundaries
Screen missing, protected or obscured => explicit failure or captured-visible semantics, not fake unoccluded view. Network storage failure => captured-but-unsaved result; optional local spool must be opt-in, visible and bounded. Report failure != screenshot failure. Durable identifiers prevent overwrite, but do not imply absolute time accuracy or cryptographic authenticity.

## Architectural review questions (R04)
What is a clean-core C++ API without Qt? Where are OS coordinate conversions performed? How do source frames survive scene rerender? How does WordBridge prevent duplicate append? How can external AIT read-only systems safely supply context? Which guarantees need actual Win7 native evidence? Record answers in ADRs; revise only under PR review.
