# Professional Video Editor – Architecture & Build Guide

> Living document describing vision, scope, architecture, standards, roadmap, and operational guidelines for the C++ professional NLE (non‑linear editor). Keep it updated as decisions are made. Sections marked (TBD) require validation.

---
## 1. Vision & Scope
A cross‑platform (Windows first, macOS/Linux later) professional non‑linear video editor focused on responsive timeline interaction, accurate color management, and efficient 4K/8K workflows with proxy support. Initial target: stable 4K60 multi‑track editing with essential trims, cuts, transitions, color adjustment basics, audio mixing, and H.264/HEVC export.

### MVP (v1) Must-Haves
- Multi-track video & audio timeline (ripple/overwrite modes)
- Core edit ops: insert, delete, blade, ripple/roll/trim, slip, slide
- Keyframes for effect parameters & basic transforms
- Basic effects: transform, opacity, crop, LUT, color wheels, curves, Gaussian blur, sharpen
- Transitions: crossfade + simple wipes
- Audio: waveform display, volume/pan, basic meters
- Real-time playback with adaptive dropped-frame strategy (never drop audio)
- Proxy media workflow
- Color management (linear internal, OCIO integration base, Rec.709 output)
- Export: H.264 (x264), HEVC (x265 or hardware), WAV/FLAC audio
- Undo/redo with command compression
- Autosave & crash recovery

### Post‑MVP (future)
Scopes, HDR, advanced grading, scripting (Python), plugin SDK, collaboration features, advanced audio FX, smart rendering & partial re-encode avoidance.

---
## 2. Non-Functional Requirements
| Aspect | Target |
|--------|--------|
| Playback latency (scrub to frame shown) | < 80 ms |
| Timeline edit op (p95) | < 5 ms |
| 4K60 playback frame budget | < 16 ms total (CPU+GPU) |
| Startup cold | < 8 s |
| Memory footprint | < 3× active media working set, adaptive cache eviction |
| Export real-time factor (4K to H.264 high quality) | ≥ 0.75× on reference hardware |
| Crash rate (post‑beta) | < 1% sessions |

---
## 3. High-Level Architecture
Layered + modular:

```
+-----------------------+  UI (Qt) / Panels / Interaction / Command System
|        UI Layer       |
+-----------------------+  Application (session mgr, project, autosave)
|      App Layer        |
+-----------------------+  Timeline Model / Undo / Serialization
|   Timeline & Model    |
+-----------------------+  Render Graph / Effects / Compositor
|   Render & FX Engine  |
+-----------------------+  Media IO (demux/decode), Audio Engine, Caching
|  Media & Audio Core   |
+-----------------------+  Platform Abstraction (GPU, FS, Threads, HW accel)
|   Platform / Runtime  |
+-----------------------+  Third-party libs (FFmpeg, x264, OCIO, etc.)
```

Communication principles:
- Pull-based frame requests: UI asks timeline for Frame@Time -> render graph resolves dependencies.
- Strict ownership: timeline immutable snapshots for playback threads; edits produce a new snapshot.
- Thread-safe, lock-minimized design using job system & double-buffered state objects.

---
## 4. Modules Overview
| Module | Responsibility | Key Dependencies |
|--------|----------------|------------------|
| `core` | Common types (time, ids, logging, config) | spdlog, fmt |
| `media_io` | Container probe, demux, packet queues | FFmpeg | 
| `decode` | Video/audio decode (software + HW abstraction) | media_io |
| `timeline` | Project, clips, tracks, edits, undo/redo | core |
| `render` | Compositor, render graph scheduler | GPU backend, fx |
| `fx` | Effect nodes & transitions | render |
| `audio` | Mixing graph, resample, meters | decode |
| `cache` | Frame & waveform caches (RAM/disk) | core |
| `ui` | Qt panels, interaction, command mapping | timeline, app |
| `app` | Session mgmt, autosave, crash recovery | timeline, serialization |
| `persistence` | Project serialization (JSON→binary) | timeline |
| `plugin_host` (future) | Plugin loading & ABI boundary | core |
| `scripting` (future) | Python bindings | timeline, app |

---
## 5. Directory Layout (Initial Proposal)
```
/ CMakeLists.txt
/cmake/               # CMake modules (FindFFmpeg, warnings, compiler settings)
/external/            # Patched or bundled 3rd party (if vendored minimal)
/include/             # Public headers (mirrors src structure)
/src/
  core/
  media_io/
  decode/
  timeline/
  render/
  fx/
  audio/
  cache/
  ui/
  app/
  persistence/
  plugin_host/        # (future)
  scripting/          # (future)
/tests/
/docs/                # Extended docs (this file root-level for visibility)
/tools/               # Dev utilities (media probe, benchmark harness)
/scripts/             # CI, formatting, packaging
/resources/           # Shaders, LUTs, icons
```

---
## 6. Time & Synchronization
- Internal time type: `TimeRational { int64 num; int32 den; }` plus helpers; canonical project timebase (e.g. 1/48000 for audio precision) or per-clip rational conversions.
- Master clock: audio engine. Video frames scheduled relative to audio PTS.
- Variable frame rate handling via per-clip time map (original DTS/PTS mapping preserved; timeline uses logical edit frames).
- Avoid floating drift by prohibiting storing seconds in `double` except transient UI formatting.

---
## 7. Data Model Key Objects
| Object | Purpose | Notes |
|--------|---------|-------|
| `MediaSource` | Original file/stream & metadata | Hash for relink detection |
| `MediaClip` | In/out range referencing `MediaSource` | Has time mapping & color space |
| `Segment` | Placed clip instance on a track with start time | Holds per-instance effect overrides |
| `Track` | Ordered non-overlapping segments (mode: video/audio) | Gap implicit; constraints validated on edit |
| `Timeline` | Collection of tracks + global settings | Immutable snapshot for playback |
| `Project` | Assets, timelines, bins, settings | Serialized root |
| `Command` | Edit operation (undo/redo) | Coalescing for drags |
| `KeyframeCurve` | Parameter animation | Interpolation types |

Undo strategy: Command pattern with state diffs; potential structural sharing of large unchanged portions (copy-on-write for track segment vectors).

---
## 8. Render Graph & Compositor
- Pull-based DAG: leaf nodes = decoded frames or generated sources; internal nodes = effects, transitions, color management; root = output frame.
- Node interface: `RenderResult render(const RenderContext& ctx)` returning either GPU texture handle or CPU buffer.
- Hashing: Each node computes a content hash (inputs + parameters + version); enables memoization & partial invalidation.
- Effects kernel fusion (future optimization) by analyzing small chains of color transforms into a single shader pass.
- Color pipeline: Convert source (YUV / camera log) -> linear float16 working space -> apply effects -> tone map -> UI display transform (Rec.709 sRGB). Use OCIO config for transforms.

---
## 9. Audio Engine
- Graph of nodes: sources (decoded clip streams), gain/pan nodes, effects (EQ, compressor), master mix.
- Fixed block size (e.g., 512 samples) mixing; latency compensation tracked per node.
- Waveform generation asynchronous: decimated multi-resolution pyramid stored on disk (e.g., 1:1, 1:16, 1:256) for fast zoom rendering.

---
## 10. Caching Strategy
| Cache | Purpose | Eviction |
|-------|---------|----------|
| Frame Cache (RAM) | Recently rendered frames | LRU + cost-aware weighting |
| Decode Frame Queue | Ready decoded frames ahead of playhead | Bounded by memory & GOP size |
| Disk Proxy Cache | Lower-res transcodes | LRU by last access + pin while in timeline |
| Waveform Tile Cache | Audio visualization | LRU; prefetch around viewport |
| Render Graph Memo | Reuse effect chain outputs | Hash invalidation on param change |

Adaptive memory: monitor RSS; on pressure reduce decode lookahead, drop high-cost frames, degrade preview resolution.

---
## 11. Concurrency & Job System
- Central work-stealing thread pool; tasks labeled (Decode, Audio, Render, IO, Background) for priority scheduling.
- Avoid giant global locks; prefer lock-free queues (MPSC) for packet/frame passing.
- Double-buffered timeline snapshot: UI edits produce new snapshot atomically swapped for playback thread.
- Deterministic tests using single-thread mode + seeded scheduling.

---
## 12. External Dependencies (Initial)
| Library | Function | License Consideration |
|---------|----------|-----------------------|
| FFmpeg | Demux/Decode/Encode | LGPL/GPL components – decide build flags |
| x264 / x265 | Encoding | GPL – influences distribution |
| Qt 6 | UI | LGPL/commercial |
| OCIO | Color management | BSD |
| spdlog / fmt | Logging / formatting | MIT |
| Catch2 / GoogleTest | Testing | BSL / BSD |
| pybind11 (future) | Scripting bindings | BSD |
| Vulkan / DX12 / Metal (abstraction) | GPU | Platform specific |

Licensing Strategy: Keep core under permissive license; separate optional GPL plugin pack if needed.

---
## 13. Build System
- CMake (minimum 3.25), Unity builds optional for faster iteration.
- Dependency management: vcpkg or Conan (TBD after evaluation). For early stages, rely on developer-installed FFmpeg; later integrate package or fetch.
- Tooling: clang-tidy, include-what-you-use, clang-format, cppcheck in CI.

Example top-level (sketch):
```cmake
cmake_minimum_required(VERSION 3.25)
project(VideoEditor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build tests" ON)
add_subdirectory(src/core)
# add_subdirectory for other modules progressively
if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
```

---
## 14. Coding Standards
- C++20, no RTTI-dependent designs unless justified (plugins).
- Naming: `PascalCase` for types, `snake_case` for functions & variables, `ALL_CAPS` for compile-time constants.
- Smart pointers: `unique_ptr` default ownership, `shared_ptr` only for shared graph nodes with clear lifetime.
- Avoid raw new/delete; RAII wrappers for GPU & OS resources.
- Error handling: `expected<T,E>` style (custom or std::expected once available) for recoverable errors; exceptions limited to fatal/unexpected (or disable exceptions entirely – decision TBD).
- Logging levels: trace (dev), debug (default dev), info (user events), warn, error, critical.

---
## 15. Undo/Redo Mechanism
- Command objects with `apply()` & `revert()`; coalescing for continuous drags (time-threshold & same command type).
- Snapshot diff: store minimal changed segment ranges; persistent IDs allow reconstruction.
- Limit memory by compressing large sequences and discarding deep history beyond user preference (e.g., 200 steps) unless flagged.

---
## 16. Persistence
- Dev format: JSON (readable, versioned with schema version).
- Production: Binary chunked container (magic header + index table + compressed chunks) for faster load.
- Version upgrades: migration pipeline applying incremental transforms; never skip versions without explicit path.

---
## 17. Testing Strategy
| Test Type | Focus |
|-----------|-------|
| Unit | Time math, edit ops, cache eviction, effect parameter evaluation |
| Integration | Decode→Timeline→Render frame correctness & A/V sync |
| Golden Image | Hash frames (or perceptual diff) for regression detection |
| Performance | Benchmarks: playback frame time distribution, edit latency |
| Fuzz | Demux inputs, edit sequences, malformed project files |
| Stress | Long timelines, high track counts, 8K proxies |

CI gates: All tests pass, no new warnings (treat warnings as errors), performance regress <10% threshold triggers alert.

---
## 18. Performance Metrics (Collected & Tracked)
- Playback: average, p95 frame render time; dropped frame %.
- Decode: mean & p95 decode time by codec/resolution.
- Edit latency: p95 insertion & ripple operations.
- Memory: peak RSS, cache hit/miss ratios.
- Export: real-time factor, encoding latency distribution.

Tooling: internal profiling macros (scoped timers), GPU timestamps, aggregated into JSON artifact after runs.

---
## 19. Security & Robustness
- Validate all media probe inputs (no trusting container metadata lengths blindly).
- Sandboxed plugin process (future) to avoid host crashes.
- Defensive checks on effect parameter ranges.
- Watchdog for deadlocked decode threads (heartbeat mechanism).

---
## 20. Roadmap (High-Level Phases)
| Phase | Months | Summary |
|-------|--------|---------|
| 0 | 0–0.5 | Vision, requirements, tooling setup |
| 1 | 0.5–2 | Architecture skeleton, CMake, media probe tool |
| 2 | 2–4 | Media engine (decode, basic playback) |
| 3 | 4–5 | Timeline model & undo system |
| 4 | 5–6 | UI foundation (Qt panels) |
| 5 | 6–7 | Playback optimization & basic editing tools |
| 6 | 7–9 | Effects & transitions (initial set) |
| 7 | 8–10 | Audio engine & mixing |
| 8 | 9–11 | Export pipeline |
| 9 | 10–12 | Proxies & performance tuning (ship-cut) |
| 10+ | 12+ | Advanced color, plugins, scripting, collaboration |

Milestones produce internal tags (`v0.1-prototype`, `v0.5-playback`, `v1.0-mvp`).

---
## 21. Early Execution Plan (First 6 Weeks Detailed)
| Week | Deliverable |
|------|-------------|
| 1 | Repo init, CMake skeleton, logging, config, media probe CLI listing stream info |
| 2 | Decode single clip → CPU RGBA frames; display minimal window (Qt stub) |
| 3 | GPU path (Vulkan or DX12) uploading frames, basic swapchain & vsync control |
| 4 | Timeline data structures & command system (no UI) + programmatic cuts playback |
| 5 | Real-time playback of sequence (cuts only), prefetch & frame cache |
| 6 | Simple effect (brightness) as shader node, keyframe curve evaluation |

Definition of “Prototype Complete”: stable playback of a 4K clip with simple effect at ≥ 50 fps average on reference machine.

---
## 22. Risk Log (Initial)
| Risk | Impact | Mitigation |
|------|--------|------------|
| Hardware decode inconsistency | Playback instability | Capability abstraction + fallback software paths |
| Color pipeline errors | Visual inaccuracies | OCIO unit tests + roundtrip validations |
| Timeline scaling | UI lag on large projects | Benchmark with synthetic timelines early; optimize data structures |
| GPL licensing constraints | Distribution limitations | Optional encoding plugin pack; ship LGPL core |
| Plugin ABI churn (future) | Breaks ecosystem | Stable C ABI boundary + version negotiation |

---
## 23. Logging & Telemetry (Opt-In)
- Structured logging (JSON mode for diagnostics) with session UUID.
- Crash dumps captured (Windows: MiniDumpWriteDump) – user consent.
- Telemetry events: anonymized performance metrics only.

---
## 24. Deployment & Packaging
- Windows: MSIX or Inno Setup; code signing in CI.
- macOS: .app bundle + notarization (future phase).
- Linux: AppImage/Flatpak (future).

---
## 25. Future Extensions (Outline)
- Plugin SDK: C ABI with frame pull contract; sandbox via separate process & shared memory ring buffer.
- Python scripting: timeline batch operations, headless export.
- Collaborative review: frame hash annotation exchange (later phase).
- HDR & ACES workflows with metadata pass-through.

---
## 26. Open Decisions (To Resolve)
| Topic | Options | Target Resolution |
|-------|---------|-------------------|
| GPU API abstraction | Vulkan first vs DirectX12-first w/ abstraction layer | Before Week 2 |
| Dependency manager | vcpkg vs Conan | Week 1 evaluation |
| Exception policy | Enable with boundaries vs no-exception build | Week 3 |
| Internal timebase | Fixed tick vs rational struct everywhere | Week 2 |
| Render graph library? | Custom vs existing (e.g., RenderGraph libs) | Week 5 |

Track decisions in `DECISIONS.md` with rationale.

---
## 27. Contribution Workflow
1. Feature branch from `main` (protected).
2. Clang-format & static analysis clean.
3. Add/update tests; golden images updated via approved process.
4. PR template includes: purpose, test evidence, performance impact.
5. CI must pass all gates; reviewer signs off on architectural impacts.

---
## 28. Quick Start (Developer – Placeholder)
(Will be updated after initial scaffolding.)

```bash
# Configure (example – adjust for Windows PowerShell later)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
# Run media probe tool
./build/tools/media_probe sample.mp4
```

Windows notes: For hardware decode prototypes ensure latest GPU drivers; enable debug layers (Vulkan: `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`).

---
## 29. Glossary
| Term | Definition |
|------|------------|
| NLE | Non-linear editor |
| GOP | Group Of Pictures (keyframe interval structure) |
| PTS | Presentation Timestamp |
| OCIO | OpenColorIO library for color transforms |
| Proxy | Lower resolution transcode version of source media |

---
## 30. Maintenance
Review & refine this document at each milestone; obsolete sections moved to an archive or updated with decision records.

---

End of initial architecture & build guide. Update as soon as real constraints or measurements emerge.

Roadmap Extension: See `ROADMAP.md` for a living, granular 12‑month execution plan, milestone acceptance criteria, KPIs, and phase exit gates.
