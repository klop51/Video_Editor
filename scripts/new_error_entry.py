#!/usr/bin/env python3
"""Insert a new error tracking entry template at the top of ERROR_TRACKING.md.

Usage:
  python scripts/new_error_entry.py "Short Title" --sev 3 --area "module / tooling"
"""
from __future__ import annotations
import argparse, datetime, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TRACK_FILE = ROOT / 'ERROR_TRACKING.md'
INDEX_HEADER = '## Index'

TEMPLATE = """## {date} – {title} (Sev{sev})
**Area**: {area}

### Symptom
(Describe concise symptom and representative errors.)

### Impact
(Explain cost/risk.)

### Environment Snapshot
- OS:
- Toolchain / Compiler:
- Dependencies / Preset:

### Hypotheses Considered
1. ...

### Actions Taken
| Step | Action | Result |
|------|--------|--------|
| 1 |  |  |

### Root Cause
(Once known; or mark UNKNOWN.)

### Resolution
(What change / action resolved it.)

### Preventive Guardrails / Follow Ups
- 

### Verification
(How the fix was validated.)

---
"""

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('title', help='Short title (no date prefix)')
    ap.add_argument('--sev', type=int, default=3, choices=[1,2,3,4], help='Severity 1-4')
    ap.add_argument('--area', default='(area)', help='Area description')
    args = ap.parse_args()

    if not TRACK_FILE.exists():
        print(f"ERROR: {TRACK_FILE} not found", file=sys.stderr)
        return 1

    text = TRACK_FILE.read_text(encoding='utf-8')
    date = datetime.date.today().isoformat()

    # Insert new entry after the first horizontal rule (---) following usage section
    # We look for the first existing entry marker '## 20'
    insert_pos = text.find('\n---\n', 0)
    if insert_pos == -1:
        insert_pos = 0
    else:
        insert_pos += len('\n---\n')

    new_entry = TEMPLATE.format(date=date, title=args.title, sev=args.sev, area=args.area)

    # Update index: find '## Index' section; add line right after header
    index_match = re.search(r'## Index\n([\s\S]+)', text)
    if not index_match:
        print('WARNING: Index section not found; appending at end.')
        updated = text + '\n' + new_entry
    else:
        # Build new index line
        index_start = index_match.start(1)
        # Extract current index block end (until two newlines before template marker or EOF)
        lines = text[index_start:].splitlines()
        # Insert new index entry after header line
        # We assume lines[0] is first index line or blank; we insert at top of list.
        # Construct new index line
        index_line = f"- {date} – {args.title} (Sev{args.sev})"
        # Find position just before existing first list item
        index_block_end = index_start
        # Insert index line after '## Index' line
        pattern = r'(## Index\n)'
        text = re.sub(pattern, f"\\1{index_line}\n", text, count=1)
        # Recompute insert_pos due to text shift
        insert_pos = text.find('\n---\n', 0)
        insert_pos += len('\n---\n')
        updated = text[:insert_pos] + new_entry + text[insert_pos:]

    TRACK_FILE.write_text(updated, encoding='utf-8')
    print(f"Inserted new error entry template for '{args.title}' (Sev{args.sev}).")

if __name__ == '__main__':
    raise SystemExit(main())
