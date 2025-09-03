# 🔧 CLAUDE CI FIXER — Single-File Playbook

**Emergency CI Troubleshooting Reference** | Last Updated: September 3, 2025

## 🚨 EMERGENCY QUICK FIXES

### CI Hanging? (4+ hours)
```bash
# 1. Cancel hanging workflows immediately
# 2. Check for infinite loops in build scripts
# 3. Use timeout protections (our workflows have 20min/45min limits)
# 4. Run locally first: scripts\ci_lint_and_test.ps1 -Configuration qt-debug -SkipTidy
```

### clang-tidy Failing?
```bash
# Local check (Windows):
powershell scripts\ci_lint_and_test.ps1 -Configuration qt-debug
# If no clang-tidy locally: Use -SkipTidy, fix on Linux CI

# Common fixes:
- Install autoconf-archive in workflow
- Use VCPKG_FORCE_SYSTEM_BINARIES=1 (Linux only)
- Check compile_commands.json exists
```

### Exception Policy Violations (DR-0003)?
```bash
# These are REAL issues to fix, not CI problems:
# Core modules (media_io, decode, playback, audio, render, fx, cache, gfx) 
# must be exception-free for performance

# Current violations found:
- hdr_processor.cpp:466: throw Core::InvalidArgumentException
- integrated_gpu_manager.cpp:180: throw std::runtime_error
- wide_color_gamut_support.cpp:470: throw Core::InvalidArgumentException

# Fix: Replace throws with error codes/optional returns
```

---

## 🎯 CI AUTOPILOT SYSTEM OVERVIEW

### Split Workflow Architecture
| Workflow | Purpose | Duration | Triggers |
|----------|---------|----------|----------|
| **tests.yml** | Fast feedback (clang-tidy, lint, policy) | ~20min | Every push |
| **build.yml** | Full matrix builds (Linux/Win × Debug/Release) | ~45min | Every push |
| **quality-gate.yml** | Health monitoring, PR labeling | ~5min | Schedule |

### Local Development Workflow
```bash
# 1. Before pushing - run local CI check:
scripts\ci_lint_and_test.ps1 -Configuration qt-debug -SkipTidy

# 2. Fix issues locally first (faster iteration)
# 3. Push only when local tests pass
# 4. Monitor fast tests.yml for quick feedback
# 5. Use build.yml for comprehensive validation
```

---

## 🛠️ TROUBLESHOOTING TOOLKIT

### Local CI Reproduction Script
```powershell
# Location: scripts\ci_lint_and_test.ps1
# Usage examples:
scripts\ci_lint_and_test.ps1                           # Default (dev-debug)
scripts\ci_lint_and_test.ps1 -Configuration qt-debug   # Use qt-debug preset  
scripts\ci_lint_and_test.ps1 -SkipTidy                 # Skip clang-tidy
scripts\ci_lint_and_test.ps1 -Configuration qt-debug -SkipTidy  # Both options

# What it checks:
✅ CMake configuration with vcpkg
✅ compile_commands.json generation
✅ clang-tidy analysis (if available)
✅ Exception policy violations (DR-0003)
✅ Simple configure test
```

### VS Code Tasks (Ctrl+Shift+P → "Tasks: Run Task")
```json
- "CI: Lint and Test"           # Full local CI reproduction
- "CI: Build Check"             # Quick build validation  
- "CI: Quality Gate Check"      # Monitor CI health
- "build-qt-debug"              # Standard Qt build
- "run-video-editor-debug"      # Build and run
```

### GitHub Actions Local Testing
```bash
# Install act (if not already installed):
choco install act-cli
# or: winget install nektos.act

# Run workflows locally:
act -j tests          # Fast tests workflow
act -j build-linux    # Linux build matrix
act -j build-windows  # Windows build matrix
```

---

## 📋 SYSTEMATIC DEBUGGING CHECKLIST

### Phase 1: Environment Validation
- [ ] **CMake version**: 3.20+ required
- [ ] **vcpkg**: C:\vcpkg exists and bootstrapped
- [ ] **Qt6**: Installed via vcpkg (qtbase, qtmultimedia)
- [ ] **FFmpeg**: Available via vcpkg
- [ ] **Visual Studio**: 2022 with C++ workload
- [ ] **Python**: 3.x available for scripts

### Phase 2: Local Reproduction
- [ ] **Run local script**: `scripts\ci_lint_and_test.ps1`
- [ ] **Check preset**: Use working preset (qt-debug, not dev-debug)
- [ ] **Verify paths**: All build directories accessible
- [ ] **Test components**: CMake, vcpkg, clang-tidy (optional)

### Phase 3: CI-Specific Issues
- [ ] **Workflow syntax**: YAML indentation correct
- [ ] **Secret variables**: Available in GitHub repository
- [ ] **Dependencies**: autoconf-archive, build-essential installed
- [ ] **Timeouts**: Reasonable limits (20min tests, 45min builds)
- [ ] **Artifacts**: Generated and accessible

### Phase 4: Code Quality Gates
- [ ] **Exception policy**: No throws in core performance modules
- [ ] **Namespace usage**: Proper app::, core::, gpu:: structure
- [ ] **Header guards**: Consistent #pragma once
- [ ] **Memory management**: RAII patterns, Qt parent-child ownership

---

## 🔍 COMMON FAILURE PATTERNS & SOLUTIONS

### 1. vcpkg Installation Failures
```yaml
# Problem: "Could not fetch powershell-core"
# Solution: Don't use VCPKG_FORCE_SYSTEM_BINARIES on Windows

# Linux (CI):
- name: Setup vcpkg
  run: |
    export VCPKG_FORCE_SYSTEM_BINARIES=1
    export VCPKG_MAX_CONCURRENCY=1

# Windows (CI + Local):
- name: Setup vcpkg  
  run: |
    $env:VCPKG_MAX_CONCURRENCY = "1"
    # No VCPKG_FORCE_SYSTEM_BINARIES on Windows
```

### 2. Compiler Detection Issues
```yaml
# Problem: "C++ compiler is not able to compile a simple test program"
# Solution: Ensure proper Visual Studio environment

# Check: Developer Command Prompt vs PowerShell
# Fix: Use vcvars64.bat or proper VS setup in CI
```

### 3. Missing Dependencies
```yaml
# Problem: clang-tidy not found, autotools missing
# Solution: Install in workflow setup

- name: Install dependencies
  run: |
    sudo apt-get update
    sudo apt-get install -y \
      build-essential \
      autoconf-archive \
      clang-tidy \
      ninja-build
```

### 4. Timeout Issues
```yaml
# Problem: Workflows hanging indefinitely
# Solution: Always set timeouts

jobs:
  tests:
    timeout-minutes: 20  # Fast feedback
  build:
    timeout-minutes: 45  # Comprehensive builds
```

### 5. Exception Policy Violations
```cpp
// Problem: Core modules using exceptions
// Bad:
throw Core::InvalidArgumentException("Device cannot be null");

// Good: Use error codes or optionals
if (!device) {
    return Core::Result::InvalidArgument;
}
// or
std::optional<Device> getDevice() {
    if (!valid) return std::nullopt;
    return device;
}
```

---

## 📊 QUALITY METRICS & MONITORING

### CI Health Indicators
```bash
# Green: All workflows passing, <10min feedback
# Yellow: Some failures, 10-20min feedback  
# Red: Major issues, >20min feedback or hangs

# Quality Gate Metrics:
- MTTR (Mean Time To Resolution)
- Failure rate by component
- Build duration trends
- Exception policy compliance
```

### Performance Benchmarks
```bash
# Target CI Times:
tests.yml:     <20 minutes (lint + policy + simple build)
build.yml:     <45 minutes (full matrix)
Local script:  <10 minutes (qt-debug configure + checks)

# Local Performance:
Video Editor: 61.59 FPS (1080p), 26.2 FPS (4K)
Build time:   ~5-10 minutes (incremental)
```

---

## 🚀 WORKFLOW OPTIMIZATION

### Split Strategy Benefits
```bash
# Before: Single 4+ hour hanging workflow
# After: Fast tests (20min) + Comprehensive builds (45min)

Advantages:
✅ Rapid feedback loop for common issues
✅ Parallel execution reduces total time
✅ Easier to identify failure points
✅ Better resource utilization
✅ Reduced CI costs
```

### Caching Strategy
```yaml
# vcpkg cache (saves 10-15 minutes):
- name: Cache vcpkg
  uses: actions/cache@v4
  with:
    path: vcpkg_installed
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}

# CMake cache:
- name: Cache CMake
  uses: actions/cache@v4  
  with:
    path: build/*/CMakeCache.txt
    key: cmake-${{ runner.os }}-${{ hashFiles('CMakeLists.txt') }}
```

---

## 🎯 DECISION FLOWCHART

```
CI Failure?
├── Hanging (>1 hour)?
│   ├── YES → Cancel, check for infinite loops, add timeouts
│   └── NO → Continue to specific error
├── clang-tidy failure?
│   ├── YES → Check autoconf-archive, VCPKG_FORCE_SYSTEM_BINARIES
│   └── NO → Continue to next check
├── Build configuration failure?
│   ├── YES → Check vcpkg setup, compiler detection
│   └── NO → Continue to next check  
├── Exception policy violation?
│   ├── YES → Fix code (replace throws with error codes)
│   └── NO → Continue to next check
├── Test failures?
│   ├── YES → Run locally first, fix specific issues
│   └── NO → Check environment/dependencies
└── Unknown → Run local reproduction script first
```

---

## 📚 REFERENCE COMMANDS

### Essential Local Commands
```powershell
# Full CI reproduction:
scripts\ci_lint_and_test.ps1 -Configuration qt-debug -SkipTidy

# Standard build:
cmake --preset qt-debug
cmake --build build/qt-debug --config Debug

# Clean rebuild:  
powershell -Command "Remove-Item -Recurse -Force build/qt-debug -ErrorAction SilentlyContinue; cmake --preset qt-debug"

# Run application:
.\run_video_editor_debug.bat
```

### GitHub Actions Debugging
```bash
# Re-run failed jobs:
gh run rerun <run-id>

# View logs:
gh run view <run-id> --log

# List recent runs:
gh run list --limit 10
```

### VS Code Integration
```json
// settings.json additions for CI integration:
{
    "github-actions.workflows.pinned.workflows": [
        ".github/workflows/tests.yml",
        ".github/workflows/build.yml"
    ],
    "github-actions.workflows.pinned.refresh.enabled": true
}
```

---

## 🏆 SUCCESS CRITERIA

### CI Pipeline Health
- [ ] **tests.yml**: Passes in <20 minutes consistently
- [ ] **build.yml**: Passes in <45 minutes across all matrix combinations  
- [ ] **Local script**: Matches CI behavior 100%
- [ ] **Quality gate**: Green status, automated PR labeling working
- [ ] **Exception policy**: Zero violations in core performance modules

### Development Velocity  
- [ ] **Feedback loop**: <20 minutes from push to actionable results
- [ ] **Local-first**: Issues caught and fixed locally before CI
- [ ] **Systematic debugging**: Clear steps for any CI failure
- [ ] **Documentation**: Complete troubleshooting knowledge captured
- [ ] **Monitoring**: Proactive quality tracking and alerting

---

**🎯 Remember**: The goal is predictable, fast CI that enhances development velocity rather than blocking it. When in doubt, run the local reproduction script first!

---

## 📞 ESCALATION PATH

1. **Local script fails**: Check environment setup, preset configuration
2. **CI differs from local**: Environment differences, dependency versions
3. **Persistent hangs**: Timeout issues, infinite loops, resource constraints
4. **Quality gate alerts**: Systematic issues, need architectural review
5. **Unknown failures**: Document in CI_AUTOPILOT.md, update this playbook

**Last Resort**: Use manual debugging with act (local GitHub Actions) and detailed logging.
