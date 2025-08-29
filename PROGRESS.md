# Project Progress Snapshot

_Last updated: 2025-08-28_

This file summarizes current implementation status versus `ARCHITECTURE.md` and `ROADMAP.md`, plus the focused objectives for the next three sprints. Keep it short and update when major features land.

## Legend
| Symbol | Meaning |
|--------|---------|
| ✅ | Implemented / usable |
| 🟡 | Partial / prototype present (not feature‑complete) |
| ❌ | Not started |
| 🔬 | Planned experiment / spike in progress |

## High-Level Feature Status (MVP Scope)
| Area | Status | Notes |
|------|--------|-------|
| Multi-track timeline core | ✅ | Tracks, segments, add/move/remove, split, delete_range, gap insert, ripple delete_range. Advanced trim modes missing. |
| Edit operations (blade, insert, delete) | ✅ | Blade = split; basic ripple delete present. |
| Advanced edit tools (ripple/roll/trim, slip, slide) | ❌ | Not implemented as discrete commands yet. |
| Command system & undo/redo | 🟡 | Core + macro rollback & tests; coalescing/compression not added. |
| Timeline invariants tests | ✅ | Extensive added (ordering, overlap prevention, ripple). |
| Keyframes / parameter curves | ❌ | Data model & evaluation absent. |
| Basic effects (transform, opacity, crop, LUT, color wheels, curves, blur, sharpen) | ❌ | `fx` module placeholder only. |
| Transitions (crossfade, wipes) | ❌ | None yet. |
| Render graph / compositor (GPU) | 🟡 | Placeholder CPU graph + new `gfx` Vulkan bootstrap stub (instance/device). |
| Color management pipeline (linear + OCIO hook) | ❌ | Only rudimentary color convert helpers. |
| Audio engine (decode, mix, output) | 🟡 | Skeleton file; no active mixing graph; no meters or waveform. |
| Waveform generation & display | ❌ | Not started. |
| Real-time playback controller | 🟡 | Playback scheduling tests exist; no adaptive drop heuristics exposed. |
| Proxy workflow | ❌ | No generation or switching logic. |
| Export (H.264/HEVC + audio) | ❌ | Not started. |
| Persistence (JSON project) | 🟡 | Serializer + unit tests; versioning/migration & binary format TBD. |
| Autosave & crash recovery | ❌ | Not implemented. |
| Caching (frame cache) | 🟡 | Basic cache & tests; eviction policy sophistication pending. |
| Performance profiling infra | 🟡 | Scoped timers exist; no aggregation/report artifact. |
| Fuzz / stress tests | 🟡 | Random macro command test only; broader fuzz & stress harness absent. |
| Golden image / frame tests | ❌ | Not started. |
| Decision Records (DRs) | 🟡 | DR-0001 (GPU), 0002 (vcpkg accepted), 0003 (exceptions), 0004 (timebase) present. |
| CI build & unit tests | ✅ | Green locally with 214 assertions (expanded profiling & playback tests). |

## Technical Debt / Gaps (Top 10)
1. No GPU render path or effect DAG — blocks effects, transitions, color.
2. Missing keyframe system — blocks animated parameters.
3. No audio mixing graph or master-clock enforcement — limits A/V sync fidelity. 
4. Lack of export pipeline. 
5. No proxy or adaptive playback quality system. 
6. Missing advanced trim / slip / slide / roll commands. 
7. No autosave / crash recovery. 
8. Insufficient caching policies & memory pressure response. 
9. Absence of performance metrics aggregation & KPIs. 
10. Enforcement tooling for DR policies (no-throw audit script, time normalization use) incomplete.

## Near-Term Strategic Focus
Establish vertical slice from decode → timeline edit → (shim) render path → audio → export. GPU work temporarily slowed by MSVC namespace/compiler conflicts (see Risk Watch); focus on de-risking playback correctness & profiling while isolating graphics issues.

## Upcoming Sprints (Assuming 2-week cadence)

### Sprint 1 (Current → +2 weeks) : "Foundations for Visual & Audio Pipeline"
Objectives:
- Decide & record DR: GPU API abstraction choice (DONE: DR-0001).
- Introduce minimal render graph interface (Node, RenderContext, hash placeholder) (PARTIAL: basic graph + nodes placeholder, no context yet).
- Implement YUV→linear upload + simple passthrough/transform shader (even if mocked CPU for now if GPU deferred).
- Audio engine: basic decode → mixing to null/output sink; adopt audio as master clock in playback controller.
- Command coalescing (drag move/trim) simple time-threshold.
- Create DRs: dependency manager (vcpkg), timebase rationale, exception policy draft (DONE: DR-0002 accepted earlier, DR-0003, DR-0004 proposed).
- Add PROGRESS.md (done) + baseline performance JSON aggregator capturing frame decode & edit op timings.
Acceptance Criteria (updated progress inline):
- Render graph can request a frame through a node chain (even 2 nodes) returning a texture/placeholder. (NOT YET: stub only)
- Audio playback of single track synchronized within <50 ms drift vs video over 60s test (manual acceptable). (PENDING)
- Coalesced undo reduces drag sequence commands by >80% in synthetic test. (PENDING)
- GFX Vulkan bootstrap stub builds even without Vulkan SDK (DONE).
- expected<T,E> utility in core for exception-free APIs (DONE).
- Time rational normalize() + tests (DONE).
- Playback controller advances time on cache hits (FIXED: regression resolved 2025-08-27; added tests increasing assertion count).

### Sprint 2 (+2 → +4 weeks) : "Effects & Keyframes Seed"
Objectives:
- Keyframe curve structure (linear interpolation) & evaluation API.
- Implement transform + opacity effects as render nodes using keyframes.
- Crossfade transition node integration (timeline insertion model + render blending).
- Performance aggregation enhancements: export JSON includes p95 frame time & edit latencies.
- Extend timeline commands: ripple trim (edge extend + ripple), roll edit skeleton.
- Begin waveform extraction (CPU downsample) & store tiles to memory (no disk persistence yet).
Acceptance Criteria:
- Keyframed opacity on a clip renders varying alpha over time.
- Crossfade between two adjacent segments renders expected blend (manual QA).
- p95 frame render metrics generated after a scripted playback session.

### Sprint 3 (+4 → +6 weeks) : "Export & Proxy Introduction"
Objectives:
- Export pipeline: offline frame pull + encode stub (write sequential raw frames or integrate x264 if ready).
- Proxy generation stub: background task creates 1/4 res frames (CPU scale) for opened media; selection logic chooses proxy if available.
- Add slip & slide edit commands with invariants tests.
- Implement basic frame cache LRU capacity with eviction metrics.
- Autosave timer writing project JSON snapshot; manual recovery path.
- DR: Proxy policy & file layout.
Acceptance Criteria:
- Export produces playable MP4 (if encoder integrated) OR raw frame sequence & WAV (if not yet grouped) for 10s test timeline.
- Proxy chosen automatically when toggled to "performance" mode; log indicates switch.
- Autosave file appears at interval and can be loaded after simulated crash.

## Risk Watch (Current Top)
| Risk | Status | Mitigation Next Step |
|------|--------|----------------------|
| GPU path delay stalls effects (MSVC namespace conflicts in gfx) | Elevated | Isolate gfx build, reproduce in minimal TU, consider temporary rename of namespace &/or switch to Vulkan SDK validation branch |
| Audio engine complexity underestimated | Open | Start minimal mixing + master clock early (Sprint 1) |
| Lack of metrics hides regressions | Open | Add JSON profiling aggregator Sprint 1 |
| Export licensing (x264/x265) | Open | Start with software x264; isolate licensing in DR |

## Decision Records Status
| DR ID | Topic | Status |
|-------|-------|--------|
| DR-0001 | GPU API choice / abstraction scope | Proposed |
| DR-0002 | Dependency manager (vcpkg) | Accepted |
| DR-0003 | Exception & error handling policy | Proposed |
| DR-0004 | Rational timebase strategy | Proposed |
| DR-0005 | Proxy asset storage & invalidation strategy | Planned |

## Maintenance Instructions
- Update status table when a row advances (replace symbol, add concise note with date).
- Close the top risks or move to an archive section when mitigated.
- After each sprint, append a short "Sprint N Outcome" subsection (3–5 bullets) before defining the next sprint.

---
Ownership: Update alongside major feature PRs; reviewers ensure consistency with `ARCHITECTURE.md` & `ROADMAP.md`.

### Sprint 0 Outcome (Bootstrap)
- Extended timeline invariants; initial test suite green (155 assertions then, now 214 after added playback & profiling tests).
- Established DR framework with initial records (GPU, exceptions, timebase, vcpkg).
- Added `expected` utility & enforced no-throw policy note in `.clang-tidy`.
- Implemented time rational normalization + tests.
- Introduced `gfx` module with Vulkan bootstrap stubs compiling w/o SDK.
- Fixed early playback stagnation bug (time not advancing on cache hits) prior to Sprint 1 objectives – increased reliability baseline.
