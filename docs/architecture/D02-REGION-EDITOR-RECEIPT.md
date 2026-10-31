# D02 physical region editor

Base: D01 merge `831ba3895793eb02493560428f7fec5010e084cd`.
Adds per-display editor-only frozen backgrounds, creation on blank desktop,
selection/movement, four corner resize handles, Tab selection, arrow-key 1px
and Shift+arrow 10px movement. Native GetCursorPos drives all edit geometry;
Qt logical pixels only paint the preview. Mouse grab permits a spanning drag.
Names, enable flags and exact numeric sizes remain editable in the region table.
Completing updates the selected profile by stable ID, revalidates topology and
persists the new revision; cancel preserves the previous region geometry.

The host hides before background acquisition. Capture requests are rejected
throughout editing and compositor-close delay. Both overlay windows are hidden
and destroyed before capture is enabled again. No editor backgrounds are saved.
Missing/rearranged monitor checks from D01 remain in the capture service.

## Executed evidence

Windows 11 build 26340 x64, v143 19.44, SDK 10.0.26100; Qt6.12 x64 and
Qt5.15.2 v143 x64 host preview builds passed. Core tests passed for physical
negative-origin/reverse selection, spanning movement, bottom-right resize and
dragging through the opposite corner. Existing capture/format/hotkey tests pass.

`aitsnap-desktop-tests --native --editor --evidence <ignored-output>` passed on
both Qt majors: five exact synthetic regions on two monitors, files/clipboard/
focus, two live editor windows, capture request rejection during editing,
Escape cancellation, geometry roundtrip and no remaining visible overlay.
An accept-after-profile-switch regression also verifies the editor commits to
its original stable profile ID without overwriting the newly selected profile.
This tests the product lifecycle using synthetic component events; it does not
substitute for real mouse input or human visual acceptance. Initial build failure
from a helper named cursor colliding with QWidget::cursor was fixed by renaming
it physicalCursor. The original failing build log remains in ignored output.

The main window and entry button were inspected through computer-use, including
the corrected dark-theme text. The runtime stayed responsive with both editor
windows open, but that automation tool did not enumerate Qt Tool windows; no
overlay screenshot or drag visual PASS is claimed. The runner closed only its
own unmodified preview process after this observation. No system setting,
Explorer restart, unauthorized VM action or public screenshot upload occurred.

## Holds and review pack

Owner should create overlapping and spanning regions, drag all corner handles,
check displayed width/height against saved PNGs, disable a source, rename it,
save/restart, then replug/rearrange displays and explicitly recalibrate. Exercise
mixed-DPI 100/125/150%, rotation and third-display cases only when available.
These real pointer/topology cases remain OWNER_HOLD/NOT_RUN; native Win7/v142
and Win10 remain HOLD. x86 desktop integration is scheduled for the M2 matrix.
Full changed source reviewed locally; no independent read-only acceptance, full
CI or dependency installation. Next: R02/R03/R04 contracts, then D03 composition.
