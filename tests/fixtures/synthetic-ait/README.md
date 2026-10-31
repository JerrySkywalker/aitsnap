# Fully synthetic AIT workflow fixtures

These files describe an entirely fictitious power-system test, not real flight/test commands. A command catalog has two command definitions and one sequence. The example event is a second attempt during a second run; its event ID links to CaptureRequest and immutable CaptureBatch, which links two source artifacts, one derived scene image, and a ReportJob.

- JSON Schema shape validation: see docs/contracts/*.schema.json.
- Semantic test cases to implement during R03/D05: rerun same sequence under RUN-SYN-001 and RUN-SYN-002; retry CMD-READ-V under same StepExecution; duplicate EVT-SYN-017 must not create a second screenshot; out-of-order source sequence numbers remain detectable; ambiguous missing run ID must not silently attach to latest run.
- sourceDigest uses 64 zero characters purely as a **placeholder** satisfying the schema pattern; do not use as a verified content digest.
- Scene positions, times, file paths and filenames are demonstrative; no actual images exist in the repository fixtures.

Do not add proprietary test instructions, satellite names/parameters, private facility logs or live external connections to this directory.
