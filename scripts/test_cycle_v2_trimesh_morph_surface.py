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


def panel_video_geometry(video, result):
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
    return width, height, editor, panel


def surface_column(video, result):
    width, height, editor, panel = panel_video_geometry(video, result)
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


def first_two_harmonic_heights(video, result):
    width, height, editor, panel = panel_video_geometry(video, result)
    x = round(editor["x"] + panel["x"] + 0.067 * panel["width"])
    y = round(height - result["snapshot"]["height"] + editor["y"] + panel["y"])
    panel_height = round(panel["height"])
    if x < 0 or x + 4 > width or y < 0 or y + panel_height > height:
        raise AssertionError("Lowest harmonic sample falls outside the recorded window")

    frame = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-sseof", "-0.25", "-i", str(video),
         "-frames:v", "1", "-vf", f"crop=4:{panel_height}:{x}:{y},format=rgb24",
         "-f", "rawvideo", "-"],
        check=True, capture_output=True,
    ).stdout
    if len(frame) != 4 * panel_height * 3:
        raise AssertionError(f"Could not decode the lowest harmonic in {video}")

    orange_rows = 0
    first_orange_row = None
    row_colours = []
    for row in range(panel_height):
        pixels = frame[row * 12:(row + 1) * 12]
        row_colours.append(tuple(sum(pixels[channel::3]) / 4
                                 for channel in range(3)))

    for row in range(round(0.65 * panel_height), panel_height):
        red, green, blue = row_colours[row]
        if red > green + 10 and red > blue + 15:
            orange_rows += 1
            if first_orange_row is None:
                first_orange_row = row

    if first_orange_row is None:
        raise AssertionError("Could not find the first harmonic in the panel capture")

    # This fixture's second band is dark blue. Count it just above the orange
    # first band so the earlier, lighter blue bands do not enter the comparison.
    second_rows = 0
    for row in range(max(0, first_orange_row - round(0.25 * panel_height)),
                     first_orange_row):
        red, green, blue = row_colours[row]
        if red < 55 and green < 86 and blue < 120 and blue > red + 35:
            second_rows += 1
    return orange_rows, second_rows, panel_height


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
    first_rows, second_rows, panel_height = first_two_harmonic_heights(
        released_video, released_result)
    if first_rows < 0.08 * panel_height or second_rows < 0.08 * panel_height:
        raise AssertionError(
            f"Could not resolve both low harmonic bands: {first_rows}, {second_rows}"
        )
    if first_rows < 1.2 * second_rows:
        raise AssertionError(
            f"First log-spaced harmonic band should exceed the second: "
            f"{first_rows} vs {second_rows} pixels"
        )
    print(f"Spectral rows stable through release: mean difference={mean_difference:.1f}, "
          f"transitions={transitions}, first/second harmonic={first_rows}/{second_rows} "
          f"of {panel_height} pixels; "
          "no surface rebuild on mouse-up")
    print(f"Captures: {ARTIFACTS}")


if __name__ == "__main__":
    main()
