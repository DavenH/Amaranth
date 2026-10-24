#!/usr/bin/env python3
"""Check actual-app Envelope selection feedback and preparation costs."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "envelope-interaction-regression"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-envelope-selection-feedback.json"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    if failed:
        raise AssertionError(f"Fixture command failed: {failed[0]}")
    snapshots = [result["data"] for result in report["results"]
                 if result.get("type") == "snapshotState"]
    if len(snapshots) != 6:
        raise AssertionError("Missing hover and held/released selection checkpoints")
    initial = snapshots[0]
    initial_controls = initial["expandedNodeControls"]
    initial_panel = initial_controls["effect2D"]["panelState"]
    surface = initial_panel["morphSurface"]
    if surface["maximum"] < .8 or len(surface["vertexRails"]) < 3:
        raise AssertionError("Envelope surface lost its authored values or vertex rails")
    if surface["timeAxis"] != "x" or surface["primaryAxis"] != "y":
        raise AssertionError("Envelope surface axes are not transposed")
    if not any(rail["startX"] > 1.2 for rail in surface["vertexRails"]):
        raise AssertionError("Envelope release rails were clipped to time 1")
    initial_edit = max((int(event["editId"]) for event in initial["causalUpdates"]), default=0)
    for index, state in enumerate(snapshots[1:], 1):
        controls = state["expandedNodeControls"]
        panel = controls["effect2D"]["panelState"]
        for field in ("documentRevision", "documentGraphRevision", "graphRevision"):
            if state[field] != initial[field]:
                raise AssertionError(f"Checkpoint {index}: {field} changed")
        edit = max((int(event["editId"]) for event in state["causalUpdates"]), default=0)
        if edit != initial_edit or controls["model"] != initial_controls["model"]:
            raise AssertionError(f"Checkpoint {index}: selection scheduled or serialized an edit")
        for counter in ("rasterizerRefreshCount", "sliceEvaluationCount", "panelPreparationCount", "panelLayoutCount"):
            if panel[counter] != initial_panel[counter]:
                raise AssertionError(f"Checkpoint {index}: {counter} increased "
                                     f"from {initial_panel[counter]} to {panel[counter]}")
        if panel["morphSurface"]["rebuildCount"] != initial_panel["morphSurface"]["rebuildCount"]:
            raise AssertionError(f"Checkpoint {index}: selection rebuilt the morph surface")
        if panel["morphSurface"]["snapshotReadbackCount"] != initial_panel["morphSurface"]["snapshotReadbackCount"]:
            raise AssertionError(f"Checkpoint {index}: selection redrew/read back the unchanged morph surface")
        if state["performance"]["audioPlanCopyCount"] != initial["performance"]["audioPlanCopyCount"]:
            raise AssertionError(f"Checkpoint {index}: selection copied the audio plan")
        if not state["compileSucceeded"] or state["validationIssueCount"]:
            raise AssertionError(f"Checkpoint {index}: invalid graph")
    for state in snapshots[3:5]:
        panel = state["expandedNodeControls"]["effect2D"]["panelState"]
        if not panel["boxSelecting"] or panel["selectionBox"]["width"] <= 0 or panel["selectionBox"]["height"] <= 0:
            raise AssertionError("Pointer movement did not display the selection box")
    first_box = snapshots[3]["expandedNodeControls"]["effect2D"]["panelState"]["selectionBox"]
    second_box = snapshots[4]["expandedNodeControls"]["effect2D"]["panelState"]["selectionBox"]
    if second_box["width"] <= first_box["width"] or second_box["height"] <= first_box["height"]:
        raise AssertionError("Second pointer movement did not grow the selection box")
    if snapshots[-1]["expandedNodeControls"]["effect2D"]["panelState"]["movingVertexCount"] == 0:
        raise AssertionError("Selection did not retain enclosed vertices on release")
    final_surface = snapshots[-1]["expandedNodeControls"]["effect2D"]["panelState"]["morphSurface"]
    if final_surface["expandedRenderCacheHits"] <= initial_panel["morphSurface"]["expandedRenderCacheHits"]:
        raise AssertionError("Unchanged morph surface did not reuse its prepared snapshot")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path, help="Verify a saved report without launching the app")
    args = parser.parse_args()
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    report_path = args.report or ARTIFACTS / "interaction-report.json"
    if args.report is None:
        command = [str(REPO / "scripts/cycle_automation.py"), "run", "--product", "cycle-v2",
                   "--fixture", str(FIXTURE), "--report", str(report_path), "--json"]
        if args.no_build:
            command.append("--no-build")
        subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
    verify(json.loads(report_path.read_text()))
    print("Envelope hover and selection update presentation without graph edits, "
          "rasterization, panel preparation or surface rebuilds.")


if __name__ == "__main__":
    main()
