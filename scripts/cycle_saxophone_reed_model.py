"""Compact authored reed geometry; Cycle owns interpolation and synthesis."""

import copy
import wave

import numpy as np

from cycle_saxophone_anchor_model import VOICE_SECONDS, cube, revised
from cycle_saxophone_articulation import _loop_model


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

BODY_RECIPE = {"schema": "cycle-reed-broad-body.v1", "sampleRate": 48000, "frames": 1024,
    "controls": [
        {"shape": "lowShelf", "frequencyHz": 180, "gainDb": 2},
        {"shape": "bell", "frequencyHz": 550, "widthOctaves": .46, "gainDb": 5},
        {"shape": "bell", "frequencyHz": 1100, "widthOctaves": .40, "gainDb": -8.5},
        {"shape": "bell", "frequencyHz": 2500, "widthOctaves": .48, "gainDb": 5},
        {"shape": "highShelf", "frequencyHz": 3500, "gainDb": -14}],
    "excitation": {"quietCc32CornerHarmonic": 3, "loudCc127CornerHarmonic": 4.5, "order": 1.5},
    "purpose": "Five broad shared body controls; one pressure-dependent reed edge. Six consumed development probes, not a unique bore reconstruction."}


def body_impulse(path):
    """Write immutable minimum-phase response data from five broad controls.

    This is offline preset authoring. Cycle owns rate conversion, trimming,
    editable-curve modelling and convolution. No individual partial is fitted.
    """
    rate = BODY_RECIPE["sampleRate"]
    fft_size = 16384
    frequency = np.maximum(np.fft.rfftfreq(fft_size, 1 / rate), 1)
    decibels = np.zeros(len(frequency))
    for control in BODY_RECIPE["controls"]:
        ratio = frequency / control["frequencyHz"]
        if control["shape"] == "bell":
            shape = np.exp(-.5 * (np.log2(ratio) / control["widthOctaves"]) ** 2)
        elif control["shape"] == "lowShelf":
            shape = 1 / (1 + ratio ** 4)
        else:
            shape = 1 / (1 + ratio ** -4)
        decibels += control["gainDb"] * shape
    amplitude = 10 ** ((decibels - decibels.max()) / 20)
    cepstrum = np.fft.irfft(np.log(np.maximum(amplitude, 1e-8)), fft_size)
    causal = np.zeros(fft_size)
    causal[0], causal[fft_size // 2] = cepstrum[0], cepstrum[fft_size // 2]
    causal[1:fft_size // 2] = 2 * cepstrum[1:fft_size // 2]
    response = np.fft.irfft(np.exp(np.fft.rfft(causal)), fft_size)[:BODY_RECIPE["frames"]].copy()
    response[-64:] *= np.linspace(1, 0, 64)
    response /= np.max(abs(np.fft.rfft(response, 65536)))
    encoded = np.rint(response * (2 ** 31 - 1)).astype("<i4")
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
                        "weight": (.20 - .06 * blue) if local == 2 else 1.0})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": "Two reed closures", "version": 1,
                     "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def volume_model(controls, voice_seconds=VOICE_SECONDS):
    vertices, cubes = [], []
    for pole in range(5):
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    seconds = (0, .014 + .073 * red + .066 * (1 - blue), .160,
                               5.0, 5.090 + .020 * red)[pole]
                    # Combined with the external breath multiplier, this is
                    # monotonic x*(1-.45*x); sustain stays above the .5 floor.
                    amplitude = (0, 1, 1, 1, 0)[pole] * (1 - .45 * blue)
                    vertices.append({"id": len(vertices), "time": float(time),
                        "key": float(red), "mod": float(blue), "phase": seconds / voice_seconds,
                        "amp": amplitude, "weight": 1.0})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["state"] = {"version": 2, "type": "envelope", "revision": model["revision"],
        "mesh": {"name": "Tongue and breath", "mainMesh": {"name": "Tongue and breath",
            "version": 1, "vertices": vertices, "cubes": cubes},
            "loopIndices": [], "sustainIndices": [3]}, "logarithmic": False,
        "red": 0, "blue": .5, "cubeIds": list(range(1, 6))}
    return revised(controls, model)


def pitch_model(controls, voice_seconds=VOICE_SECONDS):
    """Reuse the existing delayed live vibrato loop; zero mod wheel is neutral."""
    model = _loop_model(voice_seconds, (4.5, 7.5), [.5] * 7,
        [.5, .5, .5125, .5, .4875, .5, .5])
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
    notes = [(.25, .34, 60, 80), (.85, 2.25, 62, 93),
        (3.55, .18, 62, 89), (4.05, .18, 65, 94), (4.55, .26, 67, 98),
        (5.20, .65, 69, 99), (5.78, .55, 67, 96), (6.26, .55, 65, 92), (6.74, 1.56, 62, 90),
        (8.70, .60, 48, 84), (9.22, .62, 50, 87), (9.76, .70, 60, 91), (10.38, 1.32, 62, 94),
        (12.05, .55, 65, 95), (12.53, .60, 69, 99), (13.06, 1.55, 72, 101), (15.15, 2.35, 62, 92)]
    lanes = {
        5: [(0, 24), (.15, 100), (3.15, 100), (3.30, 0), (4.90, 0),
            (5.05, 24), (8.45, 24), (8.60, 42), (11.85, 42), (12, 18),
            (14.8, 18), (15, 84), (17.75, 84), (17.95, 24)],
        2: [(0, 88), (.15, 64), (.80, 64), (1.60, 96), (2.55, 88), (3.10, 72),
            (3.40, 82), (4.90, 94), (5.20, 96), (6.75, 86), (8.30, 74),
            (8.55, 78), (9.60, 86), (10.6, 96), (11.70, 84), (11.95, 92),
            (13.60, 104), (14.60, 90), (15, 78), (15.75, 96), (16.35, 94),
            (17.40, 62), (17.65, 62), (17.95, 88)],
        3: [(0, 0), (15.80, 0), (16.02, 28), (16.22, 28), (16.45, 0), (17.95, 0)],
        1: [(0, 0), (1.90, 0), (2.35, 30), (2.75, 30), (3.05, 0),
            (7.20, 0), (7.65, 34), (8.05, 34), (8.25, 0),
            (13.60, 0), (14.05, 36), (14.35, 36), (14.60, 0),
            (15.85, 0), (16.65, 46), (17.15, 36), (17.40, 0), (17.95, 0)],
        76: [(0, 64), (13.4, 64), (13.65, 48), (14.35, 72), (14.70, 64), (17.95, 64)]}
    controls = [{"controller": cc, "timeSeconds": time, "value": value}
        for cc, points in lanes.items() for time, value in points]
    controls.sort(key=lambda value: (value["timeSeconds"], value["controller"]))
    return {"durationSeconds": 18.2, "notes": [
        {"pitch": note, "velocity": velocity, "startSeconds": start, "durationSeconds": duration}
        for start, duration, note, velocity in notes], "controls": controls}
