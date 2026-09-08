#!/usr/bin/env python3

"""Validate Cycle 1 reverb routing with a short dry/wet note render."""

import argparse
import json
import math
import subprocess
from pathlib import Path

from cycle_audio_diff import read_wav


REPO_ROOT = Path(__file__).resolve().parents[1]
FIXTURE = REPO_ROOT / "scripts/fixtures/cycle-agent-cycle1-reverb-tail.json"


def window_rms(wave, start_ms, end_ms):
    sample_rate = wave["sampleRate"]
    start = round(start_ms * sample_rate / 1000)
    end = round(end_ms * sample_rate / 1000)
    values = [sample for channel in wave["channels"] for sample in channel[start:end]]
    return math.sqrt(sum(sample * sample for sample in values) / max(1, len(values)))


def parse_block_sizes(value):
    sizes = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not sizes or any(size <= 0 for size in sizes):
        raise argparse.ArgumentTypeError("block sizes must be positive integers")
    return sizes


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-reverb-tail"),
    )
    parser.add_argument(
        "--block-sizes",
        type=parse_block_sizes,
        default=[128, 512, 1024],
    )
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def render_case(output_directory, block_size):
    output_directory.mkdir(parents=True, exist_ok=True)
    dry_path = output_directory / "dry.wav"
    wet_path = output_directory / "wet.wav"
    report_path = output_directory / "report.json"
    log_path = output_directory / "logs.txt"
    fixture_path = output_directory / "fixture.json"
    fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))
    captures = [
        command
        for command in fixture["commands"]
        if command.get("command") == "captureAudio"
    ]
    captures[0]["path"] = str(dry_path)
    captures[1]["path"] = str(wet_path)
    captures[0]["blockSize"] = block_size
    captures[1]["blockSize"] = block_size
    fixture_path.write_text(json.dumps(fixture, indent=2) + "\n", encoding="utf-8")

    subprocess.run(
        [
            str(REPO_ROOT / "scripts/run_cycle_agent.sh"),
            str(fixture_path),
            str(report_path),
            str(log_path),
        ],
        cwd=REPO_ROOT,
        check=True,
    )

    dry = read_wav(dry_path)
    wet = read_wav(wet_path)
    dry_tail_rms = window_rms(dry, 110, 200)
    wet_tail_rms = window_rms(wet, 110, 200)
    wet_late_tail_rms = window_rms(wet, 160, 200)
    return {
        "blockSize": block_size,
        "dryTailRms": dry_tail_rms,
        "wetTailRms": wet_tail_rms,
        "wetLateTailRms": wet_late_tail_rms,
        "passed": dry_tail_rms < 1.0e-7
        and wet_tail_rms > 1.0e-3
        and wet_late_tail_rms > 1.0e-4,
    }


def main():
    arguments = parse_arguments()
    cases = []
    for block_size in arguments.block_sizes:
        case = render_case(arguments.output_dir / f"block-{block_size}", block_size)
        cases.append(case)
        print(
            f"block={block_size} dry-tail={case['dryTailRms']:.9f} "
            f"wet-tail={case['wetTailRms']:.9f} "
            f"wet-late-tail={case['wetLateTailRms']:.9f}"
        )

    summary = {
        "schema": "cycle-v1-reverb-tail.v1",
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
