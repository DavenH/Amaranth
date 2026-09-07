#!/usr/bin/env python3

"""Validate Cycle 1 time surfaces, scratch modulation, and note retriggering."""

import argparse
import json
import subprocess
from pathlib import Path

from cycle_audio_diff import (
    average_spectrum,
    harmonic_profile,
    mixdown,
    read_wav,
    rms,
    spectrum_difference,
)


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def render_fixture(fixture_name, output_directory):
    with (SCRIPT_DIR / "fixtures" / fixture_name).open(encoding="utf-8") as source:
        automation = json.load(source)

    for command in automation["commands"]:
        if command["command"] in ("captureAudio", "captureLiveAudio"):
            command["path"] = str(output_directory / Path(command["path"]).name)

    stem = Path(fixture_name).stem
    automation_path = output_directory / f"{stem}.json"
    report_path = output_directory / f"{stem}-report.json"
    log_path = output_directory / f"{stem}.log"
    automation_path.write_text(json.dumps(automation, indent=2) + "\n", encoding="utf-8")
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
    failures = [result for result in report["results"] if not result["ok"]]
    if failures:
        raise RuntimeError(f"Cycle automation failed: {failures[-1]['message']}")


def load_signal(output_directory, filename):
    wave = read_wav(output_directory / filename)
    return wave["sampleRate"], mixdown(wave["channels"])


def window(signal, sample_rate, start_ms, duration_ms=120):
    start = round(start_ms * sample_rate / 1000)
    count = round(duration_ms * sample_rate / 1000)
    return signal[start:start + count]


def harmonic_distance(left, right, sample_rate):
    left_profile = harmonic_profile(left, sample_rate, 60, 24)
    right_profile = harmonic_profile(right, sample_rate, 60, 24)
    return rms([a - b for a, b in zip(left_profile, right_profile)])


def spectral_distance(left, right):
    return spectrum_difference(average_spectrum(left), average_spectrum(right))


def analyze(output_directory):
    sample_rate, dunk = load_signal(
        output_directory,
        "cycle-agent-dunk-2-evolution.wav",
    )
    _, pwm = load_signal(output_directory, "cycle-agent-pwm-evolution.wav")
    _, pwm_linear = load_signal(output_directory, "cycle-agent-pwm-linear-time.wav")
    _, bright = load_signal(output_directory, "cycle-agent-bright-lead-3-scratch.wav")
    _, bright_linear = load_signal(
        output_directory,
        "cycle-agent-bright-lead-3-linear-time.wav",
    )
    _, repeated_pwm = load_signal(output_directory, "cycle-agent-pwm-repeat.wav")

    dunk_evolution = spectral_distance(
        window(dunk, sample_rate, 25),
        window(dunk, sample_rate, 750),
    )
    pwm_evolution = spectral_distance(
        window(pwm, sample_rate, 25),
        window(pwm, sample_rate, 250),
    )
    pwm_scratch_difference = harmonic_distance(
        window(pwm, sample_rate, 500),
        window(pwm_linear, sample_rate, 500),
        sample_rate,
    )
    bright_scratch_difference = harmonic_distance(
        window(bright, sample_rate, 25),
        window(bright_linear, sample_rate, 25),
        sample_rate,
    )

    repeat_distances = []
    for start_ms in (25, 100, 250, 500, 750):
        repeat_distances.append(spectral_distance(
            window(repeated_pwm, sample_rate, start_ms),
            window(repeated_pwm, sample_rate, 1200 + start_ms),
        ))
    maximum_repeat_difference = max(repeat_distances)

    checks = {
        "dunkEvolutionDb": {
            "value": dunk_evolution,
            "minimum": 8.0,
            "passed": dunk_evolution >= 8.0,
        },
        "pwmEvolutionDb": {
            "value": pwm_evolution,
            "minimum": 4.0,
            "passed": pwm_evolution >= 4.0,
        },
        "pwmScratchHarmonicDifference": {
            "value": pwm_scratch_difference,
            "minimum": 0.08,
            "passed": pwm_scratch_difference >= 0.08,
        },
        "brightLeadScratchHarmonicDifference": {
            "value": bright_scratch_difference,
            "minimum": 0.25,
            "passed": bright_scratch_difference >= 0.25,
        },
        "pwmRepeatMaximumSpectrumDifferenceDb": {
            "value": maximum_repeat_difference,
            "maximum": 1.6,
            "passed": maximum_repeat_difference <= 1.6,
        },
    }
    return {
        "schema": "cycle-v1-time-evolution.v1",
        "sampleRate": sample_rate,
        "checks": checks,
        "passed": all(check["passed"] for check in checks.values()),
    }


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-time-evolution"),
    )
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    render_fixture("cycle-agent-cycle1-time-evolution.json", arguments.output_dir)
    render_fixture("cycle-agent-pwm-repeat.json", arguments.output_dir)
    summary = analyze(arguments.output_dir)
    summary_path = arguments.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    for name, check in summary["checks"].items():
        print(f"{name}: {check['value']:.6f} ({'pass' if check['passed'] else 'FAIL'})")
    print(f"Summary: {summary_path}")
    if not summary["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
