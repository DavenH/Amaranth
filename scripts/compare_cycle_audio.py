#!/usr/bin/env python3

"""Render one declared-equivalent preset in Cycle 1 and Cycle 2 and compare it."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
sys.path.insert(0, str(SCRIPT_DIR))

import cycle_audio_diff


def load_manifest(path, allow_unverified):
    with path.open(encoding="utf-8") as source:
        manifest = json.load(source)
    if manifest.get("schema") != "cycle-v1-v2-audio-equivalence.v1":
        raise ValueError("Unsupported or missing equivalence manifest schema")
    if manifest.get("status") != "verified" and not allow_unverified:
        raise ValueError(
            "Equivalence manifest is not verified; use --allow-unverified for diagnostics only")
    if not manifest.get("v1") or not manifest.get("v2", {}).get("graph"):
        raise ValueError("Manifest must declare v1 preset loading and a v2 graph")
    verify_artifact(manifest["v1"], "sourceDocument")
    verify_artifact(manifest["v2"], "graph")
    return manifest


def verify_artifact(configuration, path_property):
    path = REPO_ROOT / configuration[path_property]
    if not path.is_file():
        raise ValueError(f"Equivalence artifact does not exist: {path}")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != configuration.get("sha256"):
        raise ValueError(f"Equivalence artifact changed after validation: {path}")


def v1_open_command(configuration):
    if configuration.get("factoryPreset"):
        return {
            "command": "openFactoryPreset",
            "preset": configuration["factoryPreset"],
            "waitForIdle": True,
            "idleDelayMs": 300,
        }
    if configuration.get("presetPath"):
        return {
            "command": "openPreset",
            "path": str(Path(configuration["presetPath"]).resolve()),
            "waitForIdle": True,
            "idleDelayMs": 300,
        }
    raise ValueError("Manifest v1 must declare factoryPreset or presetPath")


def capture_command(path, note, arguments, overrides=None):
    start_sample = arguments.note_start_sample
    note_off_sample = start_sample + round(arguments.note_duration_ms * arguments.sample_rate / 1000.0)
    command = {
        "command": "captureAudio",
        "path": str(path),
        "sampleRate": arguments.sample_rate,
        "blockSize": arguments.block_size,
        "channels": 2,
        "durationMs": arguments.duration_ms,
        "events": [
            {
                "type": "noteOn",
                "sample": start_sample,
                "note": note,
                "velocity": arguments.velocity,
                "channel": 1,
            },
            {
                "type": "noteOff",
                "sample": note_off_sample,
                "note": note,
                "channel": 1,
            },
        ],
        "rmsGreaterThan": 1.0e-8,
    }
    command.update(overrides or {})
    return command


def write_automation(path, open_command, capture):
    document = {"commands": [open_command, capture], "quit": True}
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def run_renderer(wrapper, script, report, log):
    subprocess.run(
        [str(wrapper), str(script), str(report), str(log)],
        cwd=REPO_ROOT,
        check=True,
    )
    with report.open(encoding="utf-8") as source:
        automation_report = json.load(source)
    failures = [result for result in automation_report.get("results", []) if not result.get("ok")]
    if failures:
        raise RuntimeError(f"Automation capture failed: {failures[-1].get('message', failures[-1])}")


def threshold_verdict(analysis, thresholds):
    checks = {
        "correlation": analysis["alignment"]["correlation"] >= thresholds["correlationMin"],
        "gainMatchedResidual": (
            analysis["gainFit"]["normalizedResidual"]
            <= thresholds["gainMatchedResidualMax"]),
        "spectrum": (
            analysis["spectrum"]["logMagnitudeRmseDb"]
            <= thresholds["spectrumRmseDbMax"]),
        "cyclogram": (
            analysis["cyclogram"]["meanRowNormalizedDifference"]
            <= thresholds["cyclogramMeanRowDifferenceMax"]),
    }
    return {"passed": all(checks.values()), "checks": checks}


def render_note(manifest, note, output_directory, arguments):
    note_directory = output_directory / f"midi-{note}"
    note_directory.mkdir(parents=True, exist_ok=True)
    v1_wav = note_directory / "cycle-v1.wav"
    v2_wav = note_directory / "cycle-v2.wav"
    capture_v1 = capture_command(v1_wav, note, arguments)
    capture_v2 = capture_command(
        v2_wav,
        note,
        arguments,
        manifest["v2"].get("renderOverrides"),
    )
    v1_script = note_directory / "cycle-v1-automation.json"
    v2_script = note_directory / "cycle-v2-automation.json"

    write_automation(v1_script, v1_open_command(manifest["v1"]), capture_v1)
    write_automation(v2_script, {
        "command": "openGraph",
        "path": str((REPO_ROOT / manifest["v2"]["graph"]).resolve()),
    }, capture_v2)
    if not arguments.reuse_wavs or not v1_wav.is_file() or not v2_wav.is_file():
        run_renderer(
            SCRIPT_DIR / "run_cycle_agent.sh",
            v1_script,
            note_directory / "cycle-v1-report.json",
            note_directory / "cycle-v1.log",
        )
        run_renderer(
            SCRIPT_DIR / "run_cycle_v2_agent.sh",
            v2_script,
            note_directory / "cycle-v2-report.json",
            note_directory / "cycle-v2.log",
        )

    analysis = cycle_audio_diff.analyze_pair(
        v1_wav,
        v2_wav,
        note + manifest["translation"].get("midiNoteOffset", 0),
        arguments.analysis_start_ms,
        arguments.analysis_duration_ms,
        arguments.maximum_lag,
    )
    analysis["requestedMidiNote"] = note
    analysis["expectedGainFit"] = {
        "candidateScale": (
            manifest["translation"].get("v1MasterGain", 1.0)
            / manifest["translation"].get("v2OutputHeadroom", 1.0)),
    }
    analysis["verdict"] = threshold_verdict(analysis, manifest["thresholds"])
    (note_directory / "analysis.json").write_text(
        json.dumps(analysis, indent=2) + "\n",
        encoding="utf-8",
    )
    return analysis


def parse_notes(value):
    notes = [int(note.strip()) for note in value.split(",") if note.strip()]
    if not notes or any(note < 0 or note > 127 for note in notes):
        raise argparse.ArgumentTypeError("notes must be comma-separated MIDI values from 0 to 127")
    return notes


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/cycle-audio-parity"))
    parser.add_argument("--notes", type=parse_notes, default=[36, 48, 60, 72])
    parser.add_argument("--sample-rate", type=int, default=48000)
    parser.add_argument("--block-size", type=int, default=512)
    parser.add_argument("--duration-ms", type=float, default=500.0)
    parser.add_argument("--note-duration-ms", type=float, default=400.0)
    parser.add_argument("--note-start-sample", type=int, default=37)
    parser.add_argument("--velocity", type=float, default=0.8)
    parser.add_argument("--analysis-start-ms", type=float, default=50.0)
    parser.add_argument("--analysis-duration-ms", type=float, default=150.0)
    parser.add_argument("--maximum-lag", type=int, default=512)
    parser.add_argument("--allow-unverified", action="store_true")
    parser.add_argument("--reuse-wavs", action="store_true")
    parser.add_argument("--no-fail", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    manifest = load_manifest(arguments.manifest, arguments.allow_unverified)
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    analyses = []
    for note in arguments.notes:
        print(f"Rendering MIDI {note} in Cycle 1 and Cycle 2...", flush=True)
        analyses.append(render_note(manifest, note, arguments.output_dir, arguments))

    report = {
        "schema": "cycle-v1-v2-audio-comparison.v1",
        "manifest": str(arguments.manifest.resolve()),
        "equivalenceStatus": manifest["status"],
        "notes": analyses,
        "passed": manifest["status"] == "verified"
                  and all(result["verdict"]["passed"] for result in analyses),
    }
    report_path = arguments.output_dir / "comparison.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Comparison report: {report_path}")
    for analysis in analyses:
        print(
            f"MIDI {analysis['requestedMidiNote']}: lag={analysis['alignment']['candidateLagSamples']} "
            f"corr={analysis['alignment']['correlation']:.5f} "
            f"gain={analysis['gainFit']['candidateScaleDb']:+.2f} dB "
            f"residual={analysis['gainFit']['normalizedResidual']:.4f} "
            f"spectral={analysis['spectrum']['logMagnitudeRmseDb']:.2f} dB "
            f"cyclogram={analysis['cyclogram']['meanRowNormalizedDifference']:.4f}")
    if not report["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
