#!/usr/bin/env python3

"""Render two OohAah notes in one Cycle 1 instance and compare their envelopes."""

import argparse
import json
import subprocess
from pathlib import Path

from cycle_audio_diff import correlation_at_lag, mixdown, normalized_difference, read_wav, rms


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def parse_integer_list(value):
    values = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive comma-separated integers")
    return values


def write_automation(path, wav_path, note_length_ms, release_gap_ms, sample_rate, block_size):
    repetition_ms = note_length_ms + release_gap_ms
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
                "command": "action",
                "actionType": "Disable",
                "area": "AreaUnison",
            },
            {
                "command": "setControl",
                "area": "AreaMasterCtrls",
                "target": "TargMasterVol",
                "value": 0.005,
            },
            {
                "command": "captureAudio",
                "path": str(wav_path),
                "durationMs": 2 * repetition_ms,
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
                    {
                        "type": "noteOn",
                        "timeMs": repetition_ms,
                        "note": 60,
                        "velocity": 0.8,
                        "channel": 1,
                    },
                    {
                        "type": "noteOff",
                        "timeMs": repetition_ms + note_length_ms,
                        "note": 60,
                        "channel": 1,
                    },
                ],
                "peakGreaterThan": 0.0001,
                "final50MsRmsLessThan": 0.000001,
            },
        ],
        "quit": True,
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def amplitude_envelope(signal, sample_rate, window_ms=5):
    window_size = max(1, round(window_ms * sample_rate / 1000))
    return [
        rms(signal[start:start + window_size])
        for start in range(0, len(signal) - window_size + 1, window_size)
    ]


def render_case(output_directory, note_length_ms, release_gap_ms, sample_rate, block_size):
    case_directory = output_directory / f"{sample_rate}hz-{note_length_ms}ms"
    case_directory.mkdir(parents=True, exist_ok=True)
    automation_path = case_directory / "automation.json"
    report_path = case_directory / "report.json"
    log_path = case_directory / "cycle.log"
    wav_path = case_directory / "render.wav"
    write_automation(
        automation_path,
        wav_path,
        note_length_ms,
        release_gap_ms,
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

    wave = read_wav(wav_path)
    signal = mixdown(wave["channels"])
    repetition_samples = round((note_length_ms + release_gap_ms) * sample_rate / 1000)
    first = signal[:repetition_samples]
    second = signal[repetition_samples:2 * repetition_samples]
    first_envelope = amplitude_envelope(first, sample_rate)
    second_envelope = amplitude_envelope(second, sample_rate)
    envelope_correlation = correlation_at_lag(first_envelope, second_envelope, 0)[0]
    first_envelope_rms = rms(first_envelope)
    second_envelope_rms = rms(second_envelope)

    return {
        "sampleRate": sample_rate,
        "noteLengthMs": note_length_ms,
        "releaseGapMs": release_gap_ms,
        "firstPeak": max(abs(value) for value in first),
        "secondPeak": max(abs(value) for value in second),
        "envelopeEnergyRatio": second_envelope_rms / max(first_envelope_rms, 1.0e-12),
        "envelopeCorrelation": envelope_correlation,
        "envelopeNormalizedDifference": normalized_difference(first_envelope, second_envelope),
    }


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-ooh-aah-repeat-note"),
    )
    parser.add_argument("--note-lengths", type=parse_integer_list, default=[100, 400, 800])
    parser.add_argument("--sample-rates", type=parse_integer_list, default=[44100, 48000])
    parser.add_argument("--release-gap-ms", type=int, default=500)
    parser.add_argument("--block-size", type=int, default=512)
    parser.add_argument("--minimum-energy-ratio", type=float, default=0.8)
    parser.add_argument("--maximum-energy-ratio", type=float, default=1.25)
    parser.add_argument("--minimum-envelope-correlation", type=float, default=0.98)
    parser.add_argument("--maximum-envelope-difference", type=float, default=0.3)
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    cases = []
    for sample_rate in arguments.sample_rates:
        for note_length_ms in arguments.note_lengths:
            print(
                f"Rendering repeated {note_length_ms} ms OohAah notes at {sample_rate} Hz...",
                flush=True,
            )
            case = render_case(
                arguments.output_dir,
                note_length_ms,
                arguments.release_gap_ms,
                sample_rate,
                arguments.block_size,
            )
            case["passed"] = (
                arguments.minimum_energy_ratio <= case["envelopeEnergyRatio"]
                <= arguments.maximum_energy_ratio
                and case["envelopeCorrelation"] >= arguments.minimum_envelope_correlation
                and case["envelopeNormalizedDifference"] <= arguments.maximum_envelope_difference
            )
            cases.append(case)
            print(
                f"  ratio={case['envelopeEnergyRatio']:.5f} "
                f"correlation={case['envelopeCorrelation']:.6f} "
                f"difference={case['envelopeNormalizedDifference']:.6f}",
                flush=True,
            )

    summary = {
        "schema": "cycle-v1-ooh-aah-repeat-note.v1",
        "blockSize": arguments.block_size,
        "minimumEnergyRatio": arguments.minimum_energy_ratio,
        "maximumEnergyRatio": arguments.maximum_energy_ratio,
        "minimumEnvelopeCorrelation": arguments.minimum_envelope_correlation,
        "maximumEnvelopeDifference": arguments.maximum_envelope_difference,
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
