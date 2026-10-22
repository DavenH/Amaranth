"""Author a compact pressure-dependent edge at shared played harmonics.

Cycle owns logarithmic coordinates, key normalization, Guide rasterization and
spectral multiplication. Callers supply inspected per-key coordinates for the
voice's octave=-1 configuration; no frequency map is reproduced here.
"""

import copy
import math

from cycle_saxophone_anchor_model import cube, revised


KEY_NOTES = (44, 48, 60, 72, 76)
REFERENCE_PLAYED_NOTE = 60
PLAYED_HARMONICS = (1, 2, 4, 8, 16, 32)
QUIET_BLUE = 32 / 127
BLUE_PLANES = (0.0, QUIET_BLUE, 1.0)
GUIDE_PREFIX = "reedEdgeKeyH"


def _checked_coordinates(coordinates):
    """Require complete inspected anchors and played-note key positions."""
    coordinates = {int(key): value for key, value in coordinates.items()}
    previous_key = 0.0
    for note in KEY_NOTES:
        row = coordinates[note]
        positions = list(row["positions"][:2 * PLAYED_HARMONICS[-1]])
        key = float(row["keyPosition"])
        if (row["effectiveMidi"] != note - 12 or len(positions) != 2 * PLAYED_HARMONICS[-1]
                or not math.isfinite(key) or not previous_key < key < 1
                or any(not math.isfinite(value) or not 0 < value <= 1 for value in positions)
                or any(a >= b for a, b in zip(positions, positions[1:]))):
            raise ValueError("Supply ordered inspected octave=-1 positions and played-note keyPosition")
        previous_key = key
    return coordinates


def _phase(coordinates, note, harmonic):
    # Each played harmonic occupies bin 2h in the doubled lower-octave reed.
    return float(coordinates[note]["positions"][2 * harmonic - 1])


def _settings(controls, recipe):
    metadata = controls["spectralAuthoring"]
    gain = float(metadata["gain"])
    exponent = float(metadata["additiveExponent"])
    quiet = float(recipe["quietCc32CornerHarmonic"])
    loud = float(recipe["loudCc127CornerHarmonic"])
    order = float(recipe["order"])
    if (not math.isclose(gain, 1, rel_tol=1e-6) or not math.isfinite(exponent) or exponent <= 0
            or any(not math.isfinite(value) or value <= 0 for value in (quiet, loud, order))):
        raise ValueError("Reed edge requires unit spectral gain and positive finite recipe controls")
    return exponent, gain, quiet, loud, order


def edge_model(controls, coordinates, recipe):
    """Return one semantic edit: 8 poles, 96 vertices and 16 pressure cubes.

    recipe supplies quietCc32CornerHarmonic, loudCc127CornerHarmonic and order.
    Target amplitude is 1/sqrt(1+(playedH/Hc)**(2*order)); invert the inspected
    MultiplyBipolar transfer M=(2*g*u)**D. Pressure below CC32 holds the quiet
    profile; above it the mesh interpolates inverse-transfer amplitudes. The
    already weak >H32 tail tapers to zero at the spectral region boundary.
    """
    coordinates = _checked_coordinates(coordinates)
    exponent, gain, quiet, loud, order = _settings(controls, recipe)
    vertices, cubes = [], []
    for harmonic in (0, *PLAYED_HARMONICS, None):
        phase = (0.0 if harmonic == 0 else 1.0 if harmonic is None else
                 _phase(coordinates, REFERENCE_PLAYED_NOTE, harmonic))
        first = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in BLUE_PLANES:
                    raw = 0.0
                    if harmonic is not None:
                        corner = loud if blue == 1 else quiet
                        target = 1 / math.sqrt(1 + (harmonic / corner) ** (2 * order))
                        raw = .5 * target ** (1 / exponent) / gain
                    vertices.append({"id": len(vertices), "time": float(time),
                        "key": float(red), "mod": blue, "phase": phase,
                        "amp": raw, "weight": 1.0})
        for interval in range(len(BLUE_PLANES) - 1):
            cubes.append(cube([first + time * 6 + red * 3 + interval + high
                               for time in (0, 1) for red in (0, 1) for high in (0, 1)]))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": "Reed edge: shared harmonic landmarks", "version": 1,
                     "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def edge_resources(template_guide, controls, coordinates):
    """Return 6 noiseless key Guides and 12 typed Red assignments.

    keyPosition must be inspected at the played note, independently of the
    effective-note spectral positions. Guides hold endpoint displacement
    outside MIDI 44..76. Supply the last existing Guide as template so new shelf
    orders follow it; other factory metadata is preserved in each new resource.
    """
    coordinates = _checked_coordinates(coordinates)
    guides, assignments = [], []
    for pole, harmonic in enumerate(PLAYED_HARMONICS, start=1):
        base = _phase(coordinates, REFERENCE_PLAYED_NOTE, harmonic)
        keys = [(0.0, KEY_NOTES[0]),
                *((float(coordinates[note]["keyPosition"]), note) for note in KEY_NOTES),
                (1.0, KEY_NOTES[-1])]
        points = []
        for progress, note in keys:
            value = .5 + _phase(coordinates, note, harmonic) - base
            if not 0 <= value <= 1:
                raise ValueError("Reed key rail exceeds the unipolar Guide authoring range")
            points.append({"id": len(points) + 1, "x": .05 + .9 * progress,
                           "y": value, "curve": 1.0})
        guide = copy.deepcopy(template_guide)
        guide.update(id=GUIDE_PREFIX + str(harmonic), name=f"Reed edge key rail H{harmonic}",
            shortLabel=f"H{harmonic}", enabled=True, noise=0, dcOffset=0, phase=0,
            revision=1, shelfOrder=int(template_guide.get("shelfOrder", 0)) + pole)
        guide.pop("heatmapAssetId", None)
        guide["model"]["revision"] = 1
        guide["model"]["state"].update(revision=1, vertices=points)
        guides.append(guide)
        for interval in range(len(BLUE_PLANES) - 1):
            assignments.append({"guideId": guide["id"], "targetNodeId": controls["nodeId"],
                "target": {"kind": "trimeshCubeComponent", "cubeIndex": pole * 2 + interval,
                           "field": "red"}})
    return {"guides": guides, "guideAssignments": assignments}
