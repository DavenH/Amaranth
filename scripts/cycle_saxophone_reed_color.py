"""Author a broad quiet spectral tilt using one pressure-scaled component Guide.

Cycle owns the log-frequency map, Guide rasterization and spectral transfer.
Frequency labels describe the inspected effective oscillator note, not a new
fixed-Hz filter. The separate body IR supplies that fixed-Hz response.
"""

from bisect import bisect_left
import copy
import math

from cycle_saxophone_anchor_model import cube, revised


QUIET_PROFILE_HZ_DB = ((0, 0), (500, 0), (2200, -6), (6000, -14), (22050, -18))


def color_model(controls):
    """Two poles; pressure fades the component Guide to exact raw neutral .5."""
    vertices, cubes = [], []
    for pole in (0, 1):
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    vertices.append({"id": len(vertices), "time": float(time),
                        "key": float(red), "mod": float(blue), "phase": float(pole),
                        "amp": .5, "weight": float(1 - blue if pole == 0 else 1)})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": "Pressure spectral color", "version": 1,
                     "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def _frequency_position(frequency, harmonic_hz, positions):
    """Interpolate inspected observations without reproducing LogRegionMapping."""
    index = bisect_left(harmonic_hz, frequency)
    if index == 0:
        return positions[0]
    if index == len(harmonic_hz):
        return positions[-1]
    fraction = (frequency - harmonic_hz[index - 1]) / (harmonic_hz[index] - harmonic_hz[index - 1])
    return positions[index - 1] + fraction * (positions[index] - positions[index - 1])


def color_resources(template_guide, controls):
    """Return one noiseless quiet-tilt Guide and its valid cube-zero assignment.

    Require unit node gain so blue=1's raw .5 is neutral for MultiplyBipolar.
    Cube gain remains the canonical stored .5 (unit gain). Quiet target M uses
    raw u=.5*M**(1/D); the component Guide stores u, yielding offset u-.5.
    """
    metadata = controls["spectralAuthoring"]
    positions = list(metadata["positions"])
    frequency = float(metadata["frequencyHz"])
    exponent = float(metadata["additiveExponent"])
    gain = float(metadata["gain"])
    if not math.isclose(gain, 1, rel_tol=1e-6) or not math.isfinite(exponent) or exponent <= 0:
        raise ValueError("Quiet color requires unit spectral node gain and a positive inspected exponent")
    if not positions or len(positions) != metadata["regionSize"]:
        raise ValueError("Inspect with enough cycleFrames to cover the complete spectral mapping")
    if not math.isfinite(frequency) or frequency <= 0:
        raise ValueError("Inspect at the effective oscillator's positive fundamental frequency")
    harmonic_hz = [0, *(frequency * harmonic for harmonic in range(1, len(positions) + 1))]
    positions = [0, *positions]
    if harmonic_hz[-1] < QUIET_PROFILE_HZ_DB[-1][0]:
        raise ValueError("Inspected spectral mapping does not cover the requested tilt")
    points = []
    for hz, decibels in QUIET_PROFILE_HZ_DB:
        position = _frequency_position(hz, harmonic_hz, positions)
        raw = .5 * (10 ** (decibels / 20)) ** (1 / exponent) / gain
        points.append((position, raw))
    if points[-1][0] < 1:
        points.append((1, points[-1][1]))

    guide = copy.deepcopy(template_guide)
    guide.update(id="reedQuietTilt", name="Quiet reed spectral tilt", shortLabel="Tilt",
                 enabled=True, noise=0, dcOffset=0, phase=0, revision=1)
    guide["model"]["revision"] = 1
    state = guide["model"]["state"]
    state["revision"] = 1
    # GuideCurvePreparation samples [0.05, 0.95]; IR padding is a separate domain.
    state["vertices"] = [{"id": index + 1, "x": .05 + .9 * position,
        "y": raw, "curve": 1.0} for index, (position, raw) in enumerate(points)]
    return {"guides": [guide], "guideAssignments": [{"guideId": guide["id"],
        "targetNodeId": controls["nodeId"], "target": {"kind": "trimeshCubeComponent",
            "cubeIndex": 0, "field": "time"}}]}
