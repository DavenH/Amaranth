"""Fixture, capture, debug, and evidence operations for one validated instance."""

from datetime import datetime
import json
import shutil
import signal
import subprocess
import time
from pathlib import Path

from .identity import valid_instance
from .lifecycle import compiled_helper, stop_instance
from .registry import lease
from .session import command


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


def run_fixture(record, fixture, report):
    results = []
    for index, item in enumerate(fixture_commands(fixture)):
        if not valid_instance(record):
            raise RuntimeError(f"Owned PID {record['pid']} exited during fixture")
        response = command(record["socket"], item, timeout=30, require_ok=False)
        results.append({"commandIndex": index, **response["result"]})
    data = {"ok": all(item.get("ok", False) for item in results), "results": results,
            "pid": record["pid"], "fixture": str(fixture)}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(data, indent=2) + "\n")
    if not data["ok"]:
        raise RuntimeError(f"Fixture has failed commands: {report}")
    return data


def focus(record):
    if not valid_instance(record):
        raise RuntimeError(f"Owned PID {record['pid']} is no longer valid")
    script = (f'tell application "System Events" to set frontmost of '
              f'(first process whose unix id is {record["pid"]}) to true')
    subprocess.run(["osascript", "-e", script], check=True)


def capture(record, owner, destination, rect=None, wait=0):
    with lease("foreground", owner, "capture", wait=wait):
        destination.parent.mkdir(parents=True, exist_ok=True)
        if rect:
            focus(record)
            if not valid_instance(record):
                raise RuntimeError(f"Owned PID {record['pid']} changed before capture")
            subprocess.run(["screencapture", "-x", "-R", rect, str(destination)], check=True)
            metadata = {"method": "foreground-region", "bounds": rect}
        else:
            if not valid_instance(record):
                raise RuntimeError(f"Owned PID {record['pid']} changed before capture")
            result = subprocess.run(
                [str(compiled_helper("capture_cycle_screenshot.swift")),
                 str(record["pid"]), str(destination)],
                check=True, capture_output=True, text=True)
            fields = dict(field.split("=", 1) for field in result.stdout.strip().split())
            metadata = {
                "method": "pid-window", "window_id": int(fields["window"]),
                "bounds": {key: int(fields[key]) for key in ("x", "y", "width", "height")},
                "pixels": {key: int(fields[key]) for key in ("width", "height")},
            }
        if not destination.is_file() or destination.stat().st_size == 0:
            raise RuntimeError(f"Screenshot missing: {destination}")
    return {**metadata, "pid": record["pid"], "path": str(destination)}


def recorded_fixture(record, fixture, report, video, owner, wait):
    if video is None:
        return run_fixture(record, fixture, report)
    with lease("foreground", owner, "video", wait=wait):
        if not valid_instance(record):
            raise RuntimeError(f"Owned PID {record['pid']} changed before video capture")
        video.parent.mkdir(parents=True, exist_ok=True)
        recorder = subprocess.Popen(
            [str(compiled_helper("capture_cycle_window.swift")),
             str(record["pid"]), str(video), "30"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            time.sleep(1)
            if recorder.poll() is not None:
                raise RuntimeError(f"Video recorder exited early: {recorder.stderr.read()}")
            return run_fixture(record, fixture, report)
        finally:
            if recorder.poll() is None:
                recorder.send_signal(signal.SIGINT)
            _, errors = recorder.communicate(timeout=10)
            if recorder.returncode != 0:
                raise RuntimeError(f"Video recorder failed: {errors}")
            if not video.is_file() or video.stat().st_size == 0:
                raise RuntimeError(f"Video missing: {video}")


def debug_fixture(record, fixture, report, commands_path, log_path, artifacts):
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
            return run_fixture(record, fixture, report)
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
