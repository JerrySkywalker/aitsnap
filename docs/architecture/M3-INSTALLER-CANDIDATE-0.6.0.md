# M3 installer candidate 0.6.0

Owner requested M3 continuation after reboot, then confirmed there is no available Windows 10 target and requested the installation candidate first. This record closes the authorized software/candidate delivery; actual target and human acceptance remain open.

## Immutable artifact

- File: `AITSnap-0.6.0-windows-x64.msi` (18,083,840 bytes).
- SHA256: `4406f87d501e403b3132b7f01a3ac42acc00389e9910ad26e52573b4731c8a41`.
- Source: clean merged `10d76a4290a7a433f7a9033853d5193cc64406d2`, PR #19.
- ProductCode: `{F5FD490A-3829-5887-8BD5-C93BC5D127E6}`.
- PackageCode: `{6A4F03EA-6530-4321-B850-5CD529A760C2}`.
- Per-user x64 MSI with embedded CAB, 113 installed files, app-local Qt/VC runtime, Start menu entry and standard repair/upgrade/uninstall. No SDK/Python is required to install or run it. Word reporting requires installed Word.
- Declared candidate target: Windows 10 1809+ x64 / Windows 11 x64. Authenticode status: NotSigned.

The final receipt is a documentation-only follow-up. The installer remains bound to the source SHA above; it is not rebuilt merely to include this subsequent record.

## Exact artifact verification

Host: Windows 11 Pro Insider 10.0.26340 x64, four connected displays, Word 16.0. The final MSI was installed into an isolated owned directory using standard Windows Installer, then the installed application was tested with PATH restricted to Windows directories and Qt override variables cleared.

- All 113 installed payload hashes matched the authoring receipt. Native Windows event-loop startup, CLI and the MSI/portable-uninstaller refusal passed. Runtime Qt modules loaded from the installed directory.
- Installed capture/scene/Word integration produced three persisted batches and 18 original/composite artifacts, with two Word jobs COMMITTED and the shutdown job QUEUED. Schema validation and image SHA256 readback passed. The report contains 12 embedded pictures and two committed markers.
- The installed Word long-document harness reopened the public 510-page landscape fixture and measured 512 pages after the first insertion. Image fit, exact already-open target selection while another document was active, protected refusal, unsaved-edit preservation and unchanged source/unrelated files passed. The final saved target contains four pictures and two committed markers; 512 is the intermediate measured page count, not an asserted final pagination count.
- Installed retry/Unicode/replay tests passed using Chinese and space-containing paths. The saved report has four pictures, one mixed-layout table, two committed markers and one deliberately PREPARED marker; replay adds no duplicate pictures and the prepared job remains UNCERTAIN.
- Independent OOXML readback found no external image relationship or altChunk in the inspected reports. Source fixture and source image hashes remained unchanged.
- The Start menu entry existed. Standard uninstall removed managed executables while retaining an unknown installation-directory file and external user data. MSI product-state queries confirmed both the pilot and final test products were no longer registered.

Earlier review-stage native repair, upgrade from 0.5.3, lower-version refusal, all-file upgrade hash checks and data retention are retained separately; they are not falsely presented as a repeated final-artifact upgrade run. Targeted desktop/report CTests passed 2/2 for each of Qt6 x64 and Qt5 x64/x86. The earlier unchanged baseline matrix is reused.

Raw final evidence is under ignored `out/AITSNAP-V1-GOAL-TRAIN-001/M3-WIN10-001/`: final MSI authoring/install/uninstall logs, installed-chain/Word receipts, `final-artifact-readback.json` and the cumulative final execution receipt. Historical 0.5 seals and failed attempts remain intact. Same-session review does not assert independent read-only acceptance.

## Milestones and remaining holds

M1 capture/tray/region editing, M2 scene composition and M3 modern Word software/native samples pass the available checks. Owner visual, physical-input and report-layout acceptance remains HOLD. M4 installed synthetic-context integration passes the available checks. M5 is an installation candidate, with clean-target/release qualification HOLD.

Windows 10 has not been run: Owner has no target. Windows 7 x64/native x86, qualified legacy toolchain and Word 2010 remain HOLD. Long-duration, mixed-DPI/two-4K and actual other-account/remote qualification also remain open. No full-v1 release or human signoff is claimed.

The original cumulative 120-hour budget and ignored ledger continue without reset. The final local receipt records exact stop time, cumulative consumption, remaining budget, all merged PRs and restart instructions. Resume only when a genuine independent target or acceptance gate becomes available.
