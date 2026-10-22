"""Author quiet aspiration through the reed's shared body response."""

import copy

from cycle_saxophone_anchor_model import cube, revised


NOISE_GUIDE_IDS = ("reedGrain", "reedAspiration")


def air_model(controls):
    vertices, cubes = [], []
    for phase in (0, .5):
        start = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    vertices.append({"id": len(vertices), "time": float(time),
                        "key": float(red), "mod": float(blue), "phase": float(phase),
                        "amp": .5, "weight": 1 - .4 * blue})
        cubes.append(cube(list(range(start, start + 8))))
    model = copy.deepcopy(controls["model"])
    model["mesh"] = {"name": "Body-filtered aspiration", "version": 1,
                     "vertices": vertices, "cubes": cubes}
    return revised(controls, model)


def air_resources(template, controls):
    guide = copy.deepcopy(template)
    guide.update(id="reedAspiration", name="Body-filtered aspiration", shortLabel="Air",
        shelfOrder=template.get("shelfOrder", 0) + 1, enabled=True, noise=.2,
        dcOffset=0, phase=0, noiseSeed=8627, revision=1)
    guide.pop("heatmapAssetId", None)
    guide["model"]["revision"] = 1
    guide["model"]["state"].update(revision=1, vertices=[
        {"id": 1, "x": .05, "y": .5, "curve": 1},
        {"id": 2, "x": .95, "y": .5, "curve": 1}])
    return {"guides": [guide], "guideAssignments": [{"guideId": guide["id"],
        "targetNodeId": controls["nodeId"], "target": {"kind": "trimeshCubeComponent",
            "cubeIndex": pole, "field": "time"}} for pole in (0, 1)]}
