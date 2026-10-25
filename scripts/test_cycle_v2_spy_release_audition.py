#!/usr/bin/env python3
"""Verify expanded Spy note-off dragging defers capture and never edits the graph."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "spy-release-audition"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-spy-release-audition.json"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    if failed:
        raise AssertionError(f"Fixture command failed: {failed[0]}")
    states = [result["data"] for result in report["results"]
              if result.get("type") == "snapshotState"]
    if len(states) != 8:
        raise AssertionError(f"Expected eight gesture checkpoints, got {len(states)}")
    before, first, second, committed, unchanged, cancelled, restored, reopened = states
    for state in states:
        for field in ("documentRevision", "documentGraphRevision", "graphRevision",
                      "documentDirty", "canUndo", "canRedo", "previewRenderCount"):
            assert state[field] == before[field], f"Changed {field} during preview audition"
        assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]
        assert state["previewStats"] == before["previewStats"], "Updated unrelated preview products"
        assert state["compileSucceeded"] and not state["validationIssueCount"]
    for state, expected in ((first, .6), (second, .35)):
        assert state["probeDetailReleaseDragging"]
        assert abs(state["probeDetailNoteOffTime"] - expected) < .005
        assert state["probeDetailRenderedNoteOffTime"] == 1.
        assert state["probeDetailRenderCount"] == before["probeDetailRenderCount"]
        assert state["probeDetailContentRevision"] == before["probeDetailContentRevision"]
    for state in (committed, unchanged, cancelled):
        assert not state["probeDetailReleaseDragging"]
        assert abs(state["probeDetailNoteOffTime"] - .35) < .005
        assert state["probeDetailRenderCount"] == before["probeDetailRenderCount"] + 1
        assert state["probeDetailContentRevision"] == committed["probeDetailContentRevision"]
        assert state["probeDetailReleaseDuration"] == before["probeDetailReleaseDuration"]
    assert committed["probeDetailContentRevision"] != before["probeDetailContentRevision"]
    assert restored["probeDetailNoteOffTime"] == 1.
    assert restored["probeDetailRenderCount"] == before["probeDetailRenderCount"] + 2
    assert restored["probeDetailContentRevision"] == before["probeDetailContentRevision"]
    assert reopened["probeDetailNoteOffTime"] == 1.
    assert reopened["probeDetailRenderCount"] == 1


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
    print("Spy note-off drag: no capture during movement, one capture on release; "
          "no-op/cancel/clamp/reset and graph/audio isolation pass.")


if __name__ == "__main__":
    main()
