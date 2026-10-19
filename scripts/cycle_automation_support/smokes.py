"""Declarative Cycle smoke-suite execution under a coordinator instance lease."""

import json
import os
import re
import subprocess
import sys
from contextlib import nullcontext
from pathlib import Path

from .operations import capture, fixture_commands
from .profiles import ROOT
from .registry import lease


def selected_fixtures(manifest, names, include_visual):
    available = {item["name"]: item for item in manifest["fixtures"]}
    optional = manifest.get("optional_screenshot")
    if optional:
        available[optional["name"]] = optional
    if names:
        unknown = sorted(set(names) - set(available))
        prefix = manifest.get("dynamic_fixture_prefix")
        if prefix:
            for name in list(unknown):
                if re.fullmatch(r"[a-z0-9-]+", name):
                    path = f"scripts/fixtures/{prefix}{name}.json"
                    if (ROOT / path).is_file():
                        available[name] = {"name": name, "path": path}
                        unknown.remove(name)
        if unknown:
            raise ValueError(f"Unknown smoke fixtures: {', '.join(unknown)}")
        return [available[name] for name in names]
    return list(manifest["fixtures"]) + ([optional] if include_visual and optional else [])


def session_rect(report, area):
    data = json.loads(report.read_text())
    for fixture in reversed(data.get("fixtures", [])):
        for result in fixture.get("results", []):
            response = result.get("response", {}).get("result", {})
            if response.get("type") != "inspectTargets":
                continue
            for target in response.get("data", {}).get("targets", []):
                if target.get("area") == area and target.get("resolved"):
                    bounds = target.get("screenBounds")
                    if bounds:
                        return ",".join(str(bounds[key]) for key in
                                        ("x", "y", "width", "height"))
    raise RuntimeError(f"No screen bounds for {area} in {report}")


def run_session_suite(record, manifest, fixtures, report, artifacts, owner, wait):
    script = ROOT / "scripts" / "cycle_agent_smoke_session.py"
    reset = os.environ.get("CYCLE_AGENT_SMOKE_RESET_PRESET",
                           manifest.get("reset_preset", ""))
    command = [sys.executable, str(script), record["socket"],
               "--report", str(report), "--keep-alive", "--reset-preset", reset]
    for expected in manifest.get("expected_failures", []):
        command.extend(["--expected-failure", expected])
    for env_name, flag in (("CYCLE_AGENT_SMOKE_RESUME", "--resume"),
                           ("CYCLE_AGENT_SMOKE_SKIP_MAIN_TAB_RESET", "--skip-main-tab-reset"),
                           ("CYCLE_AGENT_SMOKE_BOUNDARY_LOG", "--boundary-log"),
                           ("CYCLE_AGENT_SMOKE_OPENGL_DIAGNOSTICS_EACH_FIXTURE",
                            "--opengl-diagnostics-each-fixture")):
        if os.environ.get(env_name) == "1":
            command.append(flag)
    command.extend(f"{item['name']}={ROOT / item['path']}" for item in fixtures)
    contains_capture = any(
        any(isinstance(step, dict) and step.get("command") == "screenshot"
            for step in fixture_commands(ROOT / item["path"]))
        for item in fixtures)
    capture_lease = (lease("foreground", owner, "app-screenshot-suite", wait=wait)
                     if contains_capture else nullcontext())
    with capture_lease:
        status = subprocess.run(command, check=False).returncode
    optional = manifest.get("optional_screenshot")
    if optional and any(item["name"] == optional["name"] for item in fixtures) and status == 0:
        rect = session_rect(report, optional["area"])
        capture(record, owner, artifacts / optional["output"], rect, wait)
    return status


def run_manifest(record, manifest_path, names, report, artifacts, owner, wait,
                 include_visual):
    manifest = json.loads(manifest_path.read_text())
    fixtures = selected_fixtures(manifest, names, include_visual)
    if not fixtures:
        raise ValueError("No smoke fixtures selected")
    report.parent.mkdir(parents=True, exist_ok=True)
    if manifest["mode"] != "session":
        raise ValueError(f"Unknown session smoke mode: {manifest['mode']}")
    return run_session_suite(record, manifest, fixtures, report, artifacts,
                             owner, wait)


def run_fresh_manifest(profile, manifest, names, report, artifacts, include_visual,
                       no_build=False):
    fixtures = selected_fixtures(manifest, names, include_visual)
    if not fixtures:
        raise ValueError("No smoke fixtures selected")
    results = []
    overrides = manifest.get("visual_overrides", {}) if include_visual else {}
    for item in fixtures:
        name = item["name"]
        visual = overrides.get(name, item if item == manifest.get("optional_screenshot") else None)
        fixture = ROOT / (visual or item)["path"]
        fixture_report = artifacts / f"{name}-report.json"
        command = [sys.executable, str(ROOT / "scripts/cycle_automation.py"),
                   "run", "--product", profile.name, "--clean",
                   "--fixture", str(fixture), "--report", str(fixture_report),
                   "--app-log", str(artifacts / f"{name}-log.txt"), "--json"]
        if no_build:
            command.append("--no-build")
        image = None
        if visual:
            image = artifacts / visual["output"]
            command.extend(["--screenshot", str(image), "--area", visual["area"]])
        process = subprocess.run(command, capture_output=True, text=True, check=False)
        ok = process.returncode == 0
        pid = None
        if ok:
            pid = json.loads(process.stdout)["pid"]
            assertion = visual.get("assert_script") if visual else None
            if assertion:
                assert_args = [sys.executable, str(ROOT / assertion), str(image)]
                if "gl_regions" in assertion:
                    assert_args.append(str(fixture_report))
                assert_args.extend(visual.get("assert_args", []))
                ok = subprocess.run(assert_args, check=False).returncode == 0
        results.append({"name": name, "ok": ok, "pid": pid,
                        "report": str(fixture_report), "error": process.stderr.strip() if not ok else ""})
        print(f"{'ok' if ok else 'FAIL'} {name}", flush=True)
    report.write_text(json.dumps({"ok": all(item["ok"] for item in results),
                                  "fixtures": results}, indent=2) + "\n")
    return results
