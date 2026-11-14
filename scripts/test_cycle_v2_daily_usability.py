#!/usr/bin/env python3
"""Verify native node menus and explicit pattern copy saves.

Run with cycle_automation.py exec --product cycle-v2 --clean --foreground --
python3 scripts/test_cycle_v2_daily_usability.py after building the app.
"""
import json
import os
import subprocess
import time

from cycle_automation_support.lifecycle import compiled_helper
from cycle_automation_support.profiles import ROOT
from cycle_automation_support.session import Session


def main():
    pid = os.environ["CYCLE_APP_PID"]
    with Session(os.environ["CYCLE_AGENT_SESSION"], timeout=30) as session:
        def command(payload):
            return session.command(payload)["result"]["data"]

        subprocess.run([str(compiled_helper("launch_cycle_app.swift")), "--focus", pid], check=True)
        targets = command({"command": "inspectPointerTargets"})["targets"]
        bounds = next(t["screenBounds"] for t in targets if t["id"] == "canvas")
        x = round(bounds["x"] + bounds["width"] * 0.8)
        y = round(bounds["y"] + bounds["height"] * 0.85)
        before = command({"command": "snapshotState"})
        subprocess.run(["/opt/homebrew/bin/cliclick", f"rc:{x},{y}"], check=True)
        time.sleep(0.2)
        menu = command({"command": "snapshotState"})
        assert abs(menu["popupMenuTargetScreenX"] - x) <= 1, menu
        assert abs(menu["popupMenuTargetScreenY"] - y) <= 1, menu
        region = ",".join(str(round(bounds[k])) for k in ("x", "y", "width", "height"))
        subprocess.run(["screencapture", "-x", f"-R{region}", str(ROOT / ".context/node-creation-menu.png")], check=True)
        # Context is the first submenu; its first entry creates a Voice node.
        subprocess.run(["/opt/homebrew/bin/cliclick", "kp:arrow-down", "kp:arrow-right",
                        "kp:arrow-down", "kp:return"], check=True)
        time.sleep(0.2)
        after = command({"command": "snapshotState"})
        assert after["nodeCount"] == before["nodeCount"] + 1, (before["nodeCount"], after["nodeCount"])
        command({"command": "undo"})
        restored = command({"command": "snapshotState"})
        assert restored["nodeCount"] == before["nodeCount"]
        (ROOT / ".context/node-menu-verification.json").write_text(json.dumps({
            "click": [x, y], "beforeCount": before["nodeCount"],
            "createdCount": after["nodeCount"], "undoCount": restored["nodeCount"]}, indent=2) + "\n")
        command({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/trombone.cyclegraph")})
        command({"command": "pointer", "targetId": "workspace.sidebar.patterns", "event": "click"})
        command({"command": "pointer", "targetId": "workspace.sidebar.patternEdit", "event": "click"})
        command({"command": "pointer", "area": "presetMidiEditor", "event": "down", "x": 900, "y": 250})
        command({"command": "pointer", "area": "presetMidiEditor", "event": "up", "x": 900, "y": 250})
        for modifier in ("command", "control"):
            subprocess.run(["osascript", "-e",
                            f'tell application "System Events" to keystroke "s" using {{{modifier} down}}'], check=True)
            time.sleep(0.15)
            prompt = command({"command": "screenshot", "area": "modal",
                              "path": str(ROOT / f".context/pattern-{modifier}-save.png")})
            assert prompt["width"] < 700, prompt
            command({"command": "key", "area": "modal", "key": "escape"})
            time.sleep(0.05)
        command({"command": "key", "area": "presetMidiEditor", "key": "escape"})
        command({"command": "key", "area": "modal", "key": "return"})
        time.sleep(0.1)
        saved = command({"command": "snapshotState"})
        assert saved["performance"]["previewPatternId"].startswith("user-")
        first_id = saved["performance"]["previewPatternId"]
        first_file = ROOT / ".context/pattern-first-copy.cyclegraph"
        command({"command": "saveGraph", "path": str(first_file)})
        command({"command": "pointer", "targetId": "PerformanceKeyboard.EditSequence", "event": "click"})
        command({"command": "pointer", "area": "presetMidiEditor", "event": "down", "x": 700, "y": 180})
        command({"command": "pointer", "area": "presetMidiEditor", "event": "up", "x": 700, "y": 180})
        command({"command": "key", "area": "presetMidiEditor", "key": "escape"})
        command({"command": "key", "area": "modal", "key": "return"})
        time.sleep(0.1)
        second = command({"command": "snapshotState"})
        assert second["performance"]["previewPatternId"] != first_id
        command({"command": "openGraph", "path": str(first_file)})
        first = command({"command": "snapshotState"})
        assert first["performance"]["previewPatternId"] == first_id
        assert first["performance"]["previewSequenceNoteCount"] == 5
        command({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/trombone.cyclegraph")})
        source = command({"command": "snapshotState"})
        assert source["performance"]["previewSequenceNoteCount"] == 4
        print("Native Cmd-S/Ctrl-S, cancel, save on close and source preservation passed")
        print("Native node menu placement, creation and undo passed")


if __name__ == "__main__":
    main()
