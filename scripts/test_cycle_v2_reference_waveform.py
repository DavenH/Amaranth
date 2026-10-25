#!/usr/bin/env python3
"""Verify the lower reference waveform modes and linked navigation."""

import argparse
import json
from pathlib import Path
import struct
import subprocess
import time
import uuid
import wave
import math

from cycle_automation_support.identity import workspace
from cycle_automation_support.lifecycle import compiled_helper, ensure
from cycle_automation_support.operations import preflight, write_logs
from cycle_automation_support.profiles import PRODUCTS, ROOT
from cycle_automation_support.registry import lease
from cycle_automation_support.session import Session


def check(record, output):
    output.mkdir(parents=True, exist_ok=True)
    sample = output / "waveform-toggle.wav"
    with wave.open(str(sample), "wb") as writer:
        writer.setparams((1, 2, 44100, 0, "NONE", "not compressed"))
        writer.writeframes(b"".join(struct.pack("<h", round(26000 * (0.35 + 0.65 * index / 220500)
                * math.sin(2 * math.pi * 400 * index / 44100))) for index in range(220500)))
    evidence = {"ok": False, "states": {}}
    owned = []
    with Session(record["socket"], timeout=60) as session:
        def call(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            return response["result"].get("data", {})

        def state(label, ready=False):
            for _ in range(200):
                call({"command": "waitForIdle", "idleDelayMs": 50})
                value = call({"command": "snapshotState"})
                editor = value["comparison"]["pitchEditor"]
                if not ready or editor["waveform"]["ready"]:
                    break
                time.sleep(0.025)
            else:
                raise AssertionError("Waveform did not publish the current viewport")
            curve, surface = editor["curveView"]["zoom"], editor["surfaceView"]["camera"]
            for axis in ("x", "w"):
                assert abs(curve[axis] - surface[axis]) < 1e-5
                assert abs(curve[axis] - editor["waveform"][axis]) < 1e-5
            if ready:
                waveform = editor["waveform"]
                assert (waveform["mode"] == "line") == (waveform["crossings"] <= waveform["pixelWidth"])
            evidence["states"][label] = editor
            return value, editor

        def click(name):
            call({"command": "pointer", "event": "click", "targetId": "referencePitch." + name})

        def capture(name):
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / (name + ".png"))], check=True)

        try:
            call({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            baseline = call({"command": "snapshotState"})
            call({"command": "resizeWindow", "width": 1280, "height": 900})
            imported = call({"command": "importReference", "path": str(sample)})
            audio = Path(imported["audioPath"])
            owned = [audio, audio.with_name(imported["referenceId"] + ".ref.json")]
            call({"command": "openReferencePitchEditor", "referenceId": imported["referenceId"]})
            _, pitch = state("pitch")
            assert pitch["lowerMode"] == "pitch"
            pitch_y = {axis: pitch["curveView"]["zoom"][axis] for axis in ("y", "h")}
            click("waveform")
            _, density = state("density", ready=True)
            assert density["lowerMode"] == "waveform" and density["waveform"]["mode"] == "density"
            assert density["waveform"]["y"] == 0 and density["waveform"]["h"] == 1
            capture("density")
            click("zoomAttack")
            _, line = state("line", ready=True)
            assert line["waveform"]["mode"] == "line"
            capture("line")
            previous_x = line["waveform"]["x"]
            for update in range(2):
                call({"command": "pointer", "event": "wheel", "targetId": "referencePitch.curve",
                      "deltaX": -0.15, "deltaY": 0, "smooth": True})
                _, moved = state("pan-" + str(update), ready=True)
                assert moved["waveform"]["x"] > previous_x
                previous_x = moved["waveform"]["x"]
            for event, offset in (("down", 0), ("drag", 0.03), ("drag", 0.06), ("up", 0.06)):
                start = previous_x + moved["waveform"]["w"] / 2
                call({"command": "pointer", "event": event, "targetId": "referencePitch.zoomX",
                      "downTargetId": "referencePitch.zoomX", "targetX": start + offset,
                      "downTargetX": start})
            _, dragged = state("thumb-pan", ready=True)
            assert dragged["waveform"]["x"] > previous_x
            click("pitch")
            _, restored = state("restored-pitch")
            for axis in ("y", "h"):
                assert abs(restored["curveView"]["zoom"][axis] - pitch_y[axis]) < 1e-5
            click("waveform")
            _, reused = state("reused-waveform", ready=True)
            assert reused["waveform"]["imageBuilds"] == dragged["waveform"]["imageBuilds"]
            for _ in range(2):
                call({"command": "pointer", "event": "wheel", "targetId": "referencePitch.zoomY",
                      "deltaX": 0, "deltaY": -0.1, "smooth": True})
            state("amplitude-pan", ready=True)
            click("zoomFit")
            _, fit = state("amplitude-fit", ready=True)
            assert fit["waveform"]["y"] == 0 and fit["waveform"]["h"] == 1
            click("spectrum")
            state("spectrogram", ready=True)
            click("zoomFull")
            _, full = state("density-again", ready=True)
            assert full["waveform"]["mode"] == "density"
            assert full["waveform"]["sourceBuilds"] == density["waveform"]["sourceBuilds"] == 1
            assert full["surfaceGeneration"] == pitch["surfaceGeneration"]
            assert full["cyclogramRevision"] == pitch["cyclogramRevision"]
            capture("spectrogram-density")
            click("zoomAttack")
            for update in range(24):
                _, detail = state("detail-" + str(update), ready=True)
                if detail["waveform"]["crossings"] <= 20:
                    break
                call({"command": "pointer", "event": "wheel", "targetId": "referencePitch.curve",
                      "deltaX": 0, "deltaY": 0.5, "smooth": False, "targetX": 0.5, "targetY": 0.5})
            assert detail["waveform"]["mode"] == "line" and detail["waveform"]["crossings"] <= 20
            capture("line-detail")
            for event, x in (("down", 0.3), ("drag", 0.4), ("drag", 0.5), ("up", 0.5)):
                call({"command": "pointer", "event": event, "targetId": "referencePitch.curve",
                      "downTargetId": "referencePitch.curve", "targetX": x, "targetY": 0.6,
                      "downTargetX": 0.3, "downTargetY": 0.6})
            assert state("waveform-drag", ready=True)[1]["cyclogramRevision"] == pitch["cyclogramRevision"]
            click("zoomFull")
            call({"command": "resizeWindow", "width": 1000, "height": 760})
            state("narrow", ready=True)
            capture("narrow")
            call({"command": "openReferencePitchEditor", "referenceId": "factory:sax-growl"})
            _, retargeted = state("retargeted", ready=True)
            assert retargeted["lowerMode"] == "waveform" and retargeted["waveform"]["sourceBuilds"] == 2
            call({"command": "resizeWindow", "width": 1280, "height": 900})
            click("spectrum")
            state("sax-density", ready=True)
            capture("sax-density")
            call({"command": "openReferencePitchEditor", "referenceId": imported["referenceId"]})
            _, original = state("retargeted-back", ready=True)
            assert original["waveform"]["sourceBuilds"] == 3
            call({"command": "key", "key": "space"})
            assert state("audition", ready=True)[0]["comparison"]["auditioning"]
            call({"command": "key", "key": "space"})
            final, _ = state("final", ready=True)
            for key in ("documentRevision", "graphRevision", "documentDirty"):
                assert final[key] == baseline[key]
            evidence["ok"] = True
        finally:
            if not evidence["ok"]:
                capture("failure")
            for path in owned:
                path.unlink(missing_ok=True)
            (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
            write_logs(record, output, record["launched"])
    print(json.dumps({"ok": True, "output": str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/reference-waveform-ui")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": uuid.uuid4().hex[:12], "app_pid": None}

    def run(record):
        owner["app_pid"] = record["pid"]
        with lease("foreground", owner, "reference-waveform", wait=5):
            preflight(accessibility=False, screen=True)
            check(record, args.output_dir.resolve())

    ensure(PRODUCTS["cycle-v2"], owner, short_id, task=run, scoped=True, build_app=not args.no_build)


if __name__ == "__main__":
    main()
