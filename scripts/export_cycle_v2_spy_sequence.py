#!/usr/bin/env python3
"""Export deterministic, plot-only Cyclogram PNGs using a preview pattern's controllers."""

import argparse
from datetime import datetime, timezone
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys

from cycle_automation_support.session import Session


SCRIPT_DIR = Path(__file__).resolve().parent


def write_manifest(output, manifest):
    temporary = output / "manifest.json.tmp"
    temporary.write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(output / "manifest.json")


def source_snapshot(path, destination):
    content = path.read_bytes()
    destination.write_bytes(content)
    return {"path": str(path), "snapshot": destination.name,
            "sha256": hashlib.sha256(content).hexdigest()}


def frame_states(schedule, fps):
    """Consume the app's scheduled events; never interpolate controller curves here."""
    duration = Fraction(str(schedule["durationMs"])) / 1000
    rate = Fraction(str(fps))
    if duration <= 0 or rate <= 0:
        raise ValueError("Pattern duration and frame rate must be positive")
    events = schedule["events"]
    cursor = 0
    values = [0] * 128
    for index in range(math.ceil(duration * rate)):
        time_seconds = Fraction(index) / rate
        while cursor < len(events) and Fraction(str(events[cursor]["timeMs"])) <= time_seconds * 1000:
            event = events[cursor]
            if event["type"] == "controller":
                values[event["controller"]] = event["value"]
            cursor += 1
        yield index, float(time_seconds), values.copy()


def export_sequence(args, send):
    output = args.output_dir
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"status": "running", "startedUtc": datetime.now(timezone.utc).isoformat(),
                "spy": args.spy, "view": "time", "fps": args.fps,
                "width": args.width, "height": args.height, "frames": [],
                "framesFile": "frames.jsonl", "completedFrames": 0}
    failed_frame = None
    interrupted = False
    try:
        write_manifest(output, manifest)
        manifest["preset"] = source_snapshot(args.preset, output / "source-preset.cyclegraph")
        manifest["pattern"] = source_snapshot(args.pattern, output / "source-pattern.cyclepattern")
        pattern = json.loads((output / "source-pattern.cyclepattern").read_bytes())
        if not isinstance(pattern, dict) or pattern.get("version") != 1:
            raise ValueError("Expected a version 1 .cyclepattern document")
        schedule = send({"command": "exportPatternSchedule", "sequence": pattern.get("sequence")})
        if not any(event["type"] == "controller" for event in schedule["events"]):
            raise ValueError("The pattern has no controller automation")
        manifest["durationSeconds"] = schedule["durationMs"] / 1000
        manifest["scheduler"] = schedule["scheduler"]
        send({"command": "openGraph", "path": str(output / "source-preset.cyclegraph"),
              "includeSnapshot": False})
        with (output / "frames.jsonl").open("w", encoding="utf-8") as journal:
            for index, timestamp, values in frame_states(schedule, args.fps):
                failed_frame = index
                filename = f"frame-{index:06d}.png"
                payload = {"command": "renderSpyFrame", "probeId": args.spy,
                           "controllers": values, "width": args.width, "height": args.height,
                           "path": str(output / filename)}
                if args.note is not None:
                    payload["midiNote"] = args.note
                if args.gradient is not None:
                    payload["gradient"] = args.gradient
                rendered = send(payload)
                manifest["gradient"] = rendered["gradient"]
                manifest["midiNote"] = rendered["midiNote"]
                manifest["noteOffTime"] = rendered["noteOffTime"]
                frame = {"index": index, "timeSeconds": timestamp, "path": filename,
                         "controllers": values, "contentRevision": rendered["contentRevision"]}
                journal.write(json.dumps(frame, allow_nan=False) + "\n")
                journal.flush()
                manifest["frames"].append(frame)
                manifest["completedFrames"] += 1
                if index % max(1, round(args.fps)) == 0:
                    print(f"Exported {index + 1} frames ({timestamp:.3f}s)", flush=True)
        manifest["status"] = "complete"
    except (Exception, KeyboardInterrupt) as error:
        interrupted = isinstance(error, KeyboardInterrupt)
        manifest.update(status="interrupted" if interrupted else "failed",
                        error=str(error) or "Export interrupted", failedFrame=failed_frame)
    finally:
        write_manifest(output, manifest)
    print(f"{manifest['status']}: {output / 'manifest.json'}", flush=True)
    if manifest["status"] != "complete":
        print(manifest["error"], file=sys.stderr)
    return 130 if interrupted else (0 if manifest["status"] == "complete" else 1)


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", type=Path, help="Saved .cyclegraph file")
    parser.add_argument("--pattern", type=Path, help=".cyclepattern file containing controller automation")
    parser.add_argument("--output-dir", type=Path, help="New directory; existing directories are never overwritten")
    parser.add_argument("--spy", default="default-output", help="Spy ID (default: default-output)")
    parser.add_argument("--fps", type=float, default=30, help="Frames per second (default: 30)")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--note", type=int, help="Fixed MIDI note (default: preset preview note)")
    parser.add_argument("--gradient", help="Built-in gradient ID or display name (default: preset style)")
    parser.add_argument("--list-gradients", action="store_true", help="List built-in IDs, names and groups")
    parser.add_argument("--no-build", action="store_true", help="Use an already built app")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if not math.isfinite(args.fps) or args.fps <= 0:
        parser.error("--fps must be finite and positive")
    if (not 1 <= args.width <= 16384 or not 1 <= args.height <= 16384
            or args.width * args.height > 64 * 1024 * 1024):
        parser.error("Image dimensions must be in [1, 16384] and at most 64 megapixels")
    if args.note is not None and not 0 <= args.note <= 127:
        parser.error("--note must be in [0, 127]")
    if not args.spy:
        parser.error("--spy must be nonempty")
    if not args.list_gradients:
        if args.preset is None or args.pattern is None or args.output_dir is None:
            parser.error("--preset, --pattern and --output-dir are required for export")
        args.preset = args.preset.resolve(strict=True)
        args.pattern = args.pattern.resolve(strict=True)
        if not args.preset.is_file() or not args.pattern.is_file():
            parser.error("--preset and --pattern must be files")
        args.output_dir = args.output_dir.resolve()
        if args.output_dir.exists():
            parser.error(f"Output directory already exists: {args.output_dir}")
    return args


def main(argv=None):
    args = arguments(argv)
    if not args.in_session:
        coordinator = [sys.executable, str(SCRIPT_DIR / "cycle_automation.py"),
                       "exec", "--product", "cycle-v2", "--clean"]
        if args.no_build:
            coordinator.append("--no-build")
        worker = [sys.executable, str(Path(__file__).resolve()), "--in-session"]
        if args.list_gradients:
            worker.append("--list-gradients")
        else:
            worker.extend(["--preset", str(args.preset), "--pattern", str(args.pattern),
                           "--output-dir", str(args.output_dir), "--spy", args.spy,
                           "--fps", str(args.fps), "--width", str(args.width), "--height", str(args.height)])
            if args.note is not None:
                worker.extend(["--note", str(args.note)])
            if args.gradient is not None:
                worker.extend(["--gradient", args.gradient])
        return subprocess.run(coordinator + ["--"] + worker, cwd=SCRIPT_DIR.parent,
                              check=False).returncode
    if not os.environ.get("CYCLE_AGENT_SESSION") or os.environ.get("CYCLE_AUTOMATION_SCOPED") != "1":
        raise ValueError("The export worker requires an isolated coordinator session")
    with Session(os.environ["CYCLE_AGENT_SESSION"], timeout=120) as session:
        def send(payload):
            return session.command(payload)["result"].get("data", {})

        if args.list_gradients:
            for entry in send({"command": "listSpyGradients"}):
                print(f"{entry['id']:20} {entry['label']:32} {entry['group']}")
            return 0
        return export_sequence(args, send)


if __name__ == "__main__":
    def terminate(_signum, _frame):
        raise KeyboardInterrupt()

    signal.signal(signal.SIGTERM, terminate)
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Spy export: {error}", file=sys.stderr)
        sys.exit(1)
