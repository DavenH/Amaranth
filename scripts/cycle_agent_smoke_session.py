#!/usr/bin/env python3
import argparse
import json
import socket
import sys
import time
from pathlib import Path


def load_fixture(path):
    with open(path, "r", encoding="utf-8") as handle:
        fixture = json.load(handle)

    if isinstance(fixture, list):
        return fixture

    commands = fixture.get("commands")

    if not isinstance(commands, list):
        raise ValueError(f"Fixture must be an array or object with commands array: {path}")

    return commands


def send_command(socket_path, command, request_id, timeout):
    request = {
        "id": request_id,
        "command": command,
    }

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect(socket_path)
        sock.sendall((json.dumps(request, separators=(",", ":")) + "\n").encode("utf-8"))

        chunks = []

        while True:
            chunk = sock.recv(4096)

            if not chunk:
                break

            chunks.append(chunk)

            if b"\n" in chunk:
                break

    response = b"".join(chunks).split(b"\n", 1)[0]

    if not response:
        raise RuntimeError("Session returned an empty response")

    return json.loads(response.decode("utf-8"))


def command_name(command):
    if isinstance(command, dict):
        return str(command.get("command", command.get("type", "unknown")))

    return "unknown"


def make_skipped_result(fixture_name, index, command):
    return {
        "fixture": fixture_name,
        "commandIndex": index,
        "command": command_name(command),
        "ok": False,
        "skipped": True,
        "message": "Skipped after earlier fixture command failure",
    }


def run_boundary_command(args, fixture_name, phase, command_index, command):
    try:
        response = send_command(
            args.socket_path,
            command,
            f"{fixture_name}:{phase}",
            args.timeout,
        )
        ok = bool(response.get("ok"))
        message = response.get("result", {}).get("message", response.get("message", ""))
    except Exception as exc:
        response = {
            "ok": False,
            "message": str(exc),
        }
        ok = False
        message = str(exc)

    return {
        "fixture": fixture_name,
        "commandIndex": command_index,
        "command": command_name(command),
        "ok": ok,
        "message": message,
        "diagnostic": True,
        "phase": phase,
        "response": response,
    }


def add_boundary_diagnostics(args, fixture_name, phase, command_index, results):
    if args.boundary_log:
        results.append(run_boundary_command(args, fixture_name, phase + ":log", command_index, {
            "command": "logMessage",
            "message": f"smoke {fixture_name} {phase}",
        }))

    if args.opengl_diagnostics_each_fixture:
        results.append(run_boundary_command(args, fixture_name, phase + ":opengl", command_index, {
            "command": "openGLDiagnostics",
            "poll": True,
            "phase": f"smoke-{fixture_name}-{phase}",
        }))


def run_fixture(args, fixture_name, fixture_path, expected_failure):
    commands = load_fixture(fixture_path)
    results = []
    saw_failure = False
    skipped_after_failure = False

    reset_commands = []

    reset_commands.append({
        "command": "dismissTransientUi",
        "waitForIdle": True,
        "idleDelayMs": 50,
    })

    if args.reset_preset:
        reset_commands.append({
            "command": "openFactoryPreset",
            "preset": args.reset_preset,
        })
        reset_commands.append({
            "command": "waitForIdle",
            "delayMs": args.reset_delay_ms,
        })

    if not args.skip_main_tab_reset:
        reset_commands.append({
            "command": "resetMainPanelView",
            "waitForIdle": True,
            "idleDelayMs": 50,
        })

    for reset_index, reset_command in enumerate(reset_commands):
        try:
            response = send_command(
                args.socket_path,
                reset_command,
                f"{fixture_name}:reset:{reset_index}",
                args.timeout,
            )
            reset_ok = bool(response.get("ok"))
        except Exception as exc:
            response = {
                "ok": False,
                "message": str(exc),
            }
            reset_ok = False

        results.append({
            "fixture": fixture_name,
            "commandIndex": -1 - reset_index,
            "command": command_name(reset_command),
            "ok": reset_ok,
            "reset": True,
            "response": response,
        })

        if not reset_ok:
            saw_failure = True
            skipped_after_failure = True
            break

    add_boundary_diagnostics(args, fixture_name, "before", -100, results)

    for index, command in enumerate(commands):
        if skipped_after_failure:
            results.append(make_skipped_result(fixture_name, index, command))
            continue

        try:
            response = send_command(
                args.socket_path,
                command,
                f"{fixture_name}:{index}",
                args.timeout,
            )
            ok = bool(response.get("ok"))
            message = response.get("result", {}).get("message", response.get("message", ""))
        except Exception as exc:
            response = {
                "ok": False,
                "message": str(exc),
            }
            ok = False
            message = str(exc)

        results.append({
            "fixture": fixture_name,
            "commandIndex": index,
            "command": command_name(command),
            "ok": ok,
            "message": message,
            "response": response,
        })

        if not ok:
            saw_failure = True
            skipped_after_failure = True

    add_boundary_diagnostics(args, fixture_name, "after", len(commands) + 100, results)

    fixture_ok = saw_failure if expected_failure else not saw_failure

    if expected_failure and not saw_failure:
        results.append({
            "fixture": fixture_name,
            "commandIndex": len(commands),
            "command": "expectedFailure",
            "ok": False,
            "message": "Expected this fixture to fail, but every command passed",
        })
        fixture_ok = False

    return {
        "name": fixture_name,
        "path": str(fixture_path),
        "ok": fixture_ok,
        "expectedFailure": expected_failure,
        "sawFailure": saw_failure,
        "results": results,
    }


def parse_fixture_spec(spec):
    if "=" not in spec:
        path = Path(spec)
        return path.stem.removeprefix("cycle-agent-"), path

    name, raw_path = spec.split("=", 1)
    return name, Path(raw_path)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Run Cycle smoke fixtures through one automation session.")
    parser.add_argument("socket_path", help="Unix-domain socket path for a running Cycle --agent-session")
    parser.add_argument("fixtures", nargs="+", help="Fixture specs as name=path or plain paths")
    parser.add_argument("--report", required=True, help="Aggregate JSON report path")
    parser.add_argument("--reset-preset", default="ooh-aah", help="Factory preset opened before each fixture; empty disables reset")
    parser.add_argument("--reset-delay-ms", type=int, default=150, help="Idle wait after preset reset before UI normalization")
    parser.add_argument("--skip-main-tab-reset", action="store_true", help="Do not reset MainPanel top/bottom tabs before each fixture")
    parser.add_argument("--expected-failure", action="append", default=[], help="Fixture name expected to contain a failing command")
    parser.add_argument("--resume", action="store_true", help="Keep existing report fixtures and skip names already recorded")
    parser.add_argument("--timeout", type=float, default=30.0, help="Socket command timeout in seconds")
    parser.add_argument("--command-delay", type=float, default=0.0, help="Delay after each fixture in seconds")
    parser.add_argument("--boundary-log", action="store_true", help="Write fixture boundary markers into the app session log")
    parser.add_argument("--opengl-diagnostics-each-fixture", action="store_true", help="Poll OpenGL diagnostics before and after every fixture")
    args = parser.parse_args()

    args.socket_path = str(Path(args.socket_path).expanduser())
    report_path = Path(args.report).expanduser()
    expected_failures = set(args.expected_failure)

    fixtures = []
    completed_names = set()
    suite_ok = True

    if args.resume and report_path.exists():
        with open(report_path, "r", encoding="utf-8") as handle:
            previous = json.load(handle)

        for fixture in previous.get("fixtures", []):
            name = fixture.get("name")

            if name:
                completed_names.add(name)
                fixtures.append(fixture)

                if not fixture.get("ok", False):
                    suite_ok = False

    for spec in args.fixtures:
        fixture_name, fixture_path = parse_fixture_spec(spec)
        fixture_path = fixture_path.expanduser()

        if fixture_name in completed_names:
            print(f"skip {fixture_name}", flush=True)
            continue

        if not fixture_path.exists():
            fixture = {
                "name": fixture_name,
                "path": str(fixture_path),
                "ok": False,
                "expectedFailure": fixture_name in expected_failures,
                "sawFailure": True,
                "results": [{
                    "fixture": fixture_name,
                    "commandIndex": None,
                    "command": "loadFixture",
                    "ok": False,
                    "message": f"Fixture file does not exist: {fixture_path}",
                }],
            }
        else:
            fixture = run_fixture(args, fixture_name, fixture_path, fixture_name in expected_failures)

        fixtures.append(fixture)

        if not fixture["ok"]:
            suite_ok = False

        write_json(report_path, {
            "ok": suite_ok,
            "socket": args.socket_path,
            "fixtures": fixtures,
        })

        print(f"{'ok' if fixture['ok'] else 'FAIL'} {fixture_name}", flush=True)

        if args.command_delay > 0:
            time.sleep(args.command_delay)

    try:
        quit_response = send_command(args.socket_path, {"command": "quit"}, "quit", args.timeout)
    except Exception as exc:
        quit_response = {
            "ok": False,
            "message": str(exc),
        }

    report = {
        "ok": suite_ok,
        "socket": args.socket_path,
        "fixtures": fixtures,
        "quit": quit_response,
    }
    write_json(report_path, report)

    return 0 if suite_ok else 1


if __name__ == "__main__":
    sys.exit(main())
