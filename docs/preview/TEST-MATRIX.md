# Preview verification matrix — 2026-10-31

| Scope | Executed evidence | Disposition |
| --- | --- | --- |
| Qt6 6.12.0 x64, v143 19.44, Win11 26340 | Full build; 7/7 component CTests; final changed desktop scope rerun 1/1 | PASS, local host only |
| Qt5 5.15.2 x64 / x86, v143, same host | Each full build and 7/7 CTests; final desktop 1/1 each; x86 hidden application launch | PASS, host/WOW64 evidence; not Win7/x86 OS |
| Capture/scene | 20 single 1280x720 + 100 five-source 640x400 requests on four displays, 1920x1080 scene; 620 image saves, GDI4→4 | PASS for measured synthetic workload |
| Word16 x64 | Ten six-item jobs; separate corrected memory measurement, mixed inline/table/reopen/replay/PREPARED/source-hash regression | PASS for tested synthetic jobs |
| Qt5 x86 → installed Word16 | Actual native COM helper, four pictures, mixed table, reopen/replay/PREPARED, unchanged source | PASS; cross-bitness modern Office only |
| Offline copy/install/relocation | New isolated install directory including spaces, Windows-only PATH, actual Qt DLL/qwindows paths inside package, CLI help | PASS on existing developer host |
| Installed full chain | Owned patterns on four displays → CLI events → two five-original+scene batches+clipboard → two Word jobs; duplicate0, restart cache, exit manifest+QUEUED job | PASS with packaged Qt/helper/CLI and copied native test harness |
| Runtime evidence readback | 123 batches, 638 artifact hashes, 13 report jobs, 60/12 Word drawings and10/2 committed markers, no altChunk | PASS; includes separately queued shutdown job |
| Uninstall | Removed unchanged manifest files; preserved modified README, unknown sentinel and external user data | PASS, isolated test install only |
| Settings migration | v1/v2 accepted with exact original backups, v3 serialized, unsupported versions rejected | PASS, reused scene/report component tests |
| Hidden product idle | 15.003 s event loop, working/peak35,090,432 bytes; no observed CPU-time increment at GetProcessTimes resolution | Limited sample; no long-term zero-CPU claim |
| Native Win7 SP1 x64/x86 | v142 configure MSB8020, native guard rejects current Win11; VM enumeration denied | BLOCKED L00→L01→L02 |
| Word2010 / Win10 / clean machine | No qualified target used | HOLD, not tested |
| Mixed-DPI/replug/rotation, two4K, physical keyboard, Explorer restart | Pure geometry/dispatch/lifecycle evidence only for relevant cases | Native/manual cases NOT_RUN |
| Owner M1–M5 visual and Word layout | Review pack prepared; user-input guard prevented prior app control | OWNER_HOLD |
| Later agent UI sample | Actual current preview window, isolated capture output path, scene editor open, layer pointer drag86.618/66.441 and undo16/16 | PASS for this small interaction sample; Owner acceptance still HOLD |

Performance context: Intel Core Ultra X7 358H, 16 logical processors; local NVMe SSD / NTFS. The measured monitor topology had four displays; no claim that it meets the unavailable two4K/mixed-DPI fixtures. UTC strings describe wall-clock labeling; stage durations use monotonic timers and do not prove synchronized cross-screen exposure or UTC accuracy.

A reduced local preview uses 100 bounded requests, ten committed report jobs, targeted failures/replay and install/readback checks instead of a 24-hour soak, to make the priority M1/M2 candidate available promptly. This is a justified *preview evidence limit*, not a waiver of M5 owner signoff or long-duration release qualification. Native Word busy/missing/timeout behavior has deterministic queue tests; real Word2010 long-form/500-page operation remains HOLD.

No CI workflow was rerun by this train. The milestone shared-source local matrix ran once, with affected targets rerun after actual changes. Documentation-only final receipts reuse that evidence. Same-session diff review is not an independent read-only Supervisor acceptance process.
