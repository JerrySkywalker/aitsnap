# Performance, validation and evidence methodology

## Purpose
Benchmark reusable capture engine and scene renderer with both synthetic pixel fixtures and native Windows desktops. Measure impact on real operator experience, not FPS of a video tool. Historical Qt build/launch probes do not validate screenshot accuracy.

## Instrumentation
Assign a request ID and monotonically increasing stage timestamps (trigger received, frame acquisition per monitor, crop complete, encoded, atomic save, clipboard complete, queued Word, Word commit). Record wall-clock UTC separately for reports; document wall-clock synchronization source and precision. Never claim millisecond timestamp formatting equals millisecond accuracy.

## Bench cases
- Single 1280x720 region on 1080p, local SSD.
- Five regions across three displays with negative origin and mixed DPI.
- Two 4K displays, overlapping regions and one spanning both.
- 100 consecutive requests with bounded queue and failure injection.
- Compose five captured source images to Word-page, 1920x1080 and tall layouts; rerender historical batch.
- Word 2010 long-form synthetic report workloads; Word busy/document locked/restart; idempotency collision recovery.
Measure P50/P95, n and clear units per stage; preserve RAM high-water, idle CPU, disk total, file counts, failures and per-display skew. Benchmark code is reusable test infrastructure, not an overbuilt telemetry platform.

## Native vs deterministic
Deterministic unit tests validate crop geometry, naming templates, JSON schema compatibility, event provenance, scene transforms and report state transitions without needing a desktop. Native tests verify display enumeration, pixels, tray notifications, focus preservation, hotkeys and COM on each qualified OS. Use controlled synthetic patterns only in public CI.

## CI economy
Only affected target tests per PR. Full Windows Qt6/Qt5 matrix at milestone integration/release or when shared Qt compatibility changes. No repeated full CI for docs-only changes; reuse unchanged evidence. Failed or blocked tests remain visible. Long stress and manual visual checks belong to acceptance milestone, not every commit.

## Bench exit criteria
R01 defines executable benchmark specification and reproducible fixtures. D01 records baseline and derives justified latency thresholds; do not fabricate numbers. D03 measures composition, D04 measures report latency independent of capture, D07 evaluates installed products. Native Win7 measurements require L00/L01 first.
