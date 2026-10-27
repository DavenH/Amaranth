#!/usr/bin/env python3
"""Verify native search, reference drops and audible card playback in Cycle V2."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from cycle_automation_support.lifecycle import compiled_helper
from cycle_automation_support.session import Session

ROOT = Path(__file__).resolve().parent.parent
CLICK = "/opt/homebrew/bin/cliclick"


def check(output):
    output.mkdir(parents=True, exist_ok=True)
    evidence = {"ok": False, "pid": int(os.environ["CYCLE_APP_PID"])}
    with Session(os.environ["CYCLE_AGENT_SESSION"], timeout=60) as session:
        def invoke(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            return response["result"].get("data", {})

        def targets():
            return {item["id"]: item for item in invoke({"command": "inspectPointerTargets"})["targets"]}

        def point(identifier, x=0.5, y=0.5):
            items = targets()
            bounds = items[identifier]["bounds"]
            origin = items["comparisonWorkspace"]["screenBounds"]
            return (round(origin["x"] + bounds["x"] + x * bounds["width"]),
                    round(origin["y"] + bounds["y"] + y * bounds["height"]))

        def native(*commands):
            subprocess.run([CLICK, "-w", "25", *commands], check=True)
            time.sleep(0.15)

        def click(identifier):
            x, y = point(identifier)
            subprocess.run([str(compiled_helper("launch_cycle_app.swift")),
                            "--focus", os.environ["CYCLE_APP_PID"]], check=True)
            time.sleep(0.1)
            native(f"m:{x + 2},{y}", f"m:{x},{y}", f"dd:{x},{y}")
            native(f"du:{x},{y}")
            invoke({"command": "waitForIdle", "idleDelayMs": 200})
            invoke({"command": "inspectPointerTargets"})

        def select_all():
            subprocess.run(["osascript", "-e",
                            'tell application "System Events" to keystroke "a" using command down'], check=True)

        def search(text):
            click("workspace.sidebar.assetSearch")
            select_all()
            native(f"t:{text}")
            time.sleep(0.15)
            rows = [key for key in targets() if key.startswith("workspace.sidebar.asset.")]
            assert rows == ["workspace.sidebar.asset.factory:" + text], rows

        def drag(source, destination):
            sx, sy = point(source)
            dx, dy = point(destination)
            moves = [f"m:{sx + 2},{sy}", f"m:{sx},{sy}", f"dd:{sx},{sy}", "w:100"]
            for step in range(1, 31):
                moves.extend((f"dm:{round(sx + (dx - sx) * step / 30)},"
                              f"{round(sy + (dy - sy) * step / 30)}", "w:25"))
            native(*moves, "w:250")
            bounds = targets()["comparisonWorkspace"]["screenBounds"]
            rect = ",".join(str(round(bounds[key])) for key in ("x", "y", "width", "height"))
            subprocess.run(["/usr/sbin/screencapture", "-x", "-R" + rect,
                            str(output / "drag-row.png")], check=True)
            native(f"du:{dx},{dy}")
            invoke({"command": "waitForIdle", "idleDelayMs": 500})

        def state():
            return invoke({"command": "snapshotState"})

        try:
            subprocess.run([str(compiled_helper("launch_cycle_app.swift")),
                            "--focus", os.environ["CYCLE_APP_PID"]], check=True)
            invoke({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            invoke({"command": "waitForIdle", "idleDelayMs": 500})
            current = state()
            for card in current["comparison"]["cards"]:
                if card["sample"]:
                    click("comparison.close." + card["id"])
            if state()["comparison"]["visible"]:
                click("comparison.toggle")
            initial = state()
            assert not initial["comparison"]["visible"]
            click("comparison.assets")
            search("sine500")
            drag("workspace.sidebar.asset.factory:sine500", "comparison.drop")
            current = state()
            assert current["comparison"]["visible"]
            assert current["comparison"]["selectedReferenceId"] == "factory:sine500"
            assert current["comparison"]["cardCount"] == 1
            assert current["expandedNodeId"] == ""
            search("sax-growl")
            drag("workspace.sidebar.asset.factory:sax-growl", "comparisonDock")
            assert state()["comparison"]["cardCount"] == 2
            assert state()["comparison"]["selectedReferenceId"] == "factory:sax-growl"
            click("comparison.play.factory:sax-growl")
            assert state()["comparison"]["auditioning"]
            audio = invoke({"command": "captureLiveAudio", "durationMs": 250,
                            "peakGreaterThan": 0.001, "path": str(output / "native-audition.wav")})
            evidence["audio"] = audio["metrics"]
            click("comparison.play.factory:sax-growl")
            assert not state()["comparison"]["auditioning"]
            # Clear search so the final screenshot shows the asset catalog.
            click("workspace.sidebar.assetSearch")
            select_all()
            subprocess.run(["osascript", "-e",
                            'tell application "System Events" to key code 51'], check=True)
            time.sleep(0.2)
            assert len([key for key in targets() if key.startswith("workspace.sidebar.asset.")]) >= 3
            invoke({"command": "openNodeEditor", "nodeId": "timeLayer1"})
            click("comparison.assets")
            assert state()["expandedNodeId"] == ""
            assert state()["sidebarTab"] == "Assets"
            invoke({"command": "resizeWindow", "width": 1000, "height": 700})
            click("workspace.sidebar.assetPlay.factory:sax-growl")
            assert state()["comparison"]["auditioning"]
            click("workspace.sidebar.assetPlay.factory:sax-growl")
            assert not state()["comparison"]["auditioning"]
            current = state()
            for key in ("graphRevision", "documentRevision", "documentDirty"):
                assert current[key] == initial[key], (key, current[key], initial[key])
            evidence["state"] = current
            evidence["ok"] = True
        finally:
            (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps({"ok": True, "output": str(output), "audio": evidence["audio"]}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/reference-assets-native")
    parser.add_argument("--attached", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.attached:
        check(args.output_dir.resolve())
    else:
        command = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec",
                   "--product", "cycle-v2", "--foreground"]
        if args.no_build:
            command.append("--no-build")
        command.extend(["--", sys.executable, str(Path(__file__).resolve()),
                        "--attached", "--output-dir", str(args.output_dir.resolve())])
        subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
