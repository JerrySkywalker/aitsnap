# M3 continuation / Windows installer candidate

Baseline57a410284a5bbe8a6c8094df3cddaa2fab330ade. Owner requested uninterrupted progression through M3 and a Windows10 installer, then confirmed no Win10 target is available and accepted an installer candidate first. Original120-hour cumulative budget continues; previous0.5.0 packages, failures and sealed receipts remain unchanged.

Disposition: modern M3 software and current-host native evidence PASS; Windows10 native, Word2010/Win7 and Owner visual/layout acceptance HOLD. No full-v1 acceptance is asserted.

## Changes

- Audited explicit retry for FAILED jobs with confirmed pre-mutation failure. Job ID, fingerprint, original source references, target and enqueue ordering stay unchanged; prior dispatch/attempt/error history is retained. UNCERTAIN and COMMITTED cannot use this path.
- A FAILED predecessor holds later writes to the same document; other documents continue. A later capture after failed report creation remains an append intent, so it follows recovered creation rather than attempting to create a second report.
- Queue dialog has readable fixed initial columns for long paths, localized states and full-value tooltips. Retry/marker operations remain explicit and distinct.
- Literal pointer arguments are forbidden in the internal Word VARIANT wrapper after a test fixture exposed pointer-to-bool overload selection. Strings use explicit QString values; valid production argument semantics are unchanged.
-0.6.0 preview, updated current requirements/operator instructions, offline API/contracts documentation and a native MSI authoring tool using existing Windows Installer APIs and makecab. No new installer framework, OS, Office or compiler was installed.

## Executed evidence

1. After reboot: native four-display/five-region pixels, save/clipboard/focus and region editor capture guard/cancel/stable-profile lifecycle passed.
2. Qt6x64 and Qt5x64/x86 current-source builds; targeted desktop/report queue CTests2/2 per kit,6/6total. Unchanged earlier core/scene/schema matrix is reused; no redundant full CI.
3. Native Word16 recovery: missing helper exhausts bounded retries, persisted failure blocks same-target successor, restart with real helper and explicit retry commits the original jobs. Chinese/space paths, four pictures, mixed table, reopen/caption order, committed replay and PREPARED uncertainty passed; source hashes unchanged.
4. Native long-form test: actual Word reopened a public510-page source fixture and reported512pages after insertion. Landscape image sizes fit the content area; an already-open explicit target receives pictures while another document is active. Protected and unsaved targets are rejected before mutation; in-memory edits, unrelated document and source images remain unchanged.
5. MSI pilot: native install, Start menu shortcut, Windows-only PATH startup and repair of a removed qjpeg plugin passed. MSI API confirms per-user AssignmentType0; the host's actual ARP entry was observed under HKLM, not assumed to be HKCU.
6. Upgrade0.5.3→0.6.0 passed with all112review payload hashes matching. A lower version was refused with the explicit newer-product message; standard uninstall removed managed program files while preserving an unknown install-directory sentinel and external data.
7. Installed candidate agent UI sample: two committed jobs and one UNCERTAIN job survive startup, all queue columns remain visible, and clicking failed-job retry on UNCERTAIN displays refusal with attempts/state unchanged. This is limited agent inspection, not Owner signoff.

Final immutable installer build, installed full-chain regression and artifact hashes are sealed after source merge in ignored M3-WIN10-001 receipts. Review-stage packages clearly retain dirty-source provenance and are not relabeled as the final artifact.

## Failures retained and resolved

Initial long fixture generation inside Word stalled and produced no DOCX; the owned test processes were stopped and the failure retained. The replacement creates a fixed public OOXML test input, then uses real Word for the same long-report append/reopen/page-count tests. An initial repagination control made the fixture dirty and was correctly refused; it now saves the fixture before testing open-target append. Another fixture used constchar literals that selected bool; explicit strings and a compiler guard corrected it.

The first MSI shell call used forward slashes and failed to open its path. A prototype OS-query custom action received a compatibility-shim version in the MSI host; it was removed in favor of standard AppSearch/RegLocator. The first standard search lacked an empty Signature table; it was added. A later upgrade returned0 but payload verification found18missing DLLs: authoring had omitted PE File.Version, so Installer rejected versioned components as lower versions before removing the old product. Actual MsiGetFileVersion values now populate the File table; the complete upgrade/hash/launch test passed. No force-file-copy flag, permission change or compatibility lie was used to conceal that error.

All raw logs, failed packages/documents and corrected native receipts are retained in out/AITSNAP-V1-GOAL-TRAIN-001/M3-WIN10-001. Prior initial/Qt6/Win7 records remain unchanged. Same-session diff review and normal PR checks apply; independent read-only acceptance is not claimed.

## Boundaries

Host: Win11 Pro Insider10.0.26340x64, existing Qt6.12.0/MSVCv143 and Qt5.15.2 host kits. Qt lists Win10 1809+x64 as a supported dependency target, but no Win10 application run occurred. Hyper-V enumeration remains denied; no guest or privilege was changed. Native Win10, clean target, Word2010/Win7 and human M1–M3/M5 remain HOLD. Continue qualification when genuine targets and Owner review are available; the shipped MSI is an installation candidate, not proof of those missing gates.
