#!/usr/bin/env python3

"""Check Calming for note-dependent high-frequency FFT residue."""

import argparse
import json
import subprocess
from pathlib import Path

from cycle_audio_diff import average_spectrum, mixdown, read_wav, rms


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
NOTES = (
    ("A1", 45, 0),
    ("G1", 43, 900),
    ("F1", 41, 1800),
    ("E1", 40, 2700),
)


def parse_integer_list(value):
    values = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not values or any(item <= 0 for item in values):
        raise argparse.ArgumentTypeError("values must be positive comma-separated integers")
    return values


def render_case(output_directory, sample_rate):
    fixture = SCRIPT_DIR / "fixtures" / "cycle-agent-calming-spectrum.json"
    with fixture.open(encoding="utf-8") as source:
        automation = json.load(source)

    capture = next(
        command for command in automation["commands"]
        if command["command"] == "captureAudio"
    )
    wav_path = output_directory / f"calming-{sample_rate}hz.wav"
    capture["path"] = str(wav_path)
    capture["sampleRate"] = sample_rate

    case_directory = output_directory / f"{sample_rate}hz"
    case_directory.mkdir(parents=True, exist_ok=True)
    automation_path = case_directory / "automation.json"
    report_path = case_directory / "report.json"
    log_path = case_directory / "cycle.log"
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
    return wav_path


def analyze_case(wav_path):
    wave = read_wav(wav_path)
    sample_rate = wave["sampleRate"]
    signal = mixdown(wave["channels"])
    notes = []
    for name, midi_note, note_start_ms in NOTES:
        start = round((note_start_ms + 200) * sample_rate / 1000)
        count = round(400 * sample_rate / 1000)
        window = signal[start:start + count]
        spectrum = average_spectrum(window, 8192, 1024)
        bin_width = sample_rate / 8192
        total_power = sum(value * value for value in spectrum)
        high_power = sum(
            value * value for index, value in enumerate(spectrum)
            if index * bin_width >= 3000
        )
        high_ratio = high_power / max(total_power, 1.0e-30)
        note_rms = rms(window)
        notes.append({
            "name": name,
            "midiNote": midi_note,
            "rms": note_rms,
            "highBandPowerRatio": high_ratio,
            "passed": note_rms >= 0.001 and high_ratio <= 1.0e-7,
        })
    return {
        "sampleRate": sample_rate,
        "notes": notes,
        "passed": all(note["passed"] for note in notes),
    }


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-calming-spectrum"),
    )
    parser.add_argument("--sample-rates", type=parse_integer_list, default=[44100, 48000])
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    cases = []
    for sample_rate in arguments.sample_rates:
        print(f"Rendering Calming at {sample_rate} Hz...", flush=True)
        case = analyze_case(render_case(arguments.output_dir, sample_rate))
        cases.append(case)
        for note in case["notes"]:
            print(
                f"  {note['name']}: rms={note['rms']:.8f} "
                f"high-band={note['highBandPowerRatio']:.3e}",
                flush=True,
            )

    summary = {
        "schema": "cycle-v1-calming-spectrum.v1",
        "maximumHighBandPowerRatio": 1.0e-7,
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
