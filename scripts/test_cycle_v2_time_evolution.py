#!/usr/bin/env python3

"""Validate Cycle V2 time surfaces, scratch traversal, and note repetition."""

import argparse
import json
import subprocess
from pathlib import Path

from cycle_audio_diff import (
    average_spectrum,
    mixdown,
    read_wav,
    spectrum_difference,
)


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def render_fixture(output_directory):
    fixture = SCRIPT_DIR / "fixtures" / "cycle-v2-agent-time-evolution.json"
    with fixture.open(encoding="utf-8") as source:
        automation = json.load(source)

    for command in automation["commands"]:
        if command["command"] == "captureAudio":
            command["path"] = str(output_directory / Path(command["path"]).name)
        elif command["command"] == "openGraph":
            command["path"] = str((fixture.parent / command["path"]).resolve())

    automation_path = output_directory / fixture.name
    report_path = output_directory / "cycle-v2-time-evolution-report.json"
    log_path = output_directory / "cycle-v2-time-evolution.log"
    automation_path.write_text(json.dumps(automation, indent=2) + "\n", encoding="utf-8")
    subprocess.run(
        [
            str(SCRIPT_DIR / "run_cycle_v2_agent.sh"),
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
        raise RuntimeError(f"Cycle V2 automation failed: {failures[-1]['message']}")


def load_signal(output_directory, filename):
    wave = read_wav(output_directory / filename)
    return wave["sampleRate"], mixdown(wave["channels"])


def window(signal, sample_rate, start_ms, duration_ms=120):
    start = round(start_ms * sample_rate / 1000)
    count = round(duration_ms * sample_rate / 1000)
    return signal[start:start + count]


def positive_duty(signal):
    return sum(sample > 0.0 for sample in signal) / len(signal)


def analyze(output_directory, repeated_output_directory):
    sample_rate, dunk = load_signal(
        output_directory, "cycle-v2-agent-dunk-2-evolution.wav")
    _, pwm = load_signal(output_directory, "cycle-v2-agent-pwm-evolution.wav")
    _, pwm_linear = load_signal(output_directory, "cycle-v2-agent-pwm-linear-time.wav")
    _, repeated_pwm = load_signal(output_directory, "cycle-v2-agent-pwm-repeat.wav")

    early_pwm_duty = positive_duty(window(pwm, sample_rate, 25))
    late_pwm_duty = positive_duty(window(pwm, sample_rate, 500))
    late_linear_duty = positive_duty(window(pwm_linear, sample_rate, 500))
    checks = {
        "dunkEvolutionDb": {
            "value": spectrum_difference(
                average_spectrum(window(dunk, sample_rate, 25)),
                average_spectrum(window(dunk, sample_rate, 750))),
            "minimum": 4.0,
        },
        "pwmDutyCycleExcursion": {
            "value": abs(late_pwm_duty - early_pwm_duty),
            "minimum": 0.2,
        },
        "pwmScratchDutyCycleDifference": {
            "value": abs(late_pwm_duty - late_linear_duty),
            "minimum": 0.2,
        },
    }
    repeat_duty_differences = [
        abs(
            positive_duty(window(repeated_pwm, sample_rate, start_ms))
            - positive_duty(window(
                repeated_pwm, sample_rate, 1200 + start_ms)))
        for start_ms in (25, 100, 250, 500, 750)
    ]
    checks["pwmRepeatMaximumDutyCycleDifference"] = {
        "value": max(repeat_duty_differences),
        "maximum": 0.02,
    }
    checks["freshProcessWaveFilesExact"] = {
        "value": float(all(
            (output_directory / filename).read_bytes()
            == (repeated_output_directory / filename).read_bytes()
            for filename in (
                "cycle-v2-agent-dunk-2-evolution.wav",
                "cycle-v2-agent-pwm-evolution.wav",
                "cycle-v2-agent-pwm-linear-time.wav",
                "cycle-v2-agent-pwm-repeat.wav"))),
        "minimum": 1.0,
    }
    for check in checks.values():
        check["passed"] = (
            check["value"] >= check["minimum"]
            if "minimum" in check
            else check["value"] <= check["maximum"])
    return {
        "schema": "cycle-v2-time-evolution.v1",
        "sampleRate": sample_rate,
        "checks": checks,
        "passed": all(check["passed"] for check in checks.values()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle-v2-time-evolution"),
    )
    parser.add_argument("--no-fail", action="store_true")
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    render_fixture(arguments.output_dir)
    repeated_output_directory = arguments.output_dir / "fresh-process-repeat"
    repeated_output_directory.mkdir(parents=True, exist_ok=True)
    render_fixture(repeated_output_directory)
    summary = analyze(arguments.output_dir, repeated_output_directory)
    summary_path = arguments.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    for name, check in summary["checks"].items():
        verdict = "pass" if check["passed"] else "FAIL"
        print(f"{name}: {check['value']:.6f} ({verdict})")
    print(f"Summary: {summary_path}")
    if not summary["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
