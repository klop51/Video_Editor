**Non‑negotiable loop** the model must follow.

1) **Fetch evidence**: download CI artifacts; on failure, tail `buildtrees/*.log`.
2) **Root cause**: ≤1 sentence.
3) **Patch**: minimal unified diff in ```diff fences; only touch necessary workflow(s); don’t remove packages; don’t touch Windows steps.
4) **Verify**: show local tasks and `act` commands; re-run CI.
5) **PR**: title `ci: fix — <cause>`; body includes cause, what changed, validation steps, and log links.

**Keep tests fast**: `tests.yml` should avoid building Qt/vcpkg unless required.