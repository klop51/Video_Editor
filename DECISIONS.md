# Decision Records (Architecture & Process)

> Each record: immutable once accepted (only supersede). Use incremental IDs.

## Format
```
## DR-<ID>: <Short Title>
Date: YYYY-MM-DD
Status: Proposed | Accepted | Superseded (DR-<ID>) | Deprecated
Context:
- (Problem statement, forces, constraints)
Options Considered:
1. Option A – pros/cons
2. Option B – pros/cons
Decision:
- Chosen option & concise rationale.
Consequences:
- Positive: ...
- Negative / trade-offs: ...
Follow-ups:
- Tasks or metrics to validate decision effectiveness.
```

## Index
| ID | Title | Date | Status | Supersedes |
|----|-------|------|--------|------------|
| DR-0001 | (placeholder – GPU API selection) | (TBD) | Proposed | - |
| DR-0002 | Dependency Manager: vcpkg | 2025-08-23 | Accepted | - |
| DR-0003 | (placeholder – Exception Policy) | (TBD) | Proposed | - |
| DR-0004 | (placeholder – Internal Timebase Strategy) | (TBD) | Proposed | - |

## Templates
Start a new record by copying the format above beneath this line.

## DR-0002: Dependency Manager: vcpkg
Date: 2025-08-23
Status: Accepted
Context:
- Need simple, Windows-first dependency management supporting FFmpeg, Qt, Catch2, fmt, spdlog with minimal authoring.
- Want manifest mode for reproducible CI and caching.
Options Considered:
1. vcpkg (manifest) – seamless Windows integration, good library coverage, GitHub Actions cache helpers.
2. Conan – stronger multi-profile & lockfile story, but more authoring overhead early.
3. Vendoring – fastest bootstrap, but maintenance & license complexity, slower iteration.
Decision:
- Use vcpkg manifest mode (already added `vcpkg.json`).
Consequences:
- Positive: Rapid onboarding, CI cache support, minimal scripts.
- Negative: Larger initial install footprint; some version lag for latest libs.
Follow-ups:
- Pin baseline versions in `vcpkg-configuration.json` (future).
- Evaluate Conan if complex version matrix appears (DR later if needed).
