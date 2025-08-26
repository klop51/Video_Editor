# DR-0004: Internal Timebase Strategy

Date: 2025-08-26
Status: Proposed

## Context
Accurate, drift-free representation of media and timeline time is essential for editing precision, A/V sync, keyframes, and effects scheduling. The project currently employs rational representations (`TimeRational`, `TimePoint`, `TimeDuration`). Decision required to confirm strategy and constraints before expanding to render graph, audio engine, and keyframe system.

Forces / Constraints:
- Media containers expose varying time bases (e.g., 1/1000, 1/90000). Audio often 1/48000 or 1/44100.
- Floating point seconds introduce cumulative rounding in long projects (hours) and keyframe alignment errors.
- Performance: conversions must be cheap; arithmetic on hot paths (playback scheduler) should avoid heap.
- Need deterministic serialization (stable textual/binary form, no locale float formatting).

## Options Considered
1. Fixed global tick (e.g., nanoseconds or 1/48000) stored as 64-bit integer.
   - Pros: Simple arithmetic.
   - Cons: Potential precision waste or overflow risk with extreme scales; conversion from unusual source rates may introduce rounding early.
2. Rational per value (current approach): store numerator + denominator preserving exact fractions.
   - Pros: Exact representation of source timestamps; no early rounding.
   - Cons: Slightly heavier storage (16 bytes), need normalized operations, more multiplications.
3. Mixed: global fixed tick for timeline; clips keep rational mapping for decode only.
   - Pros: Simplifies internal editing math, preserves source metadata for decode.
   - Cons: Dual system complexity; potential mismatch at boundaries.
4. Floating double seconds everywhere.
   - Pros: Simplicity.
   - Cons: Precision loss over long durations and high sample/frame rates.

## Decision
Adopt Option 2 (Rational per value) with defined normalization & optimization rules:
- `TimeRational{num, den}` remains canonical; ensure `den` > 0, store without automatic GCD reduction on hot path (lazy normalize when serializing or comparing for equality).
- Provide fast helpers for common denominators (e.g., 1000, 48000, 90000) with inlined operations.
- Timeline operations (insert, move, trim) use rational arithmetic; keyframe sampling may cache converted integer ticks for current playback rate segment.

## Rationale
- Preserves exact media frame/sample boundaries across heterogeneous sources.
- Avoids early commitment to a single master tick that could introduce rounding at ingest.
- Supports future conform workflows (retiming, varying frame rates) gracefully.

## Consequences
Positive:
- Exact timestamp math; no cumulative drift from repeated conversions.
- Flexible ingest of diverse formats.

Negative / Trade-offs:
- Slightly higher memory per time value vs 64-bit integer.
- Additional multiplications for comparisons (cross denominators) – need micro-optimizations in hot loops.

## Follow-ups
- Implement `normalize()` utility (optional GCD) for persistence & deterministic hashing.
- Add benchmarking of time comparisons vs fixed tick integer to ensure acceptable overhead.
- Provide conversion to/from common fixed tick (e.g., nanoseconds) for external APIs.
- Document guidelines: never store double except transient UI formatting.

## Metrics / Validation
- Benchmark: 1M time comparisons < 5 ms on reference CPU (ensure comparator optimized).
- Serialization roundtrip of varied denominators yields identical rational values.

## Alternatives Rejected
- Fixed tick: risk of subtle rounding for mixed-rate content.
- Mixed approach: added complexity without sufficient benefit at current scope.
- Double: precision risk for long-form projects and audio sync.

## Open Questions
- Evaluate small-object optimization or packed struct if memory pressure discovered (future profiling).
- Consider caching reciprocal denominators for repeated operations (micro perf DR later).

## References
- ARCHITECTURE.md Sections 6, 7 (time & model)
- Tests: time math & timeline invariants
