# Roadmap and allocated maximum time

Planning budgets, **not completion estimates**. Count actual cumulative elapsed time across retries and sessions.

| Goal | Track | Hours | Depends on | Outcome |
| --- | --- | ---: | --- | --- |
| [R00](goals/R00.md) | Research | 3 | — | Requirements and reusable architecture |
| [R01](goals/R01.md) | Research | 5 | R00 | Multi-monitor capture and canvas feasibility |
| [R02](goals/R02.md) | Research | 4 | R00 | Word COM and report-job feasibility |
| [R03](goals/R03.md) | Research | 5 | R00 | AIT sequence, event and IPC contracts |
| [R04](goals/R04.md) | Research | 3 | R01, R02, R03 | Architecture freeze and review |
| [D00](goals/D00.md) | Development | 6 | R00 | Qt-neutral reusable domain and tests |
| [D01](goals/D01.md) | Development | 15 | D00, R01 | Working multi-region capture, hotkeys and tray |
| [D02](goals/D02.md) | Development | 8 | D01 | Multi-monitor region editor |
| [D03](goals/D03.md) | Development | 16 | D02, R04 | OBS-style scene editor and composition |
| [D04](goals/D04.md) | Development | 13 | D03, R02 | Word COM reporting and durable queue |
| [D05](goals/D05.md) | Development | 8 | D00, R03 | Static catalog import and runtime event correlation |
| [D06](goals/D06.md) | Development | 8 | D01, D05 | Local CLI/IPC reusable host integration |
| [D07](goals/D07.md) | Development | 8 | D04, D06 | Performance, installer and product consolidation |
| [L00](goals/L00.md) | Legacy | 6 | — | Win7 native compiler/SDK/CRT/VM qualification |
| [L01](goals/L01.md) | Legacy | 5 | L00, D01, D02 | Win7 native screenshot and tray qualification |
| [L02](goals/L02.md) | Legacy | 7 | L01, D07 | Win7 full scene/Word/IPC and release acceptance |
| **TOTAL** | | **120** | | **16 goals** |

## Priority and parallelism
1. Run R00/R01 -> D00/D01 first for a usable one-key screenshot and tray MVP. R02 and R03 may proceed after R00 while early development continues; R04 collects them before D03.
2. D00 -> D01 -> D02 -> D03 -> D04. D05 needs only D00+R03, not Word. D06 needs D01+D05; D07 needs D04+D06.
3. L00 is independent but requires authorized Win7 targets/toolchain; L01 after L00+D01+D02; L02 after L01+D07. When blocked, ledger records actual blocker and source work proceeds; native support stays HOLD.
4. Human visual M1–M5 are separate acceptance points. The runner can prepare a reproducible review pack and continue other eligible goals but cannot self-sign owner inspection.

## Budget operating policy
- At first run record UTC start, authoritative Git SHA, allocated hours, measured spent and remaining in an ignored durable ledger.
- Preserve cumulative consumption across restarts; do not restart 120h clock. Per-goal cap triggers PARTIAL with receipts and permits next nondependent goal.
- Unspent Legacy hours caused by authorized-environment HOLD may be used on higher-priority eligible product work only if documented and within 120h total; not to assert legacy PASS.
- Prefer targeted changed-scope tests, reuse valid unchanged build evidence, and avoid redundant CI/slow defensive refactors.
- Stop at global cap or no safe work. Missing local session continuity is a limitation, not reason to promise 120h uninterrupted activity.

## Release line
A Win10/11-only development preview may be prepared when locally verified. Full v1 declared for Win7/10/11 requires L02 native x64+x86 and owner M5. No host Qt5 preview can substitute.
