**tests.yml** snippet:
```yaml
- name: Lint (ruff)
  run: |
    mkdir -p ci_logs
    ruff check . 2>&1 | tee ci_logs/ruff.txt
- name: Type check (mypy)
  run: |
    mkdir -p ci_logs
    mypy . 2>&1 | tee ci_logs/mypy.txt
- name: Tests
  run: |
    mkdir -p ci_logs
    pytest -q 2>&1 | tee ci_logs/pytest.txt
- name: Upload step logs (always)
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: test-logs
    path: ci_logs
```

**build.yml** snippet:
```yaml
- name: Configure
  run: |
    mkdir -p ci_logs
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tee ci_logs/cmake_configure.txt
- name: Build
  run: |
    mkdir -p ci_logs
    cmake --build build --config Release -- -j 2 2>&1 | tee ci_logs/cmake_build.txt
- name: Upload build logs (always)
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: build-logs
    path: ci_logs
- name: Show vcpkg logs on failure
  if: failure()
  run: |
    find build -path '*/buildtrees/*/*.log' -print -exec tail -n 200 {} \; || true