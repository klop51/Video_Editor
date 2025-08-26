# DR-0003: Exception & Error Handling Policy

Date: 2025-08-26
Status: Proposed

## Context
Consistent strategy for signaling errors is needed across modules (timeline, media IO, decode, future render graph) to balance performance, clarity, and testability. C++ exceptions introduce potential hidden control flow and cost (stack unwinding, binary size) but can simplify early prototyping. Some subsystems (real-time playback, render loop) require predictable latency and should avoid unexpected throws.

Constraints / Forces:
- Real-time paths (decode, playback, audio) need minimal jitter.
- Library dependencies (FFmpeg) return error codes; bridging to exceptions adds wrapping overhead.
- Desire for explicit error propagation in command/edit operations for undo reliability.
- Future plugin boundary may forbid exceptions across ABI.

## Options Considered
1. Enable exceptions globally; use them for recoverable & unrecoverable errors.
2. Enable exceptions but restrict usage to initialization / non-real-time code paths.
3. Disable exceptions (compile with /EHs-c- or -fno-exceptions) and use `expected<T,E>` or status codes.
4. Hybrid: core modules exception-free; higher layers (UI/app) may throw internally but catch at boundaries.

## Decision
Adopt Option 4 (Hybrid):
- Core performance-sensitive modules (`media_io`, `decode`, `playback`, `audio`, `render`, `fx`, `cache`) are exception-free in public API: use `expected<T,E>` (custom lightweight) or explicit status structs.
- Non-real-time layers (`app`, `persistence`, `ui`, high-level command orchestration) may use exceptions sparingly for construction-time or fatal configuration failures, but must translate to structured error results at module boundaries.
- No exceptions cross module DLL boundaries or future plugin ABI.

## Rationale
- Predictable latency in real-time code by eliminating unexpected unwinds.
- Explicit error types improve test coverage & force handling (no silent swallow).
- Maintains developer ergonomics in configuration / initialization where performance not critical.
- Simplifies future move to `std::expected` when available (C++23/26 adoption).

## Consequences
Positive:
- Clear contract: performance layers never throw → easier reasoning & profiling.
- Enables centralized logging of error categories without catch-all handlers.
- Aligns with plugin ABI stability plans.

Negative / Trade-offs:
- Slight verbosity versus try/catch in non-real-time paths.
- Need custom lightweight `expected` until standard available (temporary code to maintain).
- Must enforce via tooling (clang-tidy rule) to prevent accidental throw in core modules.

## Follow-ups
- Introduce `core/expected.hpp` with minimal implementation (value, error union; no exceptions).
- Add clang-tidy configuration to flag throw usages in restricted directories.
- Update CONTRIBUTING / ARCHITECTURE to reference policy.
- Refactor existing code (if any) throwing inside performance modules (audit).

## Metrics / Validation
- Zero `throw` statements in `src/{media_io,decode,playback,audio,render,fx,cache}` after audit.
- Unit tests for expected error paths (e.g., failing demux, invalid timeline edit) using `expected`.

## Alternatives Rejected
- Global exceptions: unpredictable latency & plugin boundary issues.
- Fully disabling exceptions project-wide: reduces ergonomics for UI/app; higher barrier for rapid prototyping.

## Open Questions
- Should we escalate fatal errors (e.g., out-of-memory on GPU) via immediate abort or escalate through central error bus? (Future DR if needed.)
- Need policy for logging spam suppression (rate limit repeated errors) – separate concern.

## References
- ARCHITECTURE.md: Sections 2 (latency), 11 (concurrency), 15 (undo reliability)
- ROADMAP.md: Performance / stability phases
