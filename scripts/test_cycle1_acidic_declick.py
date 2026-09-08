#!/usr/bin/env python3

"""Validate Acidic note-off continuity with its authored declick enabled."""

import argparse
import json
from pathlib import Path

from test_cycle1_spectral_phase import capture_command, preset_setup, run_automation


def parse_integer_list(value):
    values = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive comma-separated integers")
    return values


def render_note(output_directory, note_length_ms, sample_rate):
    case_directory = output_directory / f"note-{note_length_ms}ms"
    wav_path = case_directory / "render.wav"
    commands = preset_setup("acidic")
    commands[-1]["value"] = 0.1
    capture = capture_command(wav_path, sample_rate)
    capture["durationMs"] = note_length_ms + 500
    capture["events"][1]["timeMs"] = note_length_ms
    commands.append(capture)
    run_automation(case_directory, commands)

    with (case_directory / "report.json").open(encoding="utf-8") as source:
        report = json.load(source)
    captures = [
        result["data"]
        for result in report["results"]
        if result.get("data", {}).get("path") == str(wav_path)
    ]
    if len(captures) != 1:
        raise RuntimeError(f"Expected one audio capture in {case_directory}")
    return captures[0]


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-acidic-declick"),
    )
    parser.add_argument(
        "--note-lengths",
        type=parse_integer_list,
        default=[50, 150, 400, 800],
    )
    parser.add_argument("--sample-rate", type=int, default=48000)
    parser.add_argument("--note-off-limit", type=float, default=0.002)
    parser.add_argument("--terminal-limit", type=float, default=0.0001)
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    cases = []
    for note_length_ms in arguments.note_lengths:
        print(f"Rendering a {note_length_ms} ms Acidic note...", flush=True)
        metrics = render_note(arguments.output_dir, note_length_ms, arguments.sample_rate)
        case = {
            "noteLengthMs": note_length_ms,
            "maxNoteOffSecondDifference": metrics["maxNoteOffSecondDifference"],
            "terminalDelta": metrics["terminalDelta"],
            "peak": metrics["peak"],
        }
        case["passed"] = (
            case["maxNoteOffSecondDifference"] < arguments.note_off_limit
            and case["terminalDelta"] < arguments.terminal_limit
        )
        cases.append(case)
        print(
            f"  note-off={case['maxNoteOffSecondDifference']:.8f} "
            f"terminal={case['terminalDelta']:.8f} peak={case['peak']:.6f}",
            flush=True,
        )

    summary = {
        "schema": "cycle-v1-acidic-declick.v1",
        "sampleRate": arguments.sample_rate,
        "noteOffLimit": arguments.note_off_limit,
        "terminalLimit": arguments.terminal_limit,
        "cases": cases,
        "passed": all(case["passed"] for case in cases),
    }
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = arguments.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Summary: {summary_path}")
    if not summary["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
