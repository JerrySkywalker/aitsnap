# ADR-0005: Read-only AIT command and sequence linkage
Status: Accepted for source implementation at R04; M4 human HOLD · 2026-10-31

## Decision
Import static command/sequence definitions with provenance; accept dynamic run, step execution, command invocation/attempt and event IDs. Correlate screenshots without ever sending commands. Any connection to AITC scheduling is optional metadata, distinct from authoritative executor events. Unknown/ambiguous correlation stays explicit.

## Consequence
Synthetic open-source fixtures only. Local-only IPC with ACL, idempotent external events, bounded event processing, per-stage clocks; no raw spacecraft command contents or proprietary records checked into Git.
