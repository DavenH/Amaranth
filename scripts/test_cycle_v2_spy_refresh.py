#!/usr/bin/env python3
"""Audit opened and pinned Spy refresh counts and waveform presentation continuity."""

import argparse
import json
import os
from pathlib import Path

from cycle_automation_support.identity import workspace
from cycle_automation_support.lifecycle import ensure
from cycle_automation_support.operations import run_fixture
from cycle_automation_support.profiles import PRODUCTS, ROOT


def unchanged(first, second, keys):
    for key in keys:
        assert first[key] == second[key], f"Unexpected change in {key}: {first[key]} -> {second[key]}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path, default=ROOT / ".context/spy-refresh-accepted.json")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path, "run_id": f"spy-refresh-{os.getpid()}"}

    def verify(record):
        owner["app_pid"] = record["pid"]
        result = run_fixture(record, ROOT / "scripts/fixtures/cycle-v2-agent-spy-refresh-continuity.json",
                             args.report.resolve(), owner)
        states = [item["data"] for item in result["results"] if item["type"] == "snapshotState"]
        before, pending, after, noop, edited, undone, redone, moved, zoomed, spectral, low, time, raw = states
        first = before["probeWaveform"]["waveform"]
        replacement = after["probeWaveform"]["waveform"]
        assert after["probeDetailRenderCount"] == before["probeDetailRenderCount"] + 1
        assert replacement["sourceBuilds"] == first["sourceBuilds"] + 1
        assert replacement["publications"] == first["publications"] + 1
        assert replacement["blankPaints"] == first["blankPaints"]
        assert pending["probeWaveform"]["waveform"]["hasImage"]
        assert replacement["retainedImagePaints"] > first["retainedImagePaints"]
        assert after["comparison"]["captureCount"] == before["comparison"]["captureCount"] + 1
        stages = next(item["data"]["previewPipeline"]["stages"] for item in result["results"]
                      if item["type"] == "inspectCanvasPerformance")
        assert stages["previewAudio"]["count"] == 1
        assert stages["expandedProbeTotal"]["count"] == 1
        assert stages["worker"]["count"] == 0
        unchanged(after, noop, ("probeDetailRenderCount", "probeDetailContentRevision"))
        assert after["comparison"]["captureCount"] == noop["comparison"]["captureCount"]
        for previous, current in ((noop, edited), (edited, undone), (undone, redone)):
            assert current["comparison"]["captureCount"] == previous["comparison"]["captureCount"] + 1
            assert current["probeDetailRenderCount"] == previous["probeDetailRenderCount"] + 1
            assert current["comparison"]["cards"][0]["contentRevision"] != previous["comparison"]["cards"][0]["contentRevision"]
        assert undone["comparison"]["cards"][0]["contentRevision"] == after["comparison"]["cards"][0]["contentRevision"]
        assert redone["comparison"]["cards"][0]["contentRevision"] == edited["comparison"]["cards"][0]["contentRevision"]
        for state in (moved, zoomed):
            unchanged(redone, state, ("probeDetailRenderCount", "probeDetailContentRevision"))
            assert state["comparison"]["captureCount"] == redone["comparison"]["captureCount"]
        assert 0.045 < zoomed["probeWaveform"]["camera"]["w"] < 0.06
        assert zoomed["probeWaveform"]["waveform"]["filled"]
        for state in (spectral, low):
            assert state["probes"][1]["view"] == "spectrum"
            assert state["probeDetailRows"] > 257
            assert state["comparison"]["cards"][0]["spectral"]
        assert time["probes"][1]["view"] == "time"
        assert raw["probeWaveform"]["waveform"]["ready"]
        return result

    result = ensure(PRODUCTS["cycle-v2"], owner, short_id, clean=True,
                    task=verify, scoped=True, build_app=not args.no_build)
    print(json.dumps({"ok": result["ok"], "commands": len(result["results"]),
                      "report": str(args.report.resolve())}))


if __name__ == "__main__":
    main()
