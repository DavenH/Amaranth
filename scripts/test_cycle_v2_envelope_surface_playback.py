#!/usr/bin/env python3
"""Verify Envelope 3D voice dots, section coverage and cached playback rendering."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-envelope-surface-playback.json"
ARTIFACTS = REPO / ".context/envelope-surface-playback"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    assert not failed, failed[:1]
    states = [result["data"] for result in report["results"]
              if result.get("type") == "snapshotState"]
    assert len(states) == 11, len(states)
    for group, palette in ((states[:5], "bipolar"), (states[5:9], "blues")):
        before = group[0]
        surface = before["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
        held_positions = []
        for index, state in enumerate(group):
            panel = state["expandedNodeControls"]["effect2D"]["panelState"]
            current = panel["morphSurface"]
            expected_voices = 0 if index in (0, len(group) - 1) else 2
            assert current["palette"] == palette
            assert current["playbackVoiceCount"] == expected_voices
            assert len(current["playbackDots"]) == expected_voices
            assert all(line["axis"] == "y" for line in current["playbackCursors"])
            if expected_voices:
                assert len(current["playbackCursors"]) == 2
                phase_lines = [line["position"] for line in panel["playbackCursors"]
                               if line["axis"] == "x"]
                for dot, line, time in zip(current["playbackDots"], current["playbackCursors"], phase_lines):
                    assert dot["y"] == line["position"], (dot, line)
                    assert dot["x"] == time, (dot, time)
                held_positions.append([dot["x"] for dot in current["playbackDots"]])
            for field in ("rebuildCount", "textureUploadCount"):
                assert current[field] == surface[field], (index, field)
            for field in ("documentRevision", "documentGraphRevision", "documentDirty",
                          "graphRevision", "canUndo", "canRedo", "previewRenderCount", "previewStats"):
                assert state[field] == before[field], (index, field)
            assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]
        assert held_positions[0] != held_positions[1], "Envelope dots stopped moving"
    pitch = states[0]["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
    assert all(section["releaseStart"] > pitch["timeMaximum"]
               and section["signalEnd"] < pitch["timeMaximum"] for section in pitch["sections"])
    released = states[7]["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
    assert all(dot["x"] > 1 for dot in released["playbackDots"]), "Note-off did not reach the release tail"
    zoomed = states[9]["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
    assert zoomed["zoomControls"]["horizontal"]["size"] < released["zoomControls"]["horizontal"]["size"]
    assert zoomed["rebuildCount"] == released["rebuildCount"]
    assert zoomed["textureUploadCount"] == released["textureUploadCount"]
    compact = states[10]["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
    assert compact["palette"] == "blues"
    assert compact["playbackVoiceCount"] == 2
    assert len(compact["playbackDots"]) == 2


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
    print("Envelope dots follow their voice cut lines and Phase clocks; pitch stays bipolar, "
          "missing tails stay empty, and playback/zoom preserve prepared surfaces and textures.")


if __name__ == "__main__":
    main()
