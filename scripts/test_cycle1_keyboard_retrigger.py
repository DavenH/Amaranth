#!/usr/bin/env python3

"""Verify that a Cycle 1 keyboard note can start during another note's release."""

import argparse
import json
import subprocess
from pathlib import Path

from cycle_audio_diff import midi_frequency, mixdown, read_wav, tone_amplitude


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/private/tmp/cycle1-keyboard-retrigger"),
    )
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    fixture = SCRIPT_DIR / "fixtures" / "cycle-agent-keyboard-release-retrigger.json"
    with fixture.open(encoding="utf-8") as source:
        automation = json.load(source)

    capture_paths = {}
    for command in automation["commands"]:
        if command["command"] == "captureLiveAudio":
            filename = Path(command["path"]).name
            capture_paths[filename] = arguments.output_dir / filename
            command["path"] = str(capture_paths[filename])

    automation_path = arguments.output_dir / "automation.json"
    report_path = arguments.output_dir / "report.json"
    log_path = arguments.output_dir / "cycle.log"
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

    note_downs = [
        result for result in report["results"]
        if result["type"] == "pointer" and result["data"]["event"] == "down"
    ]
    if len(note_downs) != 2:
        raise RuntimeError("Expected two keyboard pointer-down results")
    first_note = note_downs[0]["data"]["note"]
    second_note = note_downs[1]["data"]["note"]

    second_wave = read_wav(capture_paths["cycle-agent-keyboard-second-note.wav"])
    sample_rate = second_wave["sampleRate"]
    second_signal = mixdown(second_wave["channels"])
    trim = round(10 * sample_rate / 1000)
    second_signal = second_signal[trim:]
    first_tone = tone_amplitude(
        second_signal,
        sample_rate,
        midi_frequency(first_note),
    )
    second_tone = tone_amplitude(
        second_signal,
        sample_rate,
        midi_frequency(second_note),
    )
    ratio = second_tone / max(first_tone, 1.0e-12)
    passed = (
        note_downs[0]["data"]["noteOn"]
        and note_downs[1]["data"]["noteOn"]
        and first_note != second_note
        and second_tone >= 0.001
        and ratio >= 4.0
    )
    summary = {
        "schema": "cycle-v1-keyboard-release-retrigger.v1",
        "firstNote": first_note,
        "secondNote": second_note,
        "firstNoteAmplitudeDuringSecond": first_tone,
        "secondNoteAmplitude": second_tone,
        "secondToFirstToneRatio": ratio,
        "passed": passed,
    }
    summary_path = arguments.output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(
        f"notes={first_note}->{second_note} "
        f"second={second_tone:.8f} residual-first={first_tone:.8f} ratio={ratio:.3f}"
    )
    print(f"Summary: {summary_path}")
    if not passed and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
