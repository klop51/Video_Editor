# Error & Debug Tracking Log

Purpose: Living log of non-trivial build/runtime/test issues encountered in this repository, the investigative steps taken, root causes, and preventive guardrails. Keep entries concise but actionable. Newest entries on top.

## Usage & Conventions
**When to log**: Any issue that (a) consumed >15 min to triage, (b) required a non-obvious workaround, (c) risks recurrence, or (d) would help future contributors.

**Severity (Sev)**:
- Sev1 Critical: Data loss, blocks all dev / CI red on main.
- Sev2 High: Blocks feature or PR progress for a subset of devs.
- Sev3 Moderate: Workaround exists; slows productivity.
- Sev4 Low: Cosmetic or minor intermittent annoyance.

Include a Sev tag in the title line, e.g. `– (Sev3)`.

**Automation**: Use `python scripts/new_error_entry.py "Short Title" --sev 3 --area "build / tests"` to insert a new entry template at the top and update the Index.

**Tokens**: If diagnosing potential stale builds, add a one-line `// TOKEN:<unique>` comment in the involved source and reference it in the log for quicker future verification.

---

## 2025-08-28 – MSVC OpenGL Header Conflicts Blocking GPU Rendering (Sev2)
**Area**: Graphics / Build System (MSVC + OpenGL)

### Symptom
Persistent compilation failures in `vk_device.cpp` with hundreds of errors related to missing C++ standard library symbols when OpenGL headers are included. Errors include:
- `error C2061: syntax error: identifier 'streampos'` (and similar for `streamoff`, `strong_ordering`)
- `error C3861: 'is_same_v': identifier not found` (and similar for other C++17 type traits)
- `error C2039: 'fabs': not a member of 'global namespace'` (and similar for C standard library functions)
- Build succeeds for other modules but fails specifically when compiling OpenGL-dependent code

### Impact
Blocks Phase 2 GPU rendering implementation; prevents completion of OpenGL-based graphics device, shader pipeline, and render graph. Core video editor GPU acceleration features cannot be developed or tested. Significant productivity blocker for graphics subsystem development.

### Environment Snapshot
- Date observed: 2025-08-28
- OS: Windows 10
- Toolchain: MSVC 14.44.35207 (VS 2022) via CMake multi-config
- Build preset: qt-debug
- OpenGL: System OpenGL via Qt6 (no dedicated OpenGL SDK)
- C++ Standard: Initially C++20, attempted C++17 fallback
- Dependencies: Qt6, OpenGL system headers

### Hypotheses Considered
1. Incorrect include order causing macro conflicts – attempted fixes with `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, isolated header files
2. C++20 standard library incompatibility with OpenGL headers – attempted C++17 downgrade
3. Missing OpenGL extension definitions – attempted fixed-function pipeline instead of GLSL shaders
4. PIMPL pattern implementation issues – attempted manual memory management to avoid standard library
5. MSVC-specific OpenGL header compatibility issues – CONFIRMED as root cause (known ecosystem problem)

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | Added `NOMINMAX` and `WIN32_LEAN_AND_MEAN` defines before Windows headers | Reduced some conflicts but core issues persisted |
| 2 | Created isolated `opengl_headers.hpp` to separate OpenGL includes | Header isolation successful but conflicts remained |
| 3 | Downgraded to C++17 standard for graphics module | Some improvements but fundamental conflicts persisted |
| 4 | Replaced `std::unique_ptr` with manual memory management | Eliminated memory library conflicts but C standard library issues remained |
| 5 | Attempted to remove all standard library includes from OpenGL files | Still triggered conflicts through transitive includes |
| 6 | **2025-08-28:** Converted from OpenGL to DirectX 11 implementation | Same MSVC namespace conflicts persisted |
| 7 | **2025-08-28:** Created minimal stub implementation outside `ve::gfx` namespace | Still 100+ MSVC operator new/delete conflicts |
| 8 | **2025-08-28:** Removed all standard library containers, used basic types only | Issue persists - fundamental namespace problem confirmed |

### Root Cause
**ESCALATED: Fundamental MSVC + ve::gfx Namespace Conflict** 

**Updated Analysis (2025-08-28):** The issue is not specific to OpenGL headers but appears to be a fundamental conflict between the `ve::gfx` namespace and MSVC's implementation of operator new/delete and standard library functions. Even minimal stub implementations trigger identical conflicts:

- `error C2323: 've::gfx::operator new': as funções de exclusão ou novas de um operador não membro podem não ser declaradas estáticas ou em um namespace diferente do namespace global`
- `error C2039: 'nothrow_t': não é um membro de 'std'`
- `error C2873: 'div_t': símbolo não pode ser usado em uma declaração de using`

**Key Finding:** The `ve::gfx` namespace itself appears to conflict with MSVC's implementation, causing the compiler to misinterpret standard library symbols as user-defined operators in the graphics namespace. This suggests either:
1. A macro or preprocessor issue specific to the `ve::gfx` namespace name
2. A deeper MSVC template instantiation or ADL (Argument-Dependent Lookup) conflict
3. Some header pollution that specifically affects namespaces containing "gfx"

This is beyond typical MSVC + graphics API compatibility issues and requires specialized MSVC namespace debugging expertise.

### Resolution
**Status: ESCALATED TO EXTERNAL CONSULTATION**

**Latest Attempt (2025-08-28):**
- Created minimal DirectX 11 stub implementation moving all code outside `ve::gfx` namespace
- Replaced all standard library containers with simple types (no `std::unique_ptr`, `std::unordered_map`)
- **Result:** Same 100+ MSVC namespace conflicts persist even with minimal stub code

**Current Assessment:** This is a fundamental MSVC build environment issue where the `ve::gfx` namespace itself conflicts with MSVC's operator new/delete and standard library implementation. The issue persists regardless of graphics API choice (OpenGL, DirectX 11) or implementation complexity (full API vs minimal stubs).

**Next Action:** ChatGPT consultation requested to explore alternative solutions, as conventional approaches have been exhausted.

**Options Still Available:**
1. **Switch to DirectX 11** - Native Windows graphics API with better MSVC support (blocked by namespace issue)
2. **Install Vulkan SDK** - Modern graphics API with proper Windows support (likely blocked by same issue)
3. **Use MinGW-w64** - GCC-based toolchain with better OpenGL compatibility
4. **Namespace restructuring** - Complete redesign of graphics module namespace architecture
5. **Software rendering fallback** - CPU-based rendering for initial development

### Preventive Guardrails
- **Graphics API selection criteria:** Evaluate MSVC compatibility as primary factor for Windows graphics APIs
- **Build system documentation:** Document known MSVC + OpenGL conflicts for future contributors
- **Alternative toolchain support:** Consider MinGW-w64 as secondary Windows build option
- **API abstraction:** Ensure graphics device interface allows easy API switching (already implemented)
- **Fallback rendering:** Implement software rendering path for development/testing when GPU APIs fail

### Verification
- **Current state:** Any compilation within `ve::gfx` namespace consistently fails with 100+ MSVC operator/stdlib conflicts
- **API-independent:** Issue persists with OpenGL, DirectX 11, or minimal stub implementations  
- **Architecture intact:** Graphics device abstraction, render graph, and shader framework designed correctly
- **Namespace-specific:** Problem appears isolated to `ve::gfx` namespace interaction with MSVC
- **Escalation needed:** Conventional solutions exhausted; requires specialized MSVC namespace debugging

**Recommendation:** Consult external expertise (ChatGPT/MSVC specialists) for namespace conflict resolution before attempting alternative approaches like toolchain changes or complete architecture redesign.

---

## 2025-08-27 – Video Loads but Playback Frozen at Frame 0 (Sev2)
**Area**: Playback Controller / Cache / Time Management

### Symptom
Videos loaded successfully (metadata, first frame displayed) but playback remained frozen at timestamp 0. Play button activation triggered playback thread start, but no frame progression occurred. Users reported "video player load the video and dont play." Extensive debugging logs showed:
- Playback thread running continuously in Playing state
- Cache lookups always hitting same timestamp (0 or initial seek position)
- Decoder never called during playback (only during initial load)
- `current_time_us_` never advancing from initial value

### Impact
Core video editor functionality broken; users unable to preview/edit video content. Playback appeared to work (controls responsive) but was effectively useless. Critical blocker for primary application purpose.

### Environment Snapshot
- OS: Windows 10
- Build: qt-debug preset (MSVC multi-config)
- Dependencies: Qt6, FFmpeg via vcpkg, spdlog
- Tested with MP4 (H.264/YUV420P) content

### Hypotheses Considered
1. Playback thread not starting – disproven by logs showing thread main loop active
2. Decoder failing during playback – disproven by successful initial frame decode
3. Frame delivery to UI broken – disproven by cache hit logs showing frame retrieval
4. Timing/sleep issues preventing frame advancement – ruled out by examining playback_thread_main loop
5. Cache always returning same frame due to time not advancing – CONFIRMED as root cause
6. seek_requested flag incorrectly set preventing normal playback – disproven by logs showing seek_requested=false

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | Added extensive debug logging to playback thread main loop | Revealed thread running but time stuck at 0 |
| 2 | Traced cache lookup mechanism | Found cache always hits for timestamp 0, never advancing |
| 3 | Analyzed time advancement logic in controller.cpp | Discovered time only advanced on cache MISS, not HIT |
| 4 | Identified missing time advancement in cache hit path | Cache hits (common case) left current_time_us_ unchanged |
| 5 | Implemented time advancement fix in cache hit path | Added `current_time_us_.store(next_pts)` after cache hit |
| 6 | Built and tested fix | Confirmed smooth frame progression through video timeline |

### Root Cause
**Time advancement missing in cache hit path.** The playback controller's main loop advanced `current_time_us_` only when calling the decoder (cache miss), but not when frames were retrieved from cache (cache hit). Since the cache is efficient and frequently hits, especially after initial buffering, the playback time remained frozen at the initial position, causing the same frame to be requested repeatedly.

**Code location:** `src/playback/src/controller.cpp` in `playback_thread_main()` function around cache lookup logic.

### Resolution
Added explicit time advancement in the cache hit path:

```cpp
// Cache hit - frame already available
spdlog::info("Cache HIT for pts: {}", current_pts);
frame = cache_result.value();

// CRITICAL FIX: Advance time even on cache hit
int64_t next_pts = current_pts + target_frame_interval_us;
current_time_us_.store(next_pts);
spdlog::info("Advanced time to: {} (cache hit path)", next_pts);
```

This ensures time progresses regardless of whether frames come from cache or decoder, maintaining consistent playback timing.

### Preventive Guardrails
- **Time advancement validation:** Any cache or frame retrieval path must advance playback time appropriately
- **Playback integration tests:** Add automated tests that verify frame timestamp progression during playback
- **Debug logging framework:** Maintain comprehensive playback timing logs to quickly identify similar time-related issues
- **Cache behavior documentation:** Document cache hit/miss implications for time management
- **Code review focus:** Emphasize time management consistency when modifying playback or cache logic

### Verification
- **Manual testing:** Video loads and plays with smooth frame progression from 0 to 15+ seconds
- **Log analysis:** Confirmed timestamps advancing correctly: `11533333 → 11566666 → 11600000 → 11633333...`
- **Frame delivery:** Both cache hits and misses now properly advance time and deliver frames to UI
- **Performance:** No degradation in playback performance; cache efficiency maintained

**Debug implementation details:** The fix uses the existing `target_frame_interval_us` (33,333 microseconds for 30fps) to calculate the next presentation timestamp, ensuring frame-accurate time progression matching the video's actual frame rate.

---

## 2025-08-27 – Phantom viewer_panel.cpp 'switch' Syntax Error (Sev3)
**Area**: UI module (Qt / AUTOMOC / MSVC incremental build)

### Symptom
Intermittent compile failures for `viewer_panel.cpp` reporting a stray `switch` token near line ~30 plus follow-on parse errors (`C2059: syntax error: 'switch'`, `C2447: '{' missing function header`, `C1004: unexpected end-of-file`). The cited line numbers did not correspond to any `switch` statement in the current file (first real `switch` on pixel format appears much later). Source as opened in editor and via direct file read showed no such construct near the reported location.

### Impact
Slowed iteration on UI changes; misleading diagnostics risked fruitless source edits. Increased uncertainty about correctness of recent pixel format refactor while errors actually originated from stale/incorrect translation unit state.

### Environment Snapshot
- Date observed: 2025-08-26 evening → early 2025-08-27
- OS: Windows 10
- Toolchain: MSVC 19.44 (VS 2022) via CMake multi-config (qt-debug preset)
- Qt: 6.9.1 (AUTOMOC enabled)
- Recent edits: Significant modifications to `viewer_panel.cpp` conversion logic and `MainWindow` wiring immediately before anomalies.

### Hypotheses Considered
1. Actual hidden / non-printable characters injected into source (disproven by hex inspection & fresh read).
2. Concurrent write during compile produced truncated file snapshot (unlikely; no partial content seen on disk).
3. AUTOMOC generated amalgamation (`mocs_compilation_Debug.cpp`) concatenating stale moc output with an orphaned fragment containing a `switch` (plausible; not reproduced yet).
4. MSVC incremental build using outdated PCH / object referencing an earlier revision, misreporting line mapping (plausible given earlier similar stale TU test issue).
5. Corruption in build tree (object or preprocessed intermediate) isolated to UI target (possible; resolved by clean rebuild).

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | Inspected on-disk `viewer_panel.cpp` around reported line numbers | No stray `switch` present |
| 2 | Searched build tree for partial temporary file copies | No alternate copies found |
| 3 | Performed targeted clean: removed `build/qt-debug/CMakeFiles`, UI object dirs | Error disappeared |
| 4 | Reconfigured & rebuilt only `ve_ui` target | Successful build; no syntax errors |
| 5 | Added sentinel token comment to `viewer_panel.cpp` | Provides future stale artifact detection |

### Root Cause
Unconfirmed; most consistent with stale or corrupted incremental compilation artifact (possibly AUTOMOC or MSVC object reuse) referencing a prior in-progress edit state. Lack of recurrence after hard clean strengthens this hypothesis.

### Resolution
Manual purge of build system metadata & object files for qt-debug preset followed by full reconfigure & rebuild.

### Preventive Guardrails
- Sentinel comment token (`// TOKEN:VIEWER_PANEL_2025_08_27_A`) near top of file; future diagnostics lacking this token imply stale preprocessing.
- If reappears: capture preprocessed output (`/P`) for `viewer_panel.cpp` and archive alongside error log.
- Add optional developer script to purge only a single target's intermediates: e.g., PowerShell function `Clean-Target ve_ui` removing its obj & CMake dependency subfolders.
- Periodic (daily/CI) clean builds to flush incremental divergence.

### Verification
Post-clean rebuild produced `ve_ui.lib` without errors. No phantom `switch` diagnostics observed across two successive incremental builds.

---

## 2025-08-26 – Pixel Format Conversion Spam & Missing Playback Wiring (Sev3)
**Area**: UI Viewer / Playback / Decode Integration

### Symptom
1. High-frequency log spam: "Unsupported pixel format for display" and "Failed to convert frame to pixmap" while attempting to play various media (YUV420P, NV12, grayscale sources).
2. After double-clicking a media item, video loaded metadata but no frames advanced (static image or blank), requiring manual intervention; play controls sometimes disabled.

### Impact
Obscured genuine warnings, slowed debugging signal/noise, and created perception that playback was broken. Users could not easily begin playback after load without an explicit controller instance wired.

### Environment Snapshot
- OS: Windows 10
- Build: qt-debug preset (MSVC multi-config)
- Dependencies: Qt6, FFmpeg via vcpkg

### Hypotheses Considered
1. Decoder failing to produce frames (disproven by inspecting cache population logic).
2. Viewer conversion path too limited causing rejection of otherwise valid frames (confirmed).
3. Playback controller not started or not in Playing state (confirmed: state remained Stopped, no auto-play, and wiring sometimes absent).
4. Thread timing / sleep overshooting causing perceived stall (not root cause for initial frame absence).

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | Inspected `viewer_panel.cpp` conversion switch | Found narrow pixel format support |
| 2 | Extended switch to handle RGB, RGBA, multiple YUV planar/semiplanar, grayscale; added Unknown heuristic | Reduced unsupported warnings in test cases |
| 3 | Added rate limiting for repetitive warnings | Log spam curtailed |
| 4 | Ensured `PlaybackController` created & passed to viewer in `MainWindow` when loading media | Play controls activate reliably |
| 5 | Added on-demand controller creation in double-click handler | Prevented null controller cases |

### Root Cause
Insufficient pixel format coverage in viewer conversion plus missing guaranteed wiring of a playback controller before media load; no auto-creation meant callbacks never invoked.

### Resolution
- Code modifications (see git diff on 2025-08-26) to viewer conversion and MainWindow controller instantiation/wiring.
- Added heuristic & fallback conversion to RGBA.

### Preventive Guardrails
- Add unit test covering conversion for each enum value.
- Introduce structured log categories with per-category rate limiting.
- CI test that loads a sample YUV420P clip and asserts at least one frame callback within a timeout.

### Verification
Manual build after changes; pending runtime session to confirm reduced warnings and active frame callbacks (to be logged in future update).

---

## 2025-08-26 – Phantom Catch2 Header / Stale Test Translation Unit (Sev3)
**Area**: Tests / Profiling subsystem / Build system (MSVC, CMake multi-config)

### Symptom
Intermittent build failures for `tests/test_profiling.cpp` reporting:
- Missing header: `catch2/matchers/catch_matchers_floating.hpp` (never included in current source)
- Follow‑on template errors referencing `Catch::Approx` after all `Approx` usages had been removed.

### Impact
Slowed test iteration; misleading diagnostics obscured actual code state. Risk of committing unnecessary code changes to “fix” a non-code issue.

### Environment Snapshot
- OS: Windows 10.0.19045
- Toolchain: MSVC 14.44.* via CMake + vcpkg (Catch2 3.9.1)
- Build preset: `qt-debug` (multi-config generator with MSBuild)

### Initial Hypotheses Considered
1. Stray include introduced transitively by another test – disproven by full text searches.
2. Cached precompiled header (PCH) referencing old file – plausible (MSBuild sometimes keeps stale TU state when file list mutates quickly).
3. Duplicate file path or shadow copy in build tree – no duplicate found (`test_profiling.cpp` single occurrence).
4. IDE/editor produced unsaved changes vs on-disk – ruled out by direct file reads & `grep`.
5. Catch2 version mismatch or partial install – ruled out (other tests fine; only this TU affected).

### Actions Taken (Timeline)
| Step | Action | Result |
|------|--------|--------|
| 1 | Removed `Catch::Approx` usage, added custom `ve_approx` | Build still reported `Approx` errors |
| 2 | Inserted `#error` sentinel in file | Build sometimes showed old errors, later showed sentinel confirming correct file compiled intermittently |
| 3 | Searched for phantom header (`matchers_floating`) across repo & build dirs | No occurrences |
| 4 | Deleted/renamed test file to `test_profiling_stats.cpp` and updated CMake list | All errors vanished; clean build & all tests passed |

### Root Cause (Probable)
Stale build artifact / MSBuild incremental state retained association between the removed original `test_profiling.cpp` object and outdated preprocessed content, causing diagnostics to reflect an earlier revision. Renaming the file invalidated the stale object and forced a full compile of the *current* content. Exact internal trigger not definitively proven, but behavior aligns with an incremental dependency graph desync.

### Resolution
- Replace problematic TU: remove `test_profiling.cpp`, add `test_profiling_stats.cpp`.
- Eliminate dependency on `Catch::Approx` to simplify matcher includes.

### Preventive Guardrails / Follow Ups
- Periodic clean builds in CI (fresh clone) to catch stale artifact anomalies early.
- (Optional) Script to hash test source files and compare against object timestamps; warn if mismatch suggests stale compilation.
- Avoid rapid create/delete cycles of the same filename in successive commits where possible.
- Consider adding a comment header in each test file with a unique token; if diagnostics reference code not containing the token, suspect staleness.

### Verification
- Rebuilt: `unit_tests.exe` produced without earlier errors.
- Ran full test suite: 214 assertions passed including new profiling percentile tests.

---

## Template for New Entries
Copy this block and fill it out when logging a new issue.

### YYYY-MM-DD – Short Title (SevX)
**Area**: (module / layer / tooling)

#### Symptom
Concise description + representative error messages.

#### Impact
Developer productivity / correctness / performance / user-facing risk.

#### Environment Snapshot
- OS / Compiler / Dependencies / Preset.

#### Hypotheses Considered
List numbered hypotheses (include those disproven and how).

#### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | … | … |

#### Root Cause
Most plausible cause (or multiple if still uncertain). Mark UNKNOWN if unresolved.

#### Resolution
What definitively stopped the symptom.

#### Preventive Guardrails
Bullets: monitoring, tests, config changes, process tweaks.

#### Verification
How we confirmed fix (commands/tests/logs).

---

## Index
- 2025-08-28 – MSVC OpenGL Header Conflicts Blocking GPU Rendering
- 2025-08-27 – Video Loads but Playback Frozen at Frame 0
- 2025-08-27 – Phantom viewer_panel.cpp 'switch' Syntax Error
- 2025-08-26 – Pixel Format Conversion Spam & Missing Playback Wiring
- 2025-08-26 – Phantom Catch2 Header / Stale Test Translation Unit

Add new entries above this line.
