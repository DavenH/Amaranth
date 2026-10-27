#!/usr/bin/env python3
"""Measure asset scrolling and verify cached, prefetched waveform previews."""

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


def check(record, output, baseline):
    output.mkdir(parents=True, exist_ok=True)
    evidence = {"ok": False, "states": {}}
    with Session(record["socket"], timeout=60) as session:
        def call(payload):
            response = session.command(payload)
            with (output / "commands.jsonl").open("a") as log:
                log.write(json.dumps({"command": payload, "response": response}) + "\n")
            assert response["result"].get("success", True), response
            return response["result"].get("data", {})

        def state(label):
            value = call({"command": "snapshotState"})
            evidence["states"][label] = value["assetPreviews"]
            return value

        def settle():
            for _ in range(200):
                current = state("settled")
                if current["assetPreviews"]["pending"] == 0:
                    return current
                time.sleep(0.05)
            raise AssertionError("Asset preview jobs did not settle")

        try:
            call({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
            call({"command": "resizeWindow", "width": 1000, "height": 760})
            initial = state("initial")
            call({"command": "pointer", "event": "click", "targetId": "workspace.sidebar.assets"})
            settle()
            state("before-scroll")
            positions = []
            for step, delta in enumerate([-0.65] * 16 + [0.65] * 16):
                call({"command": "pointer", "event": "wheel", "targetId": "workspace.sidebar.assetList",
                      "deltaY": delta, "deltaX": 0, "smooth": True})
                call({"command": "waitForIdle", "idleDelayMs": 50})
                positions.append(state("scroll-" + str(step))["assetPreviews"]["viewY"])
            assert max(positions) > 0 and min(positions) == 0, positions
            settled = settle()
            if not baseline:
                metrics = settled["assetPreviews"]
                assert metrics["thumbnailDraws"] == 0
                assert metrics["imageDraws"] > 0
                assert metrics["cachedImages"] > metrics["visibleRows"]
                assert metrics["cachedImages"] == metrics["rowCount"]
                assert metrics["paintDecodes"] == 0
                before = metrics["imageBuilds"]
                for delta in [-0.4] * 3 + [0.4] * 3:
                    call({"command": "pointer", "event": "wheel", "targetId": "workspace.sidebar.assetList",
                          "deltaY": delta, "smooth": True})
                    call({"command": "waitForIdle", "idleDelayMs": 50})
                warm = settle()["assetPreviews"]
                assert warm["imageBuilds"] == before, (before, warm)
            final = state("final")
            for key in ("documentRevision", "graphRevision", "documentDirty"):
                assert final[key] == initial[key]
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            str(record["pid"]), str(output / "assets.png")], check=True)
            evidence["positions"] = positions
            evidence["ok"] = True
        finally:
            (output / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
            write_logs(record, output, record["launched"])
    print(json.dumps({"ok": True, "output": str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/asset-previews")
    args = parser.parse_args()
    identity, path, short_id = workspace()
    owner = {"workspace_id": identity, "workspace_path": path,
             "run_id": uuid.uuid4().hex[:12], "app_pid": None}

    def run(record):
        owner["app_pid"] = record["pid"]
        with lease("foreground", owner, "asset-previews", wait=5):
            preflight(accessibility=False, screen=True)
            check(record, args.output_dir.resolve(), args.baseline)

    ensure(PRODUCTS["cycle-v2"], owner, short_id, task=run,
           scoped=True, build_app=not args.no_build)


if __name__ == "__main__":
    main()
