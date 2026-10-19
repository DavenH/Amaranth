"""Fixture, capture, debug, and evidence operations for one validated instance."""

import ctypes
from datetime import datetime
import json
import os
import shutil
import signal
import struct
import subprocess
import time
from pathlib import Path

from .identity import valid_instance
from .lifecycle import compiled_helper, stop_instance
from .registry import lease
from .session import command


def preflight(accessibility=False, screen=False):
    if os.environ.get("CYCLE_PREFLIGHT_PERMISSIONS", "1") == "0":
        return
    if accessibility:
        application_services = ctypes.CDLL(
            "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices")
        application_services.AXIsProcessTrusted.restype = ctypes.c_bool
        if not application_services.AXIsProcessTrusted():
            raise RuntimeError("macOS Accessibility permission is required for the calling terminal/app")
    if screen:
        core_graphics = ctypes.CDLL(
            "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
        core_graphics.CGPreflightScreenCaptureAccess.restype = ctypes.c_bool
        if not core_graphics.CGPreflightScreenCaptureAccess():
            raise RuntimeError("macOS Screen Recording permission is required for the calling terminal/app")


def write_logs(record, artifacts, started):
    raw = artifacts / "app.log.raw"
    filtered = artifacts / "app.log"
    start = datetime.fromtimestamp(started).strftime("%Y-%m-%d %H:%M:%S")
    end = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    try:
        result = subprocess.run(
            ["/usr/bin/log", "show", "--style", "compact", "--start", start,
             "--end", end, "--predicate", f"processID == {record['pid']}"],
            capture_output=True, text=True, check=False, timeout=20)
        output = result.stdout
        errors = result.stderr
    except subprocess.TimeoutExpired as error:
        output = ""
        errors = f"Unified log query timed out: {error}\n"
    raw.write_text(output + errors)
    filtered.write_text("\n".join(line for line in output.splitlines()
                                  if any(token in line.lower() for token in
                                         ("error", "warning", "assert", "crash", "cycle"))) + "\n")


def collect_crash(record, artifacts):
    reports = Path.home() / "Library" / "Logs" / "DiagnosticReports"
    if not reports.is_dir():
        return None
    candidates = []
    for path in reports.glob("*.ips"):
        if path.stat().st_mtime < record["launched"] - 5:
            continue
        try:
            with path.open() as handle:
                heading = json.loads(handle.readline())
                details = json.load(handle)
        except (OSError, ValueError):
            continue
        if details.get("pid") != record["pid"]:
            continue
        if heading.get("app_name") != Path(record["executable"]).name:
            continue
        started = record["process_identity"]["started"]
        if isinstance(started, list):
            try:
                launch = datetime.strptime(details["procLaunch"], "%Y-%m-%d %H:%M:%S.%f %z")
            except (KeyError, ValueError):
                continue
            if abs(launch.timestamp() - (started[0] + started[1] / 1_000_000)) > 2:
                continue
        candidates.append(path)
    if not candidates:
        return None
    newest = max(candidates, key=lambda path: path.stat().st_mtime)
    destination = artifacts / "crash.ips"
    shutil.copyfile(newest, destination)
    return destination


def fixture_commands(path):
    value = json.loads(path.read_text())
    commands = value if isinstance(value, list) else value.get("commands")
    if not isinstance(commands, list):
        raise ValueError(f"Fixture needs a commands array: {path}")
    return commands


def resolve_fixture_paths(value, fixture_dir):
    if isinstance(value, list):
        return [resolve_fixture_paths(item, fixture_dir) for item in value]
    if isinstance(value, dict):
        return {key: (str((fixture_dir / item).resolve())
                      if key == "path" and isinstance(item, str)
                      and item.startswith(("./", "../"))
                      else resolve_fixture_paths(item, fixture_dir))
                for key, item in value.items()}
    return value


def run_fixture(record, fixture, report, owner=None, wait=0, foreground_held=False):
    results = []
    commands = resolve_fixture_paths(fixture_commands(fixture), fixture.parent)
    def execute():
        for index, item in enumerate(commands):
            if not valid_instance(record):
                raise RuntimeError(f"Owned PID {record['pid']} exited during fixture")
            response = command(record["socket"], item, timeout=30, require_ok=False)
            results.append({"commandIndex": index, **response["result"]})

    contains_capture = any(item.get("command") == "screenshot" for item in commands
                           if isinstance(item, dict))
    if contains_capture and owner and not foreground_held:
        with lease("foreground", owner, "app-screenshot", wait=wait):
            execute()
    else:
        execute()
    data = {"ok": all(item.get("ok", False) for item in results), "results": results,
            "pid": record["pid"], "fixture": str(fixture)}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(data, indent=2) + "\n")
    if not data["ok"]:
        raise RuntimeError(f"Fixture has failed commands: {report}")
    return data


def capture(record, owner, destination, rect=None, wait=0):
    with lease("foreground", owner, "capture", wait=wait):
        preflight(screen=True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not valid_instance(record):
            raise RuntimeError(f"Owned PID {record['pid']} changed before capture")
        command = [str(compiled_helper("capture_cycle_screenshot.swift")),
                   str(record["pid"]), str(destination)]
        if rect:
            command.append(rect)
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        fields = dict(field.split("=", 1) for field in result.stdout.strip().split())
        metadata = {
            "method": "pid-window-region" if rect else "pid-window",
            "window_id": int(fields["window"]),
            "bounds": {key: int(fields[key]) for key in ("x", "y", "width", "height")},
        }
        if not destination.is_file() or destination.stat().st_size == 0:
            raise RuntimeError(f"Screenshot missing: {destination}")
        with destination.open("rb") as image:
            header = image.read(24)
        if not header.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError(f"Screenshot is not PNG: {destination}")
        width, height = struct.unpack(">II", header[16:24])
        metadata["pixels"] = {"width": width, "height": height}
    return {**metadata, "pid": record["pid"], "path": str(destination)}


def recorded_fixture(record, fixture, report, video, owner, wait):
    if video is None:
        return run_fixture(record, fixture, report, owner, wait), None
    with lease("foreground", owner, "video", wait=wait):
        preflight(screen=True)
        if not valid_instance(record):
            raise RuntimeError(f"Owned PID {record['pid']} changed before video capture")
        video.parent.mkdir(parents=True, exist_ok=True)
        recorder = subprocess.Popen(
            [str(compiled_helper("capture_cycle_window.swift")),
             str(record["pid"]), str(video), "30"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        outcome = None
        errors = ""
        try:
            time.sleep(1)
            if recorder.poll() is not None:
                raise RuntimeError(f"Video recorder exited early: {recorder.stderr.read()}")
            outcome = run_fixture(record, fixture, report, owner, wait, foreground_held=True)
        finally:
            if recorder.poll() is None:
                recorder.send_signal(signal.SIGINT)
            _, errors = recorder.communicate(timeout=10)
            if recorder.returncode != 0:
                raise RuntimeError(f"Video recorder failed: {errors}")
            if not video.is_file() or video.stat().st_size == 0:
                raise RuntimeError(f"Video missing: {video}")
        line = next((line for line in errors.splitlines()
                     if line.startswith("Cycle window: pid=")), None)
        if not line:
            raise RuntimeError(f"Video window metadata missing: {errors}")
        fields = dict(field.split("=", 1) for field in line.split()[2:])
        metadata = {"method": "pid-window-video", "path": str(video),
                    "pid": record["pid"], "window_id": int(fields["window"]),
                    "bounds": {key: int(fields[key]) for key in
                               ("x", "y", "width", "height")},
                    "pixels": {key: int(fields[key]) for key in ("width", "height")}}
        return outcome, metadata


def debug_fixture(record, fixture, report, commands_path, log_path, artifacts, owner, wait):
    driver = artifacts / "debug.lldb"
    lines = [f"process attach --pid {record['pid']}",
             "process handle SIGTRAP --stop true --notify true --pass true",
             "process handle SIGSEGV --stop true --notify true --pass true",
             "process handle SIGBUS --stop true --notify true --pass true",
             "process handle SIGABRT --stop true --notify true --pass true"]
    if commands_path:
        lines.append(f'command source "{commands_path.resolve()}"')
    lines.append("continue")
    driver.write_text("\n".join(lines) + "\n")
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w") as transcript:
        debugger = subprocess.Popen(["lldb", "--batch", "-s", str(driver)],
                                    stdin=subprocess.DEVNULL, stdout=transcript,
                                    stderr=subprocess.STDOUT)
        try:
            time.sleep(2)
            if debugger.poll() is not None:
                raise RuntimeError(f"LLDB exited before fixture: {log_path}")
            return run_fixture(record, fixture, report, owner, wait)
        finally:
            stop_instance(record)
            try:
                debugger.wait(timeout=10)
            except subprocess.TimeoutExpired:
                debugger.terminate()
                debugger.wait(timeout=5)


def report_rect(report, area, target):
    data = json.loads(report.read_text())
    for result in data["results"]:
        if result.get("type") not in ("inspectTargets", "inspectPointerTargets"):
            continue
        for item in result.get("data", {}).get("targets", []):
            if item.get("area") == area and item.get("target", "") == target:
                bounds = item.get("screenBounds")
                if bounds:
                    return ",".join(str(bounds[key]) for key in ("x", "y", "width", "height"))
    raise RuntimeError(f"No screen bounds for {area}/{target} in {report}")
