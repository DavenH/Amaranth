#!/usr/bin/env python3
"""Verify real-app Trimesh pointer feedback and publication boundaries."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "trimesh-interaction-regression"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-trimesh-interaction-publication.json"


def morph_values(state):
    return {item["id"]: item["value"]
            for item in state["expandedNodeControls"]["morphSliders"]}


def authored_values(state):
    return {item["id"]: item["value"]
            for item in state["expandedNodeControls"]["parameters"]
            if item["id"] in ("yellow", "red", "blue")}


def edit_id(state):
    return max((int(event["editId"]) for event in state["causalUpdates"]), default=0)


def require_same_graph(before, after, context):
    for field in ("documentRevision", "documentGraphRevision", "graphRevision"):
        if before[field] != after[field]:
            raise AssertionError(f"{context}: {field} changed")
    if before["performance"]["audioPlanCopyCount"] != after["performance"]["audioPlanCopyCount"]:
        raise AssertionError(f"{context}: copied the audio plan")
    if authored_values(before) != authored_values(after):
        raise AssertionError(f"{context}: changed authored morph parameters")


def require_no_edits(before, after, context):
    require_same_graph(before, after, context)
    if edit_id(before) != edit_id(after):
        raise AssertionError(f"{context}: scheduled an edit/preview update")


def surface_rebuilds(state):
    return state["expandedNodeControls"]["trimesh"]["surfaceRebuildCount"]


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    if failed:
        raise AssertionError(f"Fixture command failed: {failed[0]}")
    snapshots = [result["data"] for result in report["results"]
                 if result.get("type") == "snapshotState"]
    if len(snapshots) < 7:
        raise AssertionError("Missing morph gesture checkpoints")

    before = snapshots[0]
    for index, axis in enumerate(("red", "blue", "yellow")):
        held, released = snapshots[2 * index + 1:2 * index + 3]
        value = morph_values(held)[axis]
        if abs(value - morph_values(before)[axis]) < .01 or not .75 < value < .9:
            raise AssertionError(f"{axis} pointer did not move its displayed value: {value}")
        require_no_edits(before, held, f"held {axis} rail")
        if abs(value - morph_values(released)[axis]) > .0001:
            raise AssertionError(f"{axis} value reset after release/repaint")
        if surface_rebuilds(held) != surface_rebuilds(released):
            raise AssertionError(f"{axis} release rebuilt the unchanged surface")
        if axis == "yellow":
            require_no_edits(before, released, "primary-axis release")
            if surface_rebuilds(before) != surface_rebuilds(held):
                raise AssertionError("Primary navigation rebuilt the full surface")
        elif released["documentRevision"] != before["documentRevision"] + 1:
            raise AssertionError(f"{axis} release did not commit exactly once")
        before = released

    if len(snapshots) != 12:
        raise AssertionError("Missing cube, controller or document-reset checkpoints")
    held, released, undone, controller, reloaded = snapshots[7:]
    require_no_edits(before, held, "held cube")
    if morph_values(before) == morph_values(held):
        raise AssertionError("Cube movement gave no local coordinate feedback")
    if surface_rebuilds(before) != surface_rebuilds(held):
        raise AssertionError("Held cube rebuilt the full surface")
    if released["documentRevision"] != before["documentRevision"] + 1:
        raise AssertionError("Cube release did not commit exactly once")
    if authored_values(before) == authored_values(released):
        raise AssertionError("Cube release did not change its final coordinates")
    if authored_values(undone) != authored_values(before):
        raise AssertionError("One undo did not restore the cube coordinates")
    require_same_graph(undone, controller, "live controller preview")
    if abs(morph_values(controller)["blue"] - 32 / 127) > .0001:
        raise AssertionError("Changed CC1 did not resume Blue preview")
    if morph_values(controller)["yellow"] != morph_values(undone)["yellow"]:
        raise AssertionError("Blue controller reset unrelated Yellow navigation")
    if abs(morph_values(reloaded)["yellow"] - float(authored_values(reloaded)["yellow"])) > .0001:
        raise AssertionError("Document reset retained the preceding graph's manual navigation")
    for state in snapshots:
        if not state["compileSucceeded"] or state["validationIssueCount"]:
            raise AssertionError("Gesture left an invalid graph")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--report", type=Path, help="Verify an existing report instead of running the app")
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
    print("Trimesh rails follow pointer input; primary navigation and held cube publish no edits; "
          "cube release commits and undoes once.")


if __name__ == "__main__":
    main()
