#!/usr/bin/env python3
"""Show the visible preset-load stages from Cycle V2 agent reports."""

import argparse
import json
from pathlib import Path
from statistics import mean


def load_rows(path):
    report = json.loads(path.read_text())
    failures = [result for result in report.get("results", []) if result.get("ok") is False]
    if failures:
        raise ValueError(f"{path}: {len(failures)} agent commands failed")

    rows = []
    for result in report.get("results", []):
        if result.get("type") != "inspectCanvasPerformance":
            continue
        data = result.get("data", {})
        load = data.get("presetLoad", {})
        if not load.get("fileName") or load.get("firstPaintEndMs") is None:
            raise ValueError(f"{path}: incomplete first-paint telemetry")
        workspace = load.get("workspaceStages", {})
        paint = load.get("firstPaintStages", {})
        preview = data.get("previewPipeline", {}).get("stages", {})
        tiles = data.get("slowestNodeTiles", [])
        rows.append((
            load["fileName"],
            load["firstPaintEndMs"],
            workspace.get("audioGraphPreparation", 0),
            preview.get("previewAudio", {}).get("meanMs", 0),
            paint.get("nodes", 0),
            paint.get("spyRail", 0),
            f"{tiles[0]['nodeId']} {tiles[0]['durationMs']:.0f}" if tiles else "-",
        ))
    if not rows:
        raise ValueError(f"{path}: no canvas performance inspections")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", nargs="+", type=Path)
    args = parser.parse_args()

    for path in args.reports:
        rows = load_rows(path)
        print(f"\n{path} ({len(rows)} loads)")
        print("Preset                          First paint  Prepare  Preview  Nodes  Spy rail  Slowest tile")
        for name, total, prepare, preview, nodes, spy, tile in rows:
            print(f"{name[:30]:30} {total:11.0f} {prepare:8.0f} {preview:8.0f} {nodes:6.0f} {spy:9.0f}  {tile}")
        averages = [mean(row[index] for row in rows) for index in range(1, 6)]
        print(f"{'Mean':30} {averages[0]:11.0f} {averages[1]:8.0f} {averages[2]:8.0f} {averages[3]:6.0f} {averages[4]:9.0f}")


if __name__ == "__main__":
    main()
