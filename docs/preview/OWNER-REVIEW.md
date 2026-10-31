# Human review pack — acceptance is still HOLD

Use Launch-Preview.ps1 and public synthetic windows/documents. Record exact package-manifest hash, app version, OS/architecture, Qt DLL paths, monitor rectangles/DPI and Word version with each result. Automated PASS is evidence to review, not a prefilled human result.

| Gate | Human exercise | Current result |
| --- | --- | --- |
| M1 | Choose five regions across screens; focus a harmless test window; physically press save/copy/both; inspect dimensions, selected clipboard, focus; tray profile/pause/reopen/restart | OWNER_HOLD |
| M2 | Move/resize/crop/rotate/group, lock/hide/reorder, align/distribute/snap/undo; save two layouts; capture new batch; rerender history; compare untouched originals | OWNER_HOLD |
| M3 | Open own synthetic Word report; append ordered pictures/captions in inline/table modes; inspect page fit; reopen; replay; inspect PREPARED/UNCERTAIN safely without duplicate insertion | OWNER_HOLD |
| M4 | Import synthetic catalogs; configure explicit event rule; two runs/retry/out-of-order/duplicate events; inspect source/run/step/invocation/attempt in batches and Word | OWNER_HOLD |
| M5 | Independent clean Win10/11 install; native Win7 SP1 x64/x86 v142/CRT/Qt5 verification; Word2010; mixed DPI/replug/rotation/two4K; long observation and packaging/source/license review | BLOCKED / OWNER_HOLD |

Do not use real AIT command frames or live instruments. This product has no command transmitter. Do not clear UNCERTAIN jobs to make a dashboard green; preserve job JSON, result, marker, document and source hashes for review.

Native legacy handoff starts at docs/architecture/L00-ENVIRONMENT-HOLD.md. Existing host Qt5 binaries are not the required v142 Win7 candidates. No VM authorization, OS/Office installation or machine security change is implied by this review pack.
