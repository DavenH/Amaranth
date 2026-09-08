#!/usr/bin/env python3

"""Validate Cycle 1 spectral audibility, phase behavior, and unison output."""

import argparse
import json
import math
import subprocess
from pathlib import Path

from cycle_audio_diff import read_wav


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
AUDIBILITY_PRESETS = ("acidic", "anasound-2")
EFFECT_AREAS = (
    "AreaDelay",
    "AreaWaveshaper",
    "AreaImpulse",
    "AreaUnison",
    "AreaReverb",
    "AreaEQ",
)


def parse_integer_list(value):
    values = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive comma-separated integers")
    return values


def capture_command(wav_path, sample_rate):
    return {
        "command": "captureAudio",
        "path": str(wav_path),
        "durationMs": 1000,
        "sampleRate": sample_rate,
        "blockSize": 512,
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
                "timeMs": 600,
                "note": 60,
                "channel": 1,
            },
        ],
        "final50MsRmsLessThan": 0.000001,
    }


def preset_setup(preset):
    commands = [
        {
            "command": "openFactoryPreset",
            "preset": preset,
            "waitForIdle": True,
            "idleDelayMs": 300,
        }
    ]
    commands.extend(
        {
            "command": "action",
            "actionType": "Disable",
            "area": area,
        }
        for area in EFFECT_AREAS
    )
    commands.append(
        {
            "command": "setControl",
            "area": "AreaMasterCtrls",
            "target": "TargMasterVol",
            "value": 0.02,
        }
    )
    return commands


def run_automation(case_directory, commands):
    case_directory.mkdir(parents=True, exist_ok=True)
    automation_path = case_directory / "automation.json"
    report_path = case_directory / "report.json"
    log_path = case_directory / "cycle.log"
    automation_path.write_text(
        json.dumps({"commands": commands, "quit": True}, indent=2) + "\n",
        encoding="utf-8",
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


def steady_state_power(wav_path, start_ms=100, end_ms=550):
    wave = read_wav(wav_path)
    start = round(start_ms * wave["sampleRate"] / 1000)
    end = round(end_ms * wave["sampleRate"] / 1000)
    channels = [channel[start:end] for channel in wave["channels"]]
    square_sum = sum(value * value for channel in channels for value in channel)
    sample_count = sum(len(channel) for channel in channels)
    return square_sum / max(1, sample_count)


def stereo_side_to_mid_ratio(wav_path, start_ms=100, end_ms=550):
    wave = read_wav(wav_path)
    if len(wave["channels"]) < 2:
        return 0.0
    start = round(start_ms * wave["sampleRate"] / 1000)
    end = round(end_ms * wave["sampleRate"] / 1000)
    left = wave["channels"][0][start:end]
    right = wave["channels"][1][start:end]
    pairs = zip(left, right)
    side_power = sum(
        (left_value - right_value) ** 2
        for left_value, right_value in pairs
    )
    pairs = zip(left, right)
    mid_power = sum(
        (left_value + right_value) ** 2
        for left_value, right_value in pairs
    )
    return math.sqrt(side_power / max(mid_power, 1.0e-24))


def render_audibility_case(output_directory, preset, sample_rate):
    case_directory = output_directory / f"{sample_rate}hz-{preset}"
    wav_path = case_directory / "render.wav"
    commands = preset_setup(preset)
    commands.append(capture_command(wav_path, sample_rate))
    run_automation(case_directory, commands)
    power = steady_state_power(wav_path)
    return {
        "type": "audibility",
        "preset": preset,
        "sampleRate": sample_rate,
        "steadyStatePower": power,
        "steadyStateRms": math.sqrt(power),
    }


def render_authored_audibility_case(output_directory, preset, sample_rate):
    case_directory = output_directory / f"{sample_rate}hz-{preset}-authored"
    wav_path = case_directory / "render.wav"
    commands = [
        {
            "command": "openFactoryPreset",
            "preset": preset,
            "waitForIdle": True,
            "idleDelayMs": 300,
        },
        capture_command(wav_path, sample_rate),
    ]
    run_automation(case_directory, commands)
    power = steady_state_power(wav_path)
    return {
        "type": "authoredAudibility",
        "preset": preset,
        "sampleRate": sample_rate,
        "steadyStatePower": power,
        "steadyStateRms": math.sqrt(power),
    }


def render_phase_stereo_case(output_directory, sample_rate):
    case_directory = output_directory / f"{sample_rate}hz-acidic-phase-stereo"
    wav_path = case_directory / "render.wav"
    commands = preset_setup("acidic")
    commands.append(capture_command(wav_path, sample_rate))
    run_automation(case_directory, commands)
    return {
        "type": "phaseStereo",
        "preset": "acidic",
        "sampleRate": sample_rate,
        "sideToMidRmsRatio": stereo_side_to_mid_ratio(wav_path),
    }


def render_phase_case(output_directory, sample_rate):
    case_directory = output_directory / f"{sample_rate}hz-baroque-flute-phase"
    enabled_path = case_directory / "phase-enabled.wav"
    disabled_path = case_directory / "phase-disabled.wav"
    commands = preset_setup("baroque-flute")
    commands.append(capture_command(enabled_path, sample_rate))
    commands.extend(
        [
            {
                "command": "action",
                "actionType": "SwitchMode",
                "area": "AreaSpectrum",
                "id": "IdModePhase",
                "waitForIdle": True,
                "idleDelayMs": 300,
            },
            {
                "command": "action",
                "actionType": "TriggerButton",
                "area": "AreaSpectrum",
                "id": "IdBttnEnable",
                "waitForIdle": True,
                "idleDelayMs": 300,
            },
        ]
    )
    commands.append(capture_command(disabled_path, sample_rate))
    run_automation(case_directory, commands)
    enabled_power = steady_state_power(enabled_path)
    disabled_power = steady_state_power(disabled_path)
    return {
        "type": "phasePower",
        "preset": "baroque-flute",
        "sampleRate": sample_rate,
        "enabledPower": enabled_power,
        "disabledPower": disabled_power,
        "disabledToEnabledPowerRatio": disabled_power / max(enabled_power, 1.0e-24),
    }


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-spectral-phase"),
    )
    parser.add_argument("--sample-rates", type=parse_integer_list, default=[44100, 48000])
    parser.add_argument("--minimum-rms", type=float, default=0.001)
    parser.add_argument("--minimum-side-to-mid-ratio", type=float, default=0.1)
    parser.add_argument("--minimum-power-ratio", type=float, default=0.9)
    parser.add_argument("--maximum-power-ratio", type=float, default=1.1)
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    cases = []
    for sample_rate in arguments.sample_rates:
        for preset in AUDIBILITY_PRESETS:
            print(f"Rendering {preset} at {sample_rate} Hz...", flush=True)
            case = render_audibility_case(arguments.output_dir, preset, sample_rate)
            case["passed"] = case["steadyStateRms"] >= arguments.minimum_rms
            cases.append(case)
            print(f"  steady RMS={case['steadyStateRms']:.8f}", flush=True)

        print(f"Rendering Ping with authored unison at {sample_rate} Hz...", flush=True)
        case = render_authored_audibility_case(arguments.output_dir, "ping", sample_rate)
        case["passed"] = case["steadyStateRms"] >= arguments.minimum_rms
        cases.append(case)
        print(f"  steady RMS={case['steadyStateRms']:.8f}", flush=True)

        print(f"Measuring Acidic phase stereo at {sample_rate} Hz...", flush=True)
        case = render_phase_stereo_case(arguments.output_dir, sample_rate)
        case["passed"] = case["sideToMidRmsRatio"] >= arguments.minimum_side_to_mid_ratio
        cases.append(case)
        print(f"  side/mid RMS={case['sideToMidRmsRatio']:.6f}", flush=True)

        print(f"Comparing Baroque Flute phase power at {sample_rate} Hz...", flush=True)
        case = render_phase_case(arguments.output_dir, sample_rate)
        case["passed"] = (
            arguments.minimum_power_ratio <= case["disabledToEnabledPowerRatio"]
            <= arguments.maximum_power_ratio
        )
        cases.append(case)
        print(
            f"  disabled/enabled power={case['disabledToEnabledPowerRatio']:.6f}",
            flush=True,
        )

    summary = {
        "schema": "cycle-v1-spectral-phase.v2",
        "minimumRms": arguments.minimum_rms,
        "minimumSideToMidRatio": arguments.minimum_side_to_mid_ratio,
        "minimumPowerRatio": arguments.minimum_power_ratio,
        "maximumPowerRatio": arguments.maximum_power_ratio,
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
