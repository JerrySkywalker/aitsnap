# 0.7.0 installer and brand checkpoint

## Launch intent and preparation feedback

The 0.6.2 Owner report remains a real launch failure. Its exact Windows Installer failure mechanism is still unproven. The original Owner configuration has `startInTray=false`; tray preference must not be presented as the confirmed cause of that report.

The candidate uses the documented `[#filekey]` first field for the Finish button's MsiLaunchApp event and passes `--show`. This flag overrides a saved tray preference. Normal repeat launches restore the main window of the process holding the same configuration lock instead of merely showing an already-running message. The main window has a native handle even when it starts hidden. Quiet installation, cancellation, failure and removal still have no automatic application-launch action. Start-menu shortcuts also carry the explicit show intent.

Folder Install and maintenance actions now transition directly to the modeless preparation/progress dialog before returning to the installer sequence. A property prevents a duplicate dialog; direct uninstall has a sequence fallback. The dialog explains preparation/system restore and subscribes to actual installer action/progress messages. This does not disable protection or manufacture percentages. The measured 20.236s restore-point delay from the old log is not claimed to be reduced. First-frame timing, repaint/cancel behavior and the complete Finish chain require native attended evidence.

Sources: [MsiLaunchApp](https://learn.microsoft.com/en-us/windows/win32/msi/msilaunchapp-controlevent), [NewDialog](https://learn.microsoft.com/en-us/windows/win32/msi/newdialog-controlevent), [WiX native modeless ProgressDlg reference](https://github.com/wixtoolset/wix/blob/main/src/ext/UI/wixlib/ProgressDlg.wxs). Matching documented authoring is component evidence, not proof of successful Windows clicking.

## One production brand master

`assets/brand/aitsnap.svg` is the production master; `tools/generate_brand.py` derives 16/20/24/32/40/48/64/128/256px PNGs and one ICO. Its derivation manifest records the master and output hashes. The icon is shared by Qt windows/tray, main/CLI/Word-bridge PE resources, MSI shortcut/ARP/wizard streams and Setup. File/product version resources are 0.7.0. Historical concept marks are not separate production inputs.

`tests/brand_tests.py` verifies ICO frames against their PNGs and optionally reads the actual Windows PE resources without executing the target. Main, CLI, Word bridge, Setup and the Qt5 main executable have nine exact matching frames. `tests/msi_wizard_tests.py` compares MSI Icon and Binary stream hashes with the same ICO. Qt appearance checks load small and large sizes. Explorer/taskbar appearance at every DPI and Owner brand approval remain separate visual gates.

## Branded setup entry

`aitsnap-setup` is a small Win32 launcher, statically linked to the C runtime; its observed imports are Windows system DLLs only. CMake embeds the MSI and its SHA256. The launcher checks its embedded bytes, writes a new temporary MSI, and invokes the system `msiexec.exe /i` with the ordinary full wizard. It does not download dependencies, implement a replacement wizard, request `runas`, or bypass computer-use restrictions. Temporary package cleanup is limited to the file/directory it created.

`--extract-to <new-or-empty-directory>` performs artifact extraction and returns before invoking Windows Installer. The extracted fixture was byte-identical through a Unicode/space path. This proves packaging mechanics only. The final release candidate must embed and verify the complete dependency payload, not the table fixture.

Test-only MSI authoring uses `--table-fixture`, a distinct product/upgrade identity and unconditional false LaunchCondition. Table-only manifests cannot be authored without this flag. Initial loose fixture bytes were retained under `.testdata` names after the guard was introduced; none was installed. They are never deliverables.

## Current evidence boundary

Brand/frame/resource checks, guarded MSI table checks, Setup extract/hash checks and Qt6/Qt5 appearance checks pass. A separate-config duplicate `--show` process exited 0 before physical Esc stopped Computer Use; the visible outcome of that action was not observed and is not credited. No further desktop input is authorized until Owner resumes it. The installed Owner application/configuration is unchanged.

Full new-install/upgrade/repair/uninstall, English/Unicode/space installation paths, unchecked launch, native first-frame timing and visible Finish launch remain HOLD. The current host is Win11; actual Win10/Win7/Word2010 qualification and Owner final visual acceptance remain HOLD. Independent source and full candidate packaging continue under the original cumulative budget.
