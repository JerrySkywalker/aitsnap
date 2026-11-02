# Preview release notes

Current candidate line: [0.8.1 hotkey-conflict recovery](RELEASE-0.8.1.md), retaining [0.8.0 dual hotkeys, report text, mini capture and live preview](RELEASE-0.8.0.md). Exact built artifacts and qualification are recorded in the corresponding delivery receipt.
Previous candidate: [0.7.2 startup and capture command repair](RELEASE-0.7.2.md).
Previous candidate: [0.7.1 repair and daily reporting](RELEASE-0.7.1.md).
Previous candidate: [0.7.0 visual workspace and appearance](RELEASE-0.7.0.md).

Earlier installer candidates: [0.6.2](RELEASE-0.6.2.md), [0.6.1](RELEASE-0.6.1.md), [0.6.0 M3](RELEASE-0.6.0.md).

## 0.5.0-preview — historical limited local Windows preview

The shared application implements multi-region capture, tray/hotkeys, physical region editing, versioned free scene composition, immutable source evidence, durable Word jobs, static sequence import, read-only runtime correlation and a current-logon local CLI/pipe host.

Qt6 6.12 x64 is the modern package. Qt5 5.15.2 x64/x86 builds are host compatibility evidence only. Native Windows 7 v142 qualification, Word 2010, Windows 10, clean-machine deployment and mandatory human visual/M5 acceptance remain HOLD. No public binary release or tag was published by this train.

Four-display synthetic stress: 20 single 1280x720 requests and 100 five-source 640x400 requests, 1920x1080 scene, 620 saved images. Measured capture+scene P50 461.36 ms / P95 526.94 ms, source capture+save 316.70 / 358.27 ms, scene render 4.00 / 5.27 ms; peak capture process working set 184,049,664 bytes; GDI count 4 before/after. These sequential GDI captures are not simultaneous: display-start skew P95 157.18 ms. Timings include the instrumented stages as implemented, not an end-to-end physical hotkey latency promise.

Ten Word16 jobs with six pictures each committed: helper-worker P50 2762.73 ms / P95 4098.06 ms, Word execution 2651.80 / 3934.79 ms; helper peak 19,226,624 bytes. Initial Word process peak sampling was NOT_MEASURED (zero samples), not zero memory. Corrected document-window sampling in two new independent small jobs measured Word process peaks 277,164,032 and 281,632,768 bytes; it is a separate sample from the ten-job benchmark. Word2010 long-form/500-page workload remains untested.

Evidence retains prior locked-desktop samples, Word path failures/UNCERTAIN staging documents and corrected regressions. External gates cannot be replaced by successful local tests. See the operator manual for safe recovery and migration.
