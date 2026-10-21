"""Authored saxophone shapes and an embedded performance, without DSP or playback."""

import copy
import math

from cycle_saxophone_study import named


DEFAULT_CONTROLLERS = {1: 0, 2: 32, 3: 0, 4: 0, 5: 0, 11: 104, 76: 64, 77: 0}
CONTROL_MAP = [
    {"controller": 1, "name": "Attack and release", "nodeIds": ["volumeEnvelope1"]},
    {"controller": 2, "name": "Brightness", "nodeIds": ["timeLayer1", "magnitudeLayer1"]},
    {"controller": 3, "name": "Timbral onset", "nodeIds": ["scratchEnvelope1", "scratchEnvelope2"]},
    {"controller": 4, "name": "Air and rasp", "nodeIds": ["timeLayer1"]},
    {"controller": 5, "name": "Vibrato depth", "nodeIds": ["pitchEnvelope1"]},
    {"controller": 11, "name": "Expression", "nodeIds": ["volumeEnvelope1"]},
    {"controller": 76, "name": "Vibrato rate", "nodeIds": ["pitchEnvelope1"]},
    {"controller": 77, "name": "Periodic roughness", "nodeIds": ["growlEnvelope"]},
]


def _parameters(values):
    return [{"id": key, "value": str(value).lower() if isinstance(value, bool) else str(value)}
            for key, value in values.items()]


def _model_revision(model):
    revision = model.get("revision")
    if type(revision) is not int or revision < 1:
        raise ValueError("Expected an inspected model revision")
    if model.get("schema") == "envelope" and model.get("state", {}).get("revision") != revision:
        raise ValueError("Envelope model and state revisions disagree")
    return revision


def _shape_edit(node, model):
    revision = _model_revision(node["model"])
    model["revision"] = revision + 1
    if model["schema"] == "envelope":
        model["state"]["revision"] = revision + 1
    return {"nodeId": node["id"], "expectedModelRevision": revision, "model": model}


def _loop_model(seconds, rates, low, high, revision=1):
    """Describe seven authored poles; shared domain code owns interpolation/playback."""
    vertices, cubes = [], []
    delay, release = .20 / seconds, .05 / seconds
    for pole in range(7):
        first = len(vertices)
        for time in (0, 1):
            for red in (0, 1):
                for blue in (0, 1):
                    period = 1 / (rates[red] * seconds)
                    phase = 0 if pole == 0 else delay + (min(pole, 5) - 1) * period / 4
                    if pole == 6:
                        phase += release
                    vertices.append({"id": len(vertices), "time": float(time), "key": float(red),
                                     "mod": float(blue), "phase": phase,
                                     "amp": (low, high)[blue][pole], "weight": .7})
        fields = ("time", "key", "mod", "phase", "amp", "curve")
        cubes.append({"vertexIds": list(range(first, first + 8)),
                      "guides": {field: -1 for field in fields},
                      "gains": {field: .5 for field in fields}})
    return {"schema": "envelope", "version": 2, "revision": revision,
            "state": {"version": 2, "type": "envelope", "revision": revision,
                      "mesh": {"name": "Mesh", "mainMesh": {"name": "Mesh", "version": 1,
                               "vertices": vertices, "cubes": cubes},
                               "loopIndices": [1], "sustainIndices": [5]},
                      "logarithmic": False, "red": .5, "blue": 0,
                      "cubeIds": list(range(1, 8))}}


def _edge(source, source_port, dest, dest_port):
    return {"sourceNodeId": source, "sourcePortId": source_port,
            "destNodeId": dest, "destPortId": dest_port,
            "connectionKind": "signal", "attachmentType": "none"}


def articulation_plan(graph, voice_duration_seconds):
    """Translate a compatible inspected factory graph into semantic command inputs."""
    nodes = graph["nodes"]
    if any(node["id"] in ("expressionControls", "growlEnvelope", "growlMultiply") for node in nodes):
        raise ValueError("Articulation node IDs already exist")
    named(nodes, "voice")
    if (type(voice_duration_seconds) not in (int, float)
            or not math.isfinite(voice_duration_seconds) or voice_duration_seconds <= 0):
        raise ValueError("Supply a positive physical voice duration from the canonical parameter conversion")
    seconds = voice_duration_seconds
    volume = named(nodes, "volumeEnvelope1")
    volume_model = copy.deepcopy(volume["model"])
    volume_mesh = volume_model["state"]["mesh"]["mainMesh"]
    if (volume_model.get("schema") != "envelope" or len(volume_mesh["vertices"]) != 48
            or volume_model["state"]["mesh"]["sustainIndices"] != [2]):
        raise ValueError("Expected the reviewed CC1 volume articulation")
    for vertex in volume_mesh["vertices"]:
        if vertex["key"] == 0:
            vertex["amp"] *= .35
    edits = [_shape_edit(volume, volume_model)]
    pitch = named(nodes, "pitchEnvelope1")
    pitch_model = _loop_model(seconds, (4.5, 7.5), [.5] * 7,
                              [.5, .5, .5125, .5, .4875, .5, .5])
    pitch_edit = _shape_edit(pitch, pitch_model)
    pitch_edit["parameters"] = _parameters({"enabled": True, "level": 1})
    edits.append(pitch_edit)
    time_node = named(nodes, "timeLayer1")
    time_model = copy.deepcopy(time_node["model"])
    cube = time_model["mesh"]["cubes"][1]
    if cube["vertexIds"] != list(range(8, 16)) or not any(
            assignment == {"guideId": "guide4", "targetNodeId": "timeLayer1", "target": {
                "kind": "trimeshCubeComponent", "cubeIndex": 1, "field": "time"}}
            for assignment in graph["guideAssignments"]):
        raise ValueError("Expected the existing noisy component guide")
    for identifier in cube["vertexIds"]:
        vertex = named(time_model["mesh"]["vertices"], identifier)
        vertex["weight"] = .20 if vertex["key"] == 1 else 0
    edits.append(_shape_edit(time_node, time_model))
    for node_id, parameters in (
            ("voice", {"envelopeModulation": "live", "noteMode": "legato", "portamento": True,
                       "portamentoSeconds": .06}),
            ("modulationOverride1", {"redSource": "midiCC", "redController": 4}),
            ("modulationOverride4", {"redSource": "midiCC", "redController": 11})):
        named(nodes, node_id)
        edits.append({"nodeId": node_id, "parameters": _parameters(parameters)})
    rough_model = _loop_model(seconds, (25, 25), [1] * 7,
                              [1, 1, .825, .65, .825, 1, 1])
    new_nodes = [
        {"id": "expressionControls", "kind": "modulationTriple", "position": {"x": 100, "y": 80},
         "parameters": {"yellowSource": "midiCC", "yellowController": 77,
                        "redSource": "midiCC", "redController": 76,
                        "blueSource": "midiCC", "blueController": 5}},
        {"id": "growlEnvelope", "kind": "envelope", "position": {"x": 900, "y": 1250},
         "parameters": {"enabled": True, "purpose": "control", "logarithmic": False,
                        "declick": False, "red": .5, "blue": 0, "level": 1}, "model": rough_model},
        {"id": "growlMultiply", "kind": "multiply", "position": {"x": 1750, "y": 1250},
         "parameters": {}},
    ]
    old_edge = _edge("volumeMultiply", "out", "voiceOutput", "time")
    if old_edge not in graph["edges"]:
        raise ValueError("Expected the direct existing per-voice volume output")
    return {"edits": edits, "addNodes": new_nodes, "removeEdges": [old_edge], "addEdges": [
                _edge("expressionControls", "red", "pitchEnvelope1", "red"),
                _edge("expressionControls", "blue", "pitchEnvelope1", "blue"),
                _edge("expressionControls", "yellow", "growlEnvelope", "blue"),
                _edge("volumeMultiply", "out", "growlMultiply", "left"),
                _edge("growlEnvelope", "env", "growlMultiply", "right"),
                _edge("growlMultiply", "out", "voiceOutput", "time")],
            "sequence": demonstration_sequence(), "controls": copy.deepcopy(CONTROL_MAP),
            "notes": ["CC1 changes attack/release; note-off determines short-note duration.",
                      "CC4 scales existing component noise; residual guide jitter remains at zero.",
                      "CC77 is 25 Hz amplitude roughness, an unverified acoustic growl hypothesis.",
                      "Pitch is neutral at CC5 zero; full depth is approximately +/-30 cents.",
                      "Reference recordings do not establish observed source MIDI controls."]}


def demonstration_sequence():
    """Authored 28-second phrase; existing scheduler owns CC interpolation and timing."""
    notes = []
    for start, duration, pitch, velocity in (
            (.2, 1.8, 60, 94), (2.5, .16, 60, 94), (2.95, .16, 62, 94), (3.4, .22, 64, 98),
            (4.2, 2.1, 60, 94), (6.8, 2.1, 60, 94), (9.4, 3.1, 64, 96),
            (13, 1.9, 60, 94), (15.4, 1.9, 60, 94),
            (18, .58, 48, 88), (18.5, .58, 55, 92), (19, .58, 60, 98),
            (19.5, .58, 62, 99), (20, .58, 64, 102), (20.5, 1.2, 67, 105),
            (22, .60, 72, 104), (22.5, .60, 69, 98), (23, .60, 67, 96),
            (23.5, 1.0, 64, 94), (25, 1.8, 60, 96), (25.7, .42, 67, 100)):
        notes.append({"pitch": pitch, "velocity": velocity, "startSeconds": start,
                      "durationSeconds": duration})
    lanes = {
        1: [(2.2, 0), (2.3, 127), (3.8, 127), (4, 0), (21.8, 0), (21.9, 70),
            (24.7, 70), (24.8, 0)],
        2: [(4.4, 32), (6, 110), (6.4, 110), (6.6, 32), (18, 36), (20.7, 100),
            (21.7, 100), (22, 60), (26.4, 95), (27, 32)],
        3: [(17.8, 0), (18, 80), (21.5, 80), (21.8, 0)],
        4: [(13.1, 0), (13.8, 100), (14.5, 100), (15.1, 0), (23.3, 0),
            (23.7, 35), (24.5, 35), (24.8, 0)],
        5: [(9.7, 0), (10.5, 100), (12, 100), (12.8, 0), (18.8, 0), (20.8, 55),
            (21.5, 55), (21.8, 0), (25.4, 0), (26.5, 100), (27, 0)],
        11: [(6.4, 104), (6.6, 38), (7, 38), (8.5, 118), (9.1, 104),
             (18, 70), (20.8, 118), (21.8, 104), (25, 80), (26.5, 118), (27, 104)],
        76: [(9.2, 64), (9.3, 24), (10.6, 24), (12, 112), (12.8, 64),
             (25.4, 64), (26.5, 95), (27, 64)],
        77: [(15.5, 0), (16.2, 100), (16.9, 100), (17.5, 0), (25.2, 0),
             (25.6, 30), (26.5, 30), (27, 0)],
    }
    controls = [{"controller": controller, "value": value, "timeSeconds": time}
                for controller, default in DEFAULT_CONTROLLERS.items()
                for time, value in [(0, default), *lanes[controller], (27.8, default)]]
    controls.sort(key=lambda point: (point["timeSeconds"], point["controller"]))
    return {"durationSeconds": 28, "notes": notes, "controls": controls}
