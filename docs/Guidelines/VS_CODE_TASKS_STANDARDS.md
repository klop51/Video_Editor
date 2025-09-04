Recommended tasks (names stable so models can call them):
```json
{
  "version": "2.0.0",
  "tasks": [
    { "label": "CI: run tests (act)", "type": "shell", "command": "act -P ubuntu-latest=catthehacker/ubuntu:act-latest -j tests" },
    { "label": "CI: run build (act)", "type": "shell", "command": "act -P ubuntu-latest=catthehacker/ubuntu:act-latest -j build" },
    { "label": "CI: download latest artifacts", "type": "shell", "command": "gh run download --dir .ci_artifacts" },
    { "label": "CI: view latest run log", "type": "shell", "command": "gh run view --log" }
  ]
}