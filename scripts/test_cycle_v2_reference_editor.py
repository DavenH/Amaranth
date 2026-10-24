#!/usr/bin/env python3
"""Check stable reference cards and native cyclogram/pitch-envelope expansion."""

import argparse
import json
from pathlib import Path
import subprocess
import time
import uuid

from cycle_automation_support.identity import workspace
from cycle_automation_support.lifecycle import compiled_helper, ensure
from cycle_automation_support.operations import preflight, write_logs
from cycle_automation_support.profiles import PRODUCTS, ROOT
from cycle_automation_support.registry import lease
from cycle_automation_support.session import Session

CLICK = "/opt/homebrew/bin/cliclick"


def check(record, output):
    output.mkdir(parents=True, exist_ok=True)
    (output / "commands.jsonl").write_text("")
    evidence = {"ok": False, "pid": record["pid"]}
    owned_files = []
    with Session(record["socket"], timeout=60) as session:
        def invoke(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            return response["result"].get("data", {})

        def state():
            return invoke({"command": "snapshotState"})

        def targets():
            return {item["id"]: item for item in invoke({"command": "inspectPointerTargets"})["targets"]}

        def point(identifier, x=0.5, y=0.5):
            items = targets()
            origin = items["comparisonWorkspace"]["screenBounds"]
            bounds = items[identifier]["bounds"]
            return (round(origin["x"] + bounds["x"] + x * bounds["width"]),
                    round(origin["y"] + bounds["y"] + y * bounds["height"]))

        def native(*commands):
            subprocess.run([CLICK, "-w", "25", *commands], check=True)
            time.sleep(0.15)

        def click(identifier, double=False):
            subprocess.run([str(compiled_helper("launch_cycle_app.swift")),
                            "--focus", str(record["pid"])], check=True)
            time.sleep(0.2)
            x, y = point(identifier)
            if double:
                native(f"m:{x + 2},{y}", f"m:{x},{y}",
                       f"c:{x},{y}", "w:40", f"c:{x},{y}")
            else:
                native(f"m:{x + 2},{y}", f"m:{x},{y}", f"dd:{x},{y}")
                native(f"du:{x},{y}")
            invoke({"command": "waitForIdle", "idleDelayMs": 300})
            invoke({"command": "inspectPointerTargets"})

        def placement():
            return {key: value["bounds"] for key, value in targets().items()
                    if key.startswith("comparison.surface.")}

        def layout():
            editor = state()["comparison"]["pitchEditor"]
            assert editor["visible"], editor
            upper, lower = editor["cyclogramBounds"], editor["curveBounds"]
            assert upper["x"] == lower["x"]
            assert upper["width"] == lower["width"]
            assert lower["y"] > upper["y"] + upper["height"]
            assert upper["height"] >= 90 and lower["height"] >= 90, editor
            assert editor["surfaceView"]["renderCount"] > 0
            return editor

        try:
            invoke({"command": "resizeWindow", "width": 1280, "height": 900})
            invoke({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            baseline = state()
            invoke({"command": "selectReference", "referenceId": "factory:dist3"})
            imported = invoke({"command": "importReference", "path": str(ROOT / "cycle-v2/content/references/sine500.wav")})
            reference = imported["referenceId"]
            audio_path = Path(imported["audioPath"])
            assert audio_path.stem == reference and not reference.startswith("factory:")
            owned_files = [audio_path, audio_path.with_name(reference + ".ref.json")]
            invoke({"command": "waitForIdle", "idleDelayMs": 600})
            before = placement()
            for ref in ("factory:dist3", reference, "factory:dist3", reference):
                click("comparison.surface." + ref)
                assert state()["comparison"]["selectedReferenceId"] == ref
                assert placement() == before
            click("comparison.surface." + reference, double=True)
            editor = layout()
            assert editor["visible"] and editor["referenceId"] == reference
            assert state()["comparison"]["cardCount"] == 2
            assert placement() == before
            zoom = editor["zoomControls"]["vertical"]
            assert zoom["visible"] and editor["zoomControls"]["horizontal"]["visible"]
            y = zoom["start"] + zoom["size"] / 2
            for event, position in (("down", y), ("drag", y + 0.05),
                                    ("drag", y + 0.1), ("up", y + 0.1)):
                invoke({"command": "pointer", "event": event,
                        "targetId": "referencePitch.zoomY", "downTargetId": "referencePitch.zoomY",
                        "targetY": position, "downTargetY": y})
            invoke({"command": "waitForIdle", "idleDelayMs": 300})
            panned = layout()
            assert panned["zoomControls"]["vertical"]["start"] > zoom["start"] + 0.01
            assert abs(panned["zoomControls"]["vertical"]["size"] - zoom["size"]) < 0.0001
            assert panned["cyclogramRevision"] == editor["cyclogramRevision"]
            evidence["verticalPan"] = {"before": zoom, "after": panned["zoomControls"]["vertical"]}
            # Reopening restores the fitted view before the pitch edit gesture.
            invoke({"command": "openReferencePitchEditor", "referenceId": reference})
            editor = layout()
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / "expanded-before.png")], check=True)
            original_analysis = invoke({"command": "exportReferenceAnalysis"})
            original = original_analysis["pitchPath"]
            evidence["openedEditor"] = editor
            evidence["originalPitchPath"] = original
            revision = editor["cyclogramRevision"]
            view = editor["curveView"]["zoom"]
            endpoint = original[-1]
            position_x = (endpoint["time"] - view["x"]) / view["w"]
            position_y = (1 - endpoint["pitch"] - view["y"]) / view["h"]
            x, y = point("referencePitch.curve", position_x, position_y)
            _, end_y = point("referencePitch.curve", position_x, position_y - 0.18)
            native(f"m:{x + 2},{y}", f"m:{x},{y}", f"dd:{x},{y}")
            native(f"dm:{x},{round((y + end_y) / 2)}", f"dm:{x},{end_y}")
            assert state()["comparison"]["pitchEditor"]["cyclogramRevision"] == revision
            native(f"du:{x},{end_y}")
            invoke({"command": "inspectPointerTargets"})
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / "expanded-after-drag.png")], check=True)
            invoke({"command": "waitForIdle", "idleDelayMs": 400})
            changed = state()["comparison"]["pitchEditor"]["cyclogramRevision"]
            assert changed == revision + 1, (revision, changed)
            changed_analysis = invoke({"command": "exportReferenceAnalysis"})
            assert changed_analysis["pitchPath"] != original
            assert changed_analysis["pitchFrames"] != original_analysis["pitchFrames"]
            subprocess.run([str(compiled_helper("launch_cycle_app.swift")),
                            "--focus", str(record["pid"])], check=True)
            time.sleep(0.2)
            subprocess.run(["osascript", "-e",
                            'tell application "System Events" to keystroke "z" using command down'], check=True)
            time.sleep(0.08)
            invoke({"command": "waitForIdle", "idleDelayMs": 400})
            restored = invoke({"command": "exportReferenceAnalysis"})
            assert restored["pitchPath"] == original
            assert restored["pitchFrames"] == original_analysis["pitchFrames"]
            assert placement() == before
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / "expanded.png")], check=True)
            invoke({"command": "resizeWindow", "width": 1000, "height": 760})
            invoke({"command": "waitForIdle", "idleDelayMs": 400})
            layout()
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / "expanded-small.png")], check=True)
            click("referencePitch.close")
            assert not state()["comparison"]["pitchEditor"]["visible"]
            assert state()["comparison"]["visible"]
            invoke({"command": "openReferencePitchEditor", "referenceId": reference})
            subprocess.run(["osascript", "-e",
                            'tell application "System Events" to key code 53'], check=True)
            time.sleep(0.08)
            invoke({"command": "waitForIdle", "idleDelayMs": 300})
            assert not state()["comparison"]["pitchEditor"]["visible"]
            final = state()
            for key in ("documentRevision", "graphRevision", "documentDirty"):
                assert final[key] == baseline[key], (key, final[key], baseline[key])
            evidence.update(ok=True, state=final, stablePlacement=before)
        finally:
            try:
                if not evidence["ok"]:
                    subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                                    str(record["pid"]), str(output / "failure.png")], check=False)
                (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
                write_logs(record, output, record["launched"])
            finally:
                # These files belong only to the UUID import created by this isolated run.
                for path in owned_files:
                    path.unlink(missing_ok=True)
    print(json.dumps({"ok": True, "output": str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/reference-editor-native")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": uuid.uuid4().hex[:12], "app_pid": None}

    def run(record):
        owner["app_pid"] = record["pid"]
        with lease("foreground", owner, "reference-editor"):
            preflight(accessibility=True, screen=True)
            check(record, args.output_dir.resolve())

    ensure(PRODUCTS["cycle-v2"], owner, short_id, task=run,
           scoped=True, build_app=not args.no_build)


if __name__ == "__main__":
    main()
