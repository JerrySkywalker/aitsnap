# R02 Word bridge feasibility and recovery contract

Base: D02 merge `ae2d294ece7b2b5ef636d2776040acee35824477`.
Windows 11 x64 build 26340; Qt6.12/v143 x64; interactive user session.

## Observed route

Word.Application is registered; the native probe reports Word 16.0. Direct C++
IDispatch/COM STA activation, a fresh synthetic document, two embedded inline
images/captions, a document-variable marker, explicit DOCX save, close/reopen,
image count/marker readback and normal helper exit all passed. No existing report
was opened. ZIP/XML inspection additionally checks two drawing nodes, embedded
media rather than external image links, caption order and the persisted marker.
This proves this small modern-Word workload, not Word2010 or long-form layout.

Reproduce with `AITSNAP_BUILD_RESEARCH_PROBE=ON`, target `aitsnap-word-probe`,
then `aitsnap-word-probe <new-ignored-output-root>` in an interactive user session.
The probe creates a UUID subdirectory, its own blue PNG and new DOCX. Run under
a bounded parent process; Office may block indefinitely. The reusable thin
`word_dispatch.h` adapter owns BSTR/VARIANT/IDispatch references and uses the
installed Windows COM runtime, without an Office PIA or extra installation.

## Failed routes retained

The first PowerShell STA probe exceeded its observation window (99 seconds
observed), produced no successful receipt/DOCX and was stopped. A bounded
instrumented run then failed before Word because Get-FileHash was unavailable
in that child environment. A .NET-hash version reached Word16 and inserted two
images but failed to finish save/reopen within 45 seconds. Original logs and
synthetic PNGs remain in ignored output; these are not passes. The precise
PowerShell save/binding stall is unresolved. No Office/OS install, permission
change or broad Word kill was used; only probe-created processes were closed.

Switching to direct native dispatch with explicit OpenXML format 12 completed
in under the 45-second limit. This changes both the COM binding and explicit
format choice; the result does not isolate which caused the PowerShell stall.
The product bridge will use this native path; the PowerShell research probe
remains as failed-route reproducibility evidence, not a runtime dependency.

## D04 bridge and queue contract

- A separate user-session helper initializes COM STA and owns all Word objects.
  The screenshot process never waits for COM. Persist a queued job after saving
  source images, then atomically mark RUNNING before launching the helper.
- Bind the exact canonical target path, never ActiveDocument. For an already
  open target, bind that document without closing other user documents. For a
  new document, require an unused path; Word SaveAs2 itself can overwrite files.
  Missing Office, read-only targets and busy/rejected calls produce bounded,
  separately visible errors. Never substitute a different foreground document.
- Embed images using AddPicture with LinkToFile=false and explicit collapsed
  end ranges. Preserve source files. Add ordered captions and page-fit inline
  dimensions; tables/large workloads require D04 checks.
- Use a job-ID document variable with PREPARED and COMMITTED phases. Persist
  PREPARED before image mutation; save the complete insertion and COMMITTED
  marker together. A reopened COMMITTED marker acknowledges a retry without
  re-insertion. PREPARED, timeout, partial save or contradictory recovery becomes
  UNCERTAIN; never blindly insert again. Only failures proven to precede any
  mutation are automatically RETRYABLE, with bounded attempts.
- Word file mutation and queue acknowledgement are not a distributed transaction.
  User edits, marker removal and interrupted saves can require inspection. A
  RUNNING job found on restart is UNCERTAIN until its exact document is checked.
  No exactly-once guarantee is inferred from this small marker roundtrip.

## Primary references and holds

[AddPicture](https://learn.microsoft.com/en-us/office/vba/api/word.inlineshapes.addpicture),
[Document variables](https://learn.microsoft.com/en-us/office/vba/api/word.document.variables),
[InsertAfter](https://learn.microsoft.com/en-us/office/vba/api/word.range.insertafter),
[Documents.Open](https://learn.microsoft.com/en-us/office/vba/api/word.documents.open),
[SaveAs2](https://learn.microsoft.com/en-us/office/vba/api/word.saveas2),
[PSModulePath inheritance](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_psmodulepath).
These were checked against current primary documentation during research.

Word2010 x86/x64 on native Win7, large documents, busy/protected targets,
crash-recovery insertion and visual page-fit remain NOT_RUN/HOLD. The native
small-document proof enables source development; M3 and full v1 are not accepted.
Source and probe receipts were reviewed locally; no independent acceptance or
full CI. Native Qt5 bridge regression belongs to D04/M2 shared-code integration.
