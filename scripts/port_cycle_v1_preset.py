#!/usr/bin/env python3

"""Port the supported Cycle 1 canonical preset shape to a Cycle 2 graph.

Cycle 1 must first export the legacy .cyc document as canonical preset JSON.
This script translates ownership and routing while preserving authored meshes,
guide curves, envelope meshes, and cube-component guide assignments.
"""

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path


MESH_GROUPS = {
    "guides": 3,
    "time": 4,
    "magnitude": 5,
    "phase": 6,
    "waveshaper": 9,
}

LEGACY_MIDI_REFERENCE_OFFSET = -12

DEFAULT_MODULATION_MAPPINGS = [
    {"in": 1, "out": 100, "dim": 0},
    {"in": 4, "out": 100, "dim": 1},
    {"in": 2, "out": 100, "dim": 2},
    {"in": 1, "out": 200, "dim": 0},
    {"in": 4, "out": 200, "dim": 1},
    {"in": 2, "out": 200, "dim": 2},
    {"in": 1, "out": 300, "dim": 0},
    {"in": 4, "out": 300, "dim": 1},
    {"in": 2, "out": 300, "dim": 2},
    {"in": 4, "out": 400, "dim": 1},
    {"in": 2, "out": 400, "dim": 2},
    {"in": 4, "out": 450, "dim": 1},
    {"in": 2, "out": 450, "dim": 2},
    {"in": 4, "out": 500, "dim": 1},
    {"in": 2, "out": 500, "dim": 2},
]


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


def active_envelope_layer(preset, purpose):
    group = preset["envelopeProps"]["groups"][purpose]
    active = [layer for layer in group["layers"]
              if layer["properties"]["active"]]
    if len(active) > 1:
        raise ValueError(
            f"Expected at most one active {purpose} envelope, found {len(active)}")
    return active[0] if active else None


def translated_octave(octave_knob):
    mapped_octave = 4.0 * (octave_knob - 0.5) + 0.5
    preset_octave = math.floor(mapped_octave + 0.5)
    return preset_octave + LEGACY_MIDI_REFERENCE_OFFSET // 12


def envelope_node(preset, purpose, node_id, x, y, level=1.0):
    layer = active_envelope_layer(preset, purpose)
    if layer is None:
        raise ValueError(f"Cannot port an inactive {purpose} envelope")
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
            "level": level,
        },
        envelope_model(layer, morph),
    )


def convert(source):
    preset = source["preset"]
    groups = preset["meshLibrary"]["groups"]
    morph = preset["morphPanel"]
    position = morph["position"]
    axes = ["yellow", "red", "blue"]
    oscillator_knobs = preset["oscControls"]["knobs"]
    octave = translated_octave(oscillator_knobs[1])
    oversampling = preset["settings"]["OversampleFactorRltm"]

    time_layer = require_single_active_layer(groups, "time")
    magnitude_layer = require_single_active_layer(groups, "magnitude")
    phase_layer = require_single_active_layer(groups, "phase")
    guide_layers = groups[MESH_GROUPS["guides"]]["layers"]

    nodes = [
        node("voice", "voiceContext", 100, 520, {
            "domain": "waveform",
            "octave": octave,
            "pitch": 0.0,
            "portamento": False,
            "oversampling": f"{oversampling}x",
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
        node("ifft", "ifft", 1750, 500, {"cycleFrames": 2048, "mode": "cyclic"}),
    ])
    if magnitude_layer["mesh"]["vertices"]:
        nodes.extend([
        node("magnitudeLayer1", "trilinearMesh", 850, 100,
             mesh_parameters, trimesh_model(magnitude_layer["mesh"])),
        node("magnitudeLayer1Process", "spectralLayer", 1190, 160, {
            "pan": magnitude_layer["properties"]["pan"],
            "range": magnitude_layer["properties"]["range"],
            "mode": "additive" if magnitude_layer["properties"]["mode"] == 0
                    else "multiplicative",
        }),
        node("magnitudeOp1", "multiply", 1480, 340),
        ])
    if phase_layer["mesh"]["vertices"]:
        nodes.extend([
        node("phaseLayer1", "trilinearMesh", 850, 780,
             mesh_parameters, trimesh_model(phase_layer["mesh"])),
        node("phaseLayer1Process", "spectralLayer", 1190, 800, {
            "pan": phase_layer["properties"]["pan"],
            "range": phase_layer["properties"]["range"],
            "mode": "additive",
        }),
        node("phaseOp1", "add", 1480, 690),
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
            "revision": 1,
            "model": flat_curve_model(layer["mesh"]),
        })

    waveshaper = preset["effects"]["Waveshaper"]
    waveshaper_layer = groups[MESH_GROUPS["waveshaper"]]["layers"][0]
    if waveshaper["enabled"]:
        nodes.append(node("waveshaper", "waveshaper", 2050, 500, {
            "enabled": True,
            "pre": waveshaper["knobs"][0],
            "post": waveshaper["knobs"][1],
            "aaFactor": str(waveshaper["oversampleFactor"]),
        }, flat_curve_model(waveshaper_layer["mesh"])))

    volume_layer = active_envelope_layer(preset, "volume")
    scratch_layer = active_envelope_layer(preset, "scratch")
    if volume_layer is not None:
        nodes.append(envelope_node(
            preset, "volume", "volumeEnvelope", 2050, 180))
        nodes.append(node("volumeMultiply", "multiply", 2350, 500))
    if scratch_layer is not None:
        nodes.append(envelope_node(
            preset, "scratch", "scratchEnvelope", 850, 1080))
    nodes.append(node("output", "output", 2650, 500))

    edges = [
        edge("voice", "context", "timeLayer1", "context"),
        edge("timeLayer1", "out", "fft", "time"),
        edge("morph", "modulation", "voice", "modulation",
             "configurationAttachment", "modulationTriple"),
    ]
    if magnitude_layer["mesh"]["vertices"]:
        edges.extend([
            edge("magnitudeLayer1", "out", "magnitudeLayer1Process", "in"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1Process", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
        ])
    else:
        edges.append(edge("fft", "mag", "ifft", "mag"))
    if phase_layer["mesh"]["vertices"]:
        edges.extend([
            edge("phaseLayer1", "out", "phaseLayer1Process", "in"),
            edge("fft", "phase", "phaseOp1", "left"),
            edge("phaseLayer1Process", "out", "phaseOp1", "right"),
            edge("phaseOp1", "out", "ifft", "phase"),
        ])
    else:
        edges.append(edge("fft", "phase", "ifft", "phase"))
    signal_node = "ifft"
    signal_port = "time"
    if waveshaper["enabled"]:
        edges.append(edge(signal_node, signal_port, "waveshaper", "time"))
        signal_node = "waveshaper"
    if volume_layer is not None:
        edges.append(edge(signal_node, "time", "volumeMultiply", "left"))
        edges.append(edge("volumeEnvelope", "env", "volumeMultiply", "right"))
        signal_node = "volumeMultiply"
        signal_port = "out"
    edges.append(edge(signal_node, signal_port, "output", "time"))
    if scratch_layer is not None and magnitude_layer["mesh"]["vertices"]:
        edges.append(edge(
            "scratchEnvelope", "env", "magnitudeLayer1", "scratch",
            "processingAttachment", "scratchEnvelope"))
    guide_assignments = []
    guide_assignments.extend(guide_assignments_for_layer(time_layer, "timeLayer1"))
    guide_assignments.extend(guide_assignments_for_layer(magnitude_layer, "magnitudeLayer1"))
    guide_assignments.extend(guide_assignments_for_layer(phase_layer, "phaseLayer1"))

    return {
        "format": "cycle-v2-graph",
        "formatVersion": 4,
        "nodes": nodes,
        "guides": guides,
        "guideHeatmaps": [],
        "guideAssignments": guide_assignments,
        "edges": edges,
        "probes": [],
    }


def validate_audio_parity_subset(source):
    preset = source["preset"]
    groups = preset["meshLibrary"]["groups"]
    issues = []

    for group_name in ("time", "magnitude", "phase"):
        layers = groups[MESH_GROUPS[group_name]]["layers"]
        active = [layer for layer in layers if layer["properties"]["active"]]
        active_count = len(active)
        if active_count != 1:
            issues.append(
                f"{group_name} requires exactly one active layer; found {active_count}")
            continue
        properties = active[0]["properties"]
        if properties["gain"] != 0.0 or properties["fineTune"] != 0.0:
            issues.append(f"{group_name} layer gain and fine tune must be neutral")
        if group_name == "time" and properties["pan"] != 0.5:
            issues.append("time layer pan must be centered")
        if group_name == "phase" and properties["mode"] != 0:
            issues.append("phase layer must use additive mode")

    unsupported_effects = ("ImpulseModeller", "Unison", "Delay", "Reverb", "EQ")
    for effect_name in unsupported_effects:
        if preset["effects"][effect_name]["enabled"]:
            issues.append(f"active {effect_name} is not supported by strict audio parity")

    envelope_groups = preset["envelopeProps"]["groups"]
    active_envelopes = {
        purpose: [
            layer for layer in envelope_groups[purpose]["layers"]
            if layer["properties"]["active"]
        ]
        for purpose in ("volume", "pitch", "scratch")
    }
    if active_envelopes["pitch"]:
        issues.append("active pitch envelope is not supported by strict audio parity")

    if preset["multisample"]["samples"]:
        issues.append("external multisamples are not supported by strict audio parity")
    if preset["modMatrix"]["mappings"] != DEFAULT_MODULATION_MAPPINGS:
        issues.append("modulation matrix differs from the supported fixed mapping")

    oversampling = preset["settings"]["OversampleFactorRltm"]
    if oversampling not in (1, 2, 4, 8):
        issues.append(f"unsupported realtime oversampling factor: {oversampling}")

    for guide in preset["guideCurveProps"]["guides"]:
        if guide["noiseLevel"] != 0.0:
            issues.append("Guide noise must be disabled for deterministic audio parity")

    if len(active_envelopes["volume"]) > 1:
        issues.append("strict audio parity supports at most one volume envelope")
    if len(active_envelopes["scratch"]) > 1:
        issues.append("strict audio parity supports at most one scratch envelope")
    magnitude_layers = groups[MESH_GROUPS["magnitude"]]["layers"]
    active_magnitude_layers = [
        layer for layer in magnitude_layers if layer["properties"]["active"]
    ]
    if (active_envelopes["scratch"]
            and len(active_magnitude_layers) == 1
            and not active_magnitude_layers[0]["mesh"]["vertices"]):
        issues.append("scratch envelope requires an authored magnitude layer")

    return issues


def file_sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def equivalence_manifest(source, source_document, destination, factory_preset):
    preset = source["preset"]
    duration = math.exp(8.0 * preset["oscControls"]["knobs"][2] - 3.0)
    master_gain = math.exp(6.0 * preset["oscControls"]["knobs"][0] - 3.0)
    octave = translated_octave(preset["oscControls"]["knobs"][1])
    source_document = source_document.resolve()
    destination = destination.resolve()
    repository = Path(__file__).resolve().parents[1]
    return {
        "schema": "cycle-v1-v2-audio-equivalence.v1",
        "status": "verified",
        "v1": {
            "factoryPreset": factory_preset,
            "sourceDocument": str(source_document.relative_to(repository)),
            "sha256": file_sha256(source_document),
        },
        "v2": {
            "graph": str(destination.relative_to(repository)),
            "sha256": file_sha256(destination),
            "renderOverrides": {
                "voiceDurationSeconds": duration,
            },
        },
        "translation": {
            "midiNoteOffset": 12 * octave,
            "legacyMidiReferenceOffset": LEGACY_MIDI_REFERENCE_OFFSET,
            "v1MasterGain": master_gain,
            "v2OutputHeadroom": 0.125,
            "timeLayer": "timeLayer1",
            "magnitudeLayer": "magnitudeLayer1 -> magnitudeLayer1Process",
            "phaseLayer": "phaseLayer1 -> phaseLayer1Process",
            "volumeEnvelope": "volumeEnvelope when active",
            "scratchEnvelope": "scratchEnvelope when active",
            "morph": "morph",
            "constantGainPolicy": "reported separately because Cycle2 realtime output has fixed headroom",
        },
        "thresholds": {
            "correlationMin": 0.98,
            "gainMatchedResidualMax": 0.20,
            "spectrumRmseDbMax": 6.0,
            "cyclogramMeanRowDifferenceMax": 0.20,
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="Cycle 1 canonical preset JSON")
    parser.add_argument("destination", type=Path, help="Cycle 2 .cyclegraph output")
    parser.add_argument("--strict-audio-parity", action="store_true")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--source-document", type=Path)
    parser.add_argument("--factory-preset")
    args = parser.parse_args()

    with args.source.open(encoding="utf-8") as source_file:
        source = json.load(source_file)
    if args.strict_audio_parity:
        issues = validate_audio_parity_subset(source)
        if issues:
            formatted = "\n".join(f"- {issue}" for issue in issues)
            raise ValueError(f"Preset is outside the strict audio parity subset:\n{formatted}")
    converted = convert(source)
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    with args.destination.open("w", encoding="utf-8") as destination_file:
        json.dump(converted, destination_file, indent=4)
        destination_file.write("\n")
    if args.manifest is not None:
        if not args.strict_audio_parity:
            raise ValueError("--manifest requires --strict-audio-parity")
        if args.source_document is None or not args.factory_preset:
            raise ValueError("--manifest requires --source-document and --factory-preset")
        manifest = equivalence_manifest(
            source, args.source_document, args.destination, args.factory_preset)
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        with args.manifest.open("w", encoding="utf-8") as manifest_file:
            json.dump(manifest, manifest_file, indent=4)
            manifest_file.write("\n")


if __name__ == "__main__":
    main()
