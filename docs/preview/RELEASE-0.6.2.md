# 0.6.2-preview — installer path controls visible on entry

Owner's actual0.6.1click-through exposed a first-paint defect on the destination page: neither the path edit nor Browse button appeared until clicked. The shared body-text rectangle ran from y67to177 while both controls occupied y150to172, so the opaque text background could paint over them. The destination text now ends at137. The progress-page body is also shortened so it cannot overlap the changing action text.

The actual-MSI regression now checks text rectangles against interactive controls. It fails on the retained0.6.1MSI with FolderDlg/Body-versus-Path and passes corrected geometry. This is an authoring check, not a substitute for the Owner's next visible first-paint/full-flow test. Original0.6.1bytes, logs and screenshot remain retained. The software keeps0.6.1's scene-placeholder proportion fix.
