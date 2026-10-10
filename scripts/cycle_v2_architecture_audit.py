#!/usr/bin/env python3
"""Report Cycle V2 source-size review triggers without imposing a build gate."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "cycle-v2" / "src"
REVIEW_LINES = {".cpp": 800, ".h": 300}
PLAN_LINES = {".cpp": 1200, ".h": 450}


def main() -> None:
    sources = sorted(
        path for path in SOURCE.rglob("*") if path.suffix in REVIEW_LINES
    )
    measured = []
    for path in sources:
        with path.open(encoding="utf-8") as source:
            lines = sum(1 for _ in source)
        if lines >= REVIEW_LINES[path.suffix]:
            level = "PLAN" if lines >= PLAN_LINES[path.suffix] else "REVIEW"
            measured.append((lines, level, path.relative_to(ROOT)))

    print(f"Cycle V2 architecture size triggers: {len(measured)} of {len(sources)} files")
    print("PLAN: record an extraction plan or cohesive-file reason before growth")
    print("REVIEW: inspect responsibilities, composition, and repeated policy")
    for lines, level, path in sorted(measured, reverse=True):
        print(f"{level:6} {lines:5} {path}")


if __name__ == "__main__":
    main()
