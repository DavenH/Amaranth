#!/usr/bin/env python3

"""Render one declared-equivalent preset in Cycle 1 and Cycle 2 and compare it."""

import argparse
import hashlib
import json
import os
import struct
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
    verify_artifact(manifest["v1"], "sourceDocument", allow_unverified)
    verify_artifact(manifest["v2"], "graph", allow_unverified)
    return manifest


def verify_artifact(configuration, path_property, allow_unverified):
    path = REPO_ROOT / configuration[path_property]
    if not path.is_file():
        raise ValueError(f"Equivalence artifact does not exist: {path}")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != configuration.get("sha256") and not allow_unverified:
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
        "rawPath": str(path.with_suffix(".f32le")),
        "sampleRate": arguments.sample_rate,
        "blockSize": arguments.block_size,
        "channels": 2,
        "outputGain": 1.0,
        "ratePolicy": "legacyInternal44100",
        "randomSeed": 1129927500,
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


def cycle_v1_note(manifest, requested_note):
    legacy_offset = manifest["translation"].get("legacyMidiReferenceOffset", 0)
    return requested_note - legacy_offset


def translated_output_gain(manifest):
    translation = manifest.get("translation", {})
    if "v2OutputGainUnitValue" not in translation:
        return None
    return translation.get("v1MasterGain")


def write_automation(path, open_command, capture, setup_commands=None):
    document = {
        "commands": [open_command, *(setup_commands or []), capture],
        "quit": True,
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def deterministic_renderer_environment():
    environment = os.environ.copy()
    environment["VECLIB_MAXIMUM_THREADS"] = "1"
    return environment


def run_renderer(wrapper, script, report, log):
    subprocess.run(
        [str(wrapper), str(script), str(report), str(log)],
        cwd=REPO_ROOT,
        check=True,
        env=deterministic_renderer_environment(),
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
    repeatability_results = analysis.get("repeatability")
    if repeatability_results is not None:
        checks["repeatability"] = all(
            result["samplesEqual"] for result in repeatability_results.values())
    if thresholds.get("exactSamplesRequired", False):
        checks["exactSamples"] = analysis["rawExact"]["samplesEqual"]
    return {"passed": all(checks.values()), "checks": checks}


def render_capture(
        wrapper,
        script,
        report,
        log,
        wav,
        open_command,
        capture,
        reuse,
        setup_commands=None):
    write_automation(script, open_command, capture, setup_commands)
    raw_path = Path(capture["rawPath"])
    required_paths = [wav, raw_path]
    if capture.get("stageCapturePath"):
        required_paths.append(Path(capture["stageCapturePath"]))
    if not reuse or not all(path.is_file() for path in required_paths):
        run_renderer(wrapper, script, report, log)


def raw_capture(wav):
    wav_data = cycle_audio_diff.read_wav(wav)
    return cycle_audio_diff.read_raw_f32(
        wav.with_suffix(".f32le"),
        wav_data["sampleRate"],
        len(wav_data["channels"]),
        wav_data["frames"],
    )


def repeatability(reference_wav, repeat_wavs):
    reference = raw_capture(reference_wav)
    comparisons = [
        cycle_audio_diff.exact_sample_comparison(
            reference, raw_capture(repeat_wav))
        for repeat_wav in repeat_wavs
    ]
    return {
        "renders": 1 + len(repeat_wavs),
        "checked": bool(comparisons),
        "samplesEqual": (
            all(result["samplesEqual"] for result in comparisons)
            if comparisons else None),
        "comparisons": comparisons,
    }


def load_stage_capture(path):
    with path.open(encoding="utf-8") as source:
        manifest = json.load(source)
    if manifest.get("schema") != "cycle-spectral-stage-capture.v1":
        raise ValueError(f"Unsupported spectral stage capture schema: {path}")

    records = {}
    for encoded in manifest.get("records", []):
        raw_path = Path(encoded["rawPath"])
        if not raw_path.is_absolute():
            raw_path = path.parent / raw_path
        payload = raw_path.read_bytes()
        if hashlib.sha256(payload).hexdigest() != encoded.get("sha256"):
            raise ValueError(f"Spectral stage payload changed after capture: {raw_path}")
        primary_count = int(encoded["primaryValueCount"])
        secondary_count = int(encoded["secondaryValueCount"])
        value_count = primary_count + secondary_count
        if len(payload) != value_count * 4:
            raise ValueError(f"Spectral stage payload has the wrong length: {raw_path}")
        values = struct.unpack(f"<{value_count}f", payload)
        key = (encoded["stage"], int(encoded["channel"]))
        records[key] = {
            **encoded,
            "primaryValues": list(values[:primary_count]),
            "secondaryValues": list(values[primary_count:]),
        }
    return records


def compare_stage_values(reference, candidate):
    counts_equal = len(reference) == len(candidate)
    first_mismatch = None
    differing_values = 0
    if counts_equal:
        for index, (left, right) in enumerate(zip(reference, candidate)):
            if left == right:
                continue
            differing_values += 1
            if first_mismatch is None:
                first_mismatch = {
                    "index": index,
                    "reference": left,
                    "candidate": right,
                }

    compared_count = min(len(reference), len(candidate))
    left = reference[:compared_count]
    right = candidate[:compared_count]
    correlation = None
    normalized_residual = None
    gain_scale = None
    gain_matched_residual = None
    if compared_count:
        correlation = cycle_audio_diff.correlation_at_lag(left, right, 0)[0]
        normalized_residual = cycle_audio_diff.normalized_difference(left, right)
        gain_scale = cycle_audio_diff.fit_gain(left, right)
        gain_matched_residual = cycle_audio_diff.normalized_difference(
            left,
            [gain_scale * value for value in right],
        )
    return {
        "countsEqual": counts_equal,
        "referenceValueCount": len(reference),
        "candidateValueCount": len(candidate),
        "samplesEqual": counts_equal and differing_values == 0,
        "differingValues": differing_values if counts_equal else None,
        "firstMismatch": first_mismatch,
        "correlation": correlation,
        "normalizedResidual": normalized_residual,
        "gainScale": gain_scale,
        "gainMatchedNormalizedResidual": gain_matched_residual,
    }


def compare_stage_captures(reference_path, candidate_path):
    reference = load_stage_capture(reference_path)
    candidate = load_stage_capture(candidate_path)
    stage_order = [
        "time-raster",
        "time-frame",
        "forward-fft",
        "magnitude-raster",
        "magnitude-operand",
        "phase-raster",
        "phase-operand",
        "post-layer-spectrum",
        "reconstructed-frame",
        "pitch-clocked-cycle",
    ]
    stage_rank = {stage: index for index, stage in enumerate(stage_order)}
    keys = sorted(
        set(reference) | set(candidate),
        key=lambda key: (stage_rank.get(key[0], len(stage_order)), key[1]),
    )
    comparisons = []
    for key in keys:
        left = reference.get(key)
        right = candidate.get(key)
        comparison = {
            "stage": key[0],
            "channel": key[1],
            "present": {"v1": left is not None, "v2": right is not None},
        }
        if left is not None:
            comparison["v1"] = {
                "frameIndex": left["frameIndex"],
                "frontier": left["frontier"],
                "midiNote": left["midiNote"],
            }
        if right is not None:
            comparison["v2"] = {
                "frameIndex": right["frameIndex"],
                "frontier": right["frontier"],
                "midiNote": right["midiNote"],
            }
        if left is not None and right is not None:
            comparison["primary"] = compare_stage_values(
                left["primaryValues"], right["primaryValues"])
            comparison["secondary"] = compare_stage_values(
                left["secondaryValues"], right["secondaryValues"])
            comparison["samplesEqual"] = (
                comparison["primary"]["samplesEqual"]
                and comparison["secondary"]["samplesEqual"])
        else:
            comparison["samplesEqual"] = False
        comparisons.append(comparison)

    first_unequal = next(
        (comparison["stage"] for comparison in comparisons
         if not comparison["samplesEqual"]),
        None,
    )
    return {
        "records": comparisons,
        "samplesEqual": bool(comparisons)
            and all(item["samplesEqual"] for item in comparisons),
        "firstUnequalStage": first_unequal,
    }


def render_note(manifest, note, output_directory, arguments):
    note_directory = output_directory / f"midi-{note}"
    note_directory.mkdir(parents=True, exist_ok=True)
    v1_wav = note_directory / "cycle-v1.wav"
    v2_wav = note_directory / "cycle-v2.wav"
    capture_v1 = capture_command(v1_wav, cycle_v1_note(manifest, note), arguments)
    graph_output_gain = translated_output_gain(manifest)
    if graph_output_gain is not None:
        capture_v1["outputGain"] = graph_output_gain
    capture_v2 = capture_command(
        v2_wav,
        note,
        arguments,
        manifest["v2"].get("renderOverrides"),
    )
    capture_v2["controlNoteOffset"] = -manifest["translation"].get(
        "legacyMidiReferenceOffset", 0)
    if arguments.capture_stages:
        capture_v1["stageCapturePath"] = str(
            note_directory / "cycle-v1-stages.json")
        capture_v2["stageCapturePath"] = str(
            note_directory / "cycle-v2-stages.json")
        capture_v1["stageCaptureFrameIndex"] = arguments.stage_frame_index
        capture_v2["stageCaptureFrameIndex"] = arguments.stage_frame_index
        capture_v1["stageCaptureOccurrenceIndex"] = arguments.stage_occurrence_index
        capture_v2["stageCaptureOccurrenceIndex"] = arguments.stage_occurrence_index
    v1_script = note_directory / "cycle-v1-automation.json"
    v2_script = note_directory / "cycle-v2-automation.json"

    v1_open = v1_open_command(manifest["v1"])
    v2_open = {
        "command": "openGraph",
        "path": str((REPO_ROOT / manifest["v2"]["graph"]).resolve()),
        "waitForIdle": True,
        "idleDelayMs": 300,
    }
    effect_bindings = manifest.get("effectBindings", {})
    missing_effects = [
        effect for effect in arguments.disable_effect
        if effect not in effect_bindings
    ]
    if missing_effects:
        raise ValueError(
            "Manifest has no disable binding for: " + ", ".join(missing_effects))
    v1_setup = [
        {
            "command": "action",
            "actionType": "Disable",
            "area": effect_bindings[effect]["v1Area"],
            "waitForIdle": True,
            "idleDelayMs": 100,
        }
        for effect in arguments.disable_effect
    ]
    v2_setup = [
        {
            "command": "setNodeParameter",
            "nodeId": node_id,
            "parameterId": "enabled",
            "label": "Enabled",
            "value": "0",
            "waitForIdle": True,
        }
        for node_id in (
            arguments.v2_disable_node
            + [effect_bindings[effect]["v2Node"] for effect in arguments.disable_effect]
        )
    ]
    render_capture(
        SCRIPT_DIR / "run_cycle_agent.sh",
        v1_script,
        note_directory / "cycle-v1-report.json",
        note_directory / "cycle-v1.log",
        v1_wav,
        v1_open,
        capture_v1,
        arguments.reuse_wavs,
        v1_setup,
    )
    render_capture(
        SCRIPT_DIR / "run_cycle_v2_agent.sh",
        v2_script,
        note_directory / "cycle-v2-report.json",
        note_directory / "cycle-v2.log",
        v2_wav,
        v2_open,
        capture_v2,
        arguments.reuse_wavs,
        v2_setup,
    )

    repeat_wavs = {"v1": [], "v2": []}
    for repeat in range(2, arguments.determinism_renders + 1):
        for engine, wrapper, open_command, capture in (
                ("v1", "run_cycle_agent.sh", v1_open, capture_v1),
                ("v2", "run_cycle_v2_agent.sh", v2_open, capture_v2)):
            repeat_wav = note_directory / f"cycle-{engine}-repeat-{repeat}.wav"
            repeat_wavs[engine].append(repeat_wav)
            repeat_capture = dict(capture)
            repeat_capture["path"] = str(repeat_wav)
            repeat_capture["rawPath"] = str(repeat_wav.with_suffix(".f32le"))
            repeat_capture.pop("stageCapturePath", None)
            repeat_capture.pop("stageCaptureFrameIndex", None)
            render_capture(
                SCRIPT_DIR / wrapper,
                note_directory / f"cycle-{engine}-repeat-{repeat}-automation.json",
                note_directory / f"cycle-{engine}-repeat-{repeat}-report.json",
                note_directory / f"cycle-{engine}-repeat-{repeat}.log",
                repeat_wav,
                open_command,
                repeat_capture,
                arguments.reuse_wavs,
                v2_setup if engine == "v2" else v1_setup,
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
    analysis["scheduledMidiNotes"] = {
        "v1": cycle_v1_note(manifest, note),
        "v2": note,
    }
    analysis["rawExact"] = cycle_audio_diff.exact_sample_comparison(
        raw_capture(v1_wav), raw_capture(v2_wav))
    translated_graph_gain = graph_output_gain if graph_output_gain is not None else 1.0
    analysis["expectedGainFit"] = {
        "candidateScale": capture_v1["outputGain"]
                / (capture_v2["outputGain"] * translated_graph_gain),
    }
    analysis["repeatability"] = {
        "v1": repeatability(v1_wav, repeat_wavs["v1"]),
        "v2": repeatability(v2_wav, repeat_wavs["v2"]),
    }
    if arguments.capture_stages:
        analysis["stageCapture"] = compare_stage_captures(
            Path(capture_v1["stageCapturePath"]),
            Path(capture_v2["stageCapturePath"]),
        )
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
    parser.add_argument(
        "--disable-effect",
        action="append",
        default=[],
        help="disable a manifest-declared effect in both engines before capture",
    )
    parser.add_argument(
        "--v2-disable-node",
        action="append",
        default=[],
        help="disable a Cycle V2 node before capture for mismatch localization",
    )
    parser.add_argument(
        "--determinism-renders",
        type=int,
        default=2,
        choices=range(1, 6),
        metavar="1..5",
        help="render each engine repeatedly and require exact sample repeatability",
    )
    parser.add_argument("--allow-unverified", action="store_true")
    parser.add_argument("--reuse-wavs", action="store_true")
    parser.add_argument(
        "--capture-stages",
        action="store_true",
        help="capture and compare one oscillator frame at mature spectral boundaries",
    )
    parser.add_argument(
        "--stage-frame-index",
        type=int,
        default=0,
        help="zero-based oscillator frame to capture with --capture-stages",
    )
    parser.add_argument(
        "--stage-occurrence-index",
        type=int,
        default=0,
        help="zero-based repeated stage occurrence to capture within the selected frame",
    )
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
            f"cyclogram={analysis['cyclogram']['meanRowNormalizedDifference']:.4f} "
            f"raw-exact={analysis['rawExact']['samplesEqual']} "
            f"repeatable-v1={analysis['repeatability']['v1']['samplesEqual']} "
            f"repeatable-v2={analysis['repeatability']['v2']['samplesEqual']}"
            + (f" first-unequal-stage={analysis['stageCapture']['firstUnequalStage']}"
               if "stageCapture" in analysis else ""))
    if not report["passed"] and not arguments.no_fail:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
