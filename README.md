# Video Editor (Prototype Scaffold)

Initial scaffold for the C++ professional non-linear video editor project. See `ARCHITECTURE.md` for the comprehensive design and roadmap.

## Dependencies (via vcpkg manifest)
- FFmpeg (demux/codec libs)
- Catch2 (unit testing)

## Using CMake Presets
Set `VCPKG_ROOT` environment variable first.
```powershell
$env:VCPKG_ROOT="C:/path/to/vcpkg"
cmake --preset dev-debug
cmake --build --preset dev-debug -j
ctest --preset dev-debug-tests --output-on-failure
```
Visual Studio solution (generates .sln with MSBuild):
```powershell
cmake --preset vs-debug
cmake --build --preset vs-debug --config Debug
```
Release build:
```powershell
cmake --preset dev-release
cmake --build --preset dev-release -j
```
No FFmpeg debug build:
```powershell
cmake --preset no-ffmpeg
cmake --build --preset no-ffmpeg -j
```

Manual configure (if not using presets):
```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug -j
```

## Components
- Core module (`core`): logging (simple sink), time utilities
- Media IO (`media_io`): FFmpeg probing wrapper
- Tool: `ve_media_probe` (prints stream info)
- Tests: Catch2-based (`unit_tests` target)

## Tools
- `ve_media_probe` – print stream metadata
- `ve_playback_demo` – basic async decode + playback controller demo (console output every ~30 frames)
- `ve_qt_preview` – minimal Qt video preview window (enable with `-DENABLE_QT_TOOLS=ON` and ensure `qtbase` via vcpkg)

## Additional CMake Options
| Option | Default | Description |
|--------|---------|-------------|
| ENABLE_QT_TOOLS | ON | Build Qt preview tool (requires qtbase) |
| ENABLE_COVERAGE | OFF | Enable coverage flags (non-MSVC) |

## Running
```powershell
# Run media probe (Debug preset path may vary by generator)
./build/dev-debug/src/tools/media_probe/ve_media_probe.exe sample.mp4
# Run tests
ctest --preset dev-debug-tests --output-on-failure
```

## Configuration Options
| CMake Option | Default | Description |
|--------------|---------|-------------|
| ENABLE_FFMPEG | ON | Toggle FFmpeg integration |
| ENABLE_TESTS | ON | Build unit tests |
| ENABLE_TOOLS | ON | Build developer tools |
| ENABLE_WARNINGS_AS_ERRORS | ON | Treat warnings as errors |

Disable FFmpeg (build without dependency):
```powershell
cmake --preset no-ffmpeg
```

## Continuous Integration
GitHub Actions workflow (`.github/workflows/ci.yml`) builds Debug & Release on Windows and Ubuntu with vcpkg caching, runs tests in Debug, and performs a no-FFmpeg configure lint.

## Additional Developer Features
- Coverage (Linux): use `dev-debug-coverage` preset then run tests; generates lcov data.
- Clang-Tidy: CI job runs static analysis; local run via `run-clang-tidy` or script (future addition).
- Artifacts: Release builds uploaded by CI; coverage report artifact for Debug Linux.

## Coverage Example (Linux)
```bash
cmake --preset dev-debug-coverage
cmake --build --preset dev-debug-coverage -j
ctest --preset dev-debug-coverage-tests --output-on-failure
# Generate lcov manually if needed
lcov --capture --directory build/dev-debug-coverage --output-file coverage.info
```

## Next Steps
1. Expand error handling & logging (structured JSON mode).
2. Add UUID utilities & command pattern scaffolding.
3. Implement decode loop & frame cache.
4. Introduce continuous integration workflow (DONE basic) & add artifact upload.

## License
(TBD) – Decide license strategy (core permissive, optional GPL components separate).

## Convenience Script (Windows PowerShell)
A helper script `build_and_run.ps1` automates configure, build, and running a tool.

Examples:
```powershell
# Build (dev-debug) and run media probe
./build_and_run.ps1 -Media sample.mp4 -Tool media_probe

# Release build playback demo
./build_and_run.ps1 -Preset dev-release -Tool playback_demo -Media sample.mp4

# Qt preview (ensure ENABLE_QT_TOOLS and qtbase present)
./build_and_run.ps1 -Tool qt_preview -Media sample.mp4

# Reconfigure from scratch
./build_and_run.ps1 -Reconfigure -Media sample.mp4 -Tool media_probe

# Only configure & build (no run)
./build_and_run.ps1 -Tool none
```
Parameters:
- `-Preset` (default dev-debug)
- `-Tool` media_probe | playback_demo | qt_preview | none
- `-Media` path to media file (required unless Tool=none)
- `-Reconfigure` force reconfigure (delete existing build dir)
- `-NoBuild` skip build (run existing)

## Troubleshooting
| Symptom | Cause | Fix |
|---------|-------|-----|
| `MSBUILD : error MSB1009: Project file does not exist. Switch: ALL_BUILD.vcxproj` | Building with MSBuild in a folder where configure never ran or was deleted. | Re-run `cmake --preset vs-debug` (or `dev-debug` for Ninja) to regenerate; ensure you're inside the correct build dir. |
| Preset macro expansion error | CMake <3.25 or corrupted cache | Upgrade CMake; delete `build/<preset>` directory and reconfigure. |
| `find_package(spdlog ...) not found` | vcpkg not set yet | Fallback now fetches sources automatically; set `$env:VCPKG_ROOT` for consistent binary caching. |
| Missing FFmpeg DLLs at runtime | PATH lacks vcpkg `installed/x64-windows/bin` | Use `build_and_run.ps1` (it amends PATH) or prepend path manually. |
| Wrong generator (expects Ninja, got MSBuild) | Mixed presets or manual invoke | Explicitly run chosen preset; clean old build dir to avoid cross-generator cache. |

Clean & retry a failed preset configure:
```powershell
Remove-Item -Recurse -Force .\build\dev-debug
cmake --preset dev-debug
```

