Use this at the start of every Copilot/Claude session.

Read before changes:
- docs/Guidelines/CI_MEMORY.md
- .github/workflows/tests.yml
- .github/workflows/pre-build-linux.yml
- .github/workflows/pre-build-windows.yml
- .vscode/tasks.json
- vcpkg.json (if present)

Report back in ≤5 bullets:
1) Which workflows exist & purpose (tests vs build).
2) Ubuntu packages required (from CI_MEMORY.md).
3) Which VS Code tasks mirror CI.
4) Current failing step & one-sentence root cause.
5) The minimal scope you intend to change.

Do not propose code until this summary is shown.

Kickoff prompt (one-liner):
Use AI_SESSION_BOOTSTRAP. Then fetch artifacts, summarize root cause in 1 sentence, propose a minimal diff patch, give local/act/CI validation, open/update PR. Repeat with smaller diffs if new errors appear.