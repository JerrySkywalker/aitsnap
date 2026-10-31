# R04 architecture implementation admission

Base: R03 merge `a3709bf0bc4703b99d5ec329590731f4dc06ee1e`.
R00 requirements trace, R01 physical-pixel/scene probes, R02 native Word16
save/reopen proof and R03 context/contract tests are available. The architecture
is admitted for source implementation; this is not production or Owner acceptance.

## Reviewed invariants and resolution

| Review question | Frozen source contract | Evidence / gate |
| --- | --- | --- |
| Reusable core boundaries | C++20 standard types/ports; internal libraries, no Qt/COM/HWND in core | D00 core builds/tests; v142 still HOLD |
| Desktop pixel meaning | Physical half-open rectangles; native cursor; separate Qt paint coordinates | R01/D01/D02; real mixed-DPI/rotation HOLD |
| Capture vs scene | Frozen profile/context, immutable source artifacts, derived revisioned scenes | D01; D03 rerender tests required |
| Acquisition timing | One relevant display acquisition, per-display intervals/skew, no atomic multi-screen claim | R01/D01 baseline |
| Report isolation | Native separate STA helper, exact document path, durable queue, PREPARED/COMMITTED and UNCERTAIN recovery | R02 Word16 proof; D04 fault tests, Word2010 HOLD |
| Context provenance | Versioned catalog and run/step/invocation/attempt identity; source-event pair dedup; no transmitter | R03 semantic/context tests; D05/D06 pending |
| Local transport | Versioned bounded JSON, logon SID ACL, local name plus remote-client rejection | R03 contract; D06 platform negatives required |
| Platform acceptance | Qt6 modern and Qt5 shared source; native x64/x86 Win7 mandatory | Existing prevalidation and L00–L02 HOLD |

## Scene semantics pinned before D03

Source crop uses nonnegative source-local pixel x/y and positive width/height;
absence means the full source. Crop must fit retained image bounds. Desktop
regions may have negative global origins; source-local crop coordinates cannot.
Destination x/y/width/height describe the canvas box. Rotation is around its
centre. preserveAspect defaults true (fit centred within the box); explicit
false stretches. Draw order is `(zIndex,layerId)` ascending. Visibility/lock/group
are independent: lock constrains editor operations; groupId links move/alignment
operations without mutating source images. Background is sRGB #RRGGBB or trailing
alpha #RRGGBBAA. These optional fields complete the pre-implementation schema;
initial fixtures retain default behavior. Missing sources use explicit fail,
placeholder/incomplete, or skip policy; never reuse stale pixels silently.

Each saved batch manifest resolves artifact relativePath against its own
directory. Historical rerender reads only those immutable files, verifies
available hashes and writes a uniquely named derivation with a pinned scene
revision. Templates can be exported separately; no editor window is required
for rendering. PNG/BMP are lossless; JPEG export is explicitly lossy. Scene and
Word display copies remain derived and cannot replace original evidence.

## Version, dependency and acceptance policy

The six draft-2020-12 contracts are source v1 as amended by R03/R04. Unsupported
schema versions and unknown structural fields must fail explicitly and preserve
the input file; no silent downgrade/migration. Namespaced extensions are the
declared escape hatch. Semantically breaking changes require a new schema
version and an explicit migration path. Catalog sourceDigest is not imported-byte
verification; the actual import envelope digest is computed independently.

The 16-goal/120h DAG is acyclic. D03 now has D02 + R04 source predecessors;
human/native gaps do not create a false cyclic dependency. D04 follows D03/R02,
D05 follows D00/R03, D06 follows D01/D05, D07 follows D04/D06. L00 is independent;
L01/L02 cannot claim native proof from host preview builds. No platform scope
is silently removed and no new authorization is implied by a source dependency.

M1/M2 require human screenshot/layout review, M3 real report append/reopen and
human layout review, M4 a synthetic external event chain with no command
transmission, M5 native Win7 x64/x86, Win10/11 installation/runtime qualification
and Owner signoff. Existing automated M1 evidence is separate from human HOLD.
Word16 feasibility does not close Word2010, crash recovery or M3. Full v1 remains
HOLD; a verified modern development preview may be prepared independently.

Executed for this docs/schema scope: planning/DAG/link/schema validation and
semantic fixture validator. Reused R01/D00/D01/D02/R02/R03 native/core evidence;
no redundant Qt build or CI. All changed ADRs/contracts reviewed together; no
remaining source-contract contradiction blocks D03. Historical initialization,
Qt6 baseline and Win7 prevalidation files remain unchanged.
