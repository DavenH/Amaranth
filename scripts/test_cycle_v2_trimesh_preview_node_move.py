#!/usr/bin/env python3
"""Check that moving acid-stab-3's magnitude node reuses its stepped preview."""

import json
import os
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "trimesh-preview-node-move"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-trimesh-preview-node-move.json"
RUNNER = REPO / "scripts/run_cycle_v2_agent.sh"


def run(name, commands, video):
    fixture = ARTIFACTS / f"{name}-fixture.json"
    fixture.write_text(json.dumps({"commands": commands, "quit": True}, indent=2))
    report = ARTIFACTS / f"{name}.json"
    log = ARTIFACTS / f"{name}.log"
    environment = os.environ.copy()
    if video:
        environment["CYCLE_OS_VIDEO_PATH"] = str(ARTIFACTS / f"{name}.mp4")
        environment["CYCLE_OS_VIDEO_FPS"] = "30"
    else:
        environment.pop("CYCLE_OS_VIDEO_PATH", None)
    subprocess.run(
        [str(RUNNER), str(fixture), str(report), str(log)],
        cwd=REPO, env=environment, check=True, capture_output=True, text=True,
    )
    result = json.loads(report.read_text())
    if not all(command["ok"] for command in result["results"]):
        raise AssertionError(f"{name} fixture failed: {report}")
    return result


def node_x(snapshot):
    return next(node["bounds"]["x"] for node in snapshot["nodes"]
                if node["id"] == "magnitudeLayer1")


def raw_preview(name, result, column=False):
    video = ARTIFACTS / f"{name}.mp4"
    dimensions = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "csv=p=0", str(video)],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    video_width, video_height = (int(value) for value in dimensions.split(","))
    snapshot = result["snapshot"]
    node = next(node for node in snapshot["nodes"]
                if node["id"] == "magnitudeLayer1")
    bounds = node["bounds"]
    zoom = snapshot["zoom"]
    x_ratio = 0.16 if column else 0.10
    y_ratio = 0.27 if column else 0.30
    width = 1 if column else round(bounds["width"] * zoom * 0.55)
    height = round(bounds["height"] * zoom * (0.54 if column else 0.50))
    x = round(snapshot["panX"] + bounds["x"] * zoom
              + bounds["width"] * zoom * x_ratio)
    y = round(video_height - snapshot["height"] + snapshot["panY"]
              + bounds["y"] * zoom + bounds["height"] * zoom * y_ratio)
    if x < 0 or y < 0 or x + width > video_width or y + height > video_height:
        raise AssertionError("Magnitude preview sample is outside the captured window")
    raw = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-sseof", "-0.25", "-i", str(video),
         "-frames:v", "1", "-vf",
         f"crop={width}:{height}:{x}:{y},format=rgb24", "-f", "rawvideo", "-"],
        check=True, capture_output=True,
    ).stdout
    if len(raw) != width * height * 3:
        raise AssertionError(f"Could not decode the magnitude preview in {video}")
    return raw


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    commands = json.loads(FIXTURE.read_text())["commands"]
    commands[0]["path"] = str((REPO / "cycle-v2/content/presets/acid-stab-3.cyclegraph").resolve())
    held = run("held", commands[:12], True)
    released = run("released", commands[:15], True)
    complete = run("complete", commands, False)

    initial_x = node_x(complete["results"][2]["data"])
    held_x = node_x(complete["results"][11]["data"])
    released_x = node_x(complete["results"][14]["data"])
    undone_x = node_x(complete["results"][16]["data"])
    if not (held_x > initial_x + 10 and held_x == released_x and undone_x == initial_x):
        raise AssertionError("Two node movements, release, and undo did not preserve bounds")

    for label, index in (("drag", 10), ("release", 13)):
        cache = complete["results"][index]["data"]["presentationCache"]["nodeLayer"]
        if cache["hits"] == 0 or cache["misses"] != 0:
            raise AssertionError(f"Node tile was repainted on {label}: {cache}")

    held_image = raw_preview("held", held)
    released_image = raw_preview("released", released)
    mean_difference = sum(abs(a - b) for a, b in zip(held_image, released_image))
    mean_difference /= len(held_image)
    if mean_difference > 0.4:
        raise AssertionError(
            f"Spectral preview changed between drag and release: {mean_difference:.2f}"
        )
    for label, result in (("held", held), ("released", released)):
        pixels = raw_preview(label, result, column=True)
        rows = [pixels[index:index + 3] for index in range(0, len(pixels), 3)]
        largest_step = max(max(abs(a - b) for a, b in zip(left, right))
                           for left, right in zip(rows, rows[1:]))
        if largest_step < 30:
            raise AssertionError(f"{label} spectral rows became fuzzy: max step={largest_step}")

    print(f"Preview tile reused through two moves and release; "
          f"held/released mean difference={mean_difference:.3f}; undo restored position")
    print(f"Captures: {ARTIFACTS}")


if __name__ == "__main__":
    main()
