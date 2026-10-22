"""Compact authored reed geometry; Cycle owns interpolation and synthesis."""

import copy
import wave

import numpy as np

from cycle_saxophone_anchor_model import VOICE_SECONDS, cube, revised


ONSET_SECONDS = .12
PULSE_WIDTH_QUIET = .005
PULSE_WIDTH_LOUD = .005
PULSE_PEAK = .9
CC3_ASYMMETRY = .22
CC3_PHASE_LAG_QUIET = .004
CC3_PHASE_LAG_LOUD = .014
ONSET_PHASE_LAG = .009
ONSET_PEAK_OFFSET = -.09
ONSET_GRAIN_SHARPNESS = .08

BODY_RECIPE = {"schema": "cycle-reed-modal-ir.v1", "sampleRate": 48000, "frames": 1024,
    "modes": [{"frequencyHz": 600, "bandwidthHz": 250, "relativeModalPeak": 1},
              {"frequencyHz": 2200, "bandwidthHz": 1000, "relativeModalPeak": .25}],
    "purpose": "Shared broad body color from six consumed VCSL development probes; not a unique bore reconstruction"}


def modal_impulse(path):
    """Write authored response data from two physical modes, not a runtime DSP.

    Each causal damped sinusoid is normalized at its own center frequency.
    Cycle's existing importer owns trimming, curve fitting and convolution.
    """
    rate = BODY_RECIPE["sampleRate"]
    time = np.arange(BODY_RECIPE["frames"]) / rate
    response = np.zeros(len(time))
    for mode in BODY_RECIPE["modes"]:
        frequency = mode["frequencyHz"]
        component = np.exp(-np.pi * mode["bandwidthHz"] * time) * np.sin(2 * np.pi * frequency * time)
        center_gain = abs(np.sum(component * np.exp(-2j * np.pi * frequency * time)))
        response += component * mode["relativeModalPeak"] / center_gain
    encoded = np.rint(response / np.max(abs(response)) * (2 ** 31 - 1)).astype("<i4")
    with wave.open(str(path), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(4)
        audio.setframerate(rate)
        audio.writeframes(encoded.tobytes())


def reed_model(controls):
    """Two corresponding five-pole kicks with one cube per pole.

    The waveform remains visible at full scale. Absolute listening level belongs
    in node gain. Pressure and CC3 retain independent mesh axes. Shared 1D Guide
    rails own the brief onset change, leaving both copies equal when settled and
    CC3 is zero. Time endpoints carry no redundant onset geometry.
    """
    vertices, cubes = [], []
    for pole in range(10):
        half, local = divmod(pole, 5)
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    width = PULSE_WIDTH_QUIET * (1 - blue) + PULSE_WIDTH_LOUD * blue
                    asymmetry = red * CC3_ASYMMETRY
                    shift = half * (CC3_PHASE_LAG_QUIET * (1 - blue)
                                    + CC3_PHASE_LAG_LOUD * blue) * red
                    amplitude = PULSE_PEAK / (1 - .8 * width)
                    baseline = -.8 * width * amplitude
                    phase = (shift, .2 * width + shift, .6 * width + shift,
                             width + shift, .499)[local] + half * .5
                    # Existing fixed-frame rendering samples on pitch-dependent
                    # grids. Align crests so narrow pulses retain their peaks.
                    phase = (phase - .2 * width) % 1
                    signal = baseline + (0, 1, .25, 0, 0)[local] * amplitude
                    if half:
                        signal *= 1 - asymmetry
                    identifier = len(vertices)
                    vertices.append({"id": identifier, "time": float(time),
                        "key": float(red), "mod": float(blue), "phase": phase,
                        "amp": .5 + signal / 2,
                        "weight": (.04 + .04 * blue) if local == 2 else 1.0})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": "Two reed closures", "version": 1,
                     "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def volume_model(controls):
    vertices, cubes = [], []
    for pole in range(5):
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    seconds = (0, .025 + .075 * red, .2, 5, 5.16)[pole]
                    amplitude = (0, 1, .82, .82, 0)[pole] * (.72 + .28 * blue)
                    vertices.append({"id": len(vertices), "time": float(time),
                        "key": float(red), "mod": float(blue), "phase": seconds / VOICE_SECONDS,
                        "amp": amplitude, "weight": 1.0})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["state"] = {"version": 2, "type": "envelope", "revision": model["revision"],
        "mesh": {"name": "Tongue and breath", "mainMesh": {"name": "Tongue and breath",
            "version": 1, "vertices": vertices, "cubes": cubes},
            "loopIndices": [], "sustainIndices": [3]}, "logarithmic": False,
        "red": 0, "blue": .5, "cubeIds": list(range(1, 6))}
    return revised(controls, model)


def _onset_guide(template, identifier, name, label, offset):
    """Use canonical Guide units: table value is stored y minus one half.

    The neutral guard holds the late rail flat despite finite curve sharpness.
    Unit cube gain is already supplied by the existing cube authoring helper.
    """
    guide = copy.deepcopy(template)
    guide.update(id=identifier, name=name, shortLabel=label, revision=1,
                 noise=0, dcOffset=0, phase=0)
    guide["model"]["revision"] = 1
    state = guide["model"]["state"]
    state["revision"] = 1
    state["vertices"] = [{"id": index + 1, "x": .05 + .9 * seconds / VOICE_SECONDS,
        "y": .5 + (offset if index == 0 else 0), "curve": 1.0}
        for index, seconds in enumerate((0, ONSET_SECONDS * 5 / 6, ONSET_SECONDS, VOICE_SECONDS))]
    return guide


def grain_resources(factory, reed):
    """Factory grain plus sparse phase, amplitude and sharpness onset rails."""
    assignment = next(item for item in factory["guideAssignments"]
        if item["targetNodeId"] == "timeLayer1" and item["target"] == {
            "kind": "trimeshCubeComponent", "cubeIndex": 1, "field": "time"})
    guide = copy.deepcopy(next(item for item in factory["guides"]
        if item["id"] == assignment["guideId"]))
    guide.update(id="reedGrain", name="Reed closure grain", shortLabel="Grain", shelfOrder=0)
    guides = [guide,
        _onset_guide(guide, "reedOnsetPhase", "Second closure onset lag", "Lag", ONSET_PHASE_LAG),
        _onset_guide(guide, "reedOnsetPeak", "Second closure onset depth", "Attack", ONSET_PEAK_OFFSET),
        _onset_guide(guide, "reedOnsetGrain", "Onset grain depth", "Grain+", ONSET_GRAIN_SHARPNESS / 2)]
    for index, resource in enumerate(guides):
        resource["shelfOrder"] = index
    assignments = [
        {"guideId": guide["id"], "targetNodeId": reed, "target": {
            "kind": "trimeshCubeComponent", "cubeIndex": pole, "field": "time"}}
        for pole in (2, 7)]
    assignments += [{"guideId": identifier, "targetNodeId": reed, "target": {
        "kind": "trimeshCubeComponent", "cubeIndex": pole, "field": field}}
        for identifier, poles, field in (("reedOnsetPhase", (5, 6, 7, 8), "phase"),
            ("reedOnsetPeak", (6,), "amp"), ("reedOnsetGrain", (2, 7), "curve"))
        for pole in poles]
    return {"guides": guides, "guideAssignments": assignments}


def sequence():
    pressure = [(0, 20), (2, 100), (4.3, 50)]
    pressure += [(6.3 + i * 2.2 / 32, round(i * 127 / 32)) for i in range(33)]
    pressure += [(8.5 + i * 1.8 / 24, round(127 - i * 107 / 24)) for i in range(1, 25)]
    instability = [(0, 0), (8.4, 0)]
    instability += [(8.5 + i / 24, round(i * 90 / 24)) for i in range(25)]
    instability += [(9.5 + i * .5 / 12, round(90 * (1 - i / 12))) for i in range(1, 13)]
    return {"durationSeconds": 13, "notes": [
        {"pitch": note, "velocity": 112, "startSeconds": start, "durationSeconds": duration}
        for note, start, duration in ((48, .2, 1.5), (62, 2, 2), (72, 4.4, 1.6),
            (62, 6.4, 4), (60, 10.8, .3), (64, 11.3, .3), (67, 11.8, .6))],
        "controls": [{"controller": cc, "timeSeconds": time, "value": value}
            for cc, points in ((1, ((0, 0), (4.3, 90), (6.2, 0))),
                (2, pressure), (3, instability)) for time, value in points]}
