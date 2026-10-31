# 0.6.0-preview — M3 recovery and Windows Installer candidate

Continues the original120-hour cumulative ledger after reboot; previous0.5.0 evidence/packages remain immutable. Owner requests an installer candidate first because no Windows10 target is currently available.

Report recovery now exposes an audited explicit retry for exhausted confirmed pre-mutation failures, preserving job identity, fingerprint and FIFO order. UNCERTAIN and COMMITTED jobs cannot enter this retry path. Failed jobs block subsequent writes to the same document; independent targets continue. Queue columns remain readable for long paths, with Chinese states and full details in tooltips.

Native Word16 tests cover recovered missing-helper jobs, mixed inline/table output, Unicode/space paths, committed replay, PREPARED uncertainty and immutable sources. A separate public long-form fixture is opened by actual Word:512pages, landscape page fit, exact already-open target while another document is active, protected/unsaved and unrelated-document preservation. Earlier fixture failures remain in local evidence.

The MSI uses the existing Windows Installer and CAB APIs, per-user assignment, embedded offline payload, Start menu, repair, major upgrade/downgrade detection and standard uninstall. Minimum OS comes from standard AppSearch/RegLocator system-version values; no custom OS-query DLL or toolchain install. Native Windows10 acceptance, legacy Word2010/Win7 and Owner visual acceptance remain HOLD.
