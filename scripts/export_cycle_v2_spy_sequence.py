#!/usr/bin/env python3
"""Export automated Cyclogram PNGs, synth audio, and optional melody movies."""

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


def frame_states(schedule, fps, duration_seconds=None):
    """Consume the app's scheduled events; never interpolate controller curves here."""
    duration = (Fraction(str(schedule["durationMs"])) / 1000 if duration_seconds is None
                else Fraction(str(duration_seconds)))
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


def encode_mp4(output, fps, frame_count, bitrate_mbps=8):
    temporary = output / "sequence.partial.mp4"
    audio = output / "audio.wav"
    command = ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-n",
               "-framerate", str(Fraction(str(fps))), "-start_number", "0",
               "-i", str(output / "frame-%06d.png")]
    if audio.is_file():
        command.extend(["-i", str(audio), "-map", "0:v:0", "-map", "1:a:0",
                        "-c:a", "aac", "-b:a", "320k"])
    command.extend(["-t", str(float(Fraction(frame_count) / Fraction(str(fps)))),
               "-c:v", "libx264", "-b:v", str(round(bitrate_mbps * 1_000_000)), "-preset", "slow",
               "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", "-pix_fmt", "yuv420p",
               "-movflags", "+faststart", str(temporary)])
    try:
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, text=True)
        temporary.replace(output / "sequence.mp4")
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"ffmpeg encoding failed: {error.stderr.strip()}") from error
    finally:
        temporary.unlink(missing_ok=True)


def capture_pattern_audio(output, schedule, frame_count, fps, reference_note, cursor, send):
    sample_rate = 48000
    rate = Fraction(str(fps))
    sample_count = round(Fraction(frame_count * sample_rate) / rate)
    duration_ms = 1000 * sample_count / sample_rate
    if duration_ms > 120000:
        raise ValueError("Pattern plus tail exceeds the 120-second offline audio limit")
    samples = [round(Fraction(index * sample_rate) / rate) for index in range(frame_count)]
    request = {"command": "captureAudio", "path": str(output / "audio.wav"),
               "sampleRate": sample_rate, "channels": 2, "blockSize": 512,
               "voiceDurationSeconds": 0, "keyScaleNote": reference_note,
               "randomSeed": 1729, "durationMs": duration_ms,
               "events": [event for event in schedule["events"] if event["timeMs"] < duration_ms]}
    if cursor != "none":
        request["playbackSamples"] = samples
    print("Rendering synth audio and voice playback snapshots", flush=True)
    capture = send(request)
    playback = capture.pop("playbackFrames", [])
    if cursor != "none" and ([frame["sample"] for frame in playback] != samples):
        raise RuntimeError("Audio capture did not return all requested playback samples")
    return {**capture, "path": "audio.wav", "status": "complete"}, playback


def export_sequence(args, send):
    output = args.output_dir
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"status": "running", "startedUtc": datetime.now(timezone.utc).isoformat(),
                "spy": args.spy, "view": "time", "fps": args.fps,
                "width": args.width, "height": args.height, "frames": [],
                "framesFile": "frames.jsonl", "completedFrames": 0}
    failed_frame = None
    interrupted = False
    if args.mp4:
        manifest["mp4"] = {"path": "sequence.mp4", "status": "pending", "codec": "h264",
                           "targetBitrateMbps": args.mp4_bitrate,
                           "width": args.width + args.width % 2,
                           "height": args.height + args.height % 2}
    try:
        write_manifest(output, manifest)
        manifest["preset"] = source_snapshot(args.preset, output / "source-preset.cyclegraph")
        manifest["pattern"] = source_snapshot(args.pattern, output / "source-pattern.cyclepattern")
        pattern = json.loads((output / "source-pattern.cyclepattern").read_bytes())
        if not isinstance(pattern, dict) or pattern.get("version") != 1:
            raise ValueError("Expected a version 1 .cyclepattern document")
        schedule = send({"command": "exportPatternSchedule", "sequence": pattern.get("sequence")})
        notes = [event for event in schedule["events"] if event["type"] == "noteOn"]
        if not notes and not any(event["type"] == "controller" for event in schedule["events"]):
            raise ValueError("The pattern has no notes or controller automation")
        reference_note = args.note if args.note is not None else (notes[0]["note"] if notes else None)
        duration = Fraction(str(schedule["durationMs"])) / 1000 + Fraction(str(args.tail_seconds))
        states = list(frame_states(schedule, args.fps, duration))
        manifest["durationSeconds"] = float(duration)
        manifest["cursor"] = args.cursor
        manifest["keyScaleNote"] = reference_note
        manifest["tailSeconds"] = args.tail_seconds
        manifest["scheduler"] = schedule["scheduler"]
        send({"command": "openGraph", "path": str(output / "source-preset.cyclegraph"),
              "includeSnapshot": False})
        playback = []
        if notes:
            manifest["audio"], playback = capture_pattern_audio(
                    output, schedule, len(states), args.fps, reference_note, args.cursor, send)
            write_manifest(output, manifest)
        with (output / "frames.jsonl").open("w", encoding="utf-8") as journal:
            for index, timestamp, values in states:
                failed_frame = index
                filename = f"frame-{index:06d}.png"
                payload = {"command": "renderSpyFrame", "probeId": args.spy,
                           "controllers": values, "width": args.width, "height": args.height,
                           "path": str(output / filename)}
                if reference_note is not None:
                    payload["midiNote"] = reference_note
                voices = playback[index]["voices"] if playback else []
                if voices:
                    payload["playbackVoices"] = voices
                if args.gradient is not None:
                    payload["gradient"] = args.gradient
                rendered = send(payload)
                manifest["gradient"] = rendered["gradient"]
                manifest["midiNote"] = rendered["midiNote"]
                manifest["noteOffTime"] = rendered["noteOffTime"]
                frame = {"index": index, "timeSeconds": timestamp, "path": filename,
                         "controllers": values, "contentRevision": rendered["contentRevision"],
                         "voices": voices, "playbackPositions": rendered.get("playbackPositions", [])}
                journal.write(json.dumps(frame, allow_nan=False) + "\n")
                journal.flush()
                manifest["frames"].append(frame)
                manifest["completedFrames"] += 1
                if index % max(1, round(args.fps)) == 0:
                    print(f"Exported {index + 1} frames ({timestamp:.3f}s)", flush=True)
        failed_frame = None
        if args.mp4:
            manifest["mp4"]["status"] = "encoding"
            write_manifest(output, manifest)
            print("Encoding sequence.mp4 with ffmpeg", flush=True)
            encode_mp4(output, args.fps, manifest["completedFrames"], args.mp4_bitrate)
            manifest["mp4"]["status"] = "complete"
        manifest["status"] = "complete"
    except (Exception, KeyboardInterrupt) as error:
        interrupted = isinstance(error, KeyboardInterrupt)
        manifest.update(status="interrupted" if interrupted else "failed",
                        error=str(error) or "Export interrupted", failedFrame=failed_frame)
        if args.mp4:
            manifest["mp4"]["status"] = manifest["status"]
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
    parser.add_argument("--note", type=int,
                        help="Fixed key-scale/preview note; melody pitches stay authored (default: first pattern note)")
    parser.add_argument("--cursor", choices=["ripple", "none"], default="ripple")
    parser.add_argument("--tail-seconds", type=float, default=0,
                        help="Extra audio/video time after the pattern (default: 0)")
    parser.add_argument("--gradient", help="Built-in gradient ID or display name (default: preset style)")
    parser.add_argument("--mp4", action="store_true", help="Also encode sequence.mp4 with ffmpeg (H.264); keep PNGs")
    parser.add_argument("--mp4-bitrate", type=float, default=8, metavar="MBPS",
                        help="Target MP4 bitrate in megabits per second (default: 8)")
    parser.add_argument("--list-gradients", action="store_true", help="List built-in IDs, names and groups")
    parser.add_argument("--no-build", action="store_true", help="Use an already built app")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if not math.isfinite(args.fps) or args.fps <= 0:
        parser.error("--fps must be finite and positive")
    if not math.isfinite(args.mp4_bitrate) or args.mp4_bitrate <= 0:
        parser.error("--mp4-bitrate must be finite and positive (megabits per second)")
    if (not 1 <= args.width <= 16384 or not 1 <= args.height <= 16384
            or args.width * args.height > 64 * 1024 * 1024):
        parser.error("Image dimensions must be in [1, 16384] and at most 64 megapixels")
    if args.note is not None and not 0 <= args.note <= 127:
        parser.error("--note must be in [0, 127]")
    if not math.isfinite(args.tail_seconds) or args.tail_seconds < 0:
        parser.error("--tail-seconds must be finite and nonnegative")
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
                           "--fps", str(args.fps), "--width", str(args.width), "--height", str(args.height),
                           "--cursor", args.cursor, "--tail-seconds", str(args.tail_seconds)])
            if args.note is not None:
                worker.extend(["--note", str(args.note)])
            if args.gradient is not None:
                worker.extend(["--gradient", args.gradient])
            if args.mp4:
                worker.extend(["--mp4", "--mp4-bitrate", str(args.mp4_bitrate)])
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
