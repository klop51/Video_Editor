#!/usr/bin/env python3
"""Compute CI quality gate status & metrics from workflow run JSON.

Input JSON format (minimal): {"runs": [{"status": "completed", "conclusion": "success|failure|...", "updated_at": ISO8601}, ...]}

Outputs JSON summary to stdout and (optionally) writes detailed metrics & badge descriptors.

Metrics added beyond gate status:
 - success_rate (float)
 - total, fails, consecutive_fails
 - mttr_minutes: Mean Time To Recovery across observed windows (failure -> next success)
 - last_failure_age_minutes: Minutes since last non-success (if any), else null
 - stability_score: heuristic (success_rate adjusted by penalty for consecutive fails)

Badge JSON (if requested) uses a Shields.io-compatible minimal schema: {"schemaVersion":1,"label":"ci","message":"PASS 95%","color":"green"}
"""
from __future__ import annotations
import argparse, json, sys, statistics
from datetime import datetime

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runs', required=True, help='JSON file with runs array')
    ap.add_argument('--window', type=int, default=20)
    ap.add_argument('--warn-threshold', type=float, default=0.90)
    ap.add_argument('--fail-threshold', type=float, default=0.85)
    ap.add_argument('--consecutive-fail-limit', type=int, default=2)
    ap.add_argument('--metrics-out', help='Optional path to write extended metrics JSON')
    ap.add_argument('--badge-out', help='Optional path to write a shields.io style badge JSON')
    return ap.parse_args()

def main():
    args = parse_args()
    try:
        data = json.load(open(args.runs, 'r', encoding='utf-8'))
    except FileNotFoundError:
        print('{"error":"runs file not found"}')
        return 1
    runs = data.get('runs') or []
    # keep only completed
    completed = [r for r in runs if r.get('status') == 'completed'][:args.window]
    if not completed:
        print('{"status":"UNKNOWN","reason":"no completed runs"}')
        return 0
    total = len(completed)
    successes = sum(1 for r in completed if r.get('conclusion') == 'success')
    fails = sum(1 for r in completed if r.get('conclusion') not in ('success','skipped','cancelled'))
    # consecutive fail count from most recent first
    consecutive = 0
    for r in completed:
        if r.get('conclusion') == 'success':
            break
        if r.get('conclusion') not in ('skipped','cancelled'):
            consecutive += 1
    success_rate = successes / total if total else 0.0
    if success_rate < args.fail_threshold or consecutive >= args.consecutive_fail_limit:
        status = 'FAIL'
        exit_code = 1
    elif success_rate < args.warn_threshold or consecutive == 1:
        status = 'WARN'
        exit_code = 0
    else:
        status = 'PASS'
        exit_code = 0
    # Compute MTTR (Mean Time To Recovery): durations between a failure and next success.
    mttr_durations = []  # minutes
    last_fail_time = None
    def parse_time(ts: str):
        try:
            return datetime.fromisoformat(ts.replace('Z','+00:00'))
        except Exception:
            return None
    for r in completed:
        t = parse_time(r.get('updated_at',''))
        if not t:
            continue
        if r.get('conclusion') not in ('success','skipped','cancelled') and last_fail_time is None:
            last_fail_time = t
        elif r.get('conclusion') == 'success' and last_fail_time is not None:
            delta = (t - last_fail_time).total_seconds()/60.0
            if delta >= 0:
                mttr_durations.append(delta)
            last_fail_time = None
    mttr_minutes = round(sum(mttr_durations)/len(mttr_durations),2) if mttr_durations else None
    # Age since last failure
    last_failure_age_minutes = None
    for r in completed:
        if r.get('conclusion') not in ('success','skipped','cancelled'):
            t = parse_time(r.get('updated_at',''))
            if t:
                last_failure_age_minutes = round((datetime.utcnow() - t.replace(tzinfo=None)).total_seconds()/60.0,2)
            break
    # Stability score (arbitrary heuristic): success_rate * (1 - min(consecutive,5)/10)
    stability_score = round(success_rate * (1 - min(consecutive,5)/10),4)
    out = {
        'status': status,
        'success_rate': round(success_rate, 4),
        'total': total,
        'fails': fails,
        'consecutive_fails': consecutive,
        'mttr_minutes': mttr_minutes,
        'last_failure_age_minutes': last_failure_age_minutes,
        'stability_score': stability_score
    }
    print(json.dumps(out))
    if args.metrics_out:
        with open(args.metrics_out,'w',encoding='utf-8') as f:
            json.dump(out,f,indent=2)
    if args.badge_out:
        color = {'PASS':'green','WARN':'yellow','FAIL':'red'}.get(status,'lightgrey')
        badge = {
            'schemaVersion': 1,
            'label': 'ci',
            'message': f"{status} {int(round(success_rate*100))}%",
            'color': color
        }
        with open(args.badge_out,'w',encoding='utf-8') as f:
            json.dump(badge,f)
    return exit_code

if __name__ == '__main__':
    raise SystemExit(main())
