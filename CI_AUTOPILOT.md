# 🚀 CI Autopilot System

This repository uses an advanced CI Autopilot system that enables rapid fixing of CI issues through local reproduction and systematic debugging.

## 📋 Quick Start

### Test Before Push
```powershell
# In VS Code: Ctrl+Shift+B → "Lint & Test"
# Or manually:
pwsh scripts/ci_lint_and_test.ps1
```

### Build Locally (Mirror CI)
```powershell
# In VS Code: Ctrl+Shift+B → "CMake Build" 
# Or manually:
pwsh scripts/cmake_build.ps1 -Config Debug
pwsh scripts/cmake_build.ps1 -Config Release
```

### Check CI Status
```powershell
# In VS Code: Ctrl+Shift+B → "CI: Check GitHub status"
# Or manually (requires GitHub CLI):
gh run list --limit 10
gh run view --log  # View latest run logs
```

## 🏗️ Workflow Architecture

### 1. **tests.yml** - Fast Feedback (⚡ ~20 min)
- **Trigger**: Every push/PR
- **Purpose**: Quick lint, clang-tidy, policy checks
- **Contents**:
  - clang-tidy analysis
  - Exception policy enforcement (DR-0003)
  - Simple configure test (no FFmpeg)
  - Build dependency validation

### 2. **build.yml** - Comprehensive Builds (🔨 ~45 min)
- **Trigger**: Push to main, tags, PRs
- **Purpose**: Full cross-platform builds
- **Matrix**: Linux/Windows × Debug/Release
- **Features**:
  - vcpkg dependency management
  - ccache acceleration (Linux)
  - Artifact generation
  - Test execution
  - Failure analysis

### 3. **quality-gate.yml** - Quality Monitoring (📊 Daily)
- **Trigger**: After tests/build completion, daily schedule
- **Purpose**: Automated quality monitoring
- **Features**:
  - Success rate tracking
  - PR labeling (quality-watch, needs-stabilize)
  - MTTR metrics
  - Automated remediation guidance

## 🛠️ Local Development Tools

### VS Code Tasks
Access via `Ctrl+Shift+B` or Command Palette:

| Task | Purpose | Duration |
|------|---------|----------|
| **Lint & Test** | Mirror tests.yml locally | ~5 min |
| **CMake Build** | Mirror build.yml locally | ~10 min |
| **CI: run with act** | Run GitHub Actions locally | ~20 min |
| **CI: Check GitHub status** | View recent CI runs | Instant |
| **CI: Download latest logs** | Get detailed failure logs | ~1 min |

### PowerShell Scripts

#### `scripts/ci_lint_and_test.ps1`
```powershell
# Full CI test pipeline locally
pwsh scripts/ci_lint_and_test.ps1

# What it does:
# ✅ CMake configure (dev-debug preset)
# ✅ clang-tidy analysis 
# ✅ Exception policy check
# ✅ Simple configure test
# ✅ Environment validation
```

#### `scripts/cmake_build.ps1`
```powershell
# Build with different configurations
pwsh scripts/cmake_build.ps1 -Config Debug -Preset qt-debug
pwsh scripts/cmake_build.ps1 -Config Release -Preset vs-release

# What it does:
# ✅ CMake configure with vcpkg
# ✅ Parallel build (2 cores)
# ✅ Test execution (if available)
# ✅ Artifact validation
# ✅ Profiling data collection
```

## 🔧 Local CI Reproduction

### Install Prerequisites
```powershell
# GitHub CLI (for status checking)
winget install GitHub.cli

# Act (for local GitHub Actions)
choco install act -y
# or: scoop install act
```

### Run Specific Jobs Locally
```powershell
# Run clang-tidy job locally
act -P ubuntu-latest=catthehacker/ubuntu:act-latest -j tests

# Run build matrix locally (Debug only)
act -P ubuntu-latest=catthehacker/ubuntu:act-latest -j build
```

## 📊 Quality Gate System

### Automatic PR Management
The quality gate automatically:
- 🏷️ **Labels PRs** based on CI health
- 💬 **Comments** with remediation guidance  
- 📈 **Tracks** success rates and MTTR
- 🚨 **Alerts** when intervention needed

### Labels Used
- `quality-watch`: CI success rate below threshold
- `needs-stabilize`: Required during CI failures (with remediation plan)

### Remediation Workflow
1. **WARN**: Success rate <90% → PR gets `quality-watch` label
2. **FAIL**: Multiple failures → PR gets comment with required remediation steps
3. **Recovery**: Automatic label removal when CI stabilizes

## 🎯 Systematic CI Fixing

### The Copilot/Claude Fixing Loop
1. **Reproduce locally**: Use tasks/scripts to mirror CI exactly
2. **Identify root cause**: Use detailed error logs and diagnostics
3. **Apply targeted fix**: Make minimal, focused changes
4. **Validate locally**: Run scripts to verify fix
5. **Push and monitor**: Use GitHub Actions extension in VS Code
6. **Iterate**: Repeat for any remaining issues

### Emergency Protocols
```powershell
# Quick CI status check
gh run list --status failure --limit 5

# Download failure logs for analysis
gh run view <RUN_ID> --log

# Cancel stuck runs
gh run cancel <RUN_ID>

# Re-run failed jobs
gh run rerun <RUN_ID>
```

## 🔍 Debugging Common Issues

### clang-tidy Failures
```powershell
# Check locally first
pwsh scripts/ci_lint_and_test.ps1

# Common fixes:
# - Multiple statements per line
# - Missing const correctness
# - Unused variables
# - Exception policy violations
```

### Build Failures
```powershell
# Check locally with same config
pwsh scripts/cmake_build.ps1 -Config Debug

# Common issues:
# - Missing dependencies
# - vcpkg cache corruption
# - Compiler version mismatch
# - CMake preset issues
```

### vcpkg Issues
```powershell
# Clear vcpkg cache
Remove-Item -Recurse -Force C:\vcpkg\installed
Remove-Item -Recurse -Force C:\vcpkg\buildtrees

# Reconfigure
cmake --preset qt-debug
```

## 📈 Performance Optimizations

### Caching Strategy
- **vcpkg packages**: Cached by OS + vcpkg.json hash
- **ccache**: Cached by OS + build config
- **Build artifacts**: 7-day retention for debugging

### Parallelization
- **vcpkg**: `VCPKG_MAX_CONCURRENCY=2` (prevents OOM)
- **cmake**: `--parallel 2` (balanced performance/resources)
- **Matrix builds**: Independent parallel execution

### Timeouts
- **tests.yml**: 20 minutes (fast feedback)
- **build.yml**: 45 minutes (comprehensive builds)
- **Prevents**: Infinite hangs, resource waste

## 🚀 Future Enhancements

### Planned Features
- [ ] Slack/Discord notifications on CI failures
- [ ] Automated dependency updates with PR creation
- [ ] Performance regression detection
- [ ] Test coverage reporting
- [ ] Static analysis integration (cppcheck, PVS-Studio)

### Integration Opportunities
- [ ] Pre-commit hooks for local validation
- [ ] IDE extensions for real-time feedback
- [ ] Dashboard for CI metrics visualization
- [ ] Automated bisection for regression hunting

## 📞 Support

### Getting Help
1. **Check this README** for common patterns
2. **Run local scripts** to reproduce issues
3. **Check GitHub Actions logs** using VS Code extension
4. **Use GitHub CLI** for detailed failure analysis

### Useful Commands
```powershell
# Full local CI simulation
pwsh scripts/ci_lint_and_test.ps1; pwsh scripts/cmake_build.ps1

# Monitor CI in real-time
watch -n 10 'gh run list --limit 5'

# Emergency CI health check
gh run list --status failure --limit 10 | grep -E "(main|master)"
```

---

**📝 Note**: This CI Autopilot system is designed to make CI failures fast to diagnose and fix. The split workflow approach provides quick feedback while maintaining comprehensive testing coverage.
