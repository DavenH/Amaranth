#!/usr/bin/env python3
"""Coordinate local Cycle automation by workspace, product, and exact PID."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path

from cycle_automation_support.identity import valid_instance, workspace
from cycle_automation_support.lifecycle import ensure, stop
from cycle_automation_support.profiles import PRODUCTS, ROOT
from cycle_automation_support.registry import lease, recover, registry
from cycle_automation_support.smokes import run_fresh_manifest, run_manifest
from cycle_automation_support.operations import (
    capture, collect_crash, debug_fixture, preflight, recorded_fixture,
    report_rect, write_logs
)


def output_base(product, run_id):
    path = ROOT / ".context" / "cycle-automation" / product / run_id
    path.mkdir(parents=True, exist_ok=True)
    return path


def result_base(owner, profile, record, run_id, artifacts):
    return {
        "workspace_id": owner["workspace_id"], "workspace_path": owner["workspace_path"],
        "product": profile.name, "app": str(profile.app),
        "fingerprint": record["fingerprint"] if record else None,
        "pid": record["pid"] if record else None,
        "socket": record["socket"] if record else None,
        "run_id": run_id, "artifacts": str(artifacts) if artifacts else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="operation", required=True)
    subparsers.add_parser("status")
    for name in ("start", "run", "suite", "debug", "capture", "exec", "stop"):
        sub = subparsers.add_parser(name)
        sub.add_argument("--product", required=True, choices=PRODUCTS)
        sub.add_argument("--wait", type=float, default=0)
        sub.add_argument("--json", action="store_true")
        if name in ("start", "run", "suite", "debug", "capture", "exec"):
            sub.add_argument("--no-build", action="store_true")
            sub.add_argument("--clean", action="store_true")
            sub.add_argument("--app-log", type=Path)
        if name == "run":
            sub.add_argument("--fixture", type=Path, required=True)
            sub.add_argument("--report", type=Path)
            sub.add_argument("--screenshot", type=Path)
            sub.add_argument("--video", type=Path)
            sub.add_argument("--area")
            sub.add_argument("--target", default="")
            sub.add_argument("--allow-failures", action="store_true")
            sub.add_argument("--startup", action="store_true")
        if name == "suite":
            sub.add_argument("--fixture", action="append", default=[])
            sub.add_argument("--manifest", type=Path)
            sub.add_argument("--report", type=Path)
            sub.add_argument("--smoke-arg", action="append", default=[])
            sub.add_argument("--visual", action="store_true")
            sub.add_argument("names", nargs="*")
        if name == "debug":
            sub.add_argument("--fixture", type=Path, required=True)
            sub.add_argument("--report", type=Path)
            sub.add_argument("--commands", type=Path)
            sub.add_argument("--lldb-log", type=Path)
        if name == "capture":
            sub.add_argument("--output", type=Path)
            sub.add_argument("--rect")
        if name == "exec":
            sub.add_argument("--foreground", action="store_true")
            sub.add_argument("command", nargs=argparse.REMAINDER)
        if name == "stop":
            sub.add_argument("--owned", action="store_true", required=True)
    args = parser.parse_args()
    identity, path, short_id = workspace()
    if args.operation == "status":
        with registry() as data:
            recover(data)
            print(json.dumps(data, indent=2))
        return 0
    if sys.platform != "darwin" or os.environ.get("CONDUCTOR_IS_LOCAL") == "0":
        parser.error("macOS GUI automation requires a local macOS workspace")
    if args.wait < 0 or args.wait > 300:
        parser.error("--wait must be between 0 and 300 seconds")
    profile = PRODUCTS[args.product]
    run_id = uuid.uuid4().hex[:12]
    artifacts = output_base(profile.name, run_id)
    if args.operation == "suite":
        artifact_env = ("CYCLE_AGENT_ARTIFACT_DIR" if profile.name == "cycle1"
                        else "CYCLE_V2_AGENT_ARTIFACT_DIR")
        if os.environ.get(artifact_env):
            artifacts = Path(os.environ[artifact_env]).expanduser().resolve()
            artifacts.mkdir(parents=True, exist_ok=True)
    owner = {"workspace_id": identity, "workspace_path": path, "run_id": run_id, "app_pid": None}
    if args.operation == "stop":
        record = stop(profile, owner, short_id, args.wait)
        result = result_base(owner, profile, record, run_id, artifacts)
        (artifacts / "run.json").write_text(json.dumps(result, indent=2) + "\n")
    else:
        if args.operation in ("run", "debug"):
            fixture = args.fixture.resolve()
            if not fixture.is_file():
                parser.error(f"Fixture missing: {fixture}")
            if not fixture.name.startswith(profile.fixture_prefix):
                parser.error(f"{profile.name} requires {profile.fixture_prefix} fixtures: {fixture}")
            if args.operation == "debug" and args.commands and not args.commands.is_file():
                parser.error(f"LLDB commands missing: {args.commands}")
        if args.operation == "suite":
            if not args.manifest and not args.fixture:
                parser.error("suite requires --manifest or --fixture")
            if args.manifest and not args.manifest.is_file():
                parser.error(f"Smoke manifest missing: {args.manifest}")
            for spec in args.fixture:
                path_part = spec.split("=", 1)[-1]
                if not Path(path_part).name.startswith(profile.fixture_prefix):
                    parser.error(f"{profile.name} requires {profile.fixture_prefix} fixtures: {spec}")
        if args.operation == "exec" and not args.command:
            parser.error("exec requires a command after --")
        if args.operation == "suite" and args.manifest:
            manifest = json.loads(args.manifest.read_text())
            if manifest["mode"] == "fresh-fixtures":
                env_names = ("CYCLE_AGENT_SMOKE_FIXTURES" if profile.name == "cycle1"
                             else "CYCLE_V2_AGENT_SMOKE_FIXTURES")
                names = args.names or os.environ.get(env_names, "").split()
                visual_env = ("CYCLE_AGENT_SMOKE_OS_SCREENSHOT" if profile.name == "cycle1"
                              else "CYCLE_V2_AGENT_SMOKE_OS_SCREENSHOT")
                visual = args.visual or os.environ.get(visual_env) == "1"
                report = (args.report or artifacts / "report.json").resolve()
                results = run_fresh_manifest(profile, manifest, names, report,
                                             artifacts, visual, args.no_build)
                result = result_base(owner, profile, None, run_id, artifacts)
                result["report"] = str(report)
                result["fixtures"] = results
                result["ok"] = all(item["ok"] for item in results)
                (artifacts / "run.json").write_text(json.dumps(result, indent=2) + "\n")
                if not result["ok"]:
                    raise RuntimeError(f"Smoke suite failed: {report}")
                print(json.dumps(result, indent=2) if args.json else
                      f"suite {profile.name}: {len(results)} fixtures, artifacts {artifacts}")
                return 0

        last_record = None
        run_started = time.time()

        def task(record):
            nonlocal last_record
            last_record = record
            owner["app_pid"] = record["pid"]
            result = result_base(owner, profile, record, run_id, artifacts)
            if args.operation == "run":
                report = (args.report or artifacts / "report.json").resolve()
                try:
                    fixture_result, video_metadata = recorded_fixture(
                        record, fixture, report,
                        args.video.resolve() if args.video else None, owner, args.wait)
                except RuntimeError:
                    if not args.allow_failures or not report.is_file():
                        raise
                    fixture_result = json.loads(report.read_text())
                result["fixture_result"] = {
                    "ok": fixture_result["ok"],
                    "command_count": len(fixture_result["results"]),
                }
                result["report"] = str(report)
                if args.video:
                    result["video"] = video_metadata
                if args.screenshot:
                    rect = report_rect(report, args.area, args.target) if args.area else None
                    result["capture"] = capture(record, owner, args.screenshot.resolve(), rect, args.wait)
            elif args.operation == "suite":
                report = (args.report or artifacts / "report.json").resolve()
                if args.manifest:
                    env_names = ("CYCLE_AGENT_SMOKE_FIXTURES" if profile.name == "cycle1"
                                 else "CYCLE_V2_AGENT_SMOKE_FIXTURES")
                    names = args.names or os.environ.get(env_names, "").split()
                    visual_env = ("CYCLE_AGENT_SMOKE_OS_SCREENSHOT" if profile.name == "cycle1"
                                  else "CYCLE_V2_AGENT_SMOKE_OS_SCREENSHOT")
                    visual = args.visual or os.environ.get(visual_env) == "1"
                    status = run_manifest(record, args.manifest.resolve(), names,
                                          report, artifacts, owner, args.wait, visual)
                else:
                    suite = Path(__file__).with_name("cycle_agent_smoke_session.py")
                    status = subprocess.run(
                        [sys.executable, str(suite), record["socket"],
                         "--report", str(report), "--keep-alive", *args.smoke_arg, *args.fixture],
                        check=False).returncode
                result["report"] = str(report)
                result["ok"] = status == 0
                if status:
                    raise RuntimeError(f"Smoke suite failed with exit {status}: {report}")
            elif args.operation == "debug":
                report = (args.report or artifacts / "report.json").resolve()
                transcript = (args.lldb_log or artifacts / "lldb.log").resolve()
                fixture_result = debug_fixture(
                    record, fixture, report, args.commands, transcript, artifacts,
                    owner, args.wait)
                result["fixture_result"] = {"ok": fixture_result["ok"],
                                            "command_count": len(fixture_result["results"])}
                result["report"] = str(report)
                result["lldb_log"] = str(transcript)
            elif args.operation == "exec":
                environment = os.environ.copy()
                environment.update({
                    "CYCLE_AGENT_SESSION": record["socket"],
                    "CYCLE_APP_PID": str(record["pid"]),
                    "CYCLE_AUTOMATION_ARTIFACTS": str(artifacts),
                })
                command_args = args.command[1:] if args.command[0] == "--" else args.command
                if args.foreground:
                    with lease("foreground", owner, "exec", wait=args.wait):
                        preflight(accessibility=True)
                        status = subprocess.run(command_args, env=environment, check=False).returncode
                else:
                    status = subprocess.run(command_args, env=environment, check=False).returncode
                result["exit_code"] = status
                if status:
                    raise RuntimeError(f"Coordinated command exited {status}: {command_args}")
            elif args.operation == "capture":
                destination = (args.output or artifacts / "screenshot.png").resolve()
                result["capture"] = capture(record, owner, destination, args.rect, args.wait)
            write_logs(record, artifacts, run_started)
            if args.app_log:
                explicit_log = args.app_log.resolve()
                explicit_log.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(artifacts / "app.log", explicit_log)
                shutil.copyfile(artifacts / "app.log.raw", Path(str(explicit_log) + ".raw"))
            (artifacts / "run.json").write_text(json.dumps(result, indent=2) + "\n")
            return result

        scoped = args.operation == "debug" or (args.operation == "run" and (args.clean or args.startup))
        try:
            result = ensure(profile, owner, short_id, args.clean, args.wait, task,
                            scoped, not args.no_build)
        except Exception as error:
            failure = result_base(owner, profile, None, run_id, artifacts)
            failure["error"] = str(error)
            crash_record = last_record or getattr(error, "record", None)
            if crash_record:
                write_logs(crash_record, artifacts, run_started)
            if crash_record and not valid_instance(crash_record):
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    crash = collect_crash(crash_record, artifacts)
                    if crash:
                        failure["crash"] = str(crash)
                        break
                    time.sleep(0.5)
            (artifacts / "run.json").write_text(json.dumps(failure, indent=2) + "\n")
            raise
    print(json.dumps(result, indent=2) if args.json else
          f"{args.operation} {profile.name}: PID {result['pid']} socket {result['socket']} artifacts {result['artifacts']}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Cycle automation: {error}", file=sys.stderr)
        sys.exit(1)
