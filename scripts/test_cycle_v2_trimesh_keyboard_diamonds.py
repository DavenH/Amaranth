#!/usr/bin/env python3
"""Verify stationary Trimesh keyboard voice markers and individual retirement."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-trimesh-keyboard-diamonds.json"
ARTIFACTS = REPO / ".context/trimesh-keyboard-diamonds"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    assert not failed, failed[:1]
    states = [result["data"] for result in report["results"]
              if result.get("type") == "snapshotState"]
    assert len(states) == 15, len(states)
    panels = [state["expandedNodeControls"]["trimesh"] for state in states]
    for first, second, axis in ((1, 2, "red"), (9, 10, "blue")):
        for index in (first, second):
            panel = panels[index]
            assert panel["primaryViewAxis"] == axis
            assert panel["playbackVoiceCount"] == 2
            assert len(panel["playbackDots"]) == 2
            assert panel["playbackMarkerAnchor"] == "primaryAxis"
            for marker, line in zip(panel["playbackDots"], panel["playbackCursors"]):
                assert marker["x"] == line["position"]
                assert marker["y"] == 0
            positions = sorted(marker["x"] for marker in panel["playbackDots"])
            assert 0 < positions[0] < positions[1] < 1, positions
        assert panels[first]["playbackDots"] == panels[second]["playbackDots"], "Keyboard markers drifted"
    for index in (0, 7, 8, 11, 12, 14):
        assert panels[index]["playbackVoiceCount"] == 0
        assert not panels[index]["playbackDots"]
    for index in (3, 4):
        assert panels[index]["playbackVoiceCount"] == 2
        assert not panels[index]["playbackDots"], "Non-key or unlinked axis showed keyboard markers"
    assert len(panels[5]["playbackDots"]) == 2
    assert panels[6]["playbackVoiceCount"] == 1
    assert len(panels[6]["playbackDots"]) == 1
    assert panels[6]["playbackDots"][0]["x"] == max(marker["x"] for marker in panels[5]["playbackDots"])
    assert panels[13]["pitchSpansColumns"]
    assert len(panels[13]["playbackDots"]) == 2
    assert panels[13]["playbackDots"] == panels[9]["playbackDots"]
    for group in ((0, 1, 2), (5, 6, 7), (8, 9, 10, 11), (12, 13, 14)):
        before = states[group[0]]
        for index in group[1:]:
            state = states[index]
            assert panels[index]["surfaceRebuildCount"] == panels[group[0]]["surfaceRebuildCount"]
            for field in ("documentRevision", "documentGraphRevision", "documentDirty",
                          "graphRevision", "canUndo", "canRedo", "previewRenderCount", "previewStats"):
                assert state[field] == before[field], (index, field)
            assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]


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
    print("Red and Blue keyboard markers remain stationary; unlinking and other axes suppress them, "
          "voice retirement clears them, and playback leaves surfaces and document state unchanged.")


if __name__ == "__main__":
    main()
