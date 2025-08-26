# render Module
Responsibility: Frame composition and effect graph evaluation producing frames for preview/export.

Current State:
- Placeholder `RenderGraph` with stub `render()`.

Near-Term Integration Plan (Phase 1):
1. Introduce CPU-only node types: SourceNode (wraps decoded frame), EffectPassNode (no-op initially), OutputNode.
2. Adapt `PlaybackController` to push decoded frame into a RenderGraph (or directly call graph for preview requests) instead of invoking UI callback directly.
3. Define `FrameSurface` struct (CPU RGBA buffer) as initial graph artifact. Later replace with GPU handle abstraction.
4. Add hash/version fields to nodes for incremental invalidation (store last rendered pts + params signature).

Phase 2 (GPU Prep):
1. Add abstraction for GPU texture upload (`GpuFrameResource`).
2. Split render path: preview (interactive) vs export (offline accuracy, full quality).
3. Insert color pipeline (YUV->linear->display) as explicit nodes.

Phase 3 (Optimization):
1. Effect fusion pass for chains of color adjustments.
2. Async render scheduling with job system labels (Render, Upload).
3. Partial re-render: re-evaluate only nodes affected by param changes.

Interfaces To Stabilize Early:
- Node interface (request/time-based render function).
- Frame cache contract (cache keyed by (node_id, pts)).
- Parameter update notification path from UI to graph.

Deferred:
- Multi-resolution / proxy adaptation inside graph.
- Tiled rendering for very high resolution.

Dependencies: `core` (time/log), later `fx`, `cache`, `decode` (for source frames).
