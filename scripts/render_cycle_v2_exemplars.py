#!/usr/bin/env python3
"""Demonstrate four explained factory presets through existing semantic commands."""

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
from render_cycle_v2_performance_suite import file_hash, render_suite
from report_cycle_auditions import generate_report
from cycle_audition_study import load_study
from cycle_audio_diff import exact_sample_comparison, read_wav

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BANK = ROOT / "cycle-v2/content/exemplars/index.json"
EXAMPLES = {"sat-bass", "filter-saw", "reed", "warmth"}
PERFORMANCES = ("low-soft", "middle-held", "high-held", "middle-cc1")


def synthesis_identity(graph):
    value = copy.deepcopy(graph)
    for node in value["nodes"]:
        node.pop("rationale", None)
    value.get("presetPresentation", {}).pop("description", None)
    return content_hash(value)


def named(items, identifier):
    return next(item for item in items if item["id"] == identifier)


def load_bank(path):
    value = json.loads(path.read_text())
    if value.get("schema") != "cycle-explained-exemplars.v1" or {entry["id"] for entry in value["examples"]} != EXAMPLES or len(value["examples"]) != 4:
        raise ValueError("Expected the four fixed explained exemplars")
    for entry in value["examples"]:
        source = (path.parent / entry["graph"]).resolve(strict=True)
        graph = json.loads(source.read_text())
        if synthesis_identity(graph) != entry["synthesisSha256"] or not graph["presetPresentation"].get("description"):
            raise ValueError(f"Changed synthesis identity or missing preset note: {entry['id']}")
        for node_id in entry["annotationNodeIds"]:
            if not named(graph["nodes"], node_id).get("rationale"):
                raise ValueError(f"Missing node note: {entry['id']}/{node_id}")
        for control in entry["controls"]:
            owner = named(graph["nodes"], control["nodeId"])["parameters"] if "nodeId" in control else named(graph["guides"], control["guideId"])
            if control["parameterId"] not in owner or control["status"] != "starting-hypothesis":
                raise ValueError("Unknown control or unverified range claim")
        assert_unused_cc118(graph)
        entry.update(sourcePath=str(source), sourceSha256=file_hash(source))
    value["suitePath"] = str((path.parent / value["suite"]).resolve(strict=True))
    return value


def edited_model(controls, recipe):
    model = copy.deepcopy(controls["model"])
    model["revision"] += 1
    model["state"]["revision"] = model["revision"]
    if recipe["kind"] == "transferPoint":
        named(model["state"]["vertices"], recipe["vertexId"])["y"] = recipe["y"]
    elif recipe["kind"] == "releaseStretch":
        mesh = model["state"]["mesh"]
        if mesh["sustainIndices"] != [recipe["sustainCubeIndex"]] or mesh["loopIndices"]:
            raise ValueError("Factory release marker precondition changed")
        vertices = mesh["mainMesh"]["vertices"]
        cube = mesh["mainMesh"]["cubes"][recipe["sustainCubeIndex"]]
        if any(named(vertices, identifier)["phase"] != recipe["boundary"] for identifier in cube["vertexIds"]):
            raise ValueError("Sustain cube does not have the reviewed uniform boundary")
        for vertex in vertices:
            if vertex["phase"] > recipe["boundary"]:
                vertex["phase"] = recipe["boundary"] + recipe["factor"] * (vertex["phase"] - recipe["boundary"])
    else:
        raise ValueError("Unknown fixed model recipe")
    return {"nodeId": controls["nodeId"], "expectedModelRevision": controls["model"]["revision"], "model": model}


def assert_one_factor(before, after, recipe):
    left, right = copy.deepcopy(before), copy.deepcopy(after)
    kind = recipe["kind"]
    if kind == "guideNoise":
        a, b = [named(graph["guides"], recipe["guideId"]) for graph in (left, right)]
        if not math.isclose(b["noise"], recipe["noise"], abs_tol=1e-6):
            raise ValueError("Guide intervention was not applied")
        for item in (a, b):
            item.pop("noise"); item.pop("revision")
    else:
        a, b = [named(graph["nodes"], recipe["nodeId"]) for graph in (left, right)]
        if kind == "disableDelay":
            if b["parameters"]["enabled"] is not False:
                raise ValueError("Delay intervention was not applied")
            for item in (a, b):
                item["parameters"].pop("enabled")
        else:
            states = [item["model"]["state"] for item in (a, b)]
            vertices = [state["vertices"] if kind == "transferPoint" else state["mesh"]["mainMesh"]["vertices"] for state in states]
            for original in vertices[0]:
                active = original["id"] == recipe["vertexId"] if kind == "transferPoint" else original["phase"] > recipe["boundary"]
                if not active:
                    continue
                field = "y" if kind == "transferPoint" else "phase"
                expected = recipe["y"] if kind == "transferPoint" else recipe["boundary"] + recipe["factor"] * (original[field] - recipe["boundary"])
                changed = named(vertices[1], original["id"])
                if not math.isclose(changed[field], expected, abs_tol=1e-6):
                    raise ValueError("Model intervention differs from its exact recipe")
                original.pop(field); changed.pop(field)
            for item in (a, b):
                item["model"].pop("revision"); item["model"]["state"].pop("revision")
    if left != right:
        raise ValueError("The intervention changed unrelated graph state")



def assert_unused_cc118(graph):
    if any((value == 118 or value == "118") for node in graph["nodes"]
           for key, value in node["parameters"].items() if "controller" in key.lower()):
        raise ValueError("CC118 is referenced by this graph; it is not a negative control")


def negative_control_suite(suite_path, destination):
    document = json.loads(suite_path.read_text())
    case = copy.deepcopy(named(document["cases"], "middle-cc1"))
    moving = [event for event in case["events"] if event["type"] == "controller" and event["timeMs"] > 0]
    if [(event["timeMs"], event["controller"], event["value"]) for event in moving] != [(200, 1, 64), (450, 1, 127)]:
        raise ValueError("Reviewed controller schedule changed")
    for event in moving:
        event["controller"] = 118
    case["id"] = "unused-cc118"
    document.update(id="explained-exemplars-cc118", cases=[case], budget={"maxRenders": 2, "maxAudioSeconds": 6, "maxWallSeconds": 120})
    path = destination / "negative-control-suite.json"
    path.write_text(json.dumps(document, indent=2) + "\n")
    return load_suite(path)


def audition_manifest(example, destination):
    intervals = [{"id": name, "role": role, "startSeconds": start, "endSeconds": end}
                 for name, role, start, end in (("attack", "attack", 0, .08), ("body", "sustain", .08, .6), ("release", "release", .6, 2.2))]
    cases = [{"id": key, "partition": "objective", "a": {"performanceResults": "baseline-renders/results.json", "caseId": key},
              "b": {"performanceResults": "variant-renders/results.json", "caseId": key}, "intervals": intervals}
             for key in PERFORMANCES]
    cases.append({"id": "cc1-response", "partition": "objective", "a": {"performanceResults": "negative-renders/results.json", "caseId": "unused-cc118"},
                  "b": {"performanceResults": "baseline-renders/results.json", "caseId": "middle-cc1"},
                  "intervals": [{"id": "before", "role": "sustain", "startSeconds": 0, "endSeconds": .18},
                                {"id": "motion", "role": "trajectory", "startSeconds": .2, "endSeconds": .6}]})
    cases.append({**copy.deepcopy(cases[-1]), "id": "schedule-drift",
                  "a": {"performanceResults": "baseline-renders/results.json", "caseId": "middle-held"},
                  "b": {"performanceResults": "negative-renders/results.json", "caseId": "unused-cc118"}})
    path = destination / "audition-study.json"
    path.write_text(json.dumps({"schema": "cycle-audition-study.v1", "name": example + " — one-factor and CC1 evidence", "cases": cases}, indent=2) + "\n")
    return path



def prefix_diagnostics(prefixes):
    count = round(.2 * prefixes[0]["sampleRate"])
    comparisons = [exact_sample_comparison(
        {**prefixes[0], "channels": [a[:count]], "frames": count},
        {**prefixes[1], "channels": [b[:count]], "frames": count})
        for a, b in zip(prefixes[0]["channels"], prefixes[1]["channels"])]
    first = min((item["firstMismatch"]["frame"] for item in comparisons if item["firstMismatch"]), default=None)
    return {"windowSeconds": [0, .2], "differingSamples": sum(item["differingSamples"] for item in comparisons),
            "worstSampleError": max(abs(a - b) for left, right in zip(prefixes[0]["channels"], prefixes[1]["channels"])
                                    for a, b in zip(left[:count], right[:count])),
            "firstDifferenceFrame": first, "firstDifferenceSeconds": first / prefixes[0]["sampleRate"] if first is not None else None}



def control_evidence(report, destination):
    paths = [destination / folder / name for folder, name in
             (("baseline-renders", "middle-held-01.wav"), ("baseline-renders", "middle-cc1-01.wav"), ("negative-renders", "unused-cc118-01.wav"))]
    held, moving, negative = [read_wav(path) for path in paths]
    count = round(.18 * held["sampleRate"])
    if any(a[:count] != b[:count] for other in (moving, negative) for a, b in zip(held["channels"], other["channels"])):
        raise RuntimeError("CC comparison differs before the declared boundary guard")
    response, drift = [named(report["cases"], identifier)["intervals"][1]["raw"]["meanAbsoluteDifference"]
                       for identifier in ("cc1-response", "schedule-drift")]
    exceeds = {key: value > drift[key] + 1e-10 for key, value in response.items()}
    return {"negativeController": 118, "negativeControllerUnused": True, "negativeControllerChannelMode": False,
            "cc1PreMotionPrefixEqual": True, "cc1VerifiedPrefixSeconds": [0, .18], "cc1BoundaryGuardSeconds": .02,
            "cc1FullPrefixDiagnostics": prefix_diagnostics([held, moving]),
            "negativeControlFullPrefixDiagnostics": prefix_diagnostics([held, negative]),
            "responseFullPrefixDiagnostics": prefix_diagnostics([negative, moving]),
            "cc1DescriptorDifference": response, "scheduleDriftDescriptorDifference": drift,
            "responseExceedsDrift": exceeds, "cc1ResponseDemonstrated": any(exceeds.values())}


def demonstrate(bank, output, send, engine):
    output.mkdir(parents=True, exist_ok=False)
    evidence = {"schema": "cycle-exemplar-demonstrations.v1", "ok": False, "engine": engine, "bank": bank, "examples": []}
    def invoke(payload, **kwargs):
        response = send(payload, **kwargs)
        with (output / "commands.jsonl").open("a") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError(f"Automation failed: {response}")
        return response["result"].get("data", {})
    def export(path):
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()
    try:
        capabilities = invoke({"command": "inspectSynthesisCapabilities"})
        (output / "capabilities.json").write_text(json.dumps(capabilities, indent=2) + "\n")
        control_suite = negative_control_suite(Path(bank["suitePath"]), output)
        for entry in bank["examples"]:
            destination = output / entry["id"]; destination.mkdir()
            source = Path(entry["sourcePath"])
            invoke({"command": "openGraph", "path": str(source), "includeSnapshot": False})
            original = json.loads(source.read_text())
            for node_id in [None] + entry["annotationNodeIds"]:
                note = invoke({"command": "inspectExplanation", "scope": "node" if node_id else "preset", **({"nodeId": node_id} if node_id else {})})
                expected = named(original["nodes"], node_id)["rationale"] if node_id else original["presetPresentation"]["description"]
                if note["text"] != expected:
                    raise RuntimeError("Canonical explanation inspection lost authored text")
            for control in entry["controls"]:
                if "nodeId" in control:
                    node = named(original["nodes"], control["nodeId"])
                    definition = next(item for item in capabilities["nodes"] if item["typeId"] == node["kind"])
                    parameter = named(definition["parameters"], control["parameterId"])
                    if not parameter["minimum"] <= min(control["storedRange"]) <= max(control["storedRange"]) <= parameter["maximum"]:
                        raise RuntimeError("Suggested control range exceeds canonical capabilities")
            baseline = destination / "baseline.cyclegraph"
            invoke({"command": "saveGraph", "path": str(baseline)})
            before = export(destination / "baseline-inspection.cyclegraph")
            recipe = entry["intervention"]
            if recipe["kind"] == "guideNoise":
                invoke({"command": "setGuideParameter", "guideId": recipe["guideId"], "parameterId": "noise", "value": str(recipe["noise"])})
            else:
                controls = invoke({"command": "inspectNodeControls", "nodeId": recipe["nodeId"]})
                edit = ({"nodeId": recipe["nodeId"], "parameters": [{"id": "enabled", "value": "false"}]}
                        if recipe["kind"] == "disableDelay" else edited_model(controls, recipe))
                invoke({"command": "applyNodeEdits", "expectedDocumentRevision": controls["documentRevision"], "edits": [edit]})
            after = export(destination / "variant-inspection.cyclegraph")
            if before == after:
                raise RuntimeError("Intervention is a no-op")
            assert_one_factor(json.loads(before), json.loads(after), recipe)
            invoke({"command": "undo"})
            if export(destination / "undo.cyclegraph") != before:
                raise RuntimeError("Undo did not restore exact baseline")
            invoke({"command": "redo"})
            if export(destination / "redo.cyclegraph") != after:
                raise RuntimeError("Redo did not restore exact variant")
            variant = destination / "variant.cyclegraph"
            invoke({"command": "saveGraph", "path": str(variant)})
            invoke({"command": "openGraph", "path": str(variant), "includeSnapshot": False})
            if export(destination / "reloaded.cyclegraph") != after:
                raise RuntimeError("Variant changed on reload")
            captured = {}
            for name, preset in (("baseline", baseline), ("variant", variant), ("negative", baseline)):
                suite = copy.deepcopy(control_suite) if name == "negative" else load_suite(Path(bank["suitePath"]))
                result = render_suite(suite, preset, destination / (name + "-renders"), send, engine)
                if (not result["ok"] or not all(case["repeatabilityVerified"] for case in result["cases"])
                        or not all(math.isfinite(render["capture"][key]) and render["capture"]["rms"] > 1e-7
                                   for case in result["cases"] for render in case["renders"] for key in ("rms", "peak"))):
                    raise RuntimeError("Exemplar performance is failed, silent or nonrepeatable")
                captured[name] = result
            if (captured["negative"]["engine"]["executableSha256"] != captured["baseline"]["engine"]["executableSha256"]
                    or captured["negative"]["preset"]["effectiveGraphSha256"] != captured["baseline"]["preset"]["effectiveGraphSha256"]):
                raise RuntimeError("Negative control used a different engine or baseline graph")
            report_path = audition_manifest(entry["id"], destination)
            generate_report(load_study(report_path), destination / "audition")
            report = json.loads((destination / "audition/report.json").read_text())
            changed = any(value > 1e-10 for case in report["cases"] if case["id"] in PERFORMANCES for phase in case["intervals"] for value in phase["raw"]["meanAbsoluteDifference"].values())
            if not changed or source.stat().st_size == 0 or file_hash(source) != entry["sourceSha256"]:
                raise RuntimeError("Intervention lacks measured effect or source was modified")
            cc = control_evidence(report, destination)
            evidence["examples"].append({"id": entry["id"], "sourceSha256": entry["sourceSha256"], "synthesisSha256": entry["synthesisSha256"],
                "intervention": recipe, "unrelatedStateUnchanged": True, "undoRedoReloadExact": True, "repeatabilityVerified": True,
                "measuredDifference": True, "allCapturesNonzeroFinite": True, **cc,
                "report": entry["id"] + "/audition/index.html", "reportSha256": file_hash(destination / "audition/report.json")})
        evidence["ok"] = True
    except Exception as error:
        evidence["error"] = str(error)
    (output / "results.json").write_text(json.dumps(evidence, indent=2, allow_nan=False) + "\n")
    (output / "index.html").write_text("<!doctype html><meta charset='utf-8'><title>Explained exemplars</title><h1>Explained exemplars</h1>" + "".join(
        f"<p><a href='{html.escape(entry['report'])}'>{html.escape(entry['id'])}</a></p>" for entry in evidence["examples"]))
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bank", type=Path, default=DEFAULT_BANK)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        bank = load_bank(args.bank.resolve(strict=True))
        load_suite(Path(bank["suitePath"]))
        if args.plan:
            print(json.dumps(bank, indent=2)); return 0
        if args.output_dir is None or args.output_dir.exists():
            raise ValueError("Choose a fresh --output-dir")
        output = args.output_dir.resolve()
        if not args.in_session:
            coordinator = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
            if args.no_build:
                coordinator.append("--no-build")
            return subprocess.run(coordinator + ["--", sys.executable, str(Path(__file__).resolve()), "--bank", str(args.bank.resolve()),
                "--output-dir", str(output), "--in-session"], cwd=ROOT, check=False).returncode
        pid = int(os.environ["CYCLE_APP_PID"])
        identity = process_identity(pid)
        if identity is None or not os.environ.get("CYCLE_AGENT_SESSION"):
            raise ValueError("--in-session requires the exact-instance coordinator environment")
        engine = {"pid": pid, "processIdentity": identity, "executableSha256": file_hash(Path(identity["executable"])),
                  "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"]}
        result = demonstrate(bank, output, lambda payload, **kwargs: command(os.environ["CYCLE_AGENT_SESSION"], payload, **kwargs), engine)
        print(f"{'PASS' if result['ok'] else 'FAIL'}: {output / 'results.json'}")
        if not result["ok"]:
            print(result["error"], file=sys.stderr)
        return 0 if result["ok"] else 1
    except (OSError, ValueError, RuntimeError, KeyError, StopIteration) as error:
        parser.exit(1, f"Exemplar demonstration failed: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
