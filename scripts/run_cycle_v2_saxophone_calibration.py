#!/usr/bin/env python3
"""Compare one pitch-envelope ablation with fixed objective and reserved VCSL sources."""

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
from cycle_audition_study import load_study
from cycle_performance_suite import content_hash
from cycle_reference_study import collect_study, file_digest
from cycle_reference_pitch_evidence import pitch_evidence, plot_pitch_evidence
from cycle_saxophone_calibration import (DEFAULT_CALIBRATION, assert_pitch_ablation,
    load_calibration, pitch_ablation_edit, validate_stage_sources)
from cycle_saxophone_study import economy
from cycle_study_artifacts import freeze_objective, frozen_inputs, reference_inputs, verify, verify_objective, write
from render_cycle_v2_performance_suite import render_suite
from report_cycle_auditions import generate_report

ROOT = Path(__file__).resolve().parents[1]
FREEZE_SCHEMA = "cycle-saxophone-calibration-freeze.v1"


def audition(destination, name, cases):
    path = destination / (name + "-study.json")
    write(path, {"schema": "cycle-audition-study.v1", "name": "VCSL calibration — " + name,
                 "rmsMatch": True, "cases": cases})
    generate_report(load_study(path), destination / name)


def analysis(report_path):
    report = json.loads(report_path.read_text())
    result = {}
    for observed in report["observations"]:
        path = report_path.parent / observed["analysisPath"]
        evidence = json.loads(path.read_text())
        if file_digest(path) != observed["analysisSha256"] or evidence["referenceId"] != observed["referenceId"]:
            raise ValueError("Collected analysis identity changed: " + observed["id"])
        result[observed["id"]] = observed, evidence
    return result


def summarize(output, result):
    write(output / "results.json", result)
    sections = []
    for stage, prefix in (("objective", ""), ("audit", "audit/")):
        evidence = result.get(stage, {})
        if not evidence:
            continue
        links = " · ".join(f"<a href='{prefix}{name}/index.html'>{name}</a>" for name in
                           ("baseline-reference", "candidate-reference", "intervention"))
        plots = "".join(f"<h3>{html.escape(case['id'])}</h3><img src='{prefix}pitch/{case['id']}.png' "
                        "style='width:100%;max-width:1100px'>" for case in evidence["pitchComparisons"])
        sections.append(f"<h2>{stage.title()}</h2><p>{links} · "
            f"<a href='{prefix}pitch/pitch-evidence.json'>Full pitch evidence and provenance</a></p>" + plots)
    (output / "index.html").write_text("<!doctype html><meta charset='utf-8'><title>VCSL saxophone calibration</title>"
        "<h1>VCSL saxophone calibration</h1><p>One manually guided pitch-envelope ablation, unchanged shared structure. "
        "Tracked NV pitch spread is a limited measurement; acoustic-model, taste and search gates remain closed. "
        "Vibrato character loss and staccato uncertainty remain separate limitations.</p>"
        "<p>Filename octaves and velocity layers are unverified musical labels. Synth MIDI velocity .65 and "
        "CC1/2/3=0 are calibration choices. No source release time, source CC setting or absolute dynamic match is assumed. "
        "Source and renderer tracking describe effective library state; plots preserve original elapsed time.</p>"
        + "<p>Status: " + html.escape(result["status"]) + ". <a href='results.json'>Results and lineage</a></p>"
        + "".join(sections))


def run_stage(plan, output, stage, send, engine):
    if stage == "objective":
        output.mkdir(parents=True, exist_ok=False)
        result = {"schema": "cycle-saxophone-calibration.v1", "status": "running", "engine": engine,
                  "candidateSelection": "fixed pitch-envelope ablation based only on objective preparation",
                  "acousticModelGate": False, "tasteVerified": False, "searchGate": False,
                  "missingCoverage": plan["document"]["missingCoverage"]}
        destination = output
    else:
        result = frozen_inputs(plan, output, engine, FREEZE_SCHEMA)
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
        inputs = dict(plan["identities"])
        if stage == "objective":
            for partition in ("objective", "withheld"):
                inputs.update(validate_stage_sources(plan, partition))
            verify(inputs)
            write(output / "split.json", {"plan": plan["document"], "inputs": inputs,
                "objective": [case["id"] for case in plan["stages"]["objective"]["cases"]],
                "withheld": [case["id"] for case in plan["stages"]["withheld"]["cases"]]})
            invoke({"command": "openGraph", "path": plan["baselinePath"], "includeSnapshot": False})
            invoke({"command": "setExplanation", "scope": "preset", "text":
                "VCSL saxophone calibration: CC1 volume articulation is latched at note-on, CC2 waveform/spectrum "
                "morph is live, and CC3 remains zero. One candidate disables the pitch envelope to test straighter "
                "tracked NV pitch. Vibrato character, complete acoustic fidelity and taste remain unverified."})
            invoke({"command": "saveGraph", "path": str(output / "baseline.cyclegraph")})
            before = export("baseline-inspection.cyclegraph")
            controls = invoke({"command": "inspectNodeControls", "nodeId": "pitchEnvelope1"})
            edited = invoke({"command": "applyNodeEdits", "expectedDocumentRevision": controls["documentRevision"],
                             "edits": [pitch_ablation_edit()]})
            if not edited["changed"] or edited["documentRevision"] != controls["documentRevision"] + 1:
                raise RuntimeError("Ablation did not commit as one semantic edit")
            after = export("candidate-inspection.cyclegraph")
            assert_pitch_ablation(json.loads(before), json.loads(after))
            for action, expected in (("undo", before), ("redo", after)):
                invoke({"command": action})
                if export(action + ".cyclegraph") != expected:
                    raise RuntimeError(action + " did not restore exact content")
            invoke({"command": "saveGraph", "path": str(output / "candidate.cyclegraph")})
            invoke({"command": "openGraph", "path": str(output / "candidate.cyclegraph"), "includeSnapshot": False})
            if export("reloaded.cyclegraph") != after:
                raise RuntimeError("Candidate changed on reload")
            inputs.update({str(output / (name + ".cyclegraph")): file_digest(output / (name + ".cyclegraph"))
                           for name in ("baseline", "candidate")})
            result.update(economy={"baseline": economy(json.loads(before)), "candidate": economy(json.loads(after))},
                          semanticEdit={"edits": [pitch_ablation_edit()], "oneFactor": True, "undoRedoReloadExact": True})
        partition = "objective" if stage == "objective" else "withheld"
        selected = plan["stages"][partition]
        inputs.update(validate_stage_sources(plan, partition))
        source_path = collect_study(selected["referenceStudy"], destination / "source-analysis", invoke)
        sources = analysis(source_path)
        for case in selected["cases"]:
            if sources[case["id"]][0]["sourceAudioSha256"] != case["sourceSha256"]:
                raise RuntimeError("Reference library source differs from frozen parent")
        rendered = {}
        for name in ("baseline", "candidate"):
            captured = render_suite(copy.deepcopy(selected["suite"]), output / (name + ".cyclegraph"),
                                    destination / (name + "-renders"), transport, engine)
            if (not captured["ok"] or not all(case["repeatabilityVerified"] and all(
                    math.isfinite(render["capture"][key]) and render["capture"]["rms"] > 1e-7
                    for render in case["renders"] for key in ("rms", "peak")) for case in captured["cases"])
                    or captured["preset"]["effectiveGraphSha256"] != content_hash(
                        json.loads((output / (name + ".cyclegraph")).read_text()))):
                raise RuntimeError("Capture failed, changed graph, is silent, nonfinite or nonrepeatable")
            for case in captured["cases"]:
                for capture in case["renders"]:
                    path = destination / (name + "-renders") / capture["path"]
                    inputs[str(path)] = capture["wavSha256"]
                    if capture["repetition"] == 1:
                        rendered[name + "-" + case["id"]] = str(path)
        synthesized_path = collect_study({"schemaVersion": 1, "name": "First-repetition calibration renders",
            "references": [{"id": key, "source": path, "labels": {}, "crop": {"startSeconds": 0}}
                           for key, path in rendered.items()]}, destination / "synthesis-analysis", invoke)
        synthesized = analysis(synthesized_path)
        for key, path in rendered.items():
            if synthesized[key][0]["sourceAudioSha256"] != inputs[path]:
                raise RuntimeError("Synthesis analysis differs from the captured WAV")
        pitch_cases, comparisons = [], []
        for case in selected["cases"]:
            start, end = [case["pitchWindow"][key] for key in ("startSeconds", "endSeconds")]
            source_start = case["crop"]["startSeconds"]
            measurements = {"source": pitch_evidence(sources[case["id"]][1], source_start + start,
                                                    source_start + end, case["standardMidi"])}
            measurements.update({name: pitch_evidence(synthesized[name + "-" + case["id"]][1], start, end,
                                                      case["standardMidi"]) for name in ("baseline", "candidate")})
            pitch_cases.append({"id": case["id"], "partition": partition, "standardMidiInferred": case["standardMidi"],
                "crop": case["crop"], "articulation": case["articulation"], "pitchVarianceTrusted": case["pitchVarianceTrusted"],
                "provenance": {"source": sources[case["id"]][0], **{name: synthesized[name + "-" + case["id"]][0]
                              for name in ("baseline", "candidate")}}, **measurements})
            comparable = (case["articulation"] == "Non-Vibrato" and case["pitchVarianceTrusted"]
                          and all(value["available"] for value in measurements.values()))
            source, baseline, candidate = [measurements[name]["spreadCents"] for name in ("source", "baseline", "candidate")]
            comparisons.append({"id": case["id"], "articulation": case["articulation"],
                "trackedNvSpreadCloser": abs(candidate - source) < abs(baseline - source) if comparable else None,
                **{name: {key: value for key, value in evidence.items() if key != "frames"}
                   for name, evidence in measurements.items()}})
        plot_pitch_evidence(pitch_cases, destination / "pitch")
        def selector(name, case):
            return {"performanceResults": name + "-renders/results.json", "caseId": case["id"], "crop": case["crop"]}
        for name in ("baseline", "candidate"):
            audition(destination, name + "-reference", [{"id": case["id"], "partition": partition,
                "a": {"referenceStudy": "source-analysis/report.json", "observationId": case["id"]},
                "b": selector(name, case), "intervals": case["intervals"]} for case in selected["cases"]])
        audition(destination, "intervention", [{"id": case["id"], "partition": partition,
            "a": selector("baseline", case), "b": selector("candidate", case), "intervals": case["intervals"]}
            for case in selected["cases"]])
        verify(inputs)
        for path in (source_path, synthesized_path):
            inputs.update(reference_inputs(path))
        result[stage] = {"pitchComparisons": comparisons, "reservedReferenceParents": stage == "audit",
                         "sourceAnalysis": str(source_path.relative_to(output)), "allCapturesNonzeroFiniteRepeated": True}
        if stage == "objective":
            result["status"] = "objective-complete"
            write(output / "objective-results.json", result)
            result["freezeSha256"] = freeze_objective(plan, output, engine, inputs, FREEZE_SCHEMA)
        else:
            verify(inputs)
            verify_objective(plan, output, engine, FREEZE_SCHEMA)
            result.update(status="complete", auditEngine=engine, selectedCandidateUnchanged=True)
    except Exception as error:
        result.update(status="failed", failedStage=stage, error=str(error))
    summarize(output, result)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=("objective", "audit"), required=True)
    parser.add_argument("--plan", type=Path, default=DEFAULT_CALIBRATION)
    parser.add_argument("--corpus-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        plan, output = load_calibration(args.plan, args.corpus_root), args.output_dir.resolve()
        if not args.in_session:
            invocation = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
            if args.no_build:
                invocation.append("--no-build")
            return subprocess.run(invocation + ["--", sys.executable, str(Path(__file__).resolve()), "--stage", args.stage,
                "--plan", plan["path"], "--corpus-root", str(args.corpus_root.resolve()),
                "--output-dir", str(output), "--in-session"], cwd=ROOT, check=False).returncode
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
        print(f"Saxophone calibration failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
