#!/usr/bin/env python3
"""Author and audition an expressive saxophone through the exact Cycle V2 instance."""

import argparse
import copy
import html
import json
import math
import os
from pathlib import Path
import subprocess
import sys

from cycle_automation_support.identity import process_identity
from cycle_automation_support.session import command
from cycle_performance_suite import content_hash, load_suite
from cycle_reference_study import file_digest
from cycle_saxophone_articulation import DEFAULT_CONTROLLERS, articulation_plan
from cycle_saxophone_study import economy, named
from cycle_study_artifacts import verify, write
from render_cycle_v2_performance_suite import render_suite

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "cycle-v2/content/presets/saxophone-3.cyclegraph"
ADDRESS_FIELDS = ("sourceNodeId", "sourcePortId", "destNodeId", "destPortId")


def remap_edge(edge, identifiers):
    result = copy.deepcopy(edge)
    for field in ("sourceNodeId", "destNodeId"):
        result[field] = identifiers.get(result[field], result[field])
    return result


def edge_index(graph, address):
    matches = [index for index, edge in enumerate(graph["edges"])
               if all(edge[field] == address[field] for field in ADDRESS_FIELDS)]
    if len(matches) != 1:
        raise ValueError("Expected exactly one current edge at " + str(address))
    return matches[0]


def node_edits(plan, identifiers, inspect):
    edits = copy.deepcopy(plan["edits"])
    for node in plan["addNodes"]:
        current = inspect(identifiers[node["id"]])
        stored = {parameter["id"]: parameter["value"] for parameter in current.get("parameters", [])}
        edit = {"nodeId": identifiers[node["id"]], "parameters": [
            {"id": key, "value": str(int(value)) if isinstance(value, bool) else str(value)}
            for key, value in node["parameters"].items()
            if stored.get(key) != (str(int(value)) if isinstance(value, bool) else str(value))]}
        if "model" in node:
            revision = current["model"]["revision"]
            if type(revision) is not int or revision < 1:
                raise ValueError("New node lacks a current model revision")
            edit["expectedModelRevision"] = revision
            edit["model"] = copy.deepcopy(node["model"])
            edit["model"]["revision"] = edit["model"]["state"]["revision"] = revision + 1
        if edit["parameters"] or "model" in edit:
            edits.append(edit)
    return edits


def author_candidate(source, output, invoke):
    invoke({"command": "openGraph", "path": str(source), "includeSnapshot": False})
    def export(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()
    initial = json.loads(export("source-inspection.cyclegraph"))
    conversion = invoke({"command": "convertSynthesisParameter", "typeId": "voiceContext",
        "parameterId": "voiceLength", "value": float(named(initial["nodes"], "voice")["parameters"]["voiceLength"]),
        "direction": "toPhysical", "sampleRate": 48000, "beatsPerMeasure": 4})
    plan = articulation_plan(initial, conversion["physicalValue"])
    identifiers = {}
    for node in plan["addNodes"]:
        response = invoke({"command": "addNode", "kind": node["kind"], **node["position"]})
        identifier = response["nodeId"]
        if identifier in identifiers.values() or any(item["id"] == identifier for item in initial["nodes"]):
            raise RuntimeError("Added node has a colliding identity")
        identifiers[node["id"]] = identifier
    inspect = lambda node: invoke({"command": "inspectNodeControls", "nodeId": node})
    edits = node_edits(plan, identifiers, inspect)
    before = export("before-shapes.cyclegraph")
    revision = inspect("voice")["documentRevision"]
    edited = invoke({"command": "applyNodeEdits", "expectedDocumentRevision": revision, "edits": edits})
    if not edited.get("changed") or edited["documentRevision"] != revision + 1:
        raise RuntimeError("Authored shapes did not publish as one semantic edit")
    after = export("after-shapes.cyclegraph")
    for action, expected in (("undo", before), ("redo", after)):
        invoke({"command": action})
        if export(action + ".cyclegraph") != expected:
            raise RuntimeError("Shape " + action + " changed unrelated content")
    for move in plan.get("moves", []):
        invoke({"command": "moveNode", "nodeId": identifiers.get(move["nodeId"], move["nodeId"]),
                **move["position"]})
    for edge in plan["removeEdges"]:
        current = json.loads(export("routing-inspection.cyclegraph"))
        invoke({"command": "deleteEdge", "edgeIndex": edge_index(current, remap_edge(edge, identifiers))})
    for edge in plan["addEdges"]:
        invoke({"command": "connectPorts", **remap_edge(edge, identifiers)})
    control_text = "; ".join("CC" + str(item["controller"]) + " " + item["name"] for item in plan["controls"])
    invoke({"command": "setExplanation", "scope": "preset", "text":
        "Expressive saxophone: " + control_text + ". Overlapping notes connect in Legato mode; "
        "separated notes articulate afresh. Play the embedded phrase for isolated controls and combinations.\n\n"
        + "\n".join(plan["notes"])})
    for logical, text in (("pitchEnvelope1", "Compact delayed vibrato: CC5 depth, CC76 rate; zero depth is neutral pitch."),
            ("growlEnvelope", "CC77 fades in a 25 Hz amplitude loop. This is periodic roughness; convincing acoustic growl remains unverified."),
            ("timeLayer1", "CC2 keeps the existing brightness morph. CC4 independently scales the noisy component using the free red axis."),
            ("volumeEnvelope1", "CC1 selects attack/release character. CC11 uses the free red axis for live expression with a 0.35 gain floor.")):
        invoke({"command": "setExplanation", "scope": "node", "nodeId": identifiers.get(logical, logical), "text": text})
    invoke({"command": "setPresetSequence", "sequence": plan["sequence"]})
    saved = export("candidate-inspection.cyclegraph")
    if json.loads(saved)["presetPresentation"].get("sequence") != plan["sequence"]:
        raise RuntimeError("Embedded sequence differs from the authored phrase")
    candidate = output / "expressive-saxophone.cyclegraph"
    invoke({"command": "saveGraph", "path": str(candidate)})
    invoke({"command": "openGraph", "path": str(candidate), "includeSnapshot": False})
    if export("reloaded.cyclegraph") != saved:
        raise RuntimeError("Candidate changed on reload")
    return candidate, plan, {"logicalNodeIds": identifiers, "physicalDuration": conversion,
        "atomicShapeUndoRedoReload": True, "economy": {"source": economy(initial), "candidate": economy(json.loads(saved))}}


def control_suite(voice_duration_seconds):
    def cc(controller, value, time=0):
        return {"type": "controller", "timeMs": time, "channel": 1, "controller": controller, "value": value}
    def note(pitch=60, start=0, end=3000):
        return [{"type": "noteOn", "timeMs": start, "channel": 1, "note": pitch, "velocity": .75},
                {"type": "noteOff", "timeMs": end, "channel": 1, "note": pitch}]
    initial = [cc(controller, value) for controller, value in DEFAULT_CONTROLLERS.items()]
    cases = []
    for identifier, extra, notes in (
            ("straight", [cc(118, 127, 1000)], note()),
            ("short", [cc(1, 127)], note(end=180)),
            ("tongued", [], note(end=250) + note(start=800, end=1050) + note(start=1600, end=1850)),
            ("brightness", [cc(2, 127, 1000)], note()),
            ("pressure", [cc(11, 38), cc(11, 118, 1000)], note()),
            ("vibrato-depth", [cc(5, 100, 1000)], note()),
            ("vibrato-rate", [cc(5, 100), cc(76, 0), cc(76, 127, 1000)], note()),
            ("air", [cc(4, 127, 1000)], note()),
            ("roughness", [cc(77, 127, 1000)], note()),
            ("combined-high", [cc(2, 105), cc(5, 80, 800), cc(4, 38, 1000), cc(77, 30, 1200)], note(72)),
            ("legato-run", [], note(48, 0, 600) + note(55, 500, 1100) + note(60, 1000, 1600)
             + note(64, 1500, 2400)),
            ("held-return", [], note() + note(67, 800, 1600))):
        cases.append({"id": identifier, "durationMs": 4000, "events": initial + extra + notes,
                      "labels": {"purpose": "technical control/transition evidence; acoustic technique unverified"}})
    cases.append({"id": "embedded-phrase", "pattern": "demonstration.cyclepattern", "tailMs": 0})
    return {"schema": "cycle-performance-suite.v1", "id": "saxophone-articulation", "repetitions": 2,
        "render": {"sampleRate": 48000, "blockSize": 256, "channels": 2, "ratePolicy": "native",
                   "randomSeed": 1729, "controlNoteOffset": 0,
                   "voiceDurationSeconds": voice_duration_seconds, "outputGain": .125},
        "budget": {"maxRenders": 26, "maxAudioSeconds": 152, "maxWallSeconds": 900}, "cases": cases}


def run(source, output, send, engine):
    source, output = Path(source).resolve(strict=True), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    inputs = {str(path): file_digest(path) for path in (source, Path(__file__),
              ROOT / "scripts/cycle_saxophone_articulation.py")}
    result = {"schema": "cycle-saxophone-articulation.v1", "ok": False, "engine": engine,
              "inputs": inputs, "tasteVerified": False, "acousticModelVerified": False}
    def transport(payload, **kwargs):
        response = send(payload, **kwargs)
        with (output / "commands.jsonl").open("a") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        return response
    def invoke(payload):
        response = transport(payload, require_ok=False)
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError("Automation rejected command: " + str(response))
        return response["result"].get("data", {})
    try:
        candidate, plan, evidence = author_candidate(source, output, invoke)
        result.update(preset=candidate.name, presetSha256=file_digest(candidate), controls=plan["controls"],
                      limitations=plan["notes"], authoring=evidence)
        # A malformed sequence must fail before changing persisted content or history.
        before = invoke({"command": "snapshotState"})
        invalid = transport({"command": "setPresetSequence", "sequence": {**plan["sequence"], "durationSeconds": -1}}, require_ok=False)
        after = invoke({"command": "snapshotState"})
        if (not invalid.get("ok") or invalid.get("result", {}).get("ok") is not False
                or any(before[key] != after[key] for key in
                ("documentRevision", "documentDirty", "canUndo", "canRedo"))):
            raise RuntimeError("Invalid sequence altered graph/history")
        rejected_path = output / "after-rejection.cyclegraph"
        invoke({"command": "exportGraph", "path": str(rejected_path)})
        if rejected_path.read_bytes() != (output / "candidate-inspection.cyclegraph").read_bytes():
            raise RuntimeError("Invalid sequence changed persisted content")
        write(output / "demonstration.cyclepattern", {"version": 1, "sequence": plan["sequence"]})
        suite_path = output / "performance-suite.json"
        write(suite_path, control_suite(evidence["physicalDuration"]["physicalValue"]))
        suite = load_suite(suite_path)
        captured = render_suite(suite, candidate, output / "renders", transport, engine)
        if (not captured["ok"] or not all(case["repeatabilityVerified"] and all(
                math.isfinite(render["capture"][key]) and render["capture"]["rms"] > 1e-7
                for render in case["renders"] for key in ("rms", "peak")) for case in captured["cases"])
                or captured["preset"]["effectiveGraphSha256"] != content_hash(json.loads(candidate.read_text()))):
            raise RuntimeError("Capture failed, changed graph, or is silent/nonfinite/nonrepeatable")
        from cycle_audition_study import load_study
        from report_cycle_auditions import generate_report
        audition = output / "audition-study.json"
        write(audition, {"schema": "cycle-audition-study.v1", "name": "Independent saxophone controls",
            "rmsMatch": True, "cases": [{"id": key, "partition": "objective",
                "a": {"performanceResults": "renders/results.json", "caseId": "straight"},
                "b": {"performanceResults": "renders/results.json", "caseId": key},
                "intervals": [{"id": "body", "role": "sustain", "startSeconds": 1.2, "endSeconds": 2.8}]}
                for key in ("brightness", "pressure", "vibrato-depth", "air", "roughness")]})
        generate_report(load_study(audition), output / "auditions")
        verify(inputs)
        result.update(ok=True, invalidSequenceRejected=True,
                      renders="renders/results.json", audioSeconds=suite["audioSeconds"], allCapturesRepeated=True)
    except Exception as error:
        result["error"] = str(error)
    write(output / "results.json", result)
    (output / "index.html").write_text("<!doctype html><meta charset='utf-8'><title>Expressive saxophone</title>"
        "<h1>Expressive saxophone articulation</h1><p>Acoustic authenticity and taste remain unverified.</p>"
        "<p>" + html.escape("Complete" if result["ok"] else result.get("error", "Failed")) + "</p>"
        "<p><a href='expressive-saxophone.cyclegraph'>Preset</a> · <a href='results.json'>Evidence/control map</a> · "
        "<a href='auditions/index.html'>Raw and RMS-matched control comparisons</a></p>"
        "<p>Embedded 28-second phrase, first capture:</p><audio controls src='renders/embedded-phrase-01.wav'></audio>")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=BASELINE)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if not args.in_session:
        coordinator = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
        if args.no_build:
            coordinator.append("--no-build")
        return subprocess.run(coordinator + ["--", sys.executable, str(Path(__file__).resolve()),
            "--source", str(args.source.resolve()), "--output-dir", str(args.output_dir.resolve()), "--in-session"],
            cwd=ROOT, check=False).returncode
    if not all(os.environ.get(key) for key in ("CYCLE_AGENT_SESSION", "CYCLE_APP_PID", "CYCLE_AUTOMATION_ARTIFACTS")):
        raise ValueError("--in-session requires the exact-instance coordinator environment")
    pid = int(os.environ["CYCLE_APP_PID"])
    identity = process_identity(pid)
    if identity is None:
        raise RuntimeError("Coordinated app is no longer running")
    engine = {"pid": pid, "processIdentity": identity, "executableSha256": file_digest(identity["executable"]),
              "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"]}
    result = run(args.source, args.output_dir,
                 lambda payload, **kwargs: command(os.environ["CYCLE_AGENT_SESSION"], payload, **kwargs), engine)
    print(("PASS" if result["ok"] else "FAIL") + ": " + str(args.output_dir / "results.json"))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
