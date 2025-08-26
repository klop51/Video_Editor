# CI Quality Gate Policy

Purpose: Ensure merges to `main` occur only when recent build/test health is strong, reducing regressions and stabilizing developer velocity.

## 1. Objectives
- Keep `main` green (rapid, reliable builds).
- Detect declining quality early (rolling success-rate dip).
- Prevent regression merges (merge queue + retest exact merge commit).
- Provide clear, automated PASS/FAIL signal (single quality-gate check).

## 2. Core Metrics
| Metric | Definition | Window | Target | Fail Condition |
|--------|------------|--------|--------|----------------|
| Build Success Rate | Passed required workflow runs / total runs | Last 20 runs of main | ≥ 90% | < 85% hard fail; 85–90% warn |
| Consecutive Failures | Sequential failing runs on main | Running | 0–1 | ≥ 2 triggers stabilization mode |
| MTTR (optional) | Median time from first red to next green | Rolling 14 days | < 2h | > 4h (notify) |

Initial implementation focuses on Build Success Rate + Consecutive Failures.

## 3. Status Levels
| Level | Criteria | Action |
|-------|----------|--------|
| PASS | SuccessRate ≥ 90% & consecutiveFail < 2 | Normal merges allowed |
| WARN | 85% ≤ SuccessRate < 90% OR consecutiveFail == 1 | Label new PRs `quality-watch` |
| FAIL | SuccessRate < 85% OR consecutiveFail ≥ 2 | Block merges except labeled `stabilize` |

## 4. Gating Flow
1. Metrics job runs (schedule + after each main build) → outputs JSON & GitHub Check.
2. Merge queue requires quality-gate job to PASS (or PR has `stabilize` label if FAIL).
3. On downgrade (PASS→WARN/FAIL), bot comments on newest open PR referencing policy.
4. On recovery to PASS, bot removes `quality-watch` labels and posts summary.

## 5. Implementation Phases
| Phase | Scope | Deliverables |
|-------|-------|-------------|
| 1 | Baseline | Script `scripts/ci_quality_gate.py` computing success rate from provided run log (stub now) |
| 2 | Data Source | Workflow step uses `gh api` to fetch last N workflow runs (main) → JSON cache artifact |
| 3 | Gate | Add `quality-gate` job; set branch protection required status |
| 4 | PR Feedback | Bot comment & auto labeling (uses GitHub token) |
| 5 | Stabilization Mode | Enforce label requirement when FAIL |
| 6 | Enhancements | Add MTTR, flaky test exclusion & dashboard badge |

## 6. Data Inputs
- GitHub Actions API: `GET /repos/{owner}/{repo}/actions/runs?branch=main&per_page=50`.
- Filter: Only workflow(s) tagged `primary-ci` (or current workflow name `CI`).
- Required statuses list stored in `ci_gate_config.json` (future).

## 7. Script Contract (`ci_quality_gate.py`)
Input (flags / env):
- `--runs file.json` (array of run objects with status, conclusion, updated_at)
- `--window 20`
- `--warn-threshold 0.90`
- `--fail-threshold 0.85`
- `--consecutive-fail-limit 2`
Output:
- Exit code: 0 (PASS/WARN), 1 (FAIL) by default OR pass for WARN with summary.
- Prints JSON line: `{ "status": "PASS|WARN|FAIL", "success_rate": 0.95, "total": 20, "fails": 1, "consecutive_fails": 0 }`
- Writes optional markdown summary for PR comment.

## 8. Future Enhancements
- Flaky test index: store failing test IDs that pass on immediate re-run; exclude from metric.
- Badge generation via shields endpoint (GitHub pages or gist).
- SLO Alerts: open issue automatically when FAIL persists > 24h.
- MTTR calculation (difference between fail timestamp and next success).

## 9. Human Overrides
- `stabilize` label: permits merge during FAIL (must reference issue root cause plan).
- `quality-ignore` label: skip quality gating for doc-only or markdown-only PR (enforced by path filter script).

## 10. Quick Start Checklist (Phase 1 & 2)
- [ ] Add `scripts/ci_quality_gate.py` (stub added).
- [ ] Add workflow job to fetch last N runs (using `gh api`) & invoke script.
- [ ] Mark job required in branch protection.
- [ ] Observe for 1 week; tune thresholds.

## 11. Example Workflow Snippet (Planned Phase 2)
```yaml
quality-gate:
  runs-on: ubuntu-latest
  permissions:
    actions: read
    contents: read
  steps:
    - uses: actions/checkout@v4
    - name: Fetch workflow runs
      env:
        GH_TOKEN: ${{ github.token }}
      run: |
        gh api repos/${{ github.repository }}/actions/runs \
          -F branch=main -F per_page=30 > runs_raw.json
        jq '{runs: [.workflow_runs[] | {status, conclusion, updated_at}]}' runs_raw.json > runs.json
    - name: Quality Gate
      run: |
        python scripts/ci_quality_gate.py --runs runs.json --window 20 > gate_out.json
        cat gate_out.json
```

## 12. Maintenance
- Revisit thresholds quarterly.
- Document any manual override decisions in `ERROR_TRACKING.md` referencing the AUTO-DRAFT entry ID.

---
Track edits with concise commit messages: `ci: add quality gate policy phase 1`.
