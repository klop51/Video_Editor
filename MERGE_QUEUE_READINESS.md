# Merge Queue & Stabilization Readiness

This document outlines how CI gate status integrates with branch protection & merge queue usage.

## Components
- Gate Script: `scripts/ci_quality_gate.py` produces PASS/WARN/FAIL and metrics.
- Failure Scraper: `scripts/ci_failure_scrape.py` generates AUTO-DRAFT summaries and deduplicates via fingerprint.
- Labels:
  - `quality-watch`: Applied on WARN/FAIL.
  - `needs-stabilize`: Required (with plan comment) for merges during FAIL state.

## Recommended Branch Protection
Require the following checks before merge:
- build-test (Debug)
- build-test (Release)
- clang-tidy
- lint-configure
- quality-gate

Enable a merge queue with "Require merge queue" so only batches passing the gate land.

## Stabilization Mode
When gate=FAIL:
1. Add `needs-stabilize` label to PR only after posting a remediation plan comment (what failed, suspected root cause, rollback or fix path, added guardrail).
2. Remove label once gate returns to PASS (or automatically after 48h of sustained PASS).

## Auto Draft PRs (Optional)
The `scripts/auto_draft_ci_issue.py` script can open or update a draft PR summarizing persistent failures after N consecutive FAIL gate statuses.

## Future Enhancements
- Automated MTTR badge in README via scheduled workflow.
- Flaky quarantine workflow rerunning only newly added flaky patterns.
- Automatic label removal when gate recovers.

## Operational Runbook
| Situation | Action |
|-----------|--------|
| Gate FAIL triggered | Investigate latest AUTO-DRAFT in `ERROR_TRACKING.md`; triage failing tests/build logs. |
| Flaky suspicion | Verify count >=3 in `failure_history.json`; confirm auto-added to `tests/FLAKY_TESTS.txt`. |
| New systemic failure | Open manual entry via `new_error_entry.py` with Sev2/Sev1 as appropriate. |
| Merge during FAIL needed | Provide plan comment; apply `needs-stabilize` label; ensure change directly aids recovery. |

---
