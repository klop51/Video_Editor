#!/usr/bin/env python3
"""Scrape ctest log for failed tests and produce draft ERROR_TRACKING.md entries.

Outputs one markdown file per failure under the specified output directory.
This does not modify ERROR_TRACKING.md automatically (manual review required).
"""
from __future__ import annotations
import argparse, datetime, pathlib, re, sys, json, hashlib

FAIL_RE = re.compile(r'^\s*([^:]+)::([^ ]+)\s+FAILED', re.MULTILINE)
ASSERT_RE = re.compile(r'(?P<file>[^\s:]+):(\d+):\s+FAILED:\s+REQUIRE[_THAT]*\s*(?P<expr>.*)')
SUMMARY_RE = re.compile(r'(\d+)\s+tests?\s+failed', re.IGNORECASE)
BUILD_ERROR_RE = re.compile(r'^(?P<file>[^\s:][^:]+):(\d+):(\d+:)?\s*(fatal\s+)?error[:\s].*', re.IGNORECASE)
MSVC_ERROR_RE = re.compile(r'^(?P<file>[A-Z]:[^:(]+)\((\d+)\):\s+error\s+[A-Z0-9]+:.*')

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--log', required=True, help='Path to LastTest.log or test output')
    ap.add_argument('--out', required=True, help='Output directory for draft entries')
    ap.add_argument('--build-log', help='Optional build log to scrape compiler errors')
    ap.add_argument('--append-main', action='store_true', help='Append consolidated AUTO-DRAFT entry to ERROR_TRACKING.md')
    ap.add_argument('--flaky-list', help='File with regex patterns (one per line) for flaky test files to ignore')
    ap.add_argument('--dedupe-window', type=int, default=5, help='If existing recent AUTO-DRAFT with same hash present in top N blocks, skip re-adding')
    ap.add_argument('--auto-classify-flaky', action='store_true', help='Append newly frequent intermittent failures to flaky list (heuristic)')
    ap.add_argument('--failure-history', help='JSON file to accumulate failure fingerprints for classification')
    return ap.parse_args()

def main():
    args = parse_args()
    log_path = pathlib.Path(args.log)
    if not log_path.exists():
        print(f"Log not found: {log_path}", file=sys.stderr)
        return 1
    text = log_path.read_text(encoding='utf-8', errors='ignore')

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    date = datetime.date.today().isoformat()
    failures = []

    flaky_patterns = []
    if args.flaky_list and pathlib.Path(args.flaky_list).exists():
        for line in pathlib.Path(args.flaky_list).read_text(encoding='utf-8').splitlines():
            line=line.strip()
            if not line or line.startswith('#'): continue
            try:
                flaky_patterns.append(re.compile(line))
            except re.error:
                print(f"Invalid flaky regex skipped: {line}")

    def is_flaky(file: str) -> bool:
        return any(p.search(file) for p in flaky_patterns)

    # Basic heuristic: Catch2 prints lines with FAILED and summary lines
    for m in ASSERT_RE.finditer(text):
        file = m.group('file')
        expr = m.group('expr').strip()
        if not is_flaky(file):
            failures.append((file, expr))

    build_failures = []
    if args.build_log:
        bpath = pathlib.Path(args.build_log)
        if bpath.exists():
            btext = bpath.read_text(encoding='utf-8', errors='ignore')
            for line in btext.splitlines():
                m = BUILD_ERROR_RE.match(line) or MSVC_ERROR_RE.match(line)
                if m:
                    file = m.group('file') if 'file' in m.groupdict() else 'UNKNOWN'
                    build_failures.append((file, line.strip()))
        else:
            print(f"Build log not found: {bpath}")

    if not failures and SUMMARY_RE.search(text):
        failures.append(("UNKNOWN", "See log excerpt"))

    if not failures and not build_failures:
        print("No test or build failures parsed.")
        return 0

    combined = failures + build_failures
    for idx, (file, expr) in enumerate(combined, 1):
        is_build = (idx > len(failures))
        title = ("Build Error" if is_build else "Test Failure") + f" {idx}: {file}"
        body = f"""## {date} – {title} (Sev3)
**Area**: {'build' if is_build else 'tests'}

### Symptom
Failure in `{file}`:
```
{expr}
```

### Impact
Test suite failing in CI.

### Environment Snapshot
- OS: ${{ runner.os }} (CI)
- Preset: dev-debug

### Hypotheses Considered
1. (Fill after triage)

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 | Inspected failing assertion | Pending |

### Root Cause
UNKNOWN (auto-draft)

### Resolution
(Pending)

### Preventive Guardrails / Follow Ups
- Add targeted unit test for edge case once root cause known.

### Verification
(Pending)

---
"""
        draft_file = out_dir / f"draft_{idx:02d}.md"
        draft_file.write_text(body, encoding='utf-8')
        print(f"Wrote {draft_file}")

    if args.append_main:
        main_log = pathlib.Path('ERROR_TRACKING.md')
        if main_log.exists():
            summary_title = f"AUTO-DRAFT CI Failures ({len(combined)} issue{'s' if len(combined)!=1 else ''})"
            # Basic insertion after first '---' similar to new_error_entry script logic
            existing = main_log.read_text(encoding='utf-8')
            # Build a hash fingerprint of combined failures (file+expr)
            fingerprint = hashlib.sha256('\n'.join(f"{f}|{e}" for f,e in combined).encode('utf-8')).hexdigest()[:16]
            # Deduplication: search top blocks for same fingerprint marker
            recent_blocks = re.findall(r'## \d{4}-\d{2}-\d{2} – AUTO-DRAFT CI Failures.*?(?:\n---)', existing, re.DOTALL)
            taken = False
            count_checked = 0
            for block in recent_blocks:
                if fingerprint in block:
                    print("Duplicate AUTO-DRAFT fingerprint found; skipping append.")
                    taken = True
                    break
                count_checked += 1
                if count_checked >= args.dedupe_window:
                    break
            if taken:
                return 0
            insert_pos = existing.find('\n---\n')
            if insert_pos != -1:
                insert_pos += len('\n---\n')
            else:
                insert_pos = 0
            snippets = []
            for (file, expr) in combined[:5]:  # cap preview
                snippets.append(f"- {file}: {expr[:120] + ('…' if len(expr)>120 else '')}")
            new_block = f"## {date} – {summary_title} (Sev3)\n**Area**: CI Aggregate\n\n### Symptom\nCI detected failures in tests/build. See attached drafts (artifact) or lines below.\n\n" \
                        f"### Preview (first {len(snippets)}):\n" + '\n'.join(snippets) + f"\n\n### Fingerprint\n`{fingerprint}`\n\n### Root Cause\nUNKNOWN (auto-draft)\n\n---\n"
            updated = existing[:insert_pos] + new_block + existing[insert_pos:]
            # Prepend index entry
            updated = re.sub(r'(## Index\n)', f"\\1- {date} – {summary_title} (Sev3)\n", updated, count=1)
            main_log.write_text(updated, encoding='utf-8')
            print("Appended AUTO-DRAFT entry to ERROR_TRACKING.md")
        else:
            print("ERROR_TRACKING.md not found; skip append.")
    # Optional flaky classification (very naive placeholder: count occurrences in history)
    if args.auto_classify_flaky and args.failure_history:
        hist_path = pathlib.Path(args.failure_history)
        history = {}
        if hist_path.exists():
            try:
                history = json.loads(hist_path.read_text(encoding='utf-8'))
            except Exception:
                history = {}
        for file, expr in failures:
            key = f"{file}|{expr[:80]}"
            entry = history.get(key, {'count':0})
            entry['count'] += 1
            history[key] = entry
        hist_path.write_text(json.dumps(history, indent=2), encoding='utf-8')
        # Promote to flaky if count >=3 and not already listed
        if args.flaky_list:
            fl_path = pathlib.Path(args.flaky_list)
            existing_flaky = fl_path.read_text(encoding='utf-8').splitlines() if fl_path.exists() else []
            added = []
            for k,v in history.items():
                if v.get('count',0) >= 3:
                    test_file = k.split('|',1)[0]
                    if not any(line.strip()==test_file for line in existing_flaky):
                        existing_flaky.append(test_file)
                        added.append(test_file)
            if added:
                fl_path.write_text('\n'.join(existing_flaky)+"\n", encoding='utf-8')
                print("Auto-classified flaky tests added:", ', '.join(added))

    return 0

if __name__ == '__main__':
    raise SystemExit(main())
