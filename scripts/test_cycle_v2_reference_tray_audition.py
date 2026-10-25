#!/usr/bin/env python3
"""Check tray click dispatch followed by native Space without focusing the editor."""

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


def check(record, output):
    output.mkdir(parents=True, exist_ok=True)
    evidence = {"ok": False, "checks": {}}
    with Session(record["socket"], timeout=60) as session:
        def call(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            return response["result"].get("data", {})

        def state():
            return call({"command": "snapshotState"})

        def click(target, y=0.5):
            # Direct UI dispatch isolates selection/focus from native pointer timing.
            # Space below still travels through the real OS keyboard focus route.
            call({"command": "pointer", "event": "click", "targetId": target, "targetY": y})
            subprocess.run([str(compiled_helper("launch_cycle_app.swift")), "--focus", str(record["pid"])], check=True)
            time.sleep(0.2)

        def space():
            subprocess.run(["osascript", "-e", 'tell application "System Events" to key code 49'], check=True)
            time.sleep(0.05)

        try:
            call({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            call({"command": "resizeWindow", "width": 1280, "height": 900})
            call({"command": "selectReference", "referenceId": "factory:dist3"})
            call({"command": "selectReference", "referenceId": "factory:sax-growl"})
            call({"command": "waitForIdle", "idleDelayMs": 400})
            baseline = state()
            for context in ("canvas", "node", "reference", "already-selected", "header"):
                if context == "node":
                    call({"command": "openNodeEditor", "nodeId": "timeLayer1"})
                if context == "reference":
                    call({"command": "openReferencePitchEditor", "referenceId": "factory:dist3"})
                if context != "already-selected":
                    # Select the first card, then the second; a prior canvas focus must not be needed.
                    click("comparison.surface.factory:dist3")
                if context == "header":
                    click("comparison.card.factory:sax-growl", y=0.05)
                else:
                    click("comparison.surface.factory:sax-growl")
                selected = state()
                assert selected["comparison"]["selectedReferenceId"] == "factory:sax-growl"
                space()
                playing = state()
                evidence["checks"][context] = {"selected": selected, "playing": playing}
                assert playing["comparison"]["auditioning"], context
                assert not playing["performance"]["previewPlaying"]
                audio = call({"command": "captureLiveAudio", "durationMs": 100, "peakGreaterThan": 0.001})
                space()
                assert not state()["comparison"]["auditioning"]
                evidence["checks"][context]["audio"] = audio
            final = state()
            for key in ("documentRevision", "graphRevision", "documentDirty"):
                assert final[key] == baseline[key]
            evidence["ok"] = True
        finally:
            (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
            write_logs(record, output, record["launched"])
    print(json.dumps({"ok": True, "output": str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/reference-tray-audition")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": uuid.uuid4().hex[:12], "app_pid": None}

    def run(record):
        owner["app_pid"] = record["pid"]
        with lease("foreground", owner, "reference-tray-audition", wait=5):
            preflight(accessibility=True)
            check(record, args.output_dir.resolve())

    ensure(PRODUCTS["cycle-v2"], owner, short_id, task=run,
           scoped=True, build_app=not args.no_build)


if __name__ == "__main__":
    main()
