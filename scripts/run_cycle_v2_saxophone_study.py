#!/usr/bin/env python3
"""Run the fixed saxophone intervention, then audit reserved performances."""

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
from cycle_audio_diff import exact_sample_comparison, read_wav
from cycle_audition_study import load_study
from cycle_performance_suite import content_hash
from cycle_reference_study import collect_study, file_digest
from cycle_saxophone_study import (DEFAULT_PLAN, assert_peak_only, assert_unused_cc118,
                                   economy, load_plan, peak_edit)
from render_cycle_v2_performance_suite import render_suite
from report_cycle_auditions import generate_report

ROOT = Path(__file__).resolve().parents[1]
MUTABLE = {"freeze.json", "results.json", "index.html"}


def write(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")
    temporary.replace(path)


def verify(identities):
    for name, digest in identities.items():
        if file_digest(name) != digest:
            raise ValueError(f"Frozen input changed: {name}")


def frozen_inputs(plan, output, engine):
    if (output / "audit").exists():
        raise ValueError("Audit output already exists; it cannot be overwritten or resumed")
    freeze = json.loads((output / "freeze.json").read_text())
    results = json.loads((output / "results.json").read_text())
    if (freeze.get("schema") != "cycle-saxophone-freeze.v1"
            or results.get("status") != "objective-complete"
            or results.get("freezeSha256") != file_digest(output / "freeze.json")
            or freeze["planPath"] != plan["path"]
            or any(freeze["inputs"].get(key) != value for key, value in plan["identities"].items())
            or freeze["engineSha256"] != engine["executableSha256"]):
        raise ValueError("Objective freeze, plan or engine identity disagrees")
    verify(freeze["inputs"])
    for name, digest in freeze["artifacts"].items():
        path = (output / name).resolve()
        if not path.is_relative_to(output.resolve()) or file_digest(path) != digest:
            raise ValueError(f"Frozen objective artifact changed: {name}")
    return results


def phases(case):
    events = case["performance"]["events"]
    if sum(event["type"] == "noteOn" for event in events) > 1:
        return [{"id": "trajectory", "role": "trajectory", "startSeconds": 0, "endSeconds": 2}]
    release = next(event["timeMs"] / 1000 for event in events if event["type"] == "noteOff")
    moving = any(event["type"] == "controller" and event["timeMs"] > 0 for event in events)
    return [{"id": key, "role": role, "startSeconds": start, "endSeconds": end}
            for key, role, start, end in (("attack", "attack", 0, .08),
                ("body", "trajectory" if moving else "sustain", .08, release),
                ("release", "release", release, 2))]


def selector(folder, identifier):
    return {"performanceResults": folder + "/results.json", "caseId": identifier}


def report(destination, name, cases):
    path = destination / (name + "-study.json")
    write(path, {"schema": "cycle-audition-study.v1", "name": "Saxophone — " + name,
                 "rmsMatch": True, "cases": cases})
    generate_report(load_study(path), destination / name)
    return json.loads((destination / name / "report.json").read_text())


def summary(output, result):
    write(output / "results.json", result)
    links = [f"<li><a href='{name}/index.html'>{name}</a></li>" for name in
             ("intervention", "controls", "reference", "audit/withheld") if (output / name / "index.html").is_file()]
    (output / "index.html").write_text("<!doctype html><meta charset='utf-8'><title>Saxophone pilot</title>"
        "<h1>Bounded saxophone pilot</h1><p>Incomplete reference coverage. Acoustic-model and search gates remain closed. "
        "One predeclared slower-peak intervention; no automatic or taste-based selection.</p>"
        "<p>CC1 envelope morph is latched at note-on; CC2 mesh morph is live. One inferred growl recording "
        "cannot validate register, dynamics, attack or controller trajectories. No independent withheld recording exists.</p>"
        + "<p>Status: " + html.escape(result["status"]) + "</p><ul>" + "".join(links)
        + "</ul><p><a href='results.json'>Evidence, economy and missing coverage</a></p>")


def run_stage(plan, output, stage, send, engine):
    # Reject unsafe reuse and changed frozen inputs before dispatching any app command.
    if stage == "objective":
        output.mkdir(parents=True, exist_ok=False)
        result = {"schema": "cycle-saxophone-study.v1", "status": "running", "engine": engine,
                  "candidateSelection": "predeclared intervention; no selection or improvement claim",
                  "incompleteReferenceCoverage": True, "acousticModelGate": False, "searchGate": False,
                  "missingCoverage": plan["document"]["missingCoverage"]}
        destination = output
    else:
        result = frozen_inputs(plan, output, engine)
        destination = output / "audit"
        destination.mkdir()
    def transport(payload, **kwargs):
        response = send(payload, **kwargs)
        with (destination / "commands.jsonl").open("a") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        return response
    def invoke(payload):
        response = transport(payload)
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError(f"Automation failed: {response}")
        return response["result"].get("data", {})
    def export(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()
    try:
        if stage == "objective":
            verify(plan["identities"])
            write(output / "split.json", {"plan": plan["document"], "inputs": plan["identities"],
                "objective": [case["id"] for case in plan["objectiveSuite"]["cases"]],
                "withheld": [case["id"] for case in plan["withheldSuite"]["cases"]]})
            invoke({"command": "openGraph", "path": plan["baselinePath"], "includeSnapshot": False})
            invoke({"command": "setExplanation", "scope": "preset", "text":
                "Bounded saxophone pilot: CC1 selects volume articulation at note-on; CC2 continuously morphs "
                "waveform and spectrum. Hold CC3 at zero. Baseline and slower-peak intervention share one "
                "structure. Reference coverage is incomplete; no acoustic fidelity or taste claim is verified."})
            assert_unused_cc118(json.loads(export("baseline-inspection.cyclegraph")))
            invoke({"command": "saveGraph", "path": str(output / "baseline.cyclegraph")})
            before = export("baseline-inspection.cyclegraph")
            recipe = plan["document"]["candidate"]
            controls = invoke({"command": "inspectNodeControls", "nodeId": recipe["nodeId"]})
            edited = invoke({"command": "applyNodeEdits", "expectedDocumentRevision": controls["documentRevision"],
                             "edits": [peak_edit(controls, recipe)]})
            if not edited["changed"] or edited["documentRevision"] != controls["documentRevision"] + 1:
                raise RuntimeError("Intervention did not commit as one semantic edit")
            after = export("candidate-inspection.cyclegraph")
            assert_peak_only(json.loads(before), json.loads(after), recipe)
            for action, expected in (("undo", before), ("redo", after)):
                invoke({"command": action})
                if export(action + ".cyclegraph") != expected:
                    raise RuntimeError(action + " did not restore exact graph content")
            invoke({"command": "saveGraph", "path": str(output / "candidate.cyclegraph")})
            invoke({"command": "openGraph", "path": str(output / "candidate.cyclegraph"), "includeSnapshot": False})
            if export("reloaded.cyclegraph") != after:
                raise RuntimeError("Candidate changed on reload")
            result.update(economy={"baseline": economy(json.loads(before)), "candidate": economy(json.loads(after))},
                          semanticEdit={"recipe": recipe, "oneFactor": True, "undoRedoReloadExact": True})
            reference_path = collect_study(plan["referenceStudy"], output / "reference-analysis", invoke)
            references = json.loads(reference_path.read_text())
            for observed, declared in zip(references["observations"], plan["referenceStudy"]["references"]):
                if observed["sourceAudioSha256"] != file_digest(declared["source"]):
                    raise RuntimeError("Library reference differs from the declared source recording")
        suite = plan["objectiveSuite" if stage == "objective" else "withheldSuite"]
        for name in ("baseline", "candidate"):
            captured = render_suite(copy.deepcopy(suite), output / (name + ".cyclegraph"),
                                    destination / (name + "-renders"), transport, engine)
            if (not captured["ok"] or not all(case["repeatabilityVerified"] and all(
                    math.isfinite(render["capture"][key]) and render["capture"]["rms"] > 1e-7
                    for render in case["renders"] for key in ("rms", "peak")) for case in captured["cases"])
                    or captured["preset"]["effectiveGraphSha256"] != content_hash(
                        json.loads((output / (name + ".cyclegraph")).read_text()))):
                raise RuntimeError("Capture failed, changed graph, is silent, nonfinite or nonrepeatable")
        partition = "objective" if stage == "objective" else "withheld"
        intervention = report(destination, "intervention" if stage == "objective" else "withheld", [
            {"id": case["id"], "partition": partition, "a": selector("baseline-renders", case["id"]),
             "b": selector("candidate-renders", case["id"]), "intervals": phases(case)} for case in suite["cases"]])
        if stage == "objective":
            control_intervals = [{"id": "before", "role": "sustain", "startSeconds": 0, "endSeconds": .18},
                                 {"id": "motion", "role": "trajectory", "startSeconds": .2, "endSeconds": 1.2}]
            controls = report(output, "controls", [{"id": key, "partition": "objective",
                "a": selector("baseline-renders", left), "b": selector("baseline-renders", right),
                "intervals": control_intervals if key != "cc1-onset" else phases(next(case for case in suite["cases"] if case["id"] == "anchor"))}
                for key, left, right in (("cc1-onset", "anchor", "cc1-onset-high"),
                    ("cc2-response", "unused-cc118", "cc2-motion"), ("schedule-drift", "anchor", "unused-cc118"))])
            response, drift = [next(case for case in controls["cases"] if case["id"] == key)
                               ["intervals"][1]["raw"]["meanAbsoluteDifference"] for key in ("cc2-response", "schedule-drift")]
            result["cc2ResponseExceedsScheduleDrift"] = {key: value > drift[key] + 1e-10 for key, value in response.items()}
            decoded = [read_wav(output / "baseline-renders" / (key + "-01.wav"))
                       for key in ("anchor", "unused-cc118", "cc2-motion")]
            prefix = lambda audio: {**audio, "frames": round(.2 * audio["sampleRate"]),
                                   "channels": [channel[:round(.2 * audio["sampleRate"])] for channel in audio["channels"]]}
            result["fullPreMotionDiagnostics"] = {key: exact_sample_comparison(prefix(decoded[a]), prefix(decoded[b]))
                for key, a, b in (("scheduleDrift", 0, 1), ("cc2Response", 1, 2))}
            observed = references["observations"][0]
            length = 1.12
            report(output, "reference", [{"id": name + "-growl-body", "partition": "objective",
                "a": {"referenceStudy": "reference-analysis/report.json", "observationId": observed["id"],
                      "crop": {"startSeconds": .5, "endSeconds": .5 + length}},
                "b": {**selector(name + "-renders", "anchor"), "crop": {"startSeconds": .08, "endSeconds": .08 + length}},
                "intervals": [{"id": "body", "role": "sustain", "startSeconds": 0, "endSeconds": length}]}
                for name in ("baseline", "candidate")])
            verify(plan["identities"])
            result.update(status="objective-complete", objectiveSummary=intervention["objectiveSummary"])
            write(output / "objective-results.json", result)
            inputs = dict(plan["identities"])
            for analysis in (output / "reference-analysis/analyses").glob("*.json"):
                record = json.loads(analysis.read_text())
                inputs[record["audioPath"]] = next(observed["sourceAudioSha256"] for observed in references["observations"]
                                                  if observed["referenceId"] == record["referenceId"])
            verify(inputs)
            freeze = {"schema": "cycle-saxophone-freeze.v1", "planPath": plan["path"], "inputs": inputs,
                "engineSha256": engine["executableSha256"], "artifacts": {
                    str(path.relative_to(output)): file_digest(path) for path in output.rglob("*")
                    if path.is_file() and str(path.relative_to(output)) not in MUTABLE}}
            write(output / "freeze.json", freeze)
            result["freezeSha256"] = file_digest(output / "freeze.json")
        else:
            result.update(status="complete", audit={"selectedCandidateUnchanged": True,
                "reservedPerformanceCases": [case["id"] for case in suite["cases"]],
                "engine": engine, "report": "audit/withheld/index.html"})
    except Exception as error:
        result.update(status="failed", failedStage=stage, error=str(error))
    summary(output, result)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("objective", "audit"), required=True)
    parser.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        plan, output = load_plan(args.plan), args.output_dir.resolve()
        if not args.in_session:
            invocation = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
            if args.no_build:
                invocation.append("--no-build")
            return subprocess.run(invocation + ["--", sys.executable, str(Path(__file__).resolve()), "--stage", args.stage,
                "--plan", plan["path"], "--output-dir", str(output), "--in-session"], cwd=ROOT, check=False).returncode
        if not all(os.environ.get(key) for key in ("CYCLE_AGENT_SESSION", "CYCLE_APP_PID", "CYCLE_AUTOMATION_ARTIFACTS")):
            raise ValueError("--in-session requires the exact-instance coordinator environment")
        pid = int(os.environ["CYCLE_APP_PID"])
        identity = process_identity(pid)
        if identity is None:
            raise RuntimeError("Coordinated app is no longer running")
        engine = {"pid": pid, "processIdentity": identity, "executableSha256": file_digest(identity["executable"]),
                  "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"]}
        result = run_stage(plan, output, args.stage,
            lambda payload, **kwargs: command(os.environ["CYCLE_AGENT_SESSION"], payload, **kwargs), engine)
        print(f"{result['status']}: {output / 'results.json'}")
        return 1 if result["status"] == "failed" else 0
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f"Saxophone pilot failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
