# Video Editor – Detailed Roadmap (Living Document)

> Granular breakdown expanding section 20 of `ARCHITECTURE.md`. Update whenever scope, estimates, or sequencing changes. Keep concise & actionable.

## Legend
- 🟢 Done / on track  - 🟡 At risk  - 🔴 Blocked  - ⚪ Not started
- (M#) = Milestone tag candidate
- DR = Decision Record reference (see `DECISIONS.md` once created)

## 0. Foundation & Tooling (Weeks 0–2)
Goals: Compile skeleton, logging, media probe, CI baseline.
Deliverables:
1. CMake project with warnings-as-errors & per-module targets. (Exit: `cmake --build` succeeds clean)
2. vcpkg integration (FFmpeg, spdlog, fmt, Catch2). (Exit: cache restore in CI < 5 min)
3. Media probe CLI: list streams, codec, duration, color primaries. (Exit: `media_probe sample.mp4` JSON output)
4. Logging subsystem (`core/log.h`) with structured + text sinks.
5. Basic Continuous Integration: build + run unit tests on Windows.
Risks & Mitigation:
- FFmpeg linkage complexity → pin known version; add DR once chosen.
- Toolchain differences → create `ci/toolchain.cmake`.

KPIs:
- Clean build time (Debug) ≤ 3 min on ref dev box.
- Media probe completes on 10 sample files w/o crash.

## 1. Decode & Playback Prototype (Weeks 2–6) (M0.1-prototype)
Goals: Decode single clip, present frames, measure baseline perf.
Deliverables:
1. `media_io` demux abstraction returning packets.
2. `decode` module: software video decode (H.264) to CPU RGBA buffers.
3. Minimal playback loop (headless or console FPS counter).
4. Frame timing & basic drop strategy (log when behind > 2 frames).
5. Performance capture harness writing JSON metrics.
Stretch: Hardware decode abstraction stub.
Exit Criteria:
- 4K30 H.264 clip decodes & plays ≥ 28 fps average (software path) on ref GPU/CPU.
- No memory leaks (validated by instrumentation / ASAN locally on Linux later).

Risks:
- Frame conversion cost → introduce sws_scale baseline; later SIMD path.

## 2. Timeline & Command System (Weeks 6–9)
Goals: Immutable snapshot timeline model + edit operations programmatic.
Deliverables:
1. Core data structures (`timeline/`): Track, Segment, Timeline, Project.
2. Command pattern with undo/redo + coalescing policy.
3. Basic edit ops: insert clip, ripple delete, blade.
4. Unit tests for time math & edit invariants (no overlap, ordering).
5. Serialization draft (JSON) for simple project with 1–2 tracks.
Exit Criteria:
- 100% unit tests for timeline core pass.
- Insert + ripple delete p95 latency < 3 ms (synthetic 1000 segment track).

## 3. Early UI Shell (Weeks 9–12)
Goals: Qt window, playback surface, basic transport controls.
Deliverables:
1. Qt app skeleton (app layer initialization, logging panel).
2. Render surface placeholder (CPU blit; GPU later).
3. Transport: open media, play/pause, current time display.
4. Central event loop integration with playback thread.
5. Crash recovery stub (session autosave timer writing checkpoint JSON).
Exit Criteria:
- User can open a clip & play/pause with stable UI (< 5% dropped input events under playback load).

## 4. GPU & Render Graph Foundation (Weeks 12–16) (M0.5-playback)
Goals: Introduce GPU path & simple render graph.
Deliverables:
1. Chosen graphics API abstraction decision (DR#GPU-API).
2. Swapchain & frame pacing control (vsync toggle).
3. Upload decoded frames to GPU textures.
4. Basic shader pipeline (YUV→linear, simple brightness effect).
5. Hash-based node memoization skeleton.
Exit Criteria:
- Brightness effect applied in real time to 4K30 clip w/ ≤ 14 ms avg frame render.

## 5. Frame & Decode Caching (Weeks 16–19)
Goals: Smooth scrubbing & playback resilience.
Deliverables:
1. Decode lookahead queue with priority (playhead ± range).
2. RAM frame cache (LRU + size budget config).
3. Cache metrics reporting (hit ratio, evictions).
4. Configurable proxy generation stub (no transcode yet) returning scaled decode.
Exit Criteria:
- Scrub latency (seek to new position) first frame < 120 ms average.
- Frame cache hit ratio ≥ 60% during linear playback.

## 6. Basic Editing UI & Keyframes (Weeks 19–23)
Goals: Visual timeline, selection, trim tools, keyframe curves for one effect.
Deliverables:
1. Timeline panel (tracks stacked, segments drawn, basic zoom).
2. Mouse interactions: select, drag, blade.
3. Trim operations hooking commands.
4. Keyframe curve editor for brightness parameter.
5. Undo visualization (history panel).
Exit Criteria:
- Perform 50 mixed edits w/o crash; undo stack integrity verified.

## 7. Effects & Transitions Set (Weeks 23–27)
Goals: Initial library of core effects/transitions.
Deliverables:
1. Effects: transform (position/scale/rotate), opacity, crop, LUT apply, Gaussian blur, sharpen.
2. Transition: crossfade + wipe (direction param).
3. Effect parameter animation generalized from brightness.
4. Render graph optimization pass (fuse color ops).
Exit Criteria:
- Chain of 5 effects + 1 transition on 4K clip real-time ≥ 24 fps.

## 8. Audio Engine & Waveforms (Weeks 27–31) (M0.8-audio)
Goals: Basic audio decode, mixing, waveform display.
Deliverables:
1. Audio decode path (shared design with video for demux).
2. Mixing graph (gain/pan nodes) + master output via OS backend (WASAPI initial).
3. Waveform extraction pipeline w/ multi-resolution tiles.
4. Audio-driven master clock adoption & A/V sync test.
5. Basic meters in UI.
Exit Criteria:
- A/V sync drift < 10 ms over 10 min playback test.

## 9. Export Pipeline (Weeks 31–35)
Goals: Encode timeline to H.264/HEVC + WAV.
Deliverables:
1. Render to export graph (separate from interactive preview) w/ progress callbacks.
2. x264 software encode integration; HEVC optional if available.
3. Audio encode (AAC or PCM/WAV initially depending on licensing choice).
4. Preset system (quality, bitrate, resolution scaling, FPS conversion).
5. Cancel & resume (checkpoint segments or GOP boundaries) stretch.
Exit Criteria:
- Export 60s 4K clip @ target bitrate; real-time factor ≥ 0.7× (software) reference.

## 10. Proxy & Performance Tuning (Weeks 35–39) (M1.0-mvp Candidate)
Goals: Proxy generation, performance polish for MVP.
Deliverables:
1. Background proxy transcode (resolution ladder: 1/2, 1/4).
2. Dynamic playback quality switching based on frame budget.
3. Performance dashboards (internal UI or JSON -> HTML script).
4. Memory pressure adaptation (shrink caches gracefully).
5. Crash recovery restore path (autosave reopen prompt).
Exit Criteria:
- 4K60 multi-track (3 video + 2 audio) simple timeline plays ≥ 50 fps.
- Proxy generation throughput ≥ 3× realtime at 1/4 res.

## 11. Hardening & Pre-Beta (Weeks 39–48)
Goals: Stabilize, fill gaps, automated regression safeguards.
Deliverables:
1. Golden frame image tests (hash/perceptual) for sample project set.
2. Fuzz harnesses for demux & edit sequences.
3. Stress tests (long timeline, high track count) metrics.
4. Crash reporting pipeline stub (opt-in) writing dumps & JSON metadata.
5. Documentation pass: developer quick start, contribution guide, user getting started.
Exit Criteria:
- Zero known P1 bugs; < 10 open P2.
- Test suite deterministic in 5 consecutive CI runs.

## 12. Post-MVP Extensions (48w+)
Outline only (prioritize later):
- Advanced color (OCIO config integration complete, scopes)
- Additional transitions & effects (glows, keying, stabilizer)
- Plugin SDK design & prototype (C ABI)
- Python scripting (pybind11) for batch ops
- Collaboration features (review annotations)

## Milestone Summary Table
| Milestone | Target Week | Exit Snapshot Tag | Criteria (Abbrev) |
|-----------|-------------|-------------------|-------------------|
| Prototype (M0.1) | 6 | v0.1-prototype | Single-clip playback + effect ≥28 fps |
| Playback Core (M0.5) | 16 | v0.5-playback | GPU path + render graph ≥14 ms frame |
| Audio (M0.8) | 31 | v0.8-audio | A/V sync <10 ms drift |
| MVP Candidate (M1.0) | 39 | v1.0-mvp | 4K60 multi-track near realtime |
| Beta Prep | 48 | v1.0-beta | Stability + tests hardened |

## KPI Tracking (Initial Metrics to Automate)
| KPI | Source | Frequency |
|-----|--------|-----------|
| Avg frame render time | profiling macros | per playback session |
| p95 edit op latency | command profiler | nightly CI perf job |
| Cache hit ratio | cache stats aggregated | per playback session |
| Export real-time factor | export harness | per export |
| A/V sync drift | sync test tool | nightly |
| Crash count / session | crash reporter | release telemetry |

## Risk Radar (Dynamic)
Update weekly with top emerging risks & owner.
| Risk | Owner | Status | Mitigation |
|------|-------|--------|-----------|
| TBD | - | - | - |

## Operational Cadence
- Weekly engineering sync: review KPIs, risks, roadmap adjustments.
- Bi-weekly milestone review: accept/adjust deliverables.
- Monthly architecture review: challenge assumptions, record DRs.

## Change Management
All roadmap alterations require: PR updating this file + JIRA (or tracker) link + rationale referencing metrics or new constraints.

---
Maintainers: (Add names/emails or GitHub handles)
Last Updated: (Update date when editing)
