#!/usr/bin/env python3
"""Check the acid-stab-3 morph gesture in both expanded surface render paths."""

import json
import os
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "trimesh-morph-regression"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-trimesh-spectral-morph-surface-stability.json"
RUNNER = REPO / "scripts/run_cycle_v2_agent.sh"


def run_fixture(name, disable_shader):
    video = ARTIFACTS / f"{name}.mp4"
    report = ARTIFACTS / f"{name}.json"
    log = ARTIFACTS / f"{name}.log"
    env = os.environ.copy()
    env["CYCLE_OS_VIDEO_PATH"] = str(video)
    env["CYCLE_OS_VIDEO_FPS"] = "30"
    if disable_shader:
        env["CYCLE_DISABLE_SCALAR_SURFACE_SHADER"] = "1"
    else:
        env.pop("CYCLE_DISABLE_SCALAR_SURFACE_SHADER", None)
    subprocess.run(
        [str(RUNNER), str(FIXTURE), str(report), str(log)],
        cwd=REPO,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    result = json.loads(report.read_text())
    if not all(command["ok"] for command in result["results"]):
        raise AssertionError(f"{name} fixture failed: {report}")

    # The two snapshots bracket pointer-up at the unchanged drag position.
    fixture_commands = json.loads(FIXTURE.read_text())["commands"]
    snapshots = [result["results"][index]["data"]
                 for index, command in enumerate(fixture_commands)
                 if command["command"] == "snapshotState"]
    if len(snapshots) != 2:
        raise AssertionError(f"Expected two gesture snapshots in {report}")
    rebuilds = [state["expandedNodeControls"]["trimesh"]["surfaceRebuildCount"]
                for state in snapshots + [result["snapshot"]]]
    initial = result["results"][1]["data"]["trimesh"]
    if rebuilds[0] <= initial["surfaceRebuildCount"]:
        raise AssertionError("Red drag did not rebuild the secondary-axis surface")
    dragged_rows = snapshots[0]["expandedNodeControls"]["trimesh"]["surfaceRowCount"]
    if dragged_rows == initial["surfaceRowCount"]:
        raise AssertionError("Red drag did not change the displayed pitch rows")
    if len(set(rebuilds)) != 1:
        raise AssertionError(f"Mouse-up rebuilt an unchanged surface: {rebuilds}")
    return video, result


def row_transitions(video, result):
    dimensions = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "csv=p=0", str(video)],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    width, height = (int(value) for value in dimensions.split(","))
    diagnostics = next(command["data"] for command in result["results"]
                       if command.get("data", {}).get("panels"))
    editor = diagnostics["expandedEditorBounds"]
    panel = next(panel["bounds"] for panel in diagnostics["panels"]
                 if panel["id"] == "trimeshPanel3D")
    x = 2 * round((editor["x"] + panel["x"] + 0.25 * panel["width"]) / 2)
    y = 2 * round((height - result["snapshot"]["height"]
                   + editor["y"] + panel["y"] + 0.03 * panel["height"]) / 2)
    sample_height = 2 * round(0.64 * panel["height"] / 2)
    if x < 0 or x + 2 > width or y < 0 or y + sample_height > height:
        raise AssertionError("Spectral panel sample falls outside the recorded window")

    frame = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-sseof", "-0.25", "-i", str(video),
         "-frames:v", "1",
         "-vf", f"crop=2:{sample_height}:{x}:{y},format=gray",
         "-f", "rawvideo", "-"],
        check=True, capture_output=True,
    ).stdout
    column = frame[::2]
    if len(column) != sample_height:
        raise AssertionError(f"Could not decode the spectral panel in {video}")
    return sum(first != second for first, second in zip(column, column[1:]))


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    normal_video, normal_result = run_fixture("scalar", False)
    fallback_video, fallback_result = run_fixture("fallback", True)
    normal_transitions = row_transitions(normal_video, normal_result)
    fallback_transitions = row_transitions(fallback_video, fallback_result)
    if fallback_transitions > normal_transitions + 8:
        raise AssertionError(
            "Fallback interpolated spectral rows: "
            f"scalar={normal_transitions}, fallback={fallback_transitions}"
        )
    print(f"Spectral rows stable: scalar={normal_transitions}, "
          f"fallback={fallback_transitions}; no surface rebuild on mouse-up")
    print(f"Captures: {ARTIFACTS}")


if __name__ == "__main__":
    main()
