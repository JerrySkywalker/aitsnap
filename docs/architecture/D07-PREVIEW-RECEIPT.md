# D07 — offline modern preview and measured consolidation

Base: fd722ea48d99aaca7d9bd23ce957bdc7bfacff61. Disposition: PARTIAL overall; Win11 local preview software/deployment evidence PASS, external platform and human gates HOLD. This receipt does not declare full v1 or M5 acceptance.

## Delivered

0.5.0-preview application, CLI and separate Word helper; explicit configuration/data directory, version/help, hidden event-loop/DLL/idle receipt; default new-profile output follows the configuration directory while saved output settings remain unchanged. A separate launcher uses AITSnap-preview-0.5 user data. The default legacy settings path remains supported with exact migration backups.

tools/package_preview.py copies existing Release products and the reused Qt SDK, runs windeployqt, includes app-local MSVC CRT, Qt notices/licenses/SBOMs, synthetic examples, docs and file/DLL/source hashes. Optional raster-Widgets-unneeded DXC, touch/network/SVG plugins are excluded explicitly. No unresolved DLL origin is admitted. No runtime capture, settings, private monitor identifier or real command payload is packaged.

Install is a new-directory per-user copy with hash verification; no elevated/global changes. Uninstall deletes only unchanged manifest-listed files and empty subdirectories; unknown/modified files and external user data survive. Package smoke logs record actual loaded Qt DLL and qwindows paths within both original and relocated install roots using Windows-only PATH. This is isolated-directory testing on a developer host, not a clean VM claim.

## Executed and reused evidence

- Qt6x64, Qt5x64 and Qt5x86 full local builds, seven component CTests each: 21/21. Pure core/context, adapters, scene, queue, context store and real local pipe/client/token rejection are covered. Final changed desktop targets reran 1/1 per kit; later app-only smoke/timing additions compiled all three kits. No full CI rerun.
- Native four-display synthetic stress: 20 single1280x720 and100 five-source640x400 requests, scene1920x1080,620image files. Source+save P50/P95 316.70/358.27ms; render4.00/5.27ms; total461.36/526.94ms; display-start skew139.34/157.18ms. Capture process peak184049664bytes; GDI4→4. Durations are monotonic, not a simultaneous capture or physical-hotkey SLA.
- Ten Word16 jobs, six pictures each: allCOMMITTED; workerP50/P95 2762.73/4098.06ms; Word execution2651.80/3934.79ms; helperpeak19226624bytes. Initial Word memory sampler returned no samples, not a zero-memory result. Corrected document.ActiveWindow.Hwnd sampling in two independent small jobs measured277164032/281632768bytes. Modern Word native mixed/table/replay/PREPARED/source immutability passed in Qt6x64 and Qt5x86 clients.
- Installed native M4 harness uses actual packagedCLI/helper/QtDLLs: four displays, two batches withfive originals+scene+clipboard each, two Word commits, duplicatecapture0, restart-idempotency, final capture manifest plusoneQUEUED unattempted Word job on immediate exit. Test harness was copied into the isolated test install only; it is not a distributed executable.
- Separate schema/hash/XML readback:123batches,638artifacts,13reportjobs; performanceWord60drawings/10markers, installedWord12drawings/2markers; noaltChunk; all saved source hashes match. Shutdown job remainsQUEUED intentionally.
- Package/relocation event-loop and CLI launch, preservation-aware uninstall passed. Final candidate idle15.003seconds: working/peak35090432bytes, no observed GetProcessTimes increment at timer resolution; not a24h zero-CPU/stability claim.
- Reused D03/D04 configuration migration/backup and scene tests, D06 queue/ACL/provenance and failure recovery. No historical initial/Qt6/Win7 record was rewritten.

Hardware context: Core Ultra X7 358H/16logical CPUs, localNVMeSSD/NTFS; current host Windows11 build26340x64, MSVC19.44/v143, SDK10.0.26100. Qt6.12.0 modern x64; Qt5.15.2 x64/x86 host compatibility only. Actual measured topology four displays; two4K/mixed-DPI fixture not claimed. Original raw data remain under ignored out/AITSNAP-V1-GOAL-TRAIN-001: d07-performance, d07-word-memory, d07-qt5x86-word, d07-installed-native-evidence, d07-runtime-schema-xml-receipt.json and named build/test/smoke logs.

## Failures retained and review limits

Initial benchmark LONG/int min compile error corrected. First packaging pass refused untraced auto-deployed systemDXC; subsequent explicit raster exclusions yielded traceable DLLs. A PowerShell GUI wrapper initially read a stale LASTEXITCODE despite a valid success JSON; corrected wait/exit checking passed. One changed-target build used a nonexistent adapter-tests name after the application compiled; corrected desktop-tests/ipc-capture-tests builds completed. No failed artifact was relabeled or deleted.

Same-session diff review checks timer lifetime, shared Qt5 compilation, own-Word-process measurement, exact DLL hashes, no-overwrite copy and unknown-file-preserving uninstall. It is not independent read-only acceptance. Operator instructions, test matrix, license/source notices, release notes and M1–M5 human checklist are in docs/preview.

## Remaining holds and next branch

Win10/clean-machine deployment, native Win7x64/x86 v142, Word2010/large reports, physical key/tray/Explorer/mixed-DPI/replug/two4K visual checks and OwnerM1–M5 remainHOLD. A reduced100-request local preview is justified for promptM1/M2 delivery; it does not waive long-duration/M5 qualification. L00 isBLOCKED with actualMSB8020 and VM enumeration denial; L01 depends onL00, L02 onL01 plusWord2010/D07/humanM5. No OS/Office installs, permission bypass, public binary release/tag or real AIT execution occurred.

Next safe work: normal reviewed D07 PR/merge, exact-clean-main package provenance and final receipt. Further native acceptance requires qualified external targets and human review, not another unchanged CI run.
