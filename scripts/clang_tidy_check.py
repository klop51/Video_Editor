#!/usr/bin/env python3
"""Run clang-tidy over compile_commands and fail if any warnings/errors appear.

This is intentionally simple; refine filters as needed.
"""
import json, subprocess, sys, os

COMPDB_PATH = 'build/dev-debug/compile_commands.json'

try:
    with open(COMPDB_PATH, 'r', encoding='utf-8') as f:
        compdb = json.load(f)
except FileNotFoundError:
    print(f"Missing {COMPDB_PATH}; did you run the dev-debug configure preset?", file=sys.stderr)
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
