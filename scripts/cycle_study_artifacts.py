"""Immutable artifact and stage boundaries shared by bounded Cycle studies."""

import json

from cycle_reference_study import file_digest

DEFAULT_FREEZE_SCHEMA = "cycle-saxophone-freeze.v1"
MUTABLE = {"freeze.json", "results.json", "index.html"}


def write(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")
    temporary.replace(path)


def verify(identities):
    for name, digest in identities.items():
        if file_digest(name) != digest:
            raise ValueError(f"Frozen input changed: {name}")


def frozen_inputs(plan, output, engine, schema=DEFAULT_FREEZE_SCHEMA):
    if (output / "audit").exists():
        raise ValueError("Audit output already exists; it cannot be overwritten or resumed")
    return verify_objective(plan, output, engine, schema)


def verify_objective(plan, output, engine, schema=DEFAULT_FREEZE_SCHEMA):
    """Recheck the frozen objective before and after audit work, without a reuse check."""
    freeze = json.loads((output / "freeze.json").read_text())
    results = json.loads((output / "results.json").read_text())
    if (freeze.get("schema") != schema or results.get("status") != "objective-complete"
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


def freeze_objective(plan, output, engine, inputs, schema=DEFAULT_FREEZE_SCHEMA):
    verify(inputs)
    freeze = {"schema": schema, "planPath": plan["path"], "inputs": inputs,
        "engineSha256": engine["executableSha256"], "artifacts": {
            str(path.relative_to(output)): file_digest(path) for path in output.rglob("*")
            if path.is_file() and str(path.relative_to(output)) not in MUTABLE}}
    write(output / "freeze.json", freeze)
    return file_digest(output / "freeze.json")


def reference_inputs(report_path):
    report = json.loads(report_path.read_text())
    inputs = {}
    for observed in report["observations"]:
        analysis = json.loads((report_path.parent / observed["analysisPath"]).read_text())
        inputs[analysis["audioPath"]] = observed["sourceAudioSha256"]
    return inputs
