#!/usr/bin/env python3

"""Port the supported Cycle 1 canonical preset shape to a Cycle 2 graph.

Cycle 1 must first export the legacy .cyc document as canonical preset JSON.
This script translates ownership and routing while preserving authored meshes,
guide curves, envelope meshes, and cube-component guide assignments.
"""

import argparse
import copy
import json
from pathlib import Path


MESH_GROUPS = {
    "guides": 3,
    "time": 4,
    "magnitude": 5,
    "phase": 6,
    "waveshaper": 9,
}


def node(node_id, kind, x, y, parameters=None, model=None):
    result = {
        "id": node_id,
        "kind": kind,
        "definitionVersion": 1,
        "position": {"x": x, "y": y},
    }
    result["parameters"] = parameters if parameters is not None else {}
    if model is not None:
        result["model"] = model
    return result


def edge(source, source_port, destination, destination_port,
         connection_kind="signal", attachment_type="none"):
    return {
        "sourceNodeId": source,
        "sourcePortId": source_port,
        "destNodeId": destination,
        "destPortId": destination_port,
        "connectionKind": connection_kind,
        "attachmentType": attachment_type,
    }


def flat_curve_model(mesh):
    vertices = [
        {
            "id": vertex["id"] + 1,
            "x": vertex["phase"],
            "y": vertex["amp"],
            "curve": vertex["weight"],
        }
        for vertex in mesh["vertices"]
    ]
    return {
        "schema": "flatCurve",
        "version": 1,
        "revision": 1,
        "state": {
            "version": 1,
            "type": "flatCurve",
            "revision": 1,
            "vertices": vertices,
        },
    }


def trimesh_model(mesh):
    return {
        "schema": "trimesh",
        "version": 2,
        "revision": 1,
        "mesh": copy.deepcopy(mesh),
    }


def envelope_model(layer, morph):
    mesh = copy.deepcopy(layer["mesh"])
    cube_count = len(mesh["mainMesh"]["cubes"])
    return {
        "schema": "envelope",
        "version": 2,
        "revision": 1,
        "state": {
            "version": 2,
            "type": "envelope",
            "revision": 1,
            "mesh": mesh,
            "logarithmic": bool(layer["properties"].get("logarithmic", False)),
            "red": morph["position"]["red"],
            "blue": morph["position"]["blue"],
            "redLinked": bool(morph["linking"]["red"]),
            "blueLinked": bool(morph["linking"]["blue"]),
            "cubeIds": list(range(1, cube_count + 1)),
        },
    }


def require_single_active_layer(groups, group_name):
    layers = groups[MESH_GROUPS[group_name]]["layers"]
    active = [layer for layer in layers if layer["properties"]["active"]]
    if len(active) != 1:
        raise ValueError(
            f"Expected one active {group_name} layer, found {len(active)}")
    return active[0]


def guide_assignments_for_layer(layer, destination):
    field_names = {"key": "red", "mod": "blue"}
    result = []
    for cube_index, cube in enumerate(layer["mesh"]["cubes"]):
        for field, guide_index in cube["guides"].items():
            if guide_index < 0:
                continue
            target_field = field_names.get(field, field)
            result.append({
                "guideId": f"guide{guide_index + 1}",
                "targetNodeId": destination,
                "target": {
                    "kind": "trimeshCubeComponent",
                    "cubeIndex": cube_index,
                    "field": target_field,
                },
            })
    return result


def envelope_node(preset, purpose, node_id, x, y):
    group = preset["envelopeProps"]["groups"][purpose]
    layer = group["layers"][group["currentLayer"]]
    morph = preset["morphPanel"]
    return node(
        node_id,
        "envelope",
        x,
        y,
        {
            "purpose": purpose,
            "logarithmic": bool(layer["properties"].get("logarithmic", False)),
            "red": morph["position"]["red"],
            "blue": morph["position"]["blue"],
            "level": 1.0,
        },
        envelope_model(layer, morph),
    )


def convert(source):
    preset = source["preset"]
    groups = preset["meshLibrary"]["groups"]
    morph = preset["morphPanel"]
    position = morph["position"]
    axes = ["yellow", "red", "blue"]

    time_layer = require_single_active_layer(groups, "time")
    magnitude_layer = require_single_active_layer(groups, "magnitude")
    phase_layer = require_single_active_layer(groups, "phase")
    guide_layers = groups[MESH_GROUPS["guides"]]["layers"]

    nodes = [
        node("voice", "voiceContext", 100, 520, {
            "domain": "waveform",
            "octave": 1,
            "pitch": 0.0,
            "portamento": False,
            "oversampling": "1x",
        }),
        node("morph", "modulationTriple", 100, 100, {
            "yellowSource": "voiceTime",
            "yellowController": 1,
            "yellowConstant": position["time"],
            "redSource": "keyScale",
            "redController": 1,
            "redConstant": position["red"],
            "blueSource": "modWheel",
            "blueController": 1,
            "blueConstant": position["blue"],
        }),
    ]

    mesh_parameters = {
        "yellow": position["time"],
        "red": position["red"],
        "blue": position["blue"],
        "primaryAxis": axes[morph["primaryAxis"]],
    }
    nodes.extend([
        node("timeLayer1", "trilinearMesh", 480, 500,
             mesh_parameters, trimesh_model(time_layer["mesh"])),
        node("fft", "fft", 800, 500, {"cycleFrames": 2048, "mode": "cycle"}),
        node("magnitudeLayer1", "trilinearMesh", 850, 100,
             mesh_parameters, trimesh_model(magnitude_layer["mesh"])),
        node("magnitudeLayer1Process", "spectralLayer", 1190, 160, {
            "pan": magnitude_layer["properties"]["pan"],
            "range": magnitude_layer["properties"]["range"],
            "mode": "additive" if magnitude_layer["properties"]["mode"] == 0
                    else "multiplicative",
        }),
        node("magnitudeOp1", "multiply", 1480, 340),
        node("phaseLayer1", "trilinearMesh", 850, 780,
             mesh_parameters, trimesh_model(phase_layer["mesh"])),
        node("phaseLayer1Process", "spectralLayer", 1190, 800, {
            "pan": phase_layer["properties"]["pan"],
            "range": phase_layer["properties"]["range"],
            "mode": "additive",
        }),
        node("phaseOp1", "add", 1480, 690),
        node("ifft", "ifft", 1750, 500, {"cycleFrames": 2048, "mode": "cyclic"}),
    ])

    guide_props = preset["guideCurveProps"]["guides"]
    guides = []
    for index, layer in enumerate(guide_layers):
        props = guide_props[index]
        guides.append({
            "id": f"guide{index + 1}",
            "shortLabel": f"G{index + 1}",
            "name": "",
            "colourIndex": index,
            "shelfOrder": index,
            "enabled": True,
            "noise": props["noiseLevel"],
            "dcOffset": props["offsetLevel"],
            "phase": props["phaseLevel"],
            "model": flat_curve_model(layer["mesh"]),
        })

    nodes.append(envelope_node(preset, "volume", "volumeEnvelope", 2050, 180))
    nodes.append(envelope_node(preset, "scratch", "scratchEnvelope", 850, 1080))

    waveshaper = preset["effects"]["Waveshaper"]
    waveshaper_layer = groups[MESH_GROUPS["waveshaper"]]["layers"][0]
    nodes.extend([
        node("waveshaper", "waveshaper", 2050, 500, {
            "enabled": True,
            "pre": waveshaper["knobs"][0],
            "post": waveshaper["knobs"][1],
            "aaFactor": str(waveshaper["oversampleFactor"]),
        }, flat_curve_model(waveshaper_layer["mesh"])),
        node("volumeMultiply", "multiply", 2350, 500),
        node("output", "output", 2650, 500),
    ])

    edges = [
        edge("voice", "context", "timeLayer1", "context"),
        edge("timeLayer1", "out", "fft", "time"),
        edge("magnitudeLayer1", "out", "magnitudeLayer1Process", "in"),
        edge("fft", "mag", "magnitudeOp1", "left"),
        edge("magnitudeLayer1Process", "out", "magnitudeOp1", "right"),
        edge("phaseLayer1", "out", "phaseLayer1Process", "in"),
        edge("fft", "phase", "phaseOp1", "left"),
        edge("phaseLayer1Process", "out", "phaseOp1", "right"),
        edge("magnitudeOp1", "out", "ifft", "mag"),
        edge("phaseOp1", "out", "ifft", "phase"),
        edge("ifft", "time", "waveshaper", "time"),
        edge("waveshaper", "time", "volumeMultiply", "left"),
        edge("volumeEnvelope", "env", "volumeMultiply", "right"),
        edge("volumeMultiply", "out", "output", "time"),
        edge("morph", "modulation", "voice", "modulation",
             "configurationAttachment", "modulationTriple"),
        edge("scratchEnvelope", "env", "magnitudeLayer1", "scratch",
             "processingAttachment", "scratchEnvelope"),
    ]
    guide_assignments = []
    guide_assignments.extend(guide_assignments_for_layer(time_layer, "timeLayer1"))
    guide_assignments.extend(guide_assignments_for_layer(magnitude_layer, "magnitudeLayer1"))
    guide_assignments.extend(guide_assignments_for_layer(phase_layer, "phaseLayer1"))

    return {
        "format": "cycle-v2-graph",
        "formatVersion": 3,
        "nodes": nodes,
        "guides": guides,
        "guideAssignments": guide_assignments,
        "edges": edges,
        "probes": [],
        "presentation": {
            "performanceKeyboardBounds": {
                "x": 2050,
                "y": -220,
                "width": 496,
                "height": 184,
            },
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="Cycle 1 canonical preset JSON")
    parser.add_argument("destination", type=Path, help="Cycle 2 .cyclegraph output")
    args = parser.parse_args()

    with args.source.open(encoding="utf-8") as source_file:
        source = json.load(source_file)
    converted = convert(source)
    with args.destination.open("w", encoding="utf-8") as destination_file:
        json.dump(converted, destination_file, indent=4)
        destination_file.write("\n")


if __name__ == "__main__":
    main()
