#!/usr/bin/env python3
"""Author and verify two semantic preset candidates through the exact app instance."""

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys

from cycle_automation_support.identity import process_identity
from cycle_automation_support.session import command
from cycle_performance_suite import load_suite
from render_cycle_v2_performance_suite import file_hash, render_suite


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_BASELINE = SCRIPT_DIR.parent / "cycle-v2/content/presets/old/sat-bass.cyclegraph"
DEFAULT_SUITE = SCRIPT_DIR / "fixtures/cycle-v2-performance-suite.json"


def curve_edit(controls, gain, exponent):
    """Edit the domain payload returned by inspectNodeControls, preserving other fields."""
    model = copy.deepcopy(controls["model"])
    revision = model["revision"]
    model["revision"] = model["state"]["revision"] = revision + 1
    for vertex in model["state"]["vertices"]:
        vertex["y"] = vertex["x"] ** exponent
    return {"nodeId": controls["nodeId"], "parameters": [{"id": "pre", "value": str(gain)}],
            "expectedModelRevision": revision, "model": model}


def mesh_edit(controls, scale):
    model = copy.deepcopy(controls["model"])
    revision = model["revision"]
    model["revision"] = revision + 1
    for vertex in model["mesh"]["vertices"]:
        vertex["amp"] = 0.5 + (vertex["amp"] - 0.5) * scale
    return {"nodeId": controls["nodeId"], "expectedModelRevision": revision, "model": model}


def create_candidates(baseline, output, suite, send, engine):
    output.mkdir(parents=True, exist_ok=False)
    evidence = {"schema": "cycle-semantic-candidates.v1", "ok": False,
                "baselineSource": str(baseline), "engine": engine, "candidates": []}

    def invoke(payload, require_ok=True, **kwargs):
        response = send(payload, require_ok=False, **kwargs)
        with (output / "commands.jsonl").open("a", encoding="utf-8") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        ok = response.get("ok") and response.get("result", {}).get("ok")
        if require_ok and not ok:
            raise RuntimeError(f"Automation command failed: {response}")
        return response["result"].get("data", {}) if require_ok else response

    def inspect(node):
        return invoke({"command": "inspectNodeControls", "nodeId": node})

    def export(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()

    def state():
        snapshot = invoke({"command": "snapshotState"})
        return {key: snapshot[key] for key in ("documentRevision", "documentDirty", "canUndo", "canRedo")}

    try:
        capabilities = invoke({"command": "inspectSynthesisCapabilities"})
        if capabilities.get("authoring", {}).get("command") != "applyNodeEdits":
            raise RuntimeError("App does not advertise semantic candidate authoring")
        invoke({"command": "openGraph", "path": str(baseline), "includeSnapshot": False})
        shape = "waveshaper"
        inspect(shape)
        inspect("timeLayer1")
        saved_baseline = output / "baseline.cyclegraph"
        invoke({"command": "saveGraph", "path": str(saved_baseline)})
        baseline_bytes = export("baseline-inspection.cyclegraph")

        for name, gain, exponent, scale in (("warm", 0.55, 0.75, None), ("lean", 0.7, 1.7, 0.6)):
            invoke({"command": "openGraph", "path": str(saved_baseline), "includeSnapshot": False})
            controls = inspect(shape)
            edits = [curve_edit(controls, gain, exponent)]
            if scale is not None:
                edits.append(mesh_edit(inspect("timeLayer1"), scale))
            result = invoke({"command": "applyNodeEdits", "expectedDocumentRevision": controls["documentRevision"],
                             "edits": edits})
            if not result["changed"] or result["documentRevision"] != controls["documentRevision"] + 1:
                raise RuntimeError("Candidate did not commit as one document change")
            candidate_bytes = export(f"{name}-inspection.cyclegraph")
            invoke({"command": "undo"})
            if export(f"{name}-undo.cyclegraph") != baseline_bytes:
                raise RuntimeError("Undo did not restore exact baseline content")
            invoke({"command": "redo"})
            if export(f"{name}-redo.cyclegraph") != candidate_bytes:
                raise RuntimeError("Redo did not restore exact candidate content")
            candidate = output / f"{name}.cyclegraph"
            invoke({"command": "saveGraph", "path": str(candidate)})
            invoke({"command": "openGraph", "path": str(candidate), "includeSnapshot": False})
            if export(f"{name}-reloaded.cyclegraph") != candidate_bytes:
                raise RuntimeError("Candidate content changed on reload")

            before = state()
            invalid = curve_edit(inspect(shape), 0.5, 1)
            invalid["model"]["state"]["vertices"][0]["x"] = "invalid"
            rejected = invoke({"command": "applyNodeEdits", "expectedDocumentRevision": before["documentRevision"],
                               "edits": [{"nodeId": "voice", "parameters": [{"id": "octave", "value": "1"}]}, invalid]},
                              require_ok=False)
            if rejected.get("result", {}).get("ok") or state() != before:
                raise RuntimeError("Malformed candidate changed document state")
            if export(f"{name}-after-rejection.cyclegraph") != candidate_bytes:
                raise RuntimeError("Malformed candidate changed graph content")
            rendered = render_suite(copy.deepcopy(suite), candidate, output / f"{name}-renders", send, engine)
            if not rendered["ok"]:
                raise RuntimeError(rendered.get("error", "Candidate audio suite failed"))
            if not any(render["capture"].get("rms", 0) > 1e-7
                       for case in rendered["cases"] for render in case["renders"]):
                raise RuntimeError("Candidate audio is silent")
            evidence["candidates"].append({"id": name, "preset": candidate.name,
                    "presetSha256": file_hash(candidate), "suite": f"{name}-renders/results.json",
                    "pcmSha256": [case["renders"][0]["pcmSha256"] for case in rendered["cases"]]})
        if evidence["candidates"][0]["pcmSha256"] == evidence["candidates"][1]["pcmSha256"]:
            raise RuntimeError("Candidates produced identical audio")
        evidence.update(ok=True, verified=["atomic commit", "undo", "redo", "reload", "rejection isolation",
                                           "distinct nonzero audio", "seeded repeatability"])
    except Exception as error:
        evidence["error"] = str(error)
    (output / "results.json").write_text(json.dumps(evidence, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE,
                        help="Compatible graph with waveshaper, timeLayer1 and voice nodes (default: old/sat-bass)")
    parser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        baseline = args.baseline.resolve(strict=True)
        suite = load_suite(args.suite)
        output = args.output_dir.resolve()
        if output.exists():
            raise ValueError(f"Output directory already exists: {output}")
        if not args.in_session:
            coordinator = [sys.executable, str(SCRIPT_DIR / "cycle_automation.py"), "exec", "--product", "cycle-v2"]
            if args.no_build:
                coordinator.append("--no-build")
            worker = [sys.executable, str(Path(__file__).resolve()), "--baseline", str(baseline),
                      "--suite", str(args.suite.resolve()), "--output-dir", str(output), "--in-session"]
            return subprocess.run(coordinator + ["--"] + worker, cwd=SCRIPT_DIR.parent, check=False).returncode
        if not all(os.environ.get(key) for key in ("CYCLE_AGENT_SESSION", "CYCLE_APP_PID", "CYCLE_AUTOMATION_ARTIFACTS")):
            raise ValueError("--in-session requires the exact-instance coordinator environment")
        pid = int(os.environ["CYCLE_APP_PID"])
        identity = process_identity(pid)
        if identity is None:
            raise RuntimeError("Coordinated app is no longer running")
        engine = {"pid": pid, "processIdentity": identity,
                  "executableSha256": file_hash(Path(identity["executable"])),
                  "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"]}
        socket_path = os.environ["CYCLE_AGENT_SESSION"]
        result = create_candidates(baseline, output, suite,
                lambda payload, **kwargs: command(socket_path, payload, **kwargs), engine)
        print(f"{'PASS' if result['ok'] else 'FAIL'}: {output / 'results.json'}")
        if not result["ok"]:
            print(result["error"], file=sys.stderr)
        return 0 if result["ok"] else 1
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Candidate authoring: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
