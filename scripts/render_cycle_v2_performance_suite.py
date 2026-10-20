#!/usr/bin/env python3
"""Render a bounded performance suite through Cycle's exact-instance coordinator."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time
import wave

from cycle_automation_support.identity import process_identity
from cycle_automation_support.session import command
from cycle_performance_suite import content_hash, event, load_suite


SCRIPT_DIR = Path(__file__).resolve().parent


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def wav_identity(path, performance):
    digest = hashlib.sha256()
    with wave.open(str(path), "rb") as source:
        frames = source.getnframes()
        rate, channels = source.getframerate(), source.getnchannels()
        width = source.getsampwidth()
        expected = math.floor(performance["durationMs"] * performance["sampleRate"] / 1000 + 0.5)
        if (frames != expected or rate != performance["sampleRate"]
                or channels != performance["channels"] or width != 3):
            raise RuntimeError(f"WAV dimensions disagree with requested capture: {path}")
        byte_count = 0
        for block in iter(lambda: source.readframes(65536), b""):
            byte_count += len(block)
            digest.update(block)
        if byte_count != frames * channels * width:
            raise RuntimeError(f"WAV capture is truncated: {path}")
    return {"wavSha256": file_hash(path), "pcmSha256": digest.hexdigest(),
            "frames": frames, "sampleRate": rate, "channels": channels, "sampleWidth": width}


def write_results(output, result):
    temporary = output / "results.json.tmp"
    temporary.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(output / "results.json")


def render_suite(plan, preset, output, send, engine, clock=time.monotonic):
    """Schedule existing commands; `send` is the canonical transport in production."""
    preset_bytes = preset.read_bytes()
    output.mkdir(parents=True, exist_ok=False)
    snapshot = output / "source-preset.cyclegraph"
    snapshot.write_bytes(preset_bytes)
    result = {
        "schema": "cycle-performance-results.v1", "ok": False, "status": "running",
        "startedUtc": datetime.now(timezone.utc).isoformat(),
        "suite": plan, "suiteSha256": content_hash(plan["source"]["document"]),
        "preset": {"sourcePath": str(preset), "snapshot": snapshot.name,
                   "sourceSha256": hashlib.sha256(preset_bytes).hexdigest()},
        "engine": engine, "cases": [],
        "resetPolicy": "fresh OfflineGraphAudioRenderer, MIDI queue, voices and processors per capture",
    }
    started = clock()
    deadline = started + plan["budget"]["maxWallSeconds"]
    write_results(output, result)

    def invoke(payload):
        remaining = deadline - clock()
        if remaining <= 0:
            raise TimeoutError("Performance suite wall budget exhausted; no further commands dispatched")
        try:
            response = send(payload, timeout=remaining, require_ok=False)
        except Exception as error:
            with (output / "commands.jsonl").open("a", encoding="utf-8") as log:
                log.write(json.dumps({"command": payload, "error": str(error)}) + "\n")
            raise
        with (output / "commands.jsonl").open("a", encoding="utf-8") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError(f"Automation command failed: {response}")
        if clock() > deadline:
            raise TimeoutError("Performance suite wall budget exhausted during command")
        return response["result"].get("data", {})

    def export_graph(filename):
        path = output / filename
        invoke({"command": "exportGraph", "path": str(path)})
        return content_hash(json.loads(path.read_bytes()))

    try:
        for case in plan["cases"]:
            pattern = case["pattern"]
            if pattern is None:
                continue
            schedule = invoke({"command": "exportPatternSchedule",
                               "sequence": pattern["document"]["sequence"],
                               "channel": pattern["channel"]})
            events = schedule.get("events")
            if not isinstance(events, list) or len(events) > 2048:
                raise RuntimeError("Pattern schedule must contain at most 2048 events")
            performance = case["performance"]
            performance["events"] = [event(item, performance["durationMs"]) for item in events]
            case["performanceSha256"] = content_hash(performance)
            pattern["scheduler"] = schedule.get("scheduler")
        write_results(output, result)
        invoke({"command": "openGraph", "path": str(snapshot), "includeSnapshot": False})
        graph_hash = export_graph("effective-preset.cyclegraph")
        result["preset"]["effectiveGraphSha256"] = graph_hash
        write_results(output, result)
        # Interleave cases between repeats to expose state leaking from a different performance.
        entries = {}
        for repetition in range(1, plan["repetitions"] + 1):
            for case in plan["cases"]:
                if case["id"] not in entries:
                    entry = {"id": case["id"], "labels": case["labels"],
                             "performanceSha256": case["performanceSha256"], "renders": [],
                             "repeatabilityVerified": False}
                    entries[case["id"]] = entry
                    result["cases"].append(entry)
                entry = entries[case["id"]]
                name = f"{case['id']}-{repetition:02d}.wav"
                performance = case["performance"]
                capture = invoke({"command": "captureAudio", **performance, "path": str(output / name)})
                rendered = {"repetition": repetition, "path": name, "capture": capture,
                            **wav_identity(output / name, performance)}
                entry["renders"].append(rendered)
                if rendered["pcmSha256"] != entry["renders"][0]["pcmSha256"]:
                    entry["repeatabilityVerified"] = False
                    raise RuntimeError(f"Nonrepeatable PCM in {case['id']}, repetition {repetition}")
                entry["repeatabilityVerified"] = repetition > 1
                write_results(output, result)
        after_hash = export_graph("verification-preset.cyclegraph")
        result["preset"]["finalGraphSha256"] = after_hash
        if after_hash != graph_hash:
            raise RuntimeError("The graph changed during the performance suite")
        (output / "verification-preset.cyclegraph").unlink()
        result.update(ok=True, status="complete")
    except Exception as error:
        result.update(status="failed", error=str(error))
    finally:
        result["elapsedSeconds"] = clock() - started
        write_results(output, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("suite", type=Path)
    parser.add_argument("--preset", type=Path, required=True, help="Saved Cycle V2 graph to render")
    parser.add_argument("--output-dir", type=Path, help="New artifact directory (never overwritten)")
    parser.add_argument("--repeat", type=int, help="Override suite repetitions, 1 to 16")
    parser.add_argument("--plan", action="store_true", help="Validate and print expanded schedule without starting the app")
    parser.add_argument("--no-build", action="store_true", help="Use an already built app through the coordinator")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        plan = load_suite(args.suite, args.repeat)
        preset = args.preset.resolve(strict=True)
        if not preset.is_file():
            raise ValueError(f"Preset is not a file: {preset}")
        if args.plan:
            print(json.dumps({"suite": plan, "presetSha256": file_hash(preset)}, indent=2))
            return 0
        if args.output_dir is None:
            raise ValueError("--output-dir is required unless --plan is used")
        output = args.output_dir.resolve()
        if output.exists():
            raise ValueError(f"Output directory already exists: {output}")
        if not args.in_session:
            coordinator = [sys.executable, str(SCRIPT_DIR / "cycle_automation.py"),
                           "exec", "--product", "cycle-v2"]
            if args.no_build:
                coordinator.append("--no-build")
            worker = [sys.executable, str(Path(__file__).resolve()), str(args.suite.resolve()),
                      "--preset", str(preset), "--output-dir", str(output), "--in-session"]
            if args.repeat is not None:
                worker.extend(["--repeat", str(args.repeat)])
            return subprocess.run(coordinator + ["--"] + worker, cwd=SCRIPT_DIR.parent,
                                  check=False).returncode
        required = ("CYCLE_AGENT_SESSION", "CYCLE_APP_PID", "CYCLE_AUTOMATION_ARTIFACTS")
        if not all(os.environ.get(key) for key in required):
            raise ValueError("--in-session requires the environment supplied by cycle_automation.py exec")
        pid = int(os.environ["CYCLE_APP_PID"])
        identity = process_identity(pid)
        if identity is None:
            raise RuntimeError(f"Coordinated app PID {pid} is no longer running")
        engine = {"pid": pid, "processIdentity": identity,
                  "executableSha256": file_hash(Path(identity["executable"])),
                  "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"]}
        socket_path = os.environ["CYCLE_AGENT_SESSION"]
        result = render_suite(plan, preset, output,
                              lambda payload, **kwargs: command(socket_path, payload, **kwargs), engine)
        print(f"{'PASS' if result['ok'] else 'FAIL'}: {output / 'results.json'}")
        if not result["ok"]:
            print(result["error"], file=sys.stderr)
        return 0 if result["ok"] else 1
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Performance suite: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
