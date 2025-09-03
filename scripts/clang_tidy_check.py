#!/usr/bin/env python3
"""Run clang-tidy over compile_commands and fail if any warnings/errors appear.

This is intentionally simple; refine filters as needed.
"""
import json, subprocess, sys, os

# Check multiple possible build directories
POSSIBLE_COMPDB_PATHS = [
    'build/dev-debug/compile_commands.json',
    'build/qt-debug/compile_commands.json',
    'build/vs-debug/compile_commands.json',
    'build/simple-debug/compile_commands.json'
]

compdb_path = None
for path in POSSIBLE_COMPDB_PATHS:
    if os.path.exists(path):
        compdb_path = path
        break

if not compdb_path:
    print(f"Missing compile_commands.json in any of: {POSSIBLE_COMPDB_PATHS}", file=sys.stderr)
    print("Run cmake --preset dev-debug or cmake --preset qt-debug to generate it.", file=sys.stderr)
    sys.exit(1)

print(f"Using compile_commands.json from {compdb_path}")

try:
    with open(compdb_path, 'r', encoding='utf-8') as f:
        compdb = json.load(f)
except FileNotFoundError:
    print(f"Missing {compdb_path}; run cmake configure with a preset that enables CMAKE_EXPORT_COMPILE_COMMANDS", file=sys.stderr)
    sys.exit(1)

issues = 0
for entry in compdb:
    file = entry.get('file')
    if not file:
        continue
    if not file.endswith(('.cpp', '.cc', '.cxx')):
        continue
    if 'vcpkg_installed' in file:
        continue
    result = subprocess.run([
        'clang-tidy', file, '--quiet', '--', '-std=c++20'
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out = result.stdout
    if 'warning:' in out or 'error:' in out:
        print(out)
        issues += 1

if issues:
    print(f"clang-tidy issues: {issues}")
    sys.exit(1)
else:
    print("clang-tidy: no issues")
