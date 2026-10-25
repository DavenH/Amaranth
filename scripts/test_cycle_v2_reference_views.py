#!/usr/bin/env python3
"""Verify reference view modes, linked time navigation, and inlaid framing controls."""

import argparse
import json
from pathlib import Path
import subprocess
import uuid

from cycle_automation_support.identity import workspace
from cycle_automation_support.lifecycle import compiled_helper, ensure
from cycle_automation_support.operations import preflight, write_logs
from cycle_automation_support.profiles import PRODUCTS, ROOT
from cycle_automation_support.registry import lease
from cycle_automation_support.session import Session
from assert_png_stats import region_stats
from compare_png_psnr import read_png_rgb


def check(record, output):
    output.mkdir(parents=True, exist_ok=True)
    evidence = {"ok": False, "states": {}}
    with Session(record["socket"], timeout=60) as session:
        def invoke(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            return response["result"].get("data", {})

        def state():
            invoke({"command": "waitForIdle", "idleDelayMs": 200})
            return invoke({"command": "snapshotState"})

        def editor(label):
            value = state()["comparison"]["pitchEditor"]
            pitch, surface = value["curveView"]["zoom"], value["surfaceView"]["camera"]
            for axis in ("x", "w"):
                assert abs(pitch[axis] - surface[axis]) < 1e-5, (label, pitch, surface)
            assert value["curveView"]["releaseEnabled"] is False
            assert value["curveView"]["interactionXMaximum"] == 1
            assert value["curveView"]["zoom"]["xMaximum"] == 1
            assert 160 < value["surfaceView"]["gridColumns"] <= 1024
            assert 4 <= value["surfaceView"]["gridRows"] <= 1024
            assert value["surfaceView"]["uniformColumnSpacing"] is False
            upper, lower = value["cyclogramBounds"], value["curveBounds"]
            assert upper["x"] == lower["x"] and upper["width"] == lower["width"]
            assert lower["y"] - upper["y"] - upper["height"] == 14
            evidence["states"][label] = value
            return value

        def click(action):
            invoke({"command": "pointer", "event": "click", "targetId": "referencePitch." + action})

        def capture(name):
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / (name + ".png"))], check=True)

        try:
            invoke({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            baseline = state()
            invoke({"command": "resizeWindow", "width": 1280, "height": 900})
            invoke({"command": "pointer", "event": "click", "targetId": "comparison.toggle"})
            invoke({"command": "openReferencePitchEditor", "referenceId": "factory:sax-growl"})
            initial = editor("opened")
            assert initial["curveView"]["zoom"]["x"] == 0 and initial["curveView"]["zoom"]["w"] == 1
            assert initial["zoomControls"]["horizontal"]["visible"]
            assert initial["surfaceMode"] == "time" and initial["spectrumGeneration"] == 0
            capture("cyclogram")
            click("zoomAttack")
            attack = editor("attack")
            assert abs(attack["curveView"]["zoom"]["w"] - 0.2) < 1e-5
            for target in ("cyclogram", "curve"):
                before_zoom = editor("before-wheel-" + target)
                invoke({"command": "pointer", "event": "wheel", "targetId": "referencePitch." + target,
                        "deltaX": 0, "deltaY": 0.1, "smooth": False, "targetX": 0.6, "targetY": 0.5})
                zoomed = editor("wheel-" + target)
                assert zoomed["curveView"]["zoom"]["w"] < before_zoom["curveView"]["zoom"]["w"]
            previous_x = zoomed["curveView"]["zoom"]["x"]
            for update in range(2):
                invoke({"command": "pointer", "event": "wheel", "targetId": "referencePitch.cyclogram",
                        "deltaX": -0.2, "deltaY": 0, "smooth": True})
                panned = editor("surface-pan-" + str(update))
                assert panned["curveView"]["zoom"]["x"] > previous_x
                previous_x = panned["curveView"]["zoom"]["x"]
            start = panned["curveView"]["zoom"]["x"] + panned["curveView"]["zoom"]["w"] / 2
            for event, offset in (("down", 0), ("drag", 0.05), ("drag", 0.1), ("up", 0.1)):
                invoke({"command": "pointer", "event": event, "targetId": "referencePitch.zoomX",
                        "downTargetId": "referencePitch.zoomX", "targetX": start + offset,
                        "downTargetX": start})
                if event == "drag":
                    moved = editor("pitch-pan-" + str(offset))
                    assert moved["curveView"]["zoom"]["x"] > previous_x
                    previous_x = moved["curveView"]["zoom"]["x"]
            before = editor("before-vertical")
            for update in range(2):
                invoke({"command": "pointer", "event": "wheel", "targetId": "referencePitch.zoomY",
                        "deltaX": 0, "deltaY": -0.1, "smooth": True})
            vertical = editor("vertical")
            assert vertical["curveView"]["zoom"]["y"] > before["curveView"]["zoom"]["y"]
            assert vertical["surfaceView"]["camera"] == before["surfaceView"]["camera"]
            click("zoomFit")
            fitted = editor("fitted")
            assert abs(fitted["curveView"]["zoom"]["y"] - initial["curveView"]["zoom"]["y"]) < 1e-5
            assert fitted["curveView"]["zoom"]["x"] == vertical["curveView"]["zoom"]["x"]
            click("spectrum")
            spectral = editor("spectrum")
            assert spectral["surfaceMode"] == "spectrum" and spectral["spectrumGeneration"] == 1
            card = next(card for card in state()["comparison"]["cards"] if card["id"] == "factory:sax-growl")
            assert card["spectral"]
            assert card["surfaceView"]["gridColumns"] == spectral["surfaceView"]["gridColumns"]
            assert card["surfaceView"]["lastColumnTime"] == spectral["surfaceView"]["lastColumnTime"]
            assert spectral["curveView"]["zoom"] == fitted["curveView"]["zoom"]
            click("zoomFull")
            full = editor("full-spectrum")
            assert full["surfaceView"]["camera"] == {"x": 0, "y": 0, "w": 1, "h": 1}
            assert full["curveView"]["zoom"]["y"] == 0 and full["curveView"]["zoom"]["h"] == 1
            click("zoomFit")
            editor("spectrum-fitted")
            capture("spectrogram")
            items = {item["id"]: item for item in invoke({"command": "inspectPointerTargets"})["targets"]}
            origin = items["comparisonWorkspace"]["screenBounds"]
            plot = items["referencePitch.cyclogram"]["bounds"]
            region = (origin["x"] + plot["x"] + 12, origin["y"] + plot["y"] + 40,
                      plot["width"] - 24, plot["height"] - 52)
            plot_image = output / "spectrogram-plot.png"
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(plot_image), ",".join(str(v) for v in region)], check=True)
            width, height, rgb = read_png_rgb(plot_image)
            mean, contrast = region_stats(rgb, width, (0, 0, width, height))
            evidence["spectralContrast"] = {"mean": mean, "stddev": contrast}
            # Drawing raw FFT amplitudes bypasses the log mapping and leaves this plot almost black.
            assert contrast > 0.02, evidence["spectralContrast"]
            for mode in ("time", "spectrum", "time"):
                click(mode)
                toggled = editor("toggle-" + mode)
                assert toggled["surfaceGeneration"] == initial["surfaceGeneration"]
                assert toggled["spectrumGeneration"] == 1
            # The card's pill changes the same per-reference mode, including while expanded.
            invoke({"command": "pointer", "event": "click", "targetId": "comparison.mode.factory:sax-growl"})
            assert editor("tray-pill-spectrum")["surfaceMode"] == "spectrum"
            click("close")
            assert not state()["comparison"]["pitchEditor"]["visible"]
            capture("tray-spectrogram")
            invoke({"command": "openReferencePitchEditor", "referenceId": "factory:sax-growl"})
            assert editor("reopened-spectrum")["surfaceMode"] == "spectrum"
            invoke({"command": "pointer", "event": "click", "targetId": "comparison.mode.factory:sax-growl"})
            assert editor("tray-pill-time")["surfaceMode"] == "time"
            invoke({"command": "resizeWindow", "width": 1000, "height": 760})
            click("zoomFit")
            editor("smaller-window")
            capture("cyclogram-small")
            invoke({"command": "key", "key": "space"})
            assert state()["comparison"]["auditioning"]
            invoke({"command": "key", "key": "space"})
            final = state()
            assert not final["comparison"]["auditioning"]
            for key in ("documentRevision", "graphRevision", "documentDirty"):
                assert final[key] == baseline[key]
            assert final["comparison"]["pitchEditor"]["cyclogramRevision"] == initial["cyclogramRevision"]
            evidence["ok"] = True
        finally:
            if not evidence["ok"]:
                capture("failure")
            (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
            write_logs(record, output, record["launched"])
    print(json.dumps({"ok": True, "output": str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/reference-views")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": uuid.uuid4().hex[:12], "app_pid": None}

    def run(record):
        owner["app_pid"] = record["pid"]
        with lease("foreground", owner, "reference-views", wait=5):
            preflight(accessibility=False, screen=True)
            check(record, args.output_dir.resolve())

    ensure(PRODUCTS["cycle-v2"], owner, short_id, task=run,
           scoped=True, build_app=not args.no_build)


if __name__ == "__main__":
    main()
