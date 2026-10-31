# ADR-0001: Modular monolith with Ports and Adapters
Status: Accepted for source implementation at R04; product gates separate · 2026-10-31

## Context
Standalone screenshot tool now; future AIT systems should reuse capture/scene/report/context without inheriting desktop UI or process lifecycle.

## Decision
One repository, C++20 Qt-neutral domain/application core, Qt Widgets host, typed ports and Windows/Qt/COM adapters. Components are internal CMake targets initially; IPC is local and optional. Avoid microservices, dynamic plugins and premature ABI promises.

## Consequences
Enables reuse and Qt5/6 builds with low deployment overhead. Architecture contract tests required; exact C++ target layout may evolve by scoped ADR. No separate research repository.
