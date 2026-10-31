# D00 Qt-neutral core

Base: R01 merge `517f5416f944b4216f7933e9ea8bd6602bf49801`.
The `aitsnap-domain`, `aitsnap-capture` and `aitsnap-report` static libraries
contain only C++20 standard library types. Scene and context value types live
in domain until real services justify separate targets. No empty libraries or
binary ABI are introduced. Desktop adapters own encoding, JSON, storage, COM
and GUI lifecycle; core APIs expose no Qt, HWND or COM type.

CaptureService accepts a frozen request, acquires each relevant display once,
and assembles exact physical regions from immutable owned BGRA8 frames. Coverage
is checked by row interval union, so monitor overlaps cannot conceal gaps.
Topology mismatch, invalid geometry, acquisition and output errors are explicit.
Report states serialize by stable names; version and state guards reject
unsupported migrations and terminal-state retries. JSON adapters follow with
the corresponding product services rather than adding a JSON library dependency.

## Verification

Windows 11 x64 build 26340; v143 19.44; SDK 10.0.26100. Qt6.12.0 x64 plus
Qt5.15.2 v143 x64 and x86 host-preview configure/build/regressions executed.
The core CTest runs without QApplication, Qt DLLs or COM; exercises spanning
known pixels, overlap acquisition reuse, disconnected topology, uncovered gap,
duplicate IDs, schema guard, reserved filename, invalid scene transform,
report transitions and retained immutable source data.

An initial CTest invocation ran before the asynchronous build had finished and
reported executable missing / NOT_RUN. It was not a product failure or a pass;
the subsequent invocation after successful build is the executed test evidence.
Build/test raw logs remain in ignored program output. No full CI, new package
install, vcpkg update or native Win7 execution was performed. Historical evidence
files remain unchanged. Full changed source was locally reviewed, including
64-bit rectangle arithmetic and source-buffer ownership. This is same-session
review, not independent read-only acceptance.

Native v142/Win7 x64/x86 remain HOLD. Human M1/M2 remain pending because this
goal does not implement the interactive product. Next: D01.
