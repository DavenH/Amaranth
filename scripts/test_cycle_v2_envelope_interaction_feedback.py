#!/usr/bin/env python3
"""Check actual-app Envelope selection costs and morph-position persistence."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "envelope-interaction-regression"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-envelope-selection-feedback.json"
MORPH_FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-envelope-morph-persistence.json"
AXES = ("yellow", "red", "blue")


def morph_values(state, prefix="display"):
    feedback = state["expandedNodeControls"]["effect2D"]
    return {axis: feedback[prefix + axis.title() + "Morph"] for axis in AXES}


def authored_values(state):
    return {item["id"]: float(item["value"])
            for item in state["expandedNodeControls"]["parameters"] if item["id"] in AXES}


def require_coordinates(expected, actual, context):
    for axis in AXES:
        if abs(expected[axis] - actual[axis]) > .0011:
            raise AssertionError(f"{context}: {axis} reset from {expected[axis]} to {actual[axis]}")


def verify_morph(report):
    failed = [result for result in report["results"] if not result["ok"]]
    if failed:
        raise AssertionError(f"Fixture command failed: {failed[0]}")
    states = [result["data"] for result in report["results"] if result.get("type") == "snapshotState"]
    if len(states) != 18:
        raise AssertionError("Missing cube, plane, controller or settled-position checkpoints")
    require_coordinates(morph_values(states[0]), morph_values(states[1]), "Cube press jumped")
    for before_index, held_indices, release_index, changed_axes in (
            (0, (1, 2, 3), 4, ("red", "blue")),
            (5, (6,), 7, ("yellow", "red")),
            (11, (12, 13), 14, ("red", "blue"))):
        before = states[before_index]
        for index in held_indices:
            held = states[index]
            for field in ("documentRevision", "documentGraphRevision", "graphRevision"):
                if held[field] != before[field]:
                    raise AssertionError(f"Held checkpoint {index}: {field} changed")
            if held["expandedNodeControls"]["model"] != before["expandedNodeControls"]["model"]:
                raise AssertionError(f"Held checkpoint {index}: published the model")
            if held["performance"]["audioPlanCopyCount"] != before["performance"]["audioPlanCopyCount"]:
                raise AssertionError(f"Held checkpoint {index}: copied the audio plan")
            require_coordinates(morph_values(held), morph_values(held, "effective"),
                                f"Held checkpoint {index} panel differs from cursor")
        held, released = states[held_indices[-1]], states[release_index]
        for axis in changed_axes:
            if abs(morph_values(held)[axis] - morph_values(before)[axis]) < .01:
                raise AssertionError(f"Gesture did not move {axis}")
            if abs(authored_values(released)[axis] - morph_values(held)[axis]) > .0011:
                raise AssertionError(f"Release did not save final {axis}")
            if axis in ("red", "blue"):
                model_value = released["expandedNodeControls"]["model"]["state"][axis]
                if abs(model_value - authored_values(released)[axis]) > .0011:
                    raise AssertionError(f"Release saved inconsistent {axis} model metadata")
        require_coordinates(morph_values(held), morph_values(released), "Release/repaint")
        if released["documentRevision"] != before["documentRevision"] + 1:
            raise AssertionError("Release did not commit exactly once")
        if held["expandedNodeControls"]["model"]["state"]["mesh"] != released["expandedNodeControls"]["model"]["state"]["mesh"]:
            raise AssertionError("Morph release changed envelope geometry")
    require_coordinates(morph_values(states[4]), morph_values(states[5]), "Editor reopen")
    for before, released, undone, redone in ((states[5], states[7], states[8], states[9]),
                                             (states[11], states[14], states[15], states[16])):
        require_coordinates(authored_values(before), authored_values(undone), "Undo")
        require_coordinates(authored_values(released), authored_values(redone), "Redo")
    controller = morph_values(states[10])
    if abs(controller["blue"] - 96 / 127) > .0001:
        raise AssertionError("Changed CC1 did not resume Blue preview")
    for axis in ("yellow", "red"):
        if abs(controller[axis] - morph_values(states[9])[axis]) > .0001:
            raise AssertionError(f"Changed Blue controller replaced unrelated {axis}")
    if states[10]["documentRevision"] != states[9]["documentRevision"]:
        raise AssertionError("Controller preview published a document edit")
    require_coordinates(authored_values(states[0]), authored_values(states[17]), "Document reset")
    if abs(morph_values(states[17])["blue"] - 96 / 127) > .0001:
        raise AssertionError("Document reset retained the preceding manual Blue position")
    for index, state in enumerate(states):
        require_coordinates(morph_values(state), morph_values(state, "effective"),
                            f"Checkpoint {index} effective position")
        if not state["compileSucceeded"] or state["validationIssueCount"]:
            raise AssertionError(f"Checkpoint {index}: invalid graph")


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
    parser.add_argument("--morph", action="store_true", help="Check morph gesture persistence instead of selection")
    parser.add_argument("--report", type=Path, help="Verify a saved report without launching the app")
    args = parser.parse_args()
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    report_path = args.report or ARTIFACTS / ("morph-report.json" if args.morph else "interaction-report.json")
    if args.report is None:
        command = [str(REPO / "scripts/cycle_automation.py"), "run", "--product", "cycle-v2",
                   "--fixture", str(MORPH_FIXTURE if args.morph else FIXTURE), "--report", str(report_path), "--json"]
        if args.no_build:
            command.append("--no-build")
        subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
    if args.morph:
        verify_morph(json.loads(report_path.read_text()))
        print("Envelope cube and plane positions survive release/reopen; controller takeover, "
              "undo/redo and document reset retain their publication boundaries.")
    else:
        verify(json.loads(report_path.read_text()))
        print("Envelope hover and selection update presentation without graph edits, "
              "rasterization, panel preparation or surface rebuilds.")


if __name__ == "__main__":
    main()
