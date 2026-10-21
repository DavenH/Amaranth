#!/usr/bin/env python3
"""Check group notes and card gestures through the exact Cycle V2 instance."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from cycle_automation_support.session import command


ROOT = Path(__file__).resolve().parent.parent
NOTE = "Harmonic body\nThe magnitude and phase layers shape a connected, expressive tone."


def check_group_explanations(output, send):
    output.mkdir(parents=True, exist_ok=False)

    def invoke(payload, require_ok=True):
        response = send(payload, require_ok=False)
        with (output / "commands.jsonl").open("a", encoding="utf-8") as log:
            log.write(json.dumps({"command": payload, "response": response}) + "\n")
        ok = response.get("ok") and response.get("result", {}).get("ok")
        if require_ok:
            assert ok, response
            return response["result"].get("data", {})
        assert not ok, response
        return response

    def state():
        return invoke({"command": "snapshotState"})

    def pointer(target, event="click", **values):
        return invoke({"command": "pointer", "targetId": target, "event": event, **values})

    def exported(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()

    evidence = {"ok": False, "pid": int(os.environ["CYCLE_APP_PID"])}
    try:
        capabilities = invoke({"command": "inspectSynthesisCapabilities"})["explanations"]
        assert capabilities["scopes"] == ["preset", "node", "group"]
        assert capabilities["groupCardCommand"] == "setGroupInformationCard"
        invoke({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/old/accoustic.cyclegraph")})
        pointer("node:fft", targetX=0.5, targetY=0.5)
        for event, x, y in (("down", 320, 260), ("drag", 680, 430), ("up", 680, 430)):
            invoke({"command": "pointer", "area": "canvas", "event": event, "x": x, "y": y, "shift": True})
        assert state()["selectedNodeCount"] == 3
        invoke({"command": "key", "area": "canvas", "key": "commandG"})
        group = state()["nodeGroups"][0]["id"]
        target = {"scope": "group", "groupId": group}

        def note(**values):
            return invoke({"command": "setExplanation", **target, **values})

        def card(**values):
            return invoke({"command": "setGroupInformationCard", **target, **values})

        initial = state()
        note(text=NOTE)
        assert state()["graphRevision"] == initial["graphRevision"]
        assert state()["informationCardCount"] == 0
        card(visible=True, offset={"x": 0, "y": 260})
        assert state()["graphRevision"] == initial["graphRevision"]
        assert state()["informationCardCount"] == 1
        assert state()["causalUpdates"] == initial["causalUpdates"]
        before = state()
        baseline = exported("before-rejection.cyclegraph")
        for invalid in ({"scope": "group", "groupId": group, "nodeId": "fft", "text": "bad"},
                        {**target, "text": 5}, {**target, "text": "x" * 16385},
                        {"scope": "group", "groupId": "missing", "text": "bad"}):
            invoke({"command": "setExplanation", **invalid}, require_ok=False)
        for invalid in ({"visible": 1}, {"visible": True, "offset": {"x": "0", "y": 1}},
                        {"visible": True, "offset": {"x": 100001, "y": 0}}):
            invoke({"command": "setGroupInformationCard", **target, **invalid}, require_ok=False)
        assert exported("after-rejection.cyclegraph") == baseline
        after = state()
        for key in ("documentRevision", "documentDirty", "canUndo", "canRedo", "graphRevision"):
            assert after[key] == before[key], (key, before[key], after[key])

        pointer("groupInformation:" + group, "move", expectedCursor="pointingHand")
        pointer("groupInformation:" + group)
        assert state()["explanationEditor"]["text"] == NOTE
        pointer("explanation.cancel")
        invoke({"command": "waitForIdle", "idleDelayMs": 100})
        assert not state()["explanationEditorOpen"]
        pointer("groupToggle:0")
        assert state()["nodeGroups"][0]["collapsed"]
        assert state()["informationCardCount"] == 1
        pointer("groupToggle:0")
        pointer("groupCard:" + group + ".header", "down")
        for x, y in ((0.6, 1.5), (0.7, 2.0)):
            pointer("groupCard:" + group + ".header", "drag", targetX=x, targetY=y)
            assert state()["informationCardDragging"]
            assert state()["documentRevision"] == before["documentRevision"] + 2
            assert state()["nodeGroups"][0]["cardOffset"] == {"x": 0, "y": 260}
        pointer("groupCard:" + group + ".header", "up")
        moved = state()
        assert not moved["informationCardDragging"]
        assert moved["nodeGroups"][0]["cardOffset"] != {"x": 0, "y": 260}
        assert moved["graphRevision"] == before["graphRevision"] + 2
        invoke({"command": "undo"})
        assert state()["nodeGroups"][0]["cardOffset"] == {"x": 0, "y": 260}
        invoke({"command": "redo"})
        assert state()["nodeGroups"][0]["cardOffset"] == moved["nodeGroups"][0]["cardOffset"]
        invoke({"command": "screenshot", "area": "canvas", "path": str(output / "expanded.png")})
        saved = output / "saved.cyclegraph"
        invoke({"command": "saveGraph", "path": str(saved)})
        note(text="")
        assert state()["informationCardCount"] == 0
        assert state()["nodeGroups"][0]["cardEnabled"]
        invoke({"command": "undo"})
        assert state()["informationCardCount"] == 1
        assert not state()["documentDirty"]
        pointer("groupCard:" + group + ".close")
        assert state()["informationCardCount"] == 0
        assert state()["nodeGroups"][0]["rationale"] == NOTE
        invoke({"command": "undo"})
        invoke({"command": "openGraph", "path": str(saved)})
        assert state()["focusedInformationGroup"] == ""
        assert state()["informationCardCount"] == 1
        pointer("groupToggle:0")
        invoke({"command": "screenshot", "area": "canvas", "path": str(output / "collapsed.png")})
        evidence.update(ok=True, groupId=group, verified=["strict rejection isolation", "shared editor",
                "collapse", "local drag previews", "one-step undo/redo", "hide/clear", "save/reload"])
    except Exception as error:
        evidence["error"] = str(error)
    (output / "results.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.in_session:
        coordinator = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
        if args.no_build:
            coordinator.append("--no-build")
        worker = [sys.executable, str(Path(__file__).resolve()), "--in-session", "--output-dir", str(args.output_dir.resolve())]
        return subprocess.run(coordinator + ["--"] + worker, cwd=ROOT, check=False).returncode
    result = check_group_explanations(args.output_dir.resolve(),
            lambda payload, **kwargs: command(os.environ["CYCLE_AGENT_SESSION"], payload, **kwargs))
    print(json.dumps(result, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
