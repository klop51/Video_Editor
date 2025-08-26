# DR-0001: GPU API Abstraction Choice

Date: 2025-08-26
Status: Proposed

## Context
The project must introduce a GPU rendering path to support real‑time playback, effects, transitions, color management, and export rendering. Early development is Windows‑first, but long‑term goals include macOS and Linux. Key requirements:
- Efficient upload and sampling of decoded video frames (YUV to linear conversion).
- Extensible render graph with node chaining and potential future compute kernels.
- Cross‑platform trajectory without excessive early complexity.
- Debug tooling availability for performance analysis.

Constraints / Forces:
- Current team velocity favors shipping an initial vertical slice quickly.
- Windows has strong tooling (RenderDoc/Vulkan, PIX/DX12). macOS requires Metal (future translation layer needed if not natively chosen).
- Avoid locking into a deprecated or niche API.
- Desire for explicit control (memory, synchronization) to optimize 4K/8K workflows.

## Options Considered
1. DirectX 12 first, add abstraction layer later.
   - Pros: Excellent Windows tooling (PIX), good driver maturity.
   - Cons: Requires separate backend (Metal/Vulkan) later; abstraction retrofitting cost.
2. Vulkan first with thin engine wrapper.
   - Pros: Cross‑platform path (Windows/Linux/Android), explicit API fits performance goals.
   - Cons: Steeper learning curve & verbosity, macOS requires MoltenVK (later compatibility testing).
3. OpenGL (Core) interim, migrate later.
   - Pros: Fast initial implementation, simpler API.
   - Cons: Legacy limitations, poor future optimization headroom, driver inconsistencies; rewrite inevitable.
4. Higher‑level framework (bgfx / Dawn / gfx-rs wrapper) early.
   - Pros: Multi‑backend out of box, faster prototype.
   - Cons: Less low‑level control for specialized video/effect pipelines; learning and constraint overhead.
5. CPU path only initially; defer GPU decision.
   - Pros: Simplifies early code.
   - Cons: Delays performance validation & effect architecture; risk of large refactor.

## Decision
Adopt Option 2: Vulkan first with a minimal internal abstraction layer focused on:
- Resource creation (buffers, images, samplers)
- Render pass / pipeline (graphics + compute) encapsulation
- Command submission & synchronization primitives (fences/semaphores)
- Descriptor management helper

Reasoning: Provides explicit performance control and cross‑platform viability. Early investment in structured wrappers reduces later duplication. MacOS support via MoltenVK acceptable mid‑term. Avoids premature generalization of multi‑backend abstractions while not locking into a Windows‑only API.

## Rationale (Key Points)
- Cross‑platform roadmap alignment (Linux early adopters) without re‑architecture.
- Explicit API suits future advanced effects (compute, timeline analysis kernels).
- Strong tooling (RenderDoc) and growing ecosystem.
- Avoids OpenGL limitations (synchronization ambiguity, driver variance).
- Earlier GPU path validation de‑risks performance goals.

## Consequences
Positive:
- Early alignment with long‑term multi‑platform objective.
- Fine‑grained control for optimization (memory aliasing, async compute later).
- Stable public API for effect nodes built on explicit primitives.

Negative / Trade‑offs:
- Higher initial implementation effort vs OpenGL.
- Additional boilerplate (instance/device creation, descriptors) before first pixel.
- macOS path depends on MoltenVK validation (potential edge cases).

## Follow-ups
- Implement `gfx/` module with minimal Vulkan bootstrap (instance, device, swapchain, command pool, staging uploader).
- Add DR later if introducing secondary backend (Metal or DirectX 12) – superseding record for multi‑backend strategy.
- Provide profiling hooks (timestamps) in abstraction.
- Establish coding guideline snippet for GPU resource lifetime & threading (future DR).

## Metrics / Validation
- Target: Upload + simple shader pass of 4K frame ≤ 2 ms average (excluding decode) by prototype test.
- Frame pacing jitter p95 < 4 ms under simple transform effect.

## Alternatives Rejected (Summary)
- DX12 first: increases later port cost; Vulkan offers broader reach now.
- OpenGL interim: rewrite risk + performance uncertainty not acceptable.
- High-level framework: potential constraints on custom video pipeline & memory layout.
- Defer decision: would postpone risk discovery on GPU performance.

## Open Questions
- Need defined minimum GPU feature set (sampler YCbCr support vs manual conversion path) – evaluate soon.
- Decide on shader language path (GLSL + SPIR-V; later HLSL cross‑compile?).

## References
- ARCHITECTURE.md Sections 3, 8 (render graph vision)
- ROADMAP.md Phase 4 & 5 GPU milestones
