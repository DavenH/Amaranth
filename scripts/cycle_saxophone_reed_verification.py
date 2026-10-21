"""Render observable source/filter invariants through the existing audio command."""

import json
from pathlib import Path

import numpy as np

from cycle_audio_diff import midi_frequency, read_wav
from cycle_saxophone_anchor_analysis import half_lattice_profile
from cycle_saxophone_anchor_model import VOICE_SECONDS
from cycle_study_artifacts import write


def verify_reed(output, invoke):
    results = json.loads((output / "results.json").read_text())
    preset = results["preset"]
    ids = results["ids"]
    evidence = {"schema": "cycle-reed-verification.v1", "ok": False, "captures": {}}

    def parameter(node, key, value):
        invoke({"command": "setNodeParameter", "nodeId": node, "parameterId": key, "value": str(value)})

    def capture(name, pressure=127, instability=0, seed=1729):
        path = output / (name + ".wav")
        invoke({"command": "captureAudio", "sampleRate": 48000, "blockSize": 256,
            "channels": 2, "randomSeed": seed, "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS,
            "note": 62, "velocity": 1, "noteDurationMs": 1800, "durationMs": 2300,
            "events": [{"type": "controller", "controller": cc, "value": value, "timeMs": 0}
                for cc, value in ((1, 0), (2, pressure), (3, instability))], "path": str(path)})
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
        return decoded

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
        invoke({"command": "setGuideParameter", "guideId": "reedGrain", "parameterId": "noise", "value": "0"})
        capture("tonal-filtered")
        invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        invoke({"command": "setGuideParameter", "guideId": "reedGrain", "parameterId": "noise", "value": "0"})
        capture("tonal-filtered-repeated")
        evidence["guideEditSurvivesReload"] = (evidence["captures"]["tonal-filtered"]["pcmSha256"]
            == evidence["captures"]["tonal-filtered-repeated"]["pcmSha256"])
        if not evidence["guideEditSurvivesReload"]:
            raise RuntimeError("Repeated Guide edit after reload produced different audio")
        parameter(ids["body"], "enabled", 0)
        capture("tonal-source")
        parameter(ids["body"], "enabled", 1)
        capture("alternating-closure", instability=100)
        for name in ("tonal-source", "tonal-filtered"):
            if evidence["captures"][name]["settledOddEvenDb"] > -55:
                raise RuntimeError("Settled two-copy tonal symmetry is not preserved: " + name)
        if evidence["captures"]["alternating-closure"]["settledOddEvenDb"] < -40:
            raise RuntimeError("Alternating closure did not expose odd content")
        if not evidence["seedChangesPcm"]:
            raise RuntimeError("Stochastic render did not change with seed")
        evidence["grainAblationChangesPcm"] = (evidence["captures"]["repeat"]["pcmSha256"]
            != evidence["captures"]["tonal-filtered"]["pcmSha256"])
        if not evidence["grainAblationChangesPcm"]:
            raise RuntimeError("Guide-noise ablation did not change audio")
        evidence["ok"] = True
    finally:
        invoke({"command": "openGraph", "path": preset, "includeSnapshot": False})
        write(output / "verification.json", evidence)
    return evidence
