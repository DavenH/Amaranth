"""Author corresponding harmonic and envelope models from measured sax anchors.

This describes graph model data; Cycle's canonical mesh, envelope and oscillator
implementations own interpolation and sound generation.
"""

import copy

import numpy as np


VOICE_SECONDS = 8.0
HARMONIC_COUNT = 48
SPECTRAL_TIMES = (0, .04, .12, .35, .8, 1.5, 3.5, 6.75, VOICE_SECONDS)
FIELDS = ("time", "key", "mod", "phase", "amp", "curve")


def revised(controls, model):
    revision = controls["model"]["revision"]
    model["revision"] = revision + 1
    if model["schema"] == "envelope":
        model["state"]["revision"] = revision + 1
    return {"nodeId": controls["nodeId"], "expectedModelRevision": revision, "model": model}


def cube(vertex_ids):
    return {"vertexIds": vertex_ids, "guides": {field: -1 for field in FIELDS},
            "gains": {field: .5 for field in FIELDS}}


def profile(anchor, seconds, channel):
    """Power-average a short observed interval into normalized harmonic targets."""
    center = min(max(.04, seconds), anchor["timings"]["selectedBodyEndSeconds"])
    frames = [frame for frame in anchor["trace"] if abs(frame["timeSeconds"] - center) <= .035]
    if not frames:
        raise ValueError("No reference observations at the authored time")
    amplitudes = np.array([frame["channelHarmonicPeakAmplitudes"][channel][:HARMONIC_COUNT]
                           for frame in frames])
    rms = np.sqrt(np.mean([frame["spectralRms"] ** 2 for frame in frames]))
    return np.sqrt(np.mean(amplitudes ** 2, axis=0)) * anchor["rawLevels"]["peak10msRms"] / max(rms, 1e-12)


def harmonic_model(controls, anchors, channel, phase=False):
    """Invert engine-exported coefficients into corresponding authored poles."""
    coordinates = controls["spectralAuthoring"]
    if coordinates["harmonicCount"] < HARMONIC_COUNT:
        raise ValueError("Spectral frame does not cover the chosen harmonic targets")
    # Stop at the next actual harmonic so the final fitted partial does not
    # interpolate across every unused bin toward a distant display-edge zero.
    stop_poles = int(coordinates["harmonicCount"] > HARMONIC_COUNT)
    positions = [0, *coordinates["positions"][:HARMONIC_COUNT + stop_poles], 1]
    times = (0, VOICE_SECONDS) if phase else SPECTRAL_TIMES
    if phase:
        phases = np.array([anchor["phaseTargets"]["channels"][channel]["settled"]["relativePhaseRadians"]
                           for anchor in anchors])[:, :HARMONIC_COUNT]
        if channel == 1:
            for index, anchor in enumerate(anchors):
                observed = anchor["phaseTargets"]["channels"]
                offset = observed[1]["settled"]["fundamentalPhaseRadians"] - observed[0]["settled"]["fundamentalPhaseRadians"]
                phases[index] = np.angle(np.exp(1j * (phases[index] + np.arange(1, HARMONIC_COUNT + 1) * offset)))
        phases[1] = phases[0] + np.angle(np.exp(1j * (phases[1] - phases[0])))
        coefficients = np.array(coordinates["phaseScales"][:HARMONIC_COUNT])
        values = .5 + phases / (2 * coefficients)
        values = np.pad(values, ((0, 0), (1, 1 + stop_poles)), constant_values=.5)
        amplitudes = np.repeat(values[:, None, :], len(times), axis=1)
    else:
        targets = np.array([[profile(anchor, seconds, channel) for seconds in times]
                             for anchor in anchors])
        # A real cosine's normalized positive-frequency FFT magnitude is A / 2.
        amplitudes = (targets / (2 * coordinates["additiveUnitScale"])) ** (
                1 / coordinates["additiveExponent"])
        amplitudes = np.pad(amplitudes, ((0, 0), (0, 0), (1, 1 + stop_poles)))
    if not np.isfinite(amplitudes).all() or np.min(amplitudes) < 0 or np.max(amplitudes) > 1:
        raise ValueError("Harmonic targets exceed the inspected model's authoring range")
    vertices, cubes = [], []
    indices = {}
    for pole, position in enumerate(positions):
        for time_index, seconds in enumerate(times):
            for red in (0, 1):
                for blue in (0, 1):
                    identifier = len(vertices)
                    indices[pole, time_index, red, blue] = identifier
                    vertices.append({"id": identifier, "time": seconds / VOICE_SECONDS,
                        "key": float(red), "mod": float(blue), "phase": position,
                        "amp": float(amplitudes[blue, time_index, pole]), "weight": 1.0})
        for time_index in range(len(times) - 1):
            cubes.append(cube([indices[pole, time_index + offset, red, blue]
                               for offset in (0, 1) for red in (0, 1) for blue in (0, 1)]))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": ("Phase" if phase else "Harmonics") + (" L" if channel == 0 else " R"),
                     "version": 1, "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def volume_model(controls, anchors):
    body_times = (0, .01, .02, .03, .04, .05, .075, .1, .15, .2, .3, .5, 1, 1.5, 2, 3, 4, 5)
    release_offsets = (.05, .1, .15, .2, .3, .4, .5, .65, .85)
    targets = []
    for anchor in anchors:
        release = anchor["timings"]["selectedTailStartSeconds"]
        times = [*body_times, release, *(release + offset for offset in release_offsets)]
        observed_times = [point["timeSeconds"] for point in anchor["volumeControlTargets"]]
        observed_rms = [point["rms"] for point in anchor["volumeControlTargets"]]
        # Keep the volume contour near unity. The mature envelope materializer
        # inserts a release pole with a .5 amplitude floor; absolute recording
        # gain belongs in the harmonic source so a quiet sustain stays continuous.
        gains = np.interp(times, observed_times, observed_rms, right=0) / anchor["rawLevels"]["peak10msRms"]
        gains[0], gains[-1] = 0, 0
        targets.append(list(zip(times, gains)))
    vertices, cubes = [], []
    for pole in range(len(targets[0])):
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    seconds, gain = targets[blue][pole]
                    vertices.append({"id": len(vertices), "time": float(time), "key": float(red),
                        "mod": float(blue), "phase": seconds / VOICE_SECONDS,
                        "amp": float(gain), "weight": 1.0})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["state"] = {"version": 2, "type": "envelope", "revision": model["revision"],
        "mesh": {"name": "Measured amplitude", "mainMesh": {"name": "Measured amplitude", "version": 1,
            "vertices": vertices, "cubes": cubes}, "loopIndices": [], "sustainIndices": [len(body_times)]},
        "logarithmic": False, "red": .5, "blue": 0, "cubeIds": list(range(1, len(cubes) + 1))}
    return revised(controls, model)
