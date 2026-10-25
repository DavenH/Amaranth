#!/usr/bin/env python3
"""Verify sustained polyphonic cursors never fall back to the idle morph slice."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-polyphonic-cursor-continuity.json"
ARTIFACTS = REPO / ".context/polyphonic-cursor-continuity"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    assert not failed, failed[:1]
    states = [result["data"] for result in report["results"]
              if result.get("type") == "snapshotState"]
    assert len(states) == 61, len(states)
    before = states[0]
    positions = set()
    for index, state in enumerate(states[:-1]):
        panel = state["expandedNodeControls"]["trimesh"]
        assert state["performance"]["activeVoiceCount"] == 2, index
        assert panel["playbackVoiceCount"] == 2, f"Lost a cursor at UI sample {index}"
        assert len(panel["playbackCursors"]) == 2, index
        positions.add(tuple(round(cursor["position"], 5) for cursor in panel["playbackCursors"]))
        assert panel["surfaceRebuildCount"] == before["expandedNodeControls"]["trimesh"]["surfaceRebuildCount"]
        for field in ("documentRevision", "documentGraphRevision", "documentDirty",
                      "graphRevision", "previewRenderCount", "previewStats"):
            assert state[field] == before[field], (index, field)
        assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]
    assert len(positions) > 5, "Voice cursors stopped following the actual scratch envelope"
    assert states[-1]["expandedNodeControls"]["trimesh"]["playbackVoiceCount"] == 0
    assert states[-1]["performance"]["activeVoiceCount"] == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path, help="Verify an existing fixture report")
    args = parser.parse_args()
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    report_path = args.report or ARTIFACTS / "report.json"
    if args.report is None:
        command = [str(REPO / "scripts/cycle_automation.py"), "run", "--product", "cycle-v2", "--clean",
                   "--fixture", str(FIXTURE), "--report", str(report_path), "--json"]
        if args.no_build:
            command.append("--no-build")
        completed = subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
        (ARTIFACTS / "run.json").write_text(completed.stdout)
    verify(json.loads(report_path.read_text()))
    print("All 60 held-note samples retain both moving cursors; release clears them. "
          "Graph, audio plan, previews and surface builds remain unchanged.")


if __name__ == "__main__":
    main()
