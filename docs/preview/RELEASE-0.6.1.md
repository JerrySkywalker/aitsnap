# 0.6.1-preview — interactive installation and honest scene proportions

Adds a native Chinese MSI welcome page with software/version information, editable destination and folder browser, progress, success/failure/cancellation pages, repair/uninstall maintenance and an explicit launch-now checkbox. Launch is a UI-only Windows Installer MsiLaunchApp event; quiet installation, cancellation, failure and uninstall do not launch the application. Standard MSI transaction/payload handling stays in Windows Installer.

Fixes missing-source scene previews that always used a 640x400 placeholder. Placeholders now use region/crop proportions with bounded thumbnail allocation; they never masquerade as captured pixels. A placeholder thumbnail is not cropped again using source-pixel coordinates. Real-image rendering continues to preserve aspect by default; the destination frame can have a different ratio and show margins. Disabling aspect fitting explicitly stretches the image. The UI now explains that choice.

Regression coverage reads actual MSI dialog/control/event tables and catches malformed tab cycles, localization/path/terminal-dialog/launch conditions. Scene golden-pixel tests reproduce wide/portrait placeholder mismatch and verify the fix; existing actual-image aspect/stretch/crop checks are retained. Native double-click wizard interaction and launch are separate from quiet-install success. Tool refusal or missing target evidence remains HOLD, never inferred from table checks.

Target remains Windows10 1809+ x64 / Windows11 x64 candidate; actual Win10, Word2010/Win7 and Owner acceptance remain separate holds. Existing0.6.0 MSI, hashes and receipts remain immutable.
