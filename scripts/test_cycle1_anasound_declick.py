#!/usr/bin/env python3

"""Validate Anasound's no-release volume envelope through its declick tail."""

import argparse
import json
from pathlib import Path

from cycle_audio_diff import read_wav
from test_cycle1_spectral_phase import capture_command, run_automation


def parse_integer_list(value):
    values = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive comma-separated integers")
    return values


def render_note(output_directory, note_length_ms, sample_rate, block_size):
    case_directory = output_directory / f"note-{note_length_ms}ms"
    wav_path = case_directory / "render.wav"
    capture = capture_command(wav_path, sample_rate)
    capture["durationMs"] = note_length_ms + 500
    capture["blockSize"] = block_size
    capture["events"][1]["timeMs"] = note_length_ms
    commands = [
        {
            "command": "openFactoryPreset",
            "preset": "anasound",
            "waitForIdle": True,
            "idleDelayMs": 300,
        },
        capture,
    ]
    run_automation(case_directory, commands)

    with (case_directory / "report.json").open(encoding="utf-8") as source:
        report = json.load(source)
    metrics = next(
        result["data"]
        for result in report["results"]
        if result.get("data", {}).get("path") == str(wav_path)
    )
    wave = read_wav(wav_path)
    note_off_sample = round(note_length_ms * sample_rate / 1000)
    window_start = note_off_sample + round(0.002 * sample_rate)
    window_end = note_off_sample + round(0.008 * sample_rate)
    tail = [
        sample
        for channel in wave["channels"]
        for sample in channel[window_start:window_end]
    ]
    metrics["middleDeclickRms"] = (
        sum(sample * sample for sample in tail) / max(1, len(tail))
    ) ** 0.5
    significant_tail = [
        sample_index - note_off_sample
        for channel in wave["channels"]
        for sample_index in range(note_off_sample, window_end + round(0.003 * sample_rate))
        if abs(channel[sample_index]) > 0.0000001
    ]
    metrics["declickDurationMs"] = (
        max(significant_tail, default=0) * 1000.0 / sample_rate
    )
    return metrics


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-anasound-declick"),
    )
    parser.add_argument(
        "--note-lengths",
        type=parse_integer_list,
        default=[50, 150, 400, 800],
    )
    parser.add_argument("--sample-rate", type=int, default=48000)
    parser.add_argument("--block-size", type=int, default=512)
    parser.add_argument("--minimum-declick-ms", type=float, default=9.5)
    parser.add_argument("--tail-rms-minimum", type=float, default=0.0001)
    parser.add_argument("--terminal-limit", type=float, default=0.0001)
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    cases = []
    for note_length_ms in arguments.note_lengths:
        print(f"Rendering a {note_length_ms} ms Anasound note...", flush=True)
        metrics = render_note(
            arguments.output_dir,
            note_length_ms,
            arguments.sample_rate,
            arguments.block_size,
        )
        case = {
            "noteLengthMs": note_length_ms,
            "declickDurationMs": metrics["declickDurationMs"],
            "middleDeclickRms": metrics["middleDeclickRms"],
            "terminalDelta": metrics["terminalDelta"],
            "peak": metrics["peak"],
        }
        case["passed"] = (
            case["declickDurationMs"] > arguments.minimum_declick_ms
            and case["middleDeclickRms"] > arguments.tail_rms_minimum
            and case["terminalDelta"] < arguments.terminal_limit
        )
        cases.append(case)
        print(
            f"  duration={case['declickDurationMs']:.3f}ms "
            f"tail-rms={case['middleDeclickRms']:.8f} "
            f"terminal={case['terminalDelta']:.8f} peak={case['peak']:.6f}",
            flush=True,
        )

    summary = {
        "schema": "cycle-v1-anasound-declick.v1",
        "sampleRate": arguments.sample_rate,
        "blockSize": arguments.block_size,
        "minimumDeclickMs": arguments.minimum_declick_ms,
        "tailRmsMinimum": arguments.tail_rms_minimum,
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
