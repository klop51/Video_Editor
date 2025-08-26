#!/usr/bin/env python3
"""Re-run flaky tests multiple times to assess stability with precise test name mapping.

Enhancements:
1. Parses CTestTestfile.cmake files under build directory to build mapping of test names to source hints.
2. Matches flaky list entries (filenames or regex) to actual test names (exact or substring fallback).
3. Runs only resolved test names individually collecting per-test attempt/fail counts.
4. Computes instability score per test: failures / attempts.
5. Auto-removes tests from flaky list after X fully clean quarantine cycles (configurable via --clean-threshold, tracked in state file).
6. Optionally comments on open PRs (if GH token & PR number present) when persistent failures remain.

Non-blocking: always exits 0.
"""
from __future__ import annotations
import argparse, pathlib, re, json, subprocess, sys, os, math

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--flaky-file', default='tests/FLAKY_TESTS.txt')
    ap.add_argument('--repeat', type=int, default=5)
    ap.add_argument('--build-dir', default='build/dev-debug')
    ap.add_argument('--report', default='flaky_quarantine_report.json')
    ap.add_argument('--ctest-preset', default='dev-debug-tests')
    ap.add_argument('--state-file', default='.flaky_quarantine_state.json', help='Tracks clean run counts & history for auto-removal')
    ap.add_argument('--clean-threshold', type=int, default=3, help='Candidate for removal after N consecutive clean quarantines')
    ap.add_argument('--pr-number', type=int, help='PR number to comment on (set by workflow)')
    ap.add_argument('--comment-persist-failures', action='store_true', help='Post PR comment if failures persist')
    ap.add_argument('--demote-threshold', type=float, default=0.05, help='Average instability below this across history-windows enables demotion when clean streak met')
    ap.add_argument('--history-windows', type=int, default=5, help='Number of recent quarantine results to average for demotion hysteresis')
    ap.add_argument('--sparkline-file', default='flaky_instability_sparkline.txt')
    return ap.parse_args()

def main():
    args = parse_args()
    fpath = pathlib.Path(args.flaky_file)
    if not fpath.exists():
        print('No flaky list file; skipping')
        pathlib.Path(args.report).write_text(json.dumps({'skipped': True, 'reason': 'no file'}), encoding='utf-8')
        return 0
    patterns = []
    for line in fpath.read_text(encoding='utf-8').splitlines():
        line=line.strip()
        if not line or line.startswith('#'): continue
        # If looks like a filename, derive base token
        m = re.match(r'([A-Za-z0-9_\-]+)\.(cpp|cc|cxx)$', line)
        if m:
            base = m.group(1)
            patterns.append(base)
        else:
            patterns.append(line)
    if not patterns:
        print('No usable flaky patterns; skip')
        pathlib.Path(args.report).write_text(json.dumps({'skipped': True, 'reason': 'empty patterns'}), encoding='utf-8')
        return 0
    # Discover test names from CTestTestfile.cmake
    test_files = list(pathlib.Path(args.build_dir).rglob('CTestTestfile.cmake'))
    discovered = set()
    test_meta: dict[str, dict] = {}
    # Capture test name and optional command/executable
    test_name_re = re.compile(r'ADD_TEST\("([^"\\]+)"\s+"([^"\\]+)"')
    test_name_simple = re.compile(r'ADD_TEST\("([^"\\]+)"')
    for tf in test_files:
        try:
            txt = tf.read_text(encoding='utf-8', errors='ignore')
        except Exception:
            continue
        for m in test_name_re.finditer(txt):
            name = m.group(1)
            cmd_path = m.group(2)
            discovered.add(name)
            test_meta.setdefault(name, {})['command'] = cmd_path
        # Fallback simple matches where command not captured
        for m in test_name_simple.finditer(txt):
            name = m.group(1)
            discovered.add(name)
            test_meta.setdefault(name, {})
    discovered_list = sorted(discovered)
    # Map patterns to concrete test names
    selected = set()
    for pat in patterns:
        try:
            rx = re.compile(pat)
            matched = [t for t in discovered_list if rx.search(t)]
            if matched:
                selected.update(matched)
                continue
        except re.error:
            pass
        # fallback substring
        matched = [t for t in discovered_list if pat in t]
        selected.update(matched)
    selected = sorted(selected)
    if not selected:
        print('No test names resolved from patterns; skipping execution')
        pathlib.Path(args.report).write_text(json.dumps({'skipped': True, 'reason': 'no test names resolved', 'patterns': patterns}, indent=2), encoding='utf-8')
        return 0
    print('Resolved flaky tests:', ', '.join(selected))
    per_test = {}
    aggregate_failures = 0
    raw_log_parts = []
    for name in selected:
        attempts = 0
        failures = 0
        for i in range(args.repeat):
            attempts += 1
            cmd = ['ctest','--preset', args.ctest_preset, '-R', f'^{re.escape(name)}$', '--output-on-failure']
            proc = subprocess.run(cmd, capture_output=True, text=True, cwd='.')
            out = proc.stdout + '\n' + proc.stderr
            raw_log_parts.append(f"=== Attempt {i+1}/{args.repeat} : {name} ===\n{out}\n")
            if proc.returncode != 0:
                failures += 1
        instability = failures / attempts if attempts else 0.0
        per_test[name] = {
            'attempts': attempts,
            'failures': failures,
            'instability': round(instability,4),
            'command': test_meta.get(name, {}).get('command')
        }
        aggregate_failures += failures
    # State management for auto-removal
    state_path = pathlib.Path(args.state_file)
    state = {}
    if state_path.exists():
        try:
            state = json.loads(state_path.read_text(encoding='utf-8'))
        except Exception:
            state = {}
    removed = []
    # For each pattern track clean streak + instability history (avg of related tests)
    pattern_status = {}
    for pat in patterns:
        try:
            rex = re.compile(pat)
            related = [t for t in selected if rex.search(t)]
        except re.error:
            related = [t for t in selected if (pat[:-4] in t if pat.endswith('.cpp') else pat in t)]
        if not related:
            continue
        all_clean = all(per_test[t]['failures']==0 for t in related)
        avg_instability = sum(per_test[t]['instability'] for t in related) / len(related)
        rec = state.get(pat, {'clean_streak':0, 'instability_history': []})
        if all_clean:
            rec['clean_streak'] = rec.get('clean_streak',0) + 1
        else:
            rec['clean_streak'] = 0
        # update instability history (bounded)
        hist = rec.get('instability_history', [])
        hist.append(round(avg_instability,4))
        # bound history length
        if len(hist) > max(args.history_windows, 10):
            hist = hist[-max(args.history_windows, 10):]
        rec['instability_history'] = hist
        state[pat] = rec
        # hysteresis removal: need clean streak + avg over last history_windows below threshold
        window_hist = hist[-args.history_windows:] if args.history_windows > 0 else hist
        avg_recent = sum(window_hist)/len(window_hist) if window_hist else 0.0
        pattern_status[pat] = {'clean_streak': rec['clean_streak'], 'avg_recent_instability': round(avg_recent,4)}
        if rec['clean_streak'] >= args.clean_threshold and avg_recent < args.demote_threshold:
            removed.append(pat)
    # Update flaky file if removals
    if removed:
        lines = [l for l in fpath.read_text(encoding='utf-8').splitlines() if l.strip() and l.strip() not in removed]
        fpath.write_text('\n'.join(lines)+'\n', encoding='utf-8')
    state_path.write_text(json.dumps(state, indent=2), encoding='utf-8')
    report = {
        'skipped': False,
        'patterns': patterns,
        'selected_tests': selected,
        'repeat': args.repeat,
        'per_test': per_test,
        'aggregate_failures': aggregate_failures,
        'removed_patterns': removed,
        'pattern_status': pattern_status,
        'clean_threshold': args.clean_threshold,
        'demote_threshold': args.demote_threshold,
        'history_windows': args.history_windows
    }
    pathlib.Path(args.report).write_text(json.dumps(report, indent=2), encoding='utf-8')
    pathlib.Path('flaky_quarantine_output.txt').write_text('\n'.join(raw_log_parts), encoding='utf-8')
    # Generate sparkline artifact
    def spark(values):
        if not values:
            return ''
        blocks = '▁▂▃▄▅▆▇█'
        vs = values
        lo = min(vs); hi = max(vs)
        span = hi - lo if hi != lo else 1e-9
        chars = []
        for v in vs:
            idx = int((v - lo)/span * (len(blocks)-1) + 0.5)
            chars.append(blocks[idx])
        return ''.join(chars)
    spark_lines = []
    for pat, rec in state.items():
        hist = rec.get('instability_history', [])
        spark_lines.append(f"{pat}: {spark(hist)} ({','.join(str(x) for x in hist)})")
    pathlib.Path(args.sparkline_file).write_text('\n'.join(spark_lines)+'\n', encoding='utf-8')
    if aggregate_failures:
        print(f"Flaky quarantine: {aggregate_failures} failures across {len(selected)} tests (non-blocking)")
    else:
        print("Flaky quarantine: all selected tests passed in quarantine attempts")
    # Optional PR comment
    if args.comment_persist_failures and aggregate_failures and args.pr_number and 'GH_TOKEN' in os.environ:
        try:
            body = [f"Flaky quarantine report: {aggregate_failures} failures across {len(selected)} tests."]
            body.append("Instability summary (failures/attempts | instability):")
            for t,d in per_test.items():
                if d['failures']:
                    body.append(f"- {t}: {d['failures']}/{d['attempts']} ({d['instability']})")
            if removed:
                body.append(f"Removed patterns (now stable): {', '.join(removed)}")
            body.append("Pattern status (clean_streak avg_recent):")
            for pat, st in pattern_status.items():
                body.append(f"- {pat}: streak={st['clean_streak']} avg_recent={st['avg_recent_instability']}")
            if os.path.exists(args.sparkline_file):
                body.append("\nInstability sparklines:\n" + pathlib.Path(args.sparkline_file).read_text(encoding='utf-8'))
            subprocess.run(['gh','pr','comment',str(args.pr_number),'--body','\n'.join(body)], check=False)
        except Exception as e:
            print('PR comment failed:', e)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
