#!/usr/bin/env python3
"""Validate the bounded semantic source map."""

from pathlib import Path
import re
import sys


MAX_DESCRIPTION_CHARACTERS = 120
MAX_ENTRIES = 100
MAX_LINES = 240
ENTRY_PATTERN = re.compile(r"^- `([^`]+)` — (.+)$")


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    map_path = repo_root / "docs" / "code-map.md"
    lines = map_path.read_text(encoding="utf-8").splitlines()
    errors: list[str] = []
    entries: dict[str, int] = {}

    if len(lines) > MAX_LINES:
        errors.append(
            f"map has {len(lines)} lines; prune it to at most {MAX_LINES}"
        )

    for line_number, line in enumerate(lines, start=1):
        match = ENTRY_PATTERN.match(line)
        if match is None:
            continue

        relative_path, description = match.groups()
        if relative_path in entries:
            errors.append(
                f"line {line_number}: duplicate path also mapped on line "
                f"{entries[relative_path]}: {relative_path}"
            )
        else:
            entries[relative_path] = line_number

        if len(description) > MAX_DESCRIPTION_CHARACTERS:
            errors.append(
                f"line {line_number}: description has {len(description)} "
                f"characters; maximum is {MAX_DESCRIPTION_CHARACTERS}"
            )

        candidate = (repo_root / relative_path.rstrip("/")).resolve()
        try:
            candidate.relative_to(repo_root)
        except ValueError:
            errors.append(f"line {line_number}: path escapes repository: {relative_path}")
            continue

        if not candidate.exists():
            errors.append(f"line {line_number}: mapped path does not exist: {relative_path}")

    if len(entries) > MAX_ENTRIES:
        errors.append(
            f"map has {len(entries)} entries; merge or prune to at most {MAX_ENTRIES}"
        )

    if not entries:
        errors.append("map contains no entries")

    if errors:
        print("Semantic code map check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        f"Semantic code map OK: {len(entries)} entries, {len(lines)} lines, "
        f"descriptions <= {MAX_DESCRIPTION_CHARACTERS} characters"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
