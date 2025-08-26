# gfx Module (Vulkan Bootstrap)

Implements minimal GPU abstraction per DR-0001:
- Vulkan instance + device wrappers (stubbed if Vulkan SDK not present).
- Future: swapchain, command pool, staging uploader, timestamp profiler.

Build Modes:
- If `find_package(Vulkan)` succeeds, `VE_GFX_VULKAN` enables real integration.
- Otherwise builds in stub mode (functions succeed but perform no GPU work) allowing other modules to compile.

Next Steps:
1. Add swapchain abstraction + surface creation (platform specific).
2. Implement staging uploader for frame upload.
3. Timestamp profiling helpers hooking into profiling module.
4. Integrate with `render` module (replace CPU placeholder path).
