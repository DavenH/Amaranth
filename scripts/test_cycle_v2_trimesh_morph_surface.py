#!/usr/bin/env python3
"""Check held and released acid-stab-3 spectral magnitude surfaces."""

import json
import os
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "trimesh-morph-regression"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-trimesh-spectral-morph-surface-stability.json"
RUNNER = REPO / "scripts/run_cycle_v2_agent.sh"


def run_fixture(name, fixture):
    video = ARTIFACTS / f"{name}.mp4"
    report = ARTIFACTS / f"{name}.json"
    log = ARTIFACTS / f"{name}.log"
    env = os.environ.copy()
    env["CYCLE_OS_VIDEO_PATH"] = str(video)
    env["CYCLE_OS_VIDEO_FPS"] = "30"
    env.pop("CYCLE_DISABLE_SCALAR_SURFACE_SHADER", None)
    subprocess.run(
        [str(RUNNER), str(fixture), str(report), str(log)],
        cwd=REPO,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    result = json.loads(report.read_text())
    if not all(command["ok"] for command in result["results"]):
        raise AssertionError(f"{name} fixture failed: {report}")

    return video, result


def surface_column(video, result):
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
    return column


def held_fixture():
    fixture = json.loads(FIXTURE.read_text())
    commands = fixture["commands"]
    held_index = next(index for index, command in enumerate(commands)
                      if command["command"] == "snapshotState")
    fixture["commands"] = commands[:held_index + 1] + [
        {"command": "inspectOpenGLDiagnostics"},
        {"command": "waitForIdle", "idleDelayMs": 450},
    ]
    fixture["commands"][0]["path"] = str((REPO / "cycle-v2/content/presets/acid-stab-3.cyclegraph").resolve())
    path = ARTIFACTS / "held-fixture.json"
    path.write_text(json.dumps(fixture, indent=2))
    return path


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    held_video, held_result = run_fixture("held", held_fixture())
    released_video, released_result = run_fixture("released", FIXTURE)
    fixture_commands = json.loads(FIXTURE.read_text())["commands"]
    snapshots = [released_result["results"][index]["data"]
                 for index, command in enumerate(fixture_commands)
                 if command["command"] == "snapshotState"]
    if len(snapshots) != 2:
        raise AssertionError("Expected snapshots before and after pointer-up")
    rebuilds = [state["expandedNodeControls"]["trimesh"]["surfaceRebuildCount"]
                for state in snapshots + [released_result["snapshot"]]]
    initial = released_result["results"][1]["data"]["trimesh"]
    if rebuilds[0] <= initial["surfaceRebuildCount"]:
        raise AssertionError("Red drag did not rebuild the secondary-axis surface")
    dragged_rows = snapshots[0]["expandedNodeControls"]["trimesh"]["surfaceRowCount"]
    if dragged_rows == initial["surfaceRowCount"]:
        raise AssertionError("Red drag did not change the displayed pitch rows")
    if len(set(rebuilds)) != 1:
        raise AssertionError(f"Mouse-up rebuilt an unchanged surface: {rebuilds}")

    held_column = surface_column(held_video, held_result)
    released_column = surface_column(released_video, released_result)
    if len(held_column) != len(released_column):
        raise AssertionError("Held and released captures have different panel heights")
    mean_difference = sum(abs(a - b) for a, b in zip(held_column, released_column)) / len(held_column)
    transitions = sum(a != b for a, b in zip(released_column, released_column[1:]))
    if mean_difference > 1 or transitions < 8 or transitions > 3 * dragged_rows:
        raise AssertionError(
            f"Spectral surface changed on release or lost its stepped rows: "
            f"mean difference={mean_difference:.1f}, transitions={transitions}"
        )
    print(f"Spectral rows stable through release: mean difference={mean_difference:.1f}, "
          f"transitions={transitions}; no surface rebuild on mouse-up")
    print(f"Captures: {ARTIFACTS}")


if __name__ == "__main__":
    main()
