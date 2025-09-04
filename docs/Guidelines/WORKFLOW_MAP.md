**tests.yml**
  - Lint (ruff/clang-tidy), type check (mypy), unit tests.
  - Optional: lightweight CMake configure only.
  - Avoid building Qt/vcpkg unless tests require it.
- **build.yml**
  - Full toolchain (vcpkg/Qt/CMake), package artifacts, matrix (ubuntu/windows).
  - Install Ubuntu packages from CI_MEMORY.md.

Rule: If you add Ubuntu packages for build, **mirror** them in tests if tests do any native build.
