#!/usr/bin/env python3
"""Compare compact and expanded acid-stab-3 magnitude bands at one state."""

import json
import os
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "trimesh-preview-parity"
RUNNER = REPO / "scripts/run_cycle_v2_agent.sh"
PRESET = REPO / "cycle-v2/content/presets/acid-stab-3.cyclegraph"


def capture(name):
    commands = [
        {"command": "openGraph", "path": str(PRESET)},
        {"command": "waitForIdle", "idleDelayMs": 300},
    ]
    if name == "expanded":
        commands += [
            {"command": "openMeshPopup", "nodeId": "magnitudeLayer1"},
            {"command": "waitForIdle", "idleDelayMs": 450},
        ]
    commands += [
        {"command": "snapshotState"},
        {"command": "inspectOpenGLDiagnostics"},
        {"command": "waitForIdle", "idleDelayMs": 300},
    ]
    fixture = ARTIFACTS / f"{name}-fixture.json"
    report = ARTIFACTS / f"{name}.json"
    video = ARTIFACTS / f"{name}.mp4"
    fixture.write_text(json.dumps({"commands": commands, "quit": True}, indent=2))
    environment = os.environ.copy()
    environment["CYCLE_OS_VIDEO_PATH"] = str(video)
    environment["CYCLE_OS_VIDEO_FPS"] = "30"
    subprocess.run(
        [str(RUNNER), str(fixture), str(report), str(ARTIFACTS / f"{name}.log")],
        cwd=REPO, env=environment, check=True, capture_output=True, text=True,
    )
    result = json.loads(report.read_text())
    if not all(command["ok"] for command in result["results"]):
        raise AssertionError(f"{name} fixture failed: {report}")
    return video, result


def video_height(video):
    dimensions = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "csv=p=0", str(video)],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    return tuple(int(value) for value in dimensions.split(","))


def rgb_column(video, x, y, height):
    width, video_height_pixels = video_height(video)
    if x < 0 or x >= width or y < 0 or y + height > video_height_pixels:
        raise AssertionError("Spectral sample falls outside the recorded window")
    raw = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-sseof", "-0.25", "-i", str(video),
         "-frames:v", "1", "-vf", f"crop=1:{height}:{x}:{y},format=rgb24",
         "-f", "rawvideo", "-"],
        check=True, capture_output=True,
    ).stdout
    if len(raw) != 3 * height:
        raise AssertionError(f"Could not decode the spectral column in {video}")
    return [tuple(raw[index:index + 3]) for index in range(0, len(raw), 3)]


def compact_column(video, result):
    snapshot = result["snapshot"]
    node = next(node for node in snapshot["nodes"]
                if node["id"] == "magnitudeLayer1")
    bounds = node["bounds"]
    zoom = snapshot["zoom"]
    _, height = video_height(video)
    x = round(snapshot["panX"] + (bounds["x"] + 0.11 * bounds["width"]) * zoom)
    y = round(height - snapshot["height"] + snapshot["panY"]
              + (bounds["y"] + 0.15 * bounds["height"]) * zoom)
    return rgb_column(video, x, y, round(0.80 * bounds["height"] * zoom))


def expanded_column(video, result):
    snapshot = result["snapshot"]
    diagnostics = next(command["data"] for command in result["results"]
                       if command.get("data", {}).get("panels"))
    editor = diagnostics["expandedEditorBounds"]
    panel = next(panel["bounds"] for panel in diagnostics["panels"]
                 if panel["id"] == "trimeshPanel3D")
    _, height = video_height(video)
    x = round(editor["x"] + panel["x"] + 0.067 * panel["width"])
    y = round(height - snapshot["height"] + editor["y"] + panel["y"])
    return rgb_column(video, x, y, round(panel["height"]))


def spectral_bands(column):
    def blue(pixel):
        red, green, blue_level = pixel
        return blue_level > red + 50 and blue_level > green + 20

    def orange(pixel):
        red, green, blue_level = pixel
        return red > green + 20 and red > blue_level + 50

    first_blue = next(index for index, pixel in enumerate(column) if blue(pixel))
    last_orange = max(index for index, pixel in enumerate(column) if orange(pixel))
    bands = column[first_blue:last_orange + 1]
    first_orange = next(index for index, pixel in enumerate(bands) if orange(pixel))
    return bands, (len(bands) - first_orange) / len(bands)


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    compact_video, compact_result = capture("compact")
    expanded_video, expanded_result = capture("expanded")
    def node_parameters(result):
        node = next(node for node in result["snapshot"]["nodes"]
                    if node["id"] == "magnitudeLayer1")
        return {item["id"]: item["value"] for item in node["parameters"]}

    compact_parameters = node_parameters(compact_result)
    expanded_parameters = node_parameters(expanded_result)
    for parameter in ("red", "yellow", "blue", "primaryAxis"):
        if compact_parameters[parameter] != expanded_parameters[parameter]:
            raise AssertionError(f"Preset state changed between captures: {parameter}")

    compact, compact_first = spectral_bands(compact_column(compact_video, compact_result))
    expanded, expanded_first = spectral_bands(expanded_column(expanded_video, expanded_result))
    difference = 0
    for index in range(3, len(compact) - 3):
        expanded_index = min(len(expanded) - 1,
                             round((index + 0.5) * len(expanded) / len(compact) - 0.5))
        difference += sum(abs(a - b) for a, b in zip(compact[index], expanded[expanded_index]))
    colour_difference = difference / (3 * (len(compact) - 6))
    if abs(compact_first - expanded_first) > 0.04 or colour_difference > 8:
        raise AssertionError(
            f"Compact and expanded bands differ: first band fractions "
            f"{compact_first:.3f}/{expanded_first:.3f}, "
            f"mean RGB difference={colour_difference:.2f}"
        )
    print(f"Compact/expanded spectral bands match at one preset state: "
          f"first band fractions {compact_first:.3f}/{expanded_first:.3f}, "
          f"mean RGB difference={colour_difference:.2f}")
    print(f"Captures: {ARTIFACTS}")


if __name__ == "__main__":
    main()
