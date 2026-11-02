#!/usr/bin/env python3
"""Verify palette selectors, preset persistence and presentation-only work."""

import argparse
import json
import os
from pathlib import Path

from cycle_automation_support.identity import workspace
from cycle_automation_support.lifecycle import ensure
from cycle_automation_support.operations import run_fixture
from cycle_automation_support.profiles import PRODUCTS, ROOT


def unchanged(before, after, keys):
    for key in keys:
        if before[key] != after[key]:
            raise AssertionError(f"Palette selection changed {key}: {before[key]} -> {after[key]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path,
                        default=ROOT / ".context/surface-appearance-accepted-ui-report.json")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": f"surface-appearance-{os.getpid()}"}

    def verify(record):
        owner["app_pid"] = record["pid"]
        result = run_fixture(record,
                             ROOT / "scripts/fixtures/cycle-v2-agent-surface-appearance.json",
                             args.report.resolve(), owner)
        states = [item["data"] for item in result["results"] if item["type"] == "snapshotState"]
        before, after, pinned_before, pinned_after = states
        unchanged(before, after, ("documentRevision", "graphRevision", "previewRenderCount",
                                  "probeDetailRenderCount", "probeDetailContentRevision"))
        for first, second in ((before["probeWaveform"]["waveform"], after["probeWaveform"]["waveform"]),
                              (pinned_before["comparison"]["cards"][0]["surfaceView"]["waveform"],
                               pinned_after["comparison"]["cards"][0]["surfaceView"]["waveform"])):
            unchanged(first, second, ("sourceBuilds", "heatBuilds", "geometryBuilds"))
            if second["colourBuilds"] <= first["colourBuilds"]:
                raise AssertionError("Palette change did not recolour the visible waveform")
        result["presentationOnly"] = True
        args.report.resolve().write_text(json.dumps(result, indent=2) + "\n")
        return result

    result = ensure(PRODUCTS["cycle-v2"], owner, short_id, clean=True,
                    task=verify, scoped=True, build_app=not args.no_build)
    print(json.dumps({"ok": result["ok"], "commands": len(result["results"]),
                      "report": str(args.report.resolve())}))


if __name__ == "__main__":
    main()
