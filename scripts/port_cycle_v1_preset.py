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
    "impulseResponse": 10,
}

ENVELOPE_GROUPS = {
    "volume": 0,
    "pitch": 1,
    "scratch": 2,
    "wavePitch": 8,
}

LEGACY_MIDI_REFERENCE_OFFSET = -12

MODULATION_SOURCE_NAMES = {
    1: "voiceTime",
    2: "velocity",
    3: "inverseVelocity",
    4: "keyScale",
    5: "aftertouch",
    101: "modWheel",
}

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


def envelope_layers(preset, purpose):
    group = preset["meshLibrary"]["groups"][ENVELOPE_GROUPS[purpose]]
    if "layers" in group:
        return group["layers"]
    return preset["envelopeProps"]["groups"][purpose]["layers"]


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
    active = [layer for layer in envelope_layers(preset, purpose)
              if layer["properties"]["active"]]
    if len(active) > 1:
        raise ValueError(
            f"Expected at most one active {purpose} envelope, found {len(active)}")
    return active[0] if active else None


def translated_octave(octave_knob):
    mapped_octave = 4.0 * (octave_knob - 0.5) + 0.5
    preset_octave = math.floor(mapped_octave + 0.5)
    return preset_octave + LEGACY_MIDI_REFERENCE_OFFSET // 12


def morph_state(preset):
    authored = preset.get("morphPanel") or {}
    return {
        "position": authored.get(
            "position", {"time": 0.5, "red": 0.5, "blue": 0.5}),
        "linking": authored.get(
            "linking", {"time": True, "red": False, "blue": False}),
        "primaryAxis": authored.get("primaryAxis", 0),
    }


def default_modulation_mappings_for_preset(preset, blue_input=2):
    groups = preset["meshLibrary"]["groups"]
    result = []
    for group_name, base_id in (("time", 100), ("magnitude", 200), ("phase", 300)):
        for index, _ in enumerate(groups[MESH_GROUPS[group_name]]["layers"]):
            output_id = base_id + 3 * index
            result.extend([
                {"in": 1, "out": output_id, "dim": 0},
                {"in": 4, "out": output_id, "dim": 1},
                {"in": blue_input, "out": output_id, "dim": 2},
            ])
    for purpose, base_id in (("volume", 400), ("pitch", 450), ("scratch", 500)):
        for index, _ in enumerate(envelope_layers(preset, purpose)):
            output_id = base_id + 2 * index
            result.extend([
                {"in": 4, "out": output_id, "dim": 1},
                {"in": blue_input, "out": output_id, "dim": 2},
            ])
    return result


def modulation_sources_for_preset(preset):
    actual = preset.get("modMatrix", {}).get("mappings")
    if actual is None:
        actual = default_modulation_mappings_for_preset(preset)

    for blue_input in (2, 101):
        if actual == default_modulation_mappings_for_preset(preset, blue_input):
            return {
                "yellow": "voiceTime",
                "red": "keyScale",
                "blue": MODULATION_SOURCE_NAMES[blue_input],
            }
    return None


def envelope_node(preset, layer, purpose, node_id, x, y, level=1.0):
    morph = morph_state(preset)
    return node(
        node_id,
        "envelope",
        x,
        y,
        {
            "enabled": bool(layer["properties"]["active"]),
            "purpose": purpose,
            "logarithmic": bool(layer["properties"].get("logarithmic", False)),
            "red": morph["position"]["red"],
            "blue": morph["position"]["blue"],
            "level": level,
        },
        envelope_model(layer, morph),
    )


def convert(source):
    issues = validate_conversion(source)
    if issues:
        formatted = "\n".join(f"- {issue}" for issue in issues)
        raise ValueError(f"Preset cannot be represented without loss:\n{formatted}")

    preset = source["preset"]
    groups = preset["meshLibrary"]["groups"]
    morph = morph_state(preset)
    position = morph["position"]
    axes = ["yellow", "red", "blue"]
    oscillator_knobs = list(preset["oscControls"].get("knobs", []))
    oscillator_knobs.extend([0.5] * (3 - len(oscillator_knobs)))
    octave = translated_octave(oscillator_knobs[1])
    oversampling = preset["settings"].get("OversampleFactorRltm", 1)
    modulation_sources = modulation_sources_for_preset(preset)
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
            "yellowSource": modulation_sources["yellow"],
            "yellowController": 1,
            "yellowConstant": position["time"],
            "redSource": modulation_sources["red"],
            "redController": 1,
            "redConstant": position["red"],
            "blueSource": modulation_sources["blue"],
            "blueController": 1,
            "blueConstant": position["blue"],
        }),
        node("fft", "fft", 1050, 500, {"cycleFrames": 2048, "mode": "cycle"}),
        node("ifft", "ifft", 2150, 500, {"cycleFrames": 2048, "mode": "cyclic"}),
    ]
    edges = [
        edge("morph", "modulation", "voice", "modulation",
             "configurationAttachment", "modulationTriple"),
    ]
    unison = preset["effects"]["Unison"]
    if unison["enabled"]:
        knobs = unison["knobs"]
        nodes.append(node("unison", "unison", 100, 300, {
            "enabled": True,
            "mode": "group",
            "order": min(10, int(10 * knobs[3] + 1)),
            "width": 70.0 * knobs[0],
            "panSpread": knobs[1],
            "phase": knobs[2],
            "jitter": knobs[4],
        }, {
            "schema": "unisonVoices",
            "version": 1,
            "revision": 1,
            "voices": [{"detune": 0.5, "pan": 0.5, "phase": 0.0}],
        }))
        edges.append(edge(
            "unison", "unison", "voice", "unison",
            "configurationAttachment", "unison"))
    guide_assignments = []
    mesh_parameters = {
        "yellow": position["time"],
        "red": position["red"],
        "blue": position["blue"],
        "primaryAxis": axes[morph["primaryAxis"]],
    }
    all_mesh_node_ids = []

    time_source = None
    for index, layer in enumerate(groups[MESH_GROUPS["time"]]["layers"], 1):
        layer_id = f"timeLayer{index}"
        parameters = dict(mesh_parameters)
        parameters["enabled"] = bool(layer["properties"]["active"])
        nodes.append(node(layer_id, "trilinearMesh", 470, 380 + 190 * index,
                          parameters, trimesh_model(layer["mesh"])))
        edges.append(edge("voice", "context", layer_id, "context"))
        guide_assignments.extend(guide_assignments_for_layer(layer, layer_id))
        all_mesh_node_ids.append(layer_id)
        if time_source is None:
            time_source = (layer_id, "out")
            continue
        operation_id = f"timeOp{index - 1}"
        nodes.append(node(operation_id, "add", 780, 430 + 95 * index))
        edges.append(edge(time_source[0], time_source[1], operation_id, "left"))
        edges.append(edge(layer_id, "out", operation_id, "right"))
        time_source = (operation_id, "out")
    edges.append(edge(time_source[0], time_source[1], "fft", "time"))

    def append_spectral_stack(group_name, fft_port, ifft_port, y, phase=False):
        signal = ("fft", fft_port)
        for index, layer in enumerate(groups[MESH_GROUPS[group_name]]["layers"], 1):
            layer_id = f"{group_name}Layer{index}"
            process_id = f"{layer_id}Process"
            operation_id = f"{group_name}Op{index}"
            parameters = dict(mesh_parameters)
            parameters["enabled"] = bool(layer["properties"]["active"])
            mode = "additive" if phase or layer["properties"]["mode"] == 0 \
                else "multiplicative"
            operation = "add" if mode == "additive" else "multiply"
            nodes.extend([
                node(layer_id, "trilinearMesh", 1150, y + 170 * (index - 1),
                     parameters, trimesh_model(layer["mesh"])),
                node(process_id, "spectralLayer", 1490, y + 170 * (index - 1), {
                    "pan": layer["properties"].get("pan", 0.5),
                    "range": layer["properties"].get("range", 0.5),
                    "mode": mode,
                }),
                node(operation_id, operation, 1810, y + 170 * (index - 1)),
            ])
            edges.extend([
                edge(layer_id, "out", process_id, "in"),
                edge(signal[0], signal[1], operation_id, "left"),
                edge(process_id, "out", operation_id, "right"),
            ])
            guide_assignments.extend(guide_assignments_for_layer(layer, layer_id))
            all_mesh_node_ids.append(layer_id)
            signal = (operation_id, "out")
        edges.append(edge(signal[0], signal[1], "ifft", ifft_port))

    append_spectral_stack("magnitude", "mag", "mag", 70)
    append_spectral_stack("phase", "phase", "phase", 820, True)

    guide_props = preset["guideCurveProps"]["guides"]
    guides = []
    for index, layer in enumerate(guide_layers):
        props = guide_props[index] if index < len(guide_props) else {
            "noiseLevel": 0.0,
            "offsetLevel": 0.0,
            "phaseLevel": 0.0,
        }
        guides.append({
            "id": f"guide{index + 1}",
            "shortLabel": f"G{index + 1}",
            "name": "",
            "colourIndex": index,
            "shelfOrder": index,
            "enabled": bool(layer["properties"]["active"]),
            "noise": props["noiseLevel"],
            "dcOffset": props["offsetLevel"],
            "phase": props["phaseLevel"],
            "revision": 1,
            "model": flat_curve_model(layer["mesh"]),
        })

    signal_node = "ifft"
    signal_port = "time"
    waveshaper = preset["effects"]["Waveshaper"]
    waveshaper_layers = groups[MESH_GROUPS["waveshaper"]]["layers"]
    if waveshaper["enabled"]:
        nodes.append(node("waveshaper", "waveshaper", 2450, 500, {
            "enabled": True,
            "pre": waveshaper["knobs"][0],
            "post": waveshaper["knobs"][1],
            "aaFactor": str(waveshaper["oversampleFactor"]),
        }, flat_curve_model(waveshaper_layers[0]["mesh"])))

    envelope_y = {"volume": 120, "pitch": 1050, "scratch": 1280}
    envelope_ids = {}
    for purpose in ("volume", "pitch", "scratch"):
        for index, layer in enumerate(envelope_layers(preset, purpose), 1):
            envelope_id = f"{purpose}Envelope{index}"
            nodes.append(envelope_node(
                preset, layer, purpose, envelope_id,
                2450 + 310 * (index - 1), envelope_y[purpose]))
            if layer["properties"]["active"]:
                envelope_ids[purpose] = envelope_id

    volume_id = envelope_ids.get("volume")
    if volume_id is not None:
        nodes.append(node("volumeMultiply", "multiply", 2780, 500))
        edges.append(edge(signal_node, signal_port, "volumeMultiply", "left"))
        edges.append(edge(volume_id, "env", "volumeMultiply", "right"))
        signal_node = "volumeMultiply"
        signal_port = "out"
    if waveshaper["enabled"]:
        edges.append(edge(signal_node, signal_port, "waveshaper", "time"))
        signal_node = "waveshaper"
        signal_port = "time"

    impulse = preset["effects"]["ImpulseModeller"]
    if impulse["enabled"]:
        impulse_layer = groups[MESH_GROUPS["impulseResponse"]]["layers"][0]
        nodes.append(node("impulseResponse", "impulseResponse", 2600, 660, {
            "enabled": True,
            "size": impulse["knobs"][0],
            "post": impulse["knobs"][1],
            "highPass": impulse["knobs"][2] if len(impulse["knobs"]) > 2 else 0.0,
        }, flat_curve_model(impulse_layer["mesh"])))
        edges.append(edge(
            signal_node, signal_port, "impulseResponse", "time"))
        signal_node = "impulseResponse"
        signal_port = "time"

    equalizer = preset["effects"].get("EQ") or {"enabled": False}
    if equalizer["enabled"]:
        knobs = equalizer["knobs"]
        parameters = {"enabled": True}
        for index in range(5):
            parameters[f"band{index + 1}Gain"] = knobs[index]
            parameters[f"band{index + 1}Frequency"] = knobs[index + 5]
        nodes.append(node("equalizer", "equalizer", 2720, 660, parameters))
        edges.append(edge(signal_node, signal_port, "equalizer", "time"))
        signal_node = "equalizer"
        signal_port = "time"

    delay = preset["effects"]["Delay"]
    if delay["enabled"]:
        nodes.append(node("delay", "delay", 2860, 660, {
            "enabled": True,
            "time": delay["knobs"][0],
            "feedback": delay["knobs"][1],
            "spinIters": delay["knobs"][2],
            "spin": delay["knobs"][3],
            "wet": delay["knobs"][4],
        }))
        edges.append(edge(signal_node, signal_port, "delay", "time"))
        signal_node = "delay"
        signal_port = "time"

    reverb = preset["effects"]["Reverb"]
    if reverb["enabled"]:
        nodes.append(node("reverb", "reverb", 3000, 660, {
            "enabled": True,
            "size": reverb["knobs"][0],
            "damp": reverb["knobs"][1],
            "width": reverb["knobs"][2],
            "highPass": reverb["knobs"][3],
            "wet": reverb["knobs"][4],
        }))
        edges.append(edge(signal_node, signal_port, "reverb", "time"))
        signal_node = "reverb"
        signal_port = "time"
    pitch_id = envelope_ids.get("pitch")
    if pitch_id is not None:
        edges.append(edge(pitch_id, "env", "voice", "pitch"))
    scratch_id = envelope_ids.get("scratch")
    if scratch_id is not None:
        for mesh_node_id in all_mesh_node_ids:
            edges.append(edge(
                scratch_id, "env", mesh_node_id, "scratch",
                "processingAttachment", "scratchEnvelope"))

    nodes.append(node("output", "output", 3150, 500))
    edges.append(edge(signal_node, signal_port, "output", "time"))

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


def validate_conversion(source):
    preset = source["preset"]
    issues = []
    required_sections = (
        "meshLibrary", "oscControls", "effects", "settings", "guideCurveProps",
    )
    for section in required_sections:
        if section not in preset:
            issues.append(f"missing canonical section: {section}")
    if issues:
        return issues

    groups = preset["meshLibrary"]["groups"]
    if not groups[MESH_GROUPS["time"]]["layers"]:
        issues.append("time layer group is empty")

    for group_name in ("time", "magnitude", "phase"):
        for index, layer in enumerate(groups[MESH_GROUPS[group_name]]["layers"], 1):
            properties = layer["properties"]
            if properties.get("gain", 0.0) != 0.0:
                issues.append(
                    f"{group_name} layer {index} has unmapped gain "
                    f"{properties['gain']}")
            if properties.get("fineTune", 0.0) != 0.0:
                issues.append(
                    f"{group_name} layer {index} has unmapped fine tune "
                    f"{properties['fineTune']}")
            if group_name == "time" and properties.get("pan", 0.5) != 0.5:
                issues.append(
                    f"time layer {index} has unmapped pan {properties['pan']}")

    for purpose in ("volume", "pitch", "scratch"):
        active_count = sum(
            bool(layer["properties"]["active"])
            for layer in envelope_layers(preset, purpose)
        )
        if active_count > 1:
            issues.append(
                f"{purpose} has {active_count} active Envelopes; Cycle V2 accepts one")
    wave_loaded = preset["effects"]["ImpulseModeller"].get("waveLoaded", False) \
        or bool(preset.get("multisample", {}).get("samples", []))
    if wave_loaded and any(
            layer["properties"]["active"]
            for layer in envelope_layers(preset, "wavePitch")):
        issues.append("active wave-pitch Envelope has no Cycle V2 destination")

    if preset["effects"]["Unison"]["enabled"] \
            and not preset["effects"]["Unison"].get("groupMode", True):
        issues.append("active individual-mode Unison mapping is not implemented")
    impulse = preset["effects"]["ImpulseModeller"]
    if impulse["enabled"] and impulse.get("waveLoaded", False):
        issues.append("sample-backed ImpulseModeller has no Cycle V2 resource mapping")
    if impulse["enabled"] \
            and not groups[MESH_GROUPS["impulseResponse"]]["layers"]:
        issues.append("active ImpulseModeller has no authored curve layer")
    if preset.get("multisample", {}).get("samples", []):
        issues.append("external multisamples have no Cycle V2 destination")
    if modulation_sources_for_preset(preset) is None:
        issues.append("modulation matrix differs from the supported fixed mapping")

    oversampling = preset["settings"].get("OversampleFactorRltm", 1)
    if oversampling not in (1, 2, 4, 8):
        issues.append(f"unsupported realtime oversampling factor: {oversampling}")
    waveshaper = preset["effects"]["Waveshaper"]
    if waveshaper["enabled"] \
            and not groups[MESH_GROUPS["waveshaper"]]["layers"]:
        issues.append("active Waveshaper has no authored curve layer")
    return issues


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

    active_envelopes = {
        purpose: [
            layer for layer in envelope_layers(preset, purpose)
            if layer["properties"]["active"]
        ]
        for purpose in ("volume", "pitch", "scratch")
    }
    if active_envelopes["pitch"]:
        issues.append("active pitch envelope is not supported by strict audio parity")

    if preset["multisample"]["samples"]:
        issues.append("external multisamples are not supported by strict audio parity")
    if modulation_sources_for_preset(preset) is None:
        issues.append("modulation matrix differs from the supported fixed mapping")

    oversampling = preset["settings"].get("OversampleFactorRltm", 1)
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
    oscillator_knobs = list(preset["oscControls"].get("knobs", []))
    oscillator_knobs.extend([0.5] * (3 - len(oscillator_knobs)))
    duration = math.exp(8.0 * oscillator_knobs[2] - 3.0)
    master_gain = math.exp(6.0 * oscillator_knobs[0] - 3.0)
    octave = translated_octave(oscillator_knobs[1])
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
