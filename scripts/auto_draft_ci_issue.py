#!/usr/bin/env python3
"""Create or update a CI stabilization draft PR/issue when persistent failures occur.

Heuristic:
- Triggered after N consecutive FAIL gate statuses (provided via --consecutive)
- Summarizes latest metrics JSON + recent AUTO-DRAFT fingerprint.
- Uses GitHub CLI (gh) if available; otherwise prints instructions.

This script is idempotent: it searches for an open PR with title prefix '[CI Stabilization]' and updates its body.
"""
from __future__ import annotations
import argparse, json, subprocess, shutil, sys, pathlib, re

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gate-metrics', required=True, help='Path to gate metrics JSON produced by ci_quality_gate.py')
    ap.add_argument('--fingerprint', help='Latest AUTO-DRAFT fingerprint if available')
    ap.add_argument('--consecutive', type=int, required=True)
    ap.add_argument('--threshold', type=int, default=3, help='Consecutive FAIL threshold to trigger')
    ap.add_argument('--dry-run', action='store_true')
    return ap.parse_args()

def gh(*args):
    return subprocess.check_output(['gh', *args], text=True)

def main():
    args = parse_args()
    if args.consecutive < args.threshold:
        print('Below consecutive threshold; skip draft PR.')
        return 0
    metrics = json.loads(pathlib.Path(args.gate_metrics).read_text(encoding='utf-8'))
    title = '[CI Stabilization] Address persistent FAIL gate'
    body = [
        f"### CI Stabilization Trigger\n",
        f"Consecutive FAIL count: {args.consecutive}\n",
        f"Success rate: {metrics.get('success_rate')*100:.1f}%\n",
        f"MTTR (mins): {metrics.get('mttr_minutes')}\n",
        f"Stability score: {metrics.get('stability_score')}\n",
    ]
    if args.fingerprint:
        body.append(f"Latest AUTO-DRAFT fingerprint: `{args.fingerprint}`\n")
    body.append('\n### Proposed Actions\n- [ ] Root cause investigation\n- [ ] Isolate failing tests\n- [ ] Add/adjust guardrail\n- [ ] Document resolution in ERROR_TRACKING.md\n')
    body_text = ''.join(body)
    if shutil.which('gh') and not args.dry_run:
        # find existing PR
        try:
            existing = gh('pr','list','--state','open','--json','number,title')
            import json as _json
            for pr in _json.loads(existing):
                if pr['title'].startswith('[CI Stabilization]'):
                    gh('pr','comment',str(pr['number']),'--body',body_text)
                    print('Updated existing stabilization PR #', pr['number'])
                    return 0
            # create new
            gh('pr','create','--title',title,'--body',body_text,'--draft')
            print('Created new stabilization draft PR.')
        except subprocess.CalledProcessError as e:
            print('gh CLI error:', e, file=sys.stderr)
            print(body_text)
    else:
        print('DRY RUN / gh not available. Suggested PR body:\n')
        print(body_text)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
