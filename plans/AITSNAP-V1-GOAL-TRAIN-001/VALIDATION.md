# Documentation and planning validation

After merging the documentation PR, local Codex should run checks before R00:
1. Verify GOAL-MANIFEST.json parses, has exactly 16 unique goals, 120 budget hours and no missing/cyclic dependencies; each referenced goals/*.md path exists.
2. Parse all docs/contracts/*.schema.json as UTF-8 JSON; optional local draft 2020-12 validator checks six synthetic fixtures under tests/fixtures/synthetic-ait/*.json. JSON parsing or fixture existence alone does not prove schema correctness.
3. Verify docs/ARCHITECTURE-INDEX.md links and each Goal's inputs, tasks, deliverables, verification and hold.
4. Verify scope statement Win7 mandatory **native HOLD**, modern Qt6 baseline, Qt5 host preview not release, Linux out of scope, original historical evidence unchanged.
5. Confirm no real AIT command/telemetry, credentials, unique private machine path, bundled SDK or production OS images in public diff.
6. Document outcomes in a receipt. **Do not run full Qt build for docs-only PR**; previous product skeleton evidence is unchanged.

Semantic contract tests are implementation work under R03/D00, not falsely claimed in this documentation PR.
