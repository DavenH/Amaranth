"""Render observable source/filter invariants through the existing audio command."""

import copy
import json
from pathlib import Path

import numpy as np

from cycle_audio_diff import midi_frequency, read_wav
from cycle_saxophone_anchor_analysis import half_lattice_profile, harmonic_phase
from cycle_saxophone_anchor_model import VOICE_SECONDS, revised
from cycle_saxophone_reed_air import NOISE_GUIDE_IDS
from cycle_study_artifacts import write
from cycle_reference_study import file_digest


def render_articulations(output, invoke):
    """Short playable contrasts through the same saved preset and renderer."""
    initial = [{"type": "controller", "timeMs": 0, "channel": 1,
        "controller": cc, "value": value} for cc, value in ((1, 0), (2, 88), (3, 0), (5, 24), (76, 64))]

    def gate(note, start, end):
        return [{"type": "noteOn", "timeMs": start, "channel": 1, "note": note, "velocity": .75},
                {"type": "noteOff", "timeMs": end, "channel": 1, "note": note}]

    def cc(controller, value, time):
        return {"type": "controller", "timeMs": time, "channel": 1, "controller": controller, "value": value}

    cases = [
        ("tongue", "Tongue", gate(62, 150, 400) + gate(65, 550, 800) + gate(62, 950, 1200)),
        ("slur", "Slur", gate(62, 150, 800) + gate(65, 700, 1350) + gate(69, 1250, 2100)),
        ("vibrato", "Vibrato", gate(62, 150, 3000) + [cc(1, 64, 1000), cc(76, 0, 1500), cc(76, 127, 2200)]),
        ("roughness", "Roughness", gate(62, 150, 2100) + [cc(3, 28, 1100), cc(3, 0, 1400)])]
    renders = []
    for identifier, label, events in cases:
        path = output / ("articulation-" + identifier + ".wav")
        capture = invoke({"command": "captureAudio", "sampleRate": 48000, "blockSize": 256,
            "channels": 2, "randomSeed": 1729, "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS,
            "durationMs": 3400, "events": initial + events, "path": str(path)})
        if not 1e-7 < capture["peak"] < 1:
            raise RuntimeError("Invalid articulation audition level: " + identifier)
        renders.append({"id": identifier, "label": label, "path": str(path),
            "sha256": file_digest(path), "capture": capture})
    return renders


def verify_reed(output, invoke):
    results = json.loads((output / "results.json").read_text())
    preset = results["preset"]
    ids = results["ids"]
    graph = json.loads(Path(preset).read_text())
    noise_guides = [guide["id"] for guide in graph["guides"] if guide["id"] in NOISE_GUIDE_IDS]
    evidence = {"schema": "cycle-reed-verification.v1", "ok": False, "captures": {}}

    def parameter(node, key, value):
        invoke({"command": "setNodeParameter", "nodeId": node, "parameterId": key, "value": str(value)})

    def capture(name, pressure=127, instability=0, seed=1729):
        path = output / (name + ".wav")
        invoke({"command": "captureAudio", "sampleRate": 48000, "blockSize": 256,
            "channels": 2, "randomSeed": seed, "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS,
            "note": 62, "velocity": 1, "noteDurationMs": 1800, "durationMs": 2300,
            "events": [{"type": "controller", "controller": cc, "value": value, "timeMs": 0}
                for cc, value in ((1, 0), (2, pressure), (3, instability), (5, 0), (76, 64))], "path": str(path)})
        decoded = read_wav(path)
        signal = np.array(decoded["channels"])
        evidence["captures"][name] = {"pcmSha256": decoded["samplePayloadSha256"],
            "rms": float(np.sqrt(np.mean(signal ** 2))), "peak": float(np.max(abs(signal)))}
        if pressure:
            pitch = [{"timeSeconds": time, "frequencyHz": midi_frequency(62)} for time in (0, 2.3)]
            for label, start, end in (("onset", .01, .07), ("settled", .5, .8)):
                profile = half_lattice_profile(signal, decoded["sampleRate"], pitch, start, end, 32)
                peaks = np.array(profile["channelPeakAmplitudes"])
                ratio = np.sum(peaks[:, ::2] ** 2) / np.sum(peaks[:, 1::2] ** 2)
                evidence["captures"][name][label + "OddEvenDb"] = float(10 * np.log10(max(ratio, 1e-24)))
            if name == "tonal-source":
                profile = harmonic_phase(signal[0], decoded["sampleRate"], pitch, .5, .8, 40)
                evidence["captures"][name]["settledHarmonicDbToFundamental"] = profile["dbToFundamental"]
        return decoded

    def ablate_noise():
        for guide in noise_guides:
            invoke({"command": "setGuideParameter", "guideId": guide, "parameterId": "noise", "value": "0"})

    def replace_color(model):
        controls = invoke({"command": "inspectNodeControls", "nodeId": ids["color"]})
        invoke({"command": "applyNodeEdits", "expectedDocumentRevision": controls["documentRevision"],
            "edits": [revised(controls, model)]})

    try:
        invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        repeated = capture("repeat")
        original = read_wav(output / "reed-62-127.wav")
        evidence["repeatPcmEqual"] = repeated["samplePayloadSha256"] == original["samplePayloadSha256"]
        if not evidence["repeatPcmEqual"]:
            raise RuntimeError("Seeded repeat changed")
        capture("different-seed", seed=1730)
        evidence["seedChangesPcm"] = (evidence["captures"]["repeat"]["pcmSha256"]
            != evidence["captures"]["different-seed"]["pcmSha256"])
        capture("zero-breath", pressure=0)
        if evidence["captures"]["zero-breath"]["peak"] > 1e-7:
            raise RuntimeError("Zero breath is not silent")
        if "reedAspiration" in noise_guides:
            invoke({"command": "setGuideParameter", "guideId": "reedAspiration", "parameterId": "noise", "value": "0"})
            capture("without-aspiration")
            evidence["aspirationAblationChangesPcm"] = (evidence["captures"]["repeat"]["pcmSha256"]
                != evidence["captures"]["without-aspiration"]["pcmSha256"])
            if not evidence["aspirationAblationChangesPcm"]:
                raise RuntimeError("Aspiration ablation did not change audio")
            invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        invoke({"command": "setGuideParameter", "guideId": "reedGrain", "parameterId": "noise", "value": "0"})
        capture("without-grain")
        evidence["grainAblationChangesPcm"] = (evidence["captures"]["repeat"]["pcmSha256"]
            != evidence["captures"]["without-grain"]["pcmSha256"])
        if not evidence["grainAblationChangesPcm"]:
            raise RuntimeError("Guide grain ablation did not change audio")
        ablate_noise()
        capture("tonal-filtered")
        invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        ablate_noise()
        capture("tonal-filtered-repeated")
        evidence["guideEditSurvivesReload"] = (evidence["captures"]["tonal-filtered"]["pcmSha256"]
            == evidence["captures"]["tonal-filtered-repeated"]["pcmSha256"])
        if not evidence["guideEditSurvivesReload"]:
            raise RuntimeError("Repeated Guide edit after reload produced different audio")
        parameter(ids["body"], "enabled", 0)
        reed_gain = next(node for node in graph["nodes"]
            if node["id"] == ids["reed"])["parameters"]["gain"]
        color = invoke({"command": "inspectNodeControls", "nodeId": ids["color"]})["model"]
        neutral_color = copy.deepcopy(color)
        for vertex in neutral_color["mesh"]["vertices"]:
            vertex["amp"] = .5
        replace_color(neutral_color)
        parameter(ids["reed"], "gain", .5)
        capture("tonal-source")
        parameter(ids["reed"], "gain", reed_gain)
        replace_color(copy.deepcopy(color))
        parameter(ids["body"], "enabled", 1)
        capture("alternating-closure", instability=100)
        source_harmonics = evidence["captures"]["tonal-source"]["settledHarmonicDbToFundamental"]
        if any(abs(value) > 3 for value in source_harmonics):
            raise RuntimeError("Clean source does not retain H1-H40 within 3 dB")
        for name in ("tonal-source", "tonal-filtered"):
            profile = evidence["captures"][name]
            if profile["settledOddEvenDb"] > -55:
                raise RuntimeError("Settled two-copy tonal symmetry is not preserved: " + name)
            if (profile["onsetOddEvenDb"] < -40
                    or profile["onsetOddEvenDb"] - profile["settledOddEvenDb"] < 20):
                raise RuntimeError("Startup asymmetry is not distinct from settled symmetry: " + name)
        if evidence["captures"]["alternating-closure"]["settledOddEvenDb"] < -40:
            raise RuntimeError("Alternating closure did not expose odd content")
        if not evidence["seedChangesPcm"]:
            raise RuntimeError("Stochastic render did not change with seed")
        evidence["noiseAblationChangesPcm"] = (evidence["captures"]["repeat"]["pcmSha256"]
            != evidence["captures"]["tonal-filtered"]["pcmSha256"])
        if not evidence["noiseAblationChangesPcm"]:
            raise RuntimeError("Noise ablation did not change audio")
        evidence["ok"] = True
    finally:
        invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        write(output / "verification.json", evidence)
    return evidence
