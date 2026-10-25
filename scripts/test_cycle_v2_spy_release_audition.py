#!/usr/bin/env python3
"""Verify expanded Spy note-off dragging defers capture and never edits the graph."""

import argparse
import json
import os
import shutil
import sys
import time
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ARTIFACTS = REPO / ".context" / "spy-release-audition"
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-spy-release-audition.json"


def verify(report):
    failed = [result for result in report["results"] if not result["ok"]]
    if failed:
        raise AssertionError(f"Fixture command failed: {failed[0]}")
    states = [result["data"] for result in report["results"]
              if result.get("type") == "snapshotState"]
    if len(states) != 10:
        raise AssertionError(f"Expected ten gesture checkpoints, got {len(states)}")
    before, first, second, committed, compact, retained, unchanged, cancelled, restored, reopened = states
    for state in states:
        for field in ("documentRevision", "documentGraphRevision", "graphRevision",
                      "canUndo", "canRedo", "previewRenderCount"):
            assert state[field] == before[field], f"Changed {field} during preview audition"
        assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]
        assert state["previewStats"] == before["previewStats"], "Updated unrelated preview products"
        assert state["compileSucceeded"] and not state["validationIssueCount"]
    assert not any(state["documentDirty"] for state in (before, first, second))
    assert all(state["documentDirty"] for state in (committed, compact, retained, unchanged, cancelled, restored, reopened))
    for state, expected in ((first, .6), (second, .35)):
        assert state["probeDetailReleaseDragging"]
        assert abs(state["probeDetailNoteOffTime"] - expected) < .005
        assert state["probeDetailRenderedNoteOffTime"] == 1.
        assert state["probeDetailRenderCount"] == before["probeDetailRenderCount"]
        assert state["probeDetailContentRevision"] == before["probeDetailContentRevision"]
    for state in (committed, retained, unchanged, cancelled):
        assert not state["probeDetailReleaseDragging"]
        assert abs(state["probeDetailNoteOffTime"] - .35) < .005
        assert state["probeDetailRenderCount"] == (before["probeDetailRenderCount"] + 1
                                                   if state is committed else 1)
        assert state["probeDetailContentRevision"] == committed["probeDetailContentRevision"]
        assert state["probeDetailReleaseDuration"] == before["probeDetailReleaseDuration"]
    assert committed["probeDetailContentRevision"] != before["probeDetailContentRevision"]
    assert not compact["probeDetailOpen"]
    assert abs(compact["defaultOutputPreviewStats"]["noteOffTime"] - .35) < .005
    assert compact["defaultOutputPreviewStats"] == committed["defaultOutputPreviewStats"]
    for state in (before, first, second, committed, compact, retained, restored, reopened):
        expected = .35 if state in (committed, compact, retained) else 1.
        assert abs(state["defaultOutputPreviewStats"]["noteOffTime"] - expected) < .005
    assert restored["probeDetailNoteOffTime"] == 1.
    assert restored["probeDetailRenderCount"] == 2
    assert restored["probeDetailContentRevision"] == before["probeDetailContentRevision"]
    assert reopened["probeDetailNoteOffTime"] == 1.
    assert reopened["probeDetailRenderCount"] == 1


def verify_native_session():
    from cycle_automation_support.session import Session
    from cycle_automation_support.lifecycle import compiled_helper

    click = shutil.which("cliclick")
    if click is None:
        raise RuntimeError("Native drag verification requires cliclick")
    report = []
    with Session(os.environ["CYCLE_AGENT_SESSION"], timeout=120) as session:
        def command(value):
            return session.command(value)["result"].get("data", {})

        def state():
            return command({"command": "snapshotState"})

        def capture(name, rect):
            subprocess.run([str(compiled_helper("capture_cycle_screenshot.swift")),
                            os.environ["CYCLE_APP_PID"], str(ARTIFACTS / name),
                            ",".join(str(round(rect[key])) for key in ("x", "y", "width", "height"))],
                           check=True, capture_output=True, text=True)

        def native(*events):
            subprocess.run([click, "-w", "30", *events], check=True)
            time.sleep(.15)

        command({"command": "openGraph", "path": str(
            REPO / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
        command({"command": "waitForIdle", "idleDelayMs": 700})
        subprocess.run([str(compiled_helper("launch_cycle_app.swift")),
                        "--focus", os.environ["CYCLE_APP_PID"]], check=True)
        command({"command": "pointer", "event": "doubleClick", "targetId": "spy:default-output"})
        for suffix, expected in (("release", .35), ("releaseIcon", .65)):
            before = state()
            targets = command({"command": "inspectPointerTargets"})["targets"]
            def bounds(name):
                return next(target["screenBounds"] for target in targets
                            if target["id"] == "probeDetail:default-output." + name)
            badge = next(target["screenBounds"] for target in targets
                         if target["id"] == "workspace.presetInformation")
            status_sample = {"x": badge["x"] + 60, "y": badge["y"] + 40, "width": 6, "height": 6}
            capture(f"native-{suffix}-status-before.png", status_sample)
            handle, voice = bounds(suffix), bounds("voice")
            x, y = round(handle["x"] + handle["width"] / 2), round(handle["y"] + handle["height"] / 2)
            destination = round(voice["x"] + voice["width"] * expected)
            native(f"m:{round(badge['x'] + badge['width'] / 2)},{round(badge['y'] + badge['height'] / 2)}")
            native(f"m:{x},{y}", "w:40", f"dd:{x},{y}")
            try:
                assert state()["probeDetailReleaseDragging"], f"Native {suffix} down missed"
                native(f"dm:{destination},{y}")
                held = state()
                capture(f"native-{suffix}-status-held.png", status_sample)
                status_pixels = []
                for phase in ("before", "held"):
                    png = ARTIFACTS / f"native-{suffix}-status-{phase}.png"
                    bmp = png.with_suffix(".bmp")
                    subprocess.run(["sips", "-s", "format", "bmp", str(png), "--out", str(bmp)],
                                   check=True, capture_output=True, text=True)
                    data = bmp.read_bytes()
                    offset = int.from_bytes(data[10:14], "little")
                    status_pixels.append(data[offset:])
                assert status_pixels[0] == status_pixels[1], "Status background flashed during drag"
                assert held["probeDetailReleaseDragging"]
                assert held["probeDetailRenderCount"] == before["probeDetailRenderCount"]
                assert held["probeDetailContentRevision"] == before["probeDetailContentRevision"]
            finally:
                native(f"du:{destination},{y}")
            deadline = time.monotonic() + 10
            committed = state()
            while committed["probeDetailReleaseDragging"] and time.monotonic() < deadline:
                time.sleep(.05)
                committed = state()
            assert not committed["probeDetailReleaseDragging"], "Native mouse-up did not commit"
            assert committed["documentDirty"]
            assert committed["probeDetailRenderCount"] == before["probeDetailRenderCount"] + 1
            assert abs(committed["probeDetailNoteOffTime"] - expected) < .005
            for field in ("documentRevision", "documentGraphRevision", "previewStats"):
                assert committed[field] == before[field], f"Native drag changed {field}"
            command({"command": "screenshot", "area": "canvas",
                     "path": str(ARTIFACTS / f"native-{suffix}.png")})
            detail = next(target["screenBounds"] for target in targets
                          if target["id"] == "probeDetail:default-output")
            native(f"m:{round(badge['x'] + badge['width'] / 2)},{round(badge['y'] + badge['height'] / 2)}")
            capture(f"native-{suffix}-panel.png", detail)
            command({"command": "key", "key": "escape"})
            compact = state()
            assert abs(compact["defaultOutputPreviewStats"]["noteOffTime"] - expected) < .005
            command({"command": "screenshot", "area": "canvas",
                     "path": str(ARTIFACTS / f"native-{suffix}-compact.png")})
            command({"command": "pointer", "event": "doubleClick", "targetId": "spy:default-output"})
            assert abs(state()["probeDetailNoteOffTime"] - expected) < .005
            report.append({"target": suffix, "source": [x, y], "held": held,
                           "committed": committed, "compact": compact})
        saved = ARTIFACTS / "native-saved.cyclegraph"
        command({"command": "saveGraph", "path": str(saved)})
        assert not state()["documentDirty"]
        saved_json = json.loads(saved.read_text())
        assert abs(saved_json["presetPresentation"]["spyNoteOffTimes"]["default-output"] - .65) < .005
        command({"command": "openGraph", "path": str(
            REPO / "cycle-v2/content/presets/baroque-flute.cyclegraph")})
        assert state()["defaultOutputPreviewStats"]["noteOffTime"] == 1.
        command({"command": "openGraph", "path": str(saved)})
        assert abs(state()["defaultOutputPreviewStats"]["noteOffTime"] - .65) < .005
        command({"command": "pointer", "event": "doubleClick", "targetId": "spy:default-output"})
        assert abs(state()["probeDetailNoteOffTime"] - .65) < .005
    (ARTIFACTS / "native-report.json").write_text(json.dumps(report, indent=2))
    print("Native line/icon drag, deferred capture, compact/reopen retention, preset save/load and graph isolation pass.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--native", action="store_true", help="Verify physical mouse routing in an isolated app")
    parser.add_argument("--native-session", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--report", type=Path, help="Verify an existing fixture report")
    args = parser.parse_args()
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    if args.native_session:
        verify_native_session()
        return
    if args.native:
        subprocess.run([sys.executable, str(REPO / "scripts/cycle_automation.py"),
                        "exec", "--product", "cycle-v2", "--clean", "--no-build", "--foreground",
                        "--", sys.executable, str(Path(__file__).resolve()), "--native-session"],
                       cwd=REPO, check=True)
        return
    report_path = args.report or ARTIFACTS / "report.json"
    if args.report is None:
        command = [str(REPO / "scripts/cycle_automation.py"), "run", "--product", "cycle-v2", "--clean",
                   "--fixture", str(FIXTURE), "--report", str(report_path), "--json"]
        if args.no_build:
            command.append("--no-build")
        completed = subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
        (ARTIFACTS / "run.json").write_text(completed.stdout)
    verify(json.loads(report_path.read_text()))
    print("Spy note-off drag: no capture during movement, one capture on release; "
          "compact/reopen retention, no-op/cancel/clamp and graph/audio isolation pass.")


if __name__ == "__main__":
    main()
