"""Validate the fixed saxophone pilot and translate its reviewed peak edit."""

import copy
import json
import math
from pathlib import Path

from cycle_performance_suite import load_suite, object_fields
from cycle_reference_study import file_digest, read_study


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PLAN = ROOT / "cycle-v2/content/studies/expressive-sax/plan.json"
RECIPE = {"id": "slower-peak", "nodeId": "volumeEnvelope1", "cubeIndex": 1,
          "vertexIds": list(range(8, 16)), "phaseFactor": 2.0}
OBJECTIVE_IDS = {"anchor", "low-soft", "high-short", "cc1-onset-high", "cc2-motion", "unused-cc118"}
WITHHELD_IDS = {"intermediate-register", "reversed-cc2", "repeated-note-cc1-latch", "soft-intermediate"}
CONTROLS = [
    {"controller": 1, "name": "Volume articulation", "timing": "note-on-latched",
     "nodeIds": ["volumeEnvelope1"]},
    {"controller": 2, "name": "Waveform and spectral morph", "timing": "continuous",
     "nodeIds": ["timeLayer1", "magnitudeLayer1"]},
    {"controller": 3, "name": "Pitch and scratch contour", "timing": "note-on-latched",
     "nodeIds": ["pitchEnvelope1", "scratchEnvelope1", "scratchEnvelope2"], "fixedValue": 0},
]


def named(items, identifier):
    matches = [item for item in items if item.get("id") == identifier]
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one id={identifier}")
    return matches[0]


def peak_edit(controls, recipe):
    """Copy an inspected codec payload; domain commands retain edit validation."""
    if (not isinstance(recipe, dict) or recipe != RECIPE or type(recipe.get("phaseFactor")) not in (int, float)
            or type(recipe.get("cubeIndex")) is not int
            or any(type(value) is not int for value in recipe.get("vertexIds", []))):
        raise ValueError("Only the reviewed slower-peak recipe is supported")
    if controls.get("nodeId") != recipe["nodeId"]:
        raise ValueError("Peak recipe requires volumeEnvelope1")
    model = copy.deepcopy(controls["model"])
    state = model.get("state", {})
    revision = model.get("revision")
    if (model.get("schema") != "envelope" or model.get("version") != 2
            or state.get("version") != 2 or state.get("type") != "envelope"
            or type(revision) is not int or revision < 1 or state.get("revision") != revision):
        raise ValueError("Expected the current coherent envelope codec")
    mesh = state.get("mesh", {})
    main = mesh.get("mainMesh", {})
    vertices, cubes = main.get("vertices", []), main.get("cubes", [])
    if (len(vertices) != 48 or len(cubes) != 6
            or state.get("cubeIds") != list(range(1, 7))
            or mesh.get("loopIndices") != [] or mesh.get("sustainIndices") != [2]
            or cubes[1].get("vertexIds") != recipe["vertexIds"]
            or len({vertex["id"] for vertex in vertices}) != 48):
        raise ValueError("Reviewed peak topology or envelope markers changed")
    selected = [named(vertices, identifier) for identifier in recipe["vertexIds"]]
    if any(type(vertex.get("phase")) not in (int, float)
           or not .002 <= vertex["phase"] <= .032 or vertex.get("amp") != 1 for vertex in selected):
        raise ValueError("Reviewed unit-amplitude peak coordinates changed")
    sustain = [named(vertices, identifier) for identifier in cubes[2]["vertexIds"]]
    if any(not .9 < vertex["phase"] <= 1 for vertex in sustain):
        raise ValueError("Reviewed following sustain boundary changed")
    for vertex in selected:
        vertex["phase"] *= recipe["phaseFactor"]
    model["revision"] = state["revision"] = revision + 1
    return {"nodeId": controls["nodeId"], "expectedModelRevision": revision, "model": model}


def assert_peak_only(before, after, recipe):
    """Reject any change outside the declared peak phases and codec revisions."""
    left, right = copy.deepcopy(before), copy.deepcopy(after)
    original = named(left["nodes"], recipe["nodeId"])
    changed = named(right["nodes"], recipe["nodeId"])
    expected = peak_edit({"nodeId": original["id"], "model": original["model"]}, recipe)["model"]
    actual = changed["model"]
    if actual.get("revision") != expected["revision"] or actual["state"].get("revision") != expected["revision"]:
        raise ValueError("Peak edit must advance both codec revisions once")
    for identifier in recipe["vertexIds"]:
        wanted = named(expected["state"]["mesh"]["mainMesh"]["vertices"], identifier)["phase"]
        vertex = named(actual["state"]["mesh"]["mainMesh"]["vertices"], identifier)
        if type(vertex.get("phase")) not in (int, float) or not math.isclose(vertex["phase"], wanted, rel_tol=0, abs_tol=1e-7):
            raise ValueError("Candidate does not match its exact peak recipe")
        vertex["phase"] = named(original["model"]["state"]["mesh"]["mainMesh"]["vertices"], identifier)["phase"]
    actual["revision"] = actual["state"]["revision"] = original["model"]["revision"]
    if left != right:
        raise ValueError("Peak intervention changed unrelated graph state")


def assert_unused_cc118(graph):
    if any(value in (118, "118") for node in graph["nodes"]
           for key, value in node.get("parameters", {}).items() if "controller" in key.lower()):
        raise ValueError("CC118 is routed and cannot serve as the schedule control")


def economy(graph):
    result = {"nodeCount": len(graph["nodes"]), "edgeCount": len(graph["edges"]),
              "guideCount": len(graph.get("guides", [])), "meshVertexCount": 0,
              "meshCubeCount": 0, "envelopes": []}
    for node in graph["nodes"]:
        model = node.get("model", {})
        mesh = model.get("mesh") if node["kind"] == "trilinearMesh" else None
        if node["kind"] == "envelope":
            mesh = model["state"]["mesh"]["mainMesh"]
            result["envelopes"].append({"nodeId": node["id"], "vertexCount": len(mesh["vertices"]),
                                         "cubeCount": len(mesh["cubes"])})
        if mesh is not None:
            result["meshVertexCount"] += len(mesh["vertices"])
            result["meshCubeCount"] += len(mesh["cubes"])
    return result


def checked_suite(path, expected):
    suite = load_suite(path)
    if {case["id"] for case in suite["cases"]} != expected or suite["repetitions"] != 2:
        raise ValueError("Pilot suite requires its fixed split IDs and two repetitions")
    for case in suite["cases"]:
        performance = case["performance"]
        if (case["pattern"] is not None or performance["durationMs"] != 2000
                or performance["sampleRate"] != 44100 or performance["ratePolicy"] != "native"
                or performance["randomSeed"] != 1729 or performance["controlNoteOffset"] != 0):
            raise ValueError("Pilot requires explicit two-second native 44100-Hz performances")
        events = performance["events"]
        initial = events[:3]
        if ([event.get("controller") for event in initial] != [1, 2, 3]
                or any(event["type"] != "controller" or event["timeMs"] != 0 for event in initial)
                or any(event["channel"] != 1 for event in events)
                or initial[2]["value"] != 0):
            raise ValueError("Every case must initialize CC1/2/3 before its notes; CC3 stays zero")
        held = set()
        for event in events[3:]:
            if event["type"] == "noteOn":
                if event["note"] in held:
                    raise ValueError("Pilot notes cannot overlap at the same pitch")
                held.add(event["note"])
            elif event["type"] == "noteOff":
                if event["note"] not in held or event["timeMs"] > 1200:
                    raise ValueError("Every pilot note must be released by 1200 ms")
                held.remove(event["note"])
            elif event["type"] != "controller" or event["controller"] not in (1, 2, 118):
                raise ValueError("Pilot supports only its declared MIDI controls")
        if held or not any(event["type"] == "noteOn" for event in events):
            raise ValueError("Pilot requires a nonempty balanced note performance")
    return suite


def load_plan(path):
    path = Path(path).resolve(strict=True)
    value = json.loads(path.read_text())
    object_fields(value, {"schema", "baseline", "sourceSha256", "objectiveSuite", "withheldSuite",
                          "referenceStudy", "candidate", "controls", "missingCoverage"}, "saxophone plan")
    if (value.get("schema") != "cycle-saxophone-pilot.v1"
            or value.get("baseline") != "../../presets/saxophone-3.cyclegraph"
            or value.get("objectiveSuite") != "objective-suite.json"
            or value.get("withheldSuite") != "withheld-suite.json"
            or value.get("referenceStudy") != "references.json" or value.get("controls") != CONTROLS
            or not isinstance(value.get("missingCoverage"), list) or not value["missingCoverage"]
            or any(not isinstance(item, str) or not item.strip() for item in value["missingCoverage"])):
        raise ValueError("Expected the fixed saxophone pilot and explicit missing coverage")
    baseline = (path.parent / value["baseline"]).resolve(strict=True)
    if file_digest(baseline) != value.get("sourceSha256"):
        raise ValueError("Saxophone source identity changed")
    graph = json.loads(baseline.read_text())
    peak_edit({"nodeId": "volumeEnvelope1", "model": named(graph["nodes"], "volumeEnvelope1")["model"]}, value["candidate"])
    assert_unused_cc118(graph)
    objective = checked_suite(path.parent / value["objectiveSuite"], OBJECTIVE_IDS)
    withheld = checked_suite(path.parent / value["withheldSuite"], WITHHELD_IDS)
    if {case["id"] for case in objective["cases"]} & {case["id"] for case in withheld["cases"]}:
        raise ValueError("Objective and withheld IDs must be disjoint")
    anchor = named(objective["cases"], "anchor")["performance"]
    moving = copy.deepcopy(named(objective["cases"], "cc2-motion")["performance"])
    negative = copy.deepcopy(named(objective["cases"], "unused-cc118")["performance"])
    expected = [{"type": "controller", "timeMs": time, "controller": 2, "value": amount, "channel": 1}
                for time, amount in ((200, 64), (450, 127))]
    if [event for event in moving["events"] if event["type"] == "controller" and event["timeMs"] > 0] != expected:
        raise ValueError("CC2 motion must retain the reviewed 200/450-ms trajectory")
    expected_negative = [{**event, "controller": 118} for event in expected]
    if [event for event in negative["events"] if event["type"] == "controller" and event["timeMs"] > 0] != expected_negative:
        raise ValueError("Negative control must target only unused CC118 at matching event times")
    for event in negative["events"]:
        if event["type"] == "controller" and event["timeMs"] > 0:
            event["controller"] = 2
    stripped = copy.deepcopy(moving)
    stripped["events"] = [event for event in moving["events"] if event["type"] != "controller" or event["timeMs"] == 0]
    if negative != moving or stripped != anchor:
        raise ValueError("Anchor, CC2 motion and CC118 control must differ only by their declared events")
    if ([event["value"] for event in anchor["events"][:3]] != [0, 0, 0]
            or anchor["events"][3] != {"type": "noteOn", "timeMs": 0, "channel": 1, "note": 57, "velocity": .65}
            or anchor["events"][-1] != {"type": "noteOff", "timeMs": 1200, "channel": 1, "note": 57}):
        raise ValueError("Anchor must preserve note 57, velocity .65 and the reviewed held interval")
    onset = copy.deepcopy(named(objective["cases"], "cc1-onset-high")["performance"])
    if onset["events"][0]["value"] != 127:
        raise ValueError("CC1 onset intervention must start at 127")
    onset["events"][0]["value"] = 0
    if onset != anchor:
        raise ValueError("CC1 onset case must change only its initial CC1 value")
    reference_path = (path.parent / value["referenceStudy"]).resolve(strict=True)
    reference = read_study(reference_path)
    if len(reference["references"]) != 1 or "source" not in reference["references"][0]:
        raise ValueError("Pilot requires the single declared local recording")
    entry = reference["references"][0]
    if (Path(entry["source"]) != (baseline.parent.parent / "references/sax-growl.wav").resolve(strict=True)
            or entry.get("referenceId") != "factory:sax-growl" or entry["id"] != "growl"
            or entry["crop"] != {"startSeconds": 0}
            or entry["labels"] != {"articulation": {"value": "growl", "origin": "inferred"}}):
        raise ValueError("Pilot reference must retain the full recording and unverified growl label")
    sources = [path, baseline, reference_path, Path(objective["source"]["path"]),
               Path(withheld["source"]["path"]), Path(reference["references"][0]["source"])]
    return {"document": value, "path": str(path), "baselinePath": str(baseline),
            "referenceStudyPath": str(reference_path), "objectiveSuite": objective,
            "withheldSuite": withheld, "referenceStudy": reference,
            "identities": {str(source): file_digest(source) for source in sources}}
