#!/usr/bin/env python3
"""Build level-declared local auditions for the six shared reed/body probes."""

import argparse
import json
from pathlib import Path
import shutil
import wave

import numpy as np

from cycle_audio_diff import read_wav
from cycle_reference_study import file_digest


NOTES = {"C2": 48, "D3": 62, "C4": 72}
PRESSURES = {2: 32, 3: 127}
CLIP_SECONDS = 1.5
RMS_INTERVAL = (.4, 1.4)


def verified_audio(path, expected):
    path = Path(path).resolve()
    if file_digest(path) != expected:
        raise ValueError("Audition input identity changed: " + str(path))
    audio = read_wav(path)
    channels = np.asarray(audio["channels"], dtype=np.float64)
    if not np.isfinite(channels).all():
        raise ValueError("Non-finite audition audio: " + str(path))
    return channels, audio["sampleRate"]


def write_pcm32(path, channels, rate):
    if np.max(channels) >= 1 or np.min(channels) < -1:
        raise ValueError("Audition gain would clip: " + str(path))
    encoded = np.clip(np.rint(channels.T * 2147483648), -2147483648, 2147483647).astype("<i4")
    with wave.open(str(path), "wb") as output:
        output.setparams((len(channels), 4, rate, channels.shape[1], "NONE", "not compressed"))
        output.writeframes(encoded.tobytes())


def excerpt(channels, rate):
    frames = round(rate * CLIP_SECONDS)
    if channels.shape[1] < frames:
        raise ValueError("Audition input is shorter than the requested 1.5 seconds")
    return channels[:, :frames]


def rms(channels, rate):
    start, end = (round(rate * time) for time in RMS_INTERVAL)
    return float(np.sqrt(np.mean(channels[:, start:end] ** 2)))


def build_reed_audit(results_path, profiles_path, output, alternatives_path=None):
    results_path, profiles_path, output = results_path.resolve(), profiles_path.resolve(), output.resolve()
    results = json.loads(results_path.read_text())
    profiles = json.loads(profiles_path.read_text())
    if results["schema"] != "cycle-saxophone-reed-study.v1" or profiles["schema"] != "cycle-saxophone-reed-core-analysis.v1":
        raise ValueError("Unsupported reed results or source profiles")
    preset = Path(results["preset"])
    if file_digest(preset) != results["presetSha256"]:
        raise ValueError("Rendered reed preset identity changed")
    output.mkdir(parents=True, exist_ok=True)
    renders = {(item["note"], item["cc2"]): item for item in results["renders"]}
    if len(renders) != len(results["renders"]):
        raise ValueError("Duplicate register/pressure render")
    expected = {(note, pressure) for note in NOTES.values() for pressure in PRESSURES.values()}
    selected = {}
    for profile in profiles["profiles"]:
        if profile["noteToken"] in NOTES and profile["sourceLayer"] in PRESSURES:
            key = NOTES[profile["noteToken"]], PRESSURES[profile["sourceLayer"]]
            if key in selected:
                raise ValueError("Duplicate source profile for " + str(key))
            selected[key] = profile
    if set(selected) != expected or not expected.issubset(renders):
        raise ValueError("Six source/render register-pressure pairs are required")

    def media(path):
        return {"path": str(path.relative_to(output)), "sha256": file_digest(path)}

    def local_copy(path):
        path = Path(path).resolve()
        if path.parent == output:
            return path
        destination = output / path.name
        if path != destination:
            shutil.copyfile(path, destination)
        return destination

    comparisons, bridge, measurements = [], [], []
    for (note, pressure), profile in sorted(selected.items()):
        rendered = renders[note, pressure]
        source, source_rate = verified_audio(profile["sourcePath"], profile["sourceSha256"])
        synth, synth_rate = verified_audio(rendered["path"], rendered["sha256"])
        source, synth = excerpt(source, source_rate), excerpt(synth, synth_rate)
        source_rms, synth_rms = rms(source, source_rate), rms(synth, synth_rate)
        if min(source_rms, synth_rms) <= 0:
            raise ValueError("Silent RMS matching interval")
        gain = source_rms / synth_rms
        gain_db = float(20 * np.log10(gain))
        stem = f"audit-{note}-{pressure}"
        source_clip, synth_clip, raw_clip = (output / (stem + suffix + ".wav")
            for suffix in ("-source", "-synth-rms", "-synth-raw"))
        write_pcm32(source_clip, source, source_rate)
        write_pcm32(synth_clip, synth * gain, synth_rate)
        write_pcm32(raw_clip, synth, synth_rate)
        render_id = f"reed-{note}-{pressure}-{rendered['sha256'][:12]}"
        comparisons.append({"referenceId": "vcsl:" + profile["id"], "renderId": render_id,
            "source": media(source_clip), "synth": media(synth_clip), "rawSynth": media(raw_clip),
            "note": f"First 1.50 seconds from both original starts. Synth gain ×{gain:.6f} ({gain_db:+.3f} dB), "
                "one constant across both channels and the entire excerpt, matches channel-energy RMS at 0.40–1.40 s. "
                "Raw synth level is available below. No time or frequency alignment; no fades. "
                f"Source filename {profile['noteToken']}, layer {profile['sourceLayer']}; standard MIDI {note}, CC2 {pressure}. "
                "One shared reed/body model across all three registers. These references are consumed development probes."})
        full_render = local_copy(rendered["path"])
        bridge.append({"id": f"register-{note}-{pressure}", "label": f"{profile['noteToken']} · {pressure}",
            "renderId": render_id, **media(full_render),
            "note": f"Full raw capture: standard MIDI {note}, CC2 {pressure}. No audition gain."})
        measurements.append({"referenceId": "vcsl:" + profile["id"], "renderId": render_id,
            "sourcePath": profile["sourcePath"], "sourceSha256": profile["sourceSha256"],
            "renderPath": rendered["path"], "renderSha256": rendered["sha256"],
            "excerptSeconds": [0, CLIP_SECONDS], "rmsIntervalSeconds": RMS_INTERVAL,
            "sourceRms": source_rms, "rawSynthRms": synth_rms, "synthGain": gain,
            "synthGainDb": gain_db, "matchedSynthRms": rms(synth * gain, synth_rate)})
    for filename in ("phrase.wav", "demonstration.wav"):
        phrase = results_path.parent / filename
        if phrase.is_file():
            bridge.append({"id": "phrase", "label": "Phrase", "renderId": "reed-phrase-" + file_digest(phrase)[:12],
                **media(local_copy(phrase)), "note": "Full raw phrase capture; no audition gain."})
            break
    articulation_ids = set()
    for item in results.get("articulations", []):
        identifier = item["id"]
        if not identifier.replace("-", "").isalnum() or identifier in articulation_ids:
            raise ValueError("Invalid or duplicate articulation ID: " + identifier)
        articulation_ids.add(identifier)
        verified_audio(item["path"], item["sha256"])
        clip = local_copy(item["path"])
        bridge.append({"id": "articulation-" + identifier, "label": item["label"],
            "renderId": "reed-articulation-" + identifier + "-" + item["sha256"][:12],
            **media(clip), "note": "Full raw articulation capture; no excerpt, audition gain, fades or alignment. "
                + item.get("note", "")})
    alternative_ids = set()
    if alternatives_path:
        alternatives_path = alternatives_path.resolve()
        alternatives = json.loads(alternatives_path.read_text())
        for item in alternatives["alternatives"]:
            identifier = item["id"]
            if not identifier.replace("-", "").isalnum() or identifier in alternative_ids:
                raise ValueError("Invalid or duplicate alternative ID: " + identifier)
            alternative_ids.add(identifier)
            original = Path(item["path"])
            if not original.is_absolute():
                original = alternatives_path.parent / original
            audio, rate = verified_audio(original, item["sha256"])
            clip = output / ("audit-alternative-" + identifier + ".wav")
            write_pcm32(clip, excerpt(audio, rate), rate)
            bridge.append({"id": "alternative-" + identifier, "label": item["label"],
                "renderId": item["renderId"], **media(clip), "note":
                "First 1.50 seconds at the original raw level; no gain, fades or alignment. "
                + item.get("note", "") + " Source capture: " + str(original.resolve())
                + "; SHA-256 " + item["sha256"] + "."})
    manifest = {"schema": "saxophone-anchor-comparisons.v1", "comparisons": comparisons,
        "bridge": {"title": "Shared reed/body · CC2", "note":
            "Full raw register/pressure captures retain the preset's original level. Labels follow source filenames "
            "C2/D3/C4 (standard MIDI 48/62/72); numbers are CC2 values. The six paired row excerpts use "
            "explicit per-pair RMS matching for timbre comparison. All six references are consumed development probes."
            + (" Articulation buttons play their complete raw captures." if articulation_ids else "")
            + (" Alternative D3 loud clips are the first 1.50 seconds at each version's raw level; "
               "their levels are not matched." if alternative_ids else ""),
            "clips": bridge}}
    (output / "comparisons.json").write_text(json.dumps(manifest, indent=2, allow_nan=False) + "\n")
    report = {"schema": "cycle-saxophone-reed-audit.v1", "resultsPath": str(results_path),
        "resultsSha256": file_digest(results_path), "profilesPath": str(profiles_path),
        "profilesSha256": file_digest(profiles_path), "presetPath": str(preset),
        "presetSha256": results["presetSha256"], "timeAlignment": "none", "frequencyAlignment": "none",
        "matching": "One scalar per pair; channel-energy RMS; no per-channel or time-varying gain",
        "measurements": measurements}
    if alternatives_path:
        report["alternativesManifestPath"] = str(alternatives_path)
        report["alternativesManifestSha256"] = file_digest(alternatives_path)
    (output / "audition-levels.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    return output / "comparisons.json"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--profiles", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--alternatives", type=Path,
        help="Optional JSON with alternatives containing id, label, path, sha256, renderId and note")
    args = parser.parse_args()
    print(build_reed_audit(args.results, args.profiles, args.output_dir, args.alternatives))


if __name__ == "__main__":
    main()
