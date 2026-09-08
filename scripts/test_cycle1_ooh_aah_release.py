#!/usr/bin/env python3

"""Render fresh Cycle 1 OohAah notes and validate both release boundaries."""

import argparse
import json
import subprocess
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def write_automation(path, wav_path, note_length_ms, sample_rate, block_size):
    capture_duration_ms = note_length_ms + 500
    document = {
        "commands": [
            {
                "command": "openFactoryPreset",
                "preset": "ooh-aah",
                "waitForIdle": True,
                "idleDelayMs": 300,
            },
            {
                "command": "action",
                "actionType": "Disable",
                "area": "AreaDelay",
            },
            {
                "command": "setControl",
                "area": "AreaMasterCtrls",
                "target": "TargMasterVol",
                "value": 0.1,
            },
            {
                "command": "captureAudio",
                "path": str(wav_path),
                "durationMs": capture_duration_ms,
                "sampleRate": sample_rate,
                "blockSize": block_size,
                "channels": 2,
                "events": [
                    {
                        "type": "noteOn",
                        "timeMs": 0,
                        "note": 60,
                        "velocity": 0.8,
                        "channel": 1,
                    },
                    {
                        "type": "noteOff",
                        "timeMs": note_length_ms,
                        "note": 60,
                        "channel": 1,
                    },
                ],
                "peakGreaterThan": 0.01,
                "final50MsRmsLessThan": 0.000001,
            },
        ],
        "quit": True,
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def render_note(output_directory, note_length_ms, sample_rate, block_size):
    case_directory = output_directory / f"note-{note_length_ms}ms"
    case_directory.mkdir(parents=True, exist_ok=True)
    automation_path = case_directory / "automation.json"
    report_path = case_directory / "report.json"
    log_path = case_directory / "cycle.log"
    wav_path = case_directory / "render.wav"
    write_automation(
        automation_path,
        wav_path,
        note_length_ms,
        sample_rate,
        block_size,
    )
    subprocess.run(
        [
            str(SCRIPT_DIR / "run_cycle_agent.sh"),
            str(automation_path),
            str(report_path),
            str(log_path),
        ],
        cwd=REPO_ROOT,
        check=True,
    )
    with report_path.open(encoding="utf-8") as source:
        report = json.load(source)
    failed_commands = [result for result in report["results"] if not result["ok"]]
    if failed_commands:
        raise RuntimeError(f"Cycle automation failed: {failed_commands[-1]['message']}")
    captures = [
        result["data"]
        for result in report["results"]
        if result.get("data", {}).get("path") == str(wav_path)
    ]
    if len(captures) != 1:
        raise RuntimeError(f"Expected one audio capture in {report_path}")
    return captures[0]


def parse_note_lengths(value):
    lengths = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not lengths or any(length <= 0 for length in lengths):
        raise argparse.ArgumentTypeError("note lengths must be positive comma-separated milliseconds")
    return lengths


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-ooh-aah-release"),
    )
    parser.add_argument("--note-lengths", type=parse_note_lengths, default=[50, 150, 400, 800])
    parser.add_argument("--sample-rate", type=int, default=44100)
    parser.add_argument("--block-size", type=int, default=512)
    parser.add_argument("--note-off-limit", type=float, default=0.002)
    parser.add_argument("--terminal-limit", type=float, default=0.0001)
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    cases = []
    for note_length_ms in arguments.note_lengths:
        print(f"Rendering a fresh {note_length_ms} ms OohAah note...", flush=True)
        metrics = render_note(
            arguments.output_dir,
            note_length_ms,
            arguments.sample_rate,
            arguments.block_size,
        )
        case = {
            "noteLengthMs": note_length_ms,
            "maxNoteOffSecondDifference": metrics["maxNoteOffSecondDifference"],
            "terminalDelta": metrics["terminalDelta"],
            "peak": metrics["peak"],
            "passed": (
                metrics["maxNoteOffSecondDifference"] < arguments.note_off_limit
                and metrics["terminalDelta"] < arguments.terminal_limit
            ),
        }
        cases.append(case)
        print(
            f"  note-off={case['maxNoteOffSecondDifference']:.8f} "
            f"terminal={case['terminalDelta']:.8f} peak={case['peak']:.6f}",
            flush=True,
        )

    summary = {
        "schema": "cycle-v1-ooh-aah-release.v1",
        "sampleRate": arguments.sample_rate,
        "blockSize": arguments.block_size,
        "noteOffLimit": arguments.note_off_limit,
        "terminalLimit": arguments.terminal_limit,
        "cases": cases,
        "passed": all(case["passed"] for case in cases),
    }
    summary_path = arguments.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Summary: {summary_path}")
    if not summary["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
