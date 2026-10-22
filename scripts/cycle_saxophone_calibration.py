"""Fixed VCSL source mappings and the single straight-pitch calibration edit."""

import copy
import json
from pathlib import Path

from cycle_performance_suite import load_suite, object_fields
from cycle_reference_study import file_digest


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CALIBRATION = ROOT / "cycle-v2/content/studies/expressive-sax/vcsl-calibration.json"
BASELINE_SHA = "b3b32402763646b0072a26c3ba47a69f774b8dd63bf6d5adcb140d46ff793f3a"
CORPUS_SHA = "426faf7b9dc662b252c7ebbbffe1ec8c4d5a08a7ab22b94c3b3b73736ebe6d39"
SELECTION_SHA = "6269e348be1725c7a51eeeff22d01dac6e5d512c1732c027cf160db2a4022383"
LICENSE_SHA = "a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499"
COMMIT = "c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e"
CANDIDATE = {"id": "straight-pitch", "nodeId": "pitchEnvelope1", "parameterId": "enabled", "value": False}
OBJECTIVE = [f"BrettTenor_NV_Main_{note}_vl{layer}_rr1" for note in ("C2", "C4") for layer in (2, 3)]
OBJECTIVE += [f"BrettTenor_Staccato_Main_C3_vl{layer}_rr4" for layer in (1, 2)]
OBJECTIVE += ["BrettTenor_Vib_Main_C2_var4", "BrettTenor_Vib_Main_C4_var2"]
WITHHELD = ["BrettTenor_NV_Main_D3_vl2_rr1", "BrettTenor_NV_Main_D3_vl3_rr1",
            "BrettTenor_Staccato_Main_D3_vl2_rr5", "BrettTenor_Vib_Main_D3_var1"]
NOTES = {"C2": 48, "C3": 60, "C4": 72, "D3": 62}


def pitch_ablation_edit():
    return {"nodeId": "pitchEnvelope1", "parameters": [{"id": "enabled", "value": "false"}]}


def assert_pitch_ablation(before, after):
    expected = copy.deepcopy(before)
    nodes = [node for node in expected["nodes"] if node["id"] == "pitchEnvelope1"]
    if (len(nodes) != 1 or nodes[0]["kind"] != "envelope"
            or nodes[0]["parameters"].get("purpose") != "pitch"
            or nodes[0]["parameters"].get("enabled") is not True):
        raise ValueError("Calibration requires the enabled factory pitch envelope")
    nodes[0]["parameters"]["enabled"] = False
    actual = [node for node in after["nodes"] if node["id"] == "pitchEnvelope1"]
    if len(actual) != 1 or actual[0]["parameters"].get("enabled") is not False or after != expected:
        raise ValueError("Calibration must change only pitchEnvelope1.enabled to false")


def stage_case(record, corpus):
    labels = record["sourceLabels"]
    note = labels["noteToken"]
    staccato = labels["articulationFolder"] == "Staccato"
    crop = {"startSeconds": 0, "endSeconds": .45 if staccato else 2.1}
    source = (corpus / record["localPath"]).resolve()
    if not source.is_relative_to(corpus):
        raise ValueError("Reference source escapes the supplied corpus root")
    if (record["wave"]["sampleRate"] != 48000 or record["wave"]["channels"] != 2
            or record["wave"]["sampleWidth"] != 3 or record["wave"]["durationSeconds"] < crop["endSeconds"]):
        raise ValueError("Corpus metadata disagrees with the reviewed native source format or crop")
    return {"id": record["id"], "sourcePath": str(source), "sourceSha256": record["sha256"],
            "articulation": labels["articulationFolder"], "noteToken": note, "standardMidi": NOTES[note],
            "crop": crop, "pitchWindow": {"startSeconds": .04 if staccato else .4,
                                           "endSeconds": .30 if staccato else 2.0},
            "pitchVarianceTrusted": not staccato, "sourceLabels": labels,
            "intervals": ([{"id": "short-trajectory", "role": "trajectory", **crop}] if staccato else [
                {"id": "attack", "role": "attack", "startSeconds": 0, "endSeconds": .08},
                {"id": "body", "role": "sustain", "startSeconds": .08, "endSeconds": 2.1}])}


def reference_study(cases, stage):
    """Translate fixed validated source records to the existing collection schema."""
    references = []
    for case in cases:
        tokens = case["sourceLabels"]
        layer = tokens["velocityLayer"]
        labels = {"articulation": case["articulation"], "filenameNote": case["noteToken"],
                  "standardMidi": case["standardMidi"], "dynamics": "unknown",
                  "sourceLayer": f"vl{layer}" if layer is not None else f"var{tokens['variationToken']}",
                  "performerToken": tokens["performerToken"]}
        references.append({"id": case["id"], "source": case["sourcePath"], "crop": case["crop"],
                           "labels": {key: {"value": value, "origin": "inferred"} for key, value in labels.items()}})
    return {"schemaVersion": 1, "name": "VCSL saxophone calibration — " + stage, "references": references}


def validate_suite(path, cases):
    suite = load_suite(path)
    if suite["repetitions"] != 2 or [case["id"] for case in suite["cases"]] != [case["id"] for case in cases]:
        raise ValueError("Calibration requires the fixed source split and two interleaved repeats")
    for normalized, case in zip(suite["cases"], cases):
        performance = normalized["performance"]
        short = case["articulation"] == "Staccato"
        expected = {"sampleRate": 48000, "blockSize": 512, "channels": 2, "voiceDurationSeconds": 7,
                    "outputGain": .125, "controlNoteOffset": 0, "randomSeed": 1729, "ratePolicy": "native",
                    "durationMs": 2000 if short else 3000,
                    "events": [{"type": "controller", "timeMs": 0, "controller": controller,
                                "value": 0, "channel": 1} for controller in (1, 2, 3)] + [
                        {"type": "noteOn", "timeMs": 0, "note": case["standardMidi"], "velocity": .65, "channel": 1},
                        {"type": "noteOff", "timeMs": 150 if short else 2200,
                         "note": case["standardMidi"], "channel": 1}]}
        if normalized["pattern"] is not None or performance != expected:
            raise ValueError("Calibration performance changed pitch, timing, controls or native capture settings")
    return suite


def load_calibration(path, corpus_root):
    path, corpus = Path(path).resolve(strict=True), Path(corpus_root).resolve(strict=True)
    value = json.loads(path.read_text())
    fields = {"schema", "baseline", "sourceSha256", "corpusManifestSha256", "selectionSha256",
              "objectiveSuite", "withheldSuite", "candidate", "missingCoverage"}
    object_fields(value, fields, "calibration")
    if (set(value) != fields or value["schema"] != "cycle-saxophone-calibration.v1"
            or value["baseline"] != "../../presets/saxophone-3.cyclegraph"
            or value["sourceSha256"] != BASELINE_SHA or value["corpusManifestSha256"] != CORPUS_SHA
            or value["selectionSha256"] != SELECTION_SHA or value["candidate"] != CANDIDATE
            or value["candidate"]["value"] is not False
            or value["objectiveSuite"] != "vcsl-objective-suite.json"
            or value["withheldSuite"] != "vcsl-withheld-suite.json"
            or not isinstance(value["missingCoverage"], list) or not value["missingCoverage"]
            or any(not isinstance(item, str) or not item.strip() for item in value["missingCoverage"])):
        raise ValueError("Expected the fixed VCSL calibration, source identities and explicit limitations")
    baseline = (path.parent / value["baseline"]).resolve(strict=True)
    identities = {str(path): file_digest(path), str(baseline): BASELINE_SHA}
    expected = {"download-manifest.json": CORPUS_SHA, "selection.json": SELECTION_SHA, "LICENSE": LICENSE_SHA}
    for name, digest in expected.items():
        source = corpus / name
        if file_digest(source) != digest:
            raise ValueError("Pinned corpus metadata changed: " + name)
        identities[str(source)] = digest
    if file_digest(baseline) != BASELINE_SHA:
        raise ValueError("Factory saxophone source identity changed")
    graph = json.loads(baseline.read_text())
    candidate = copy.deepcopy(graph)
    next(node for node in candidate["nodes"] if node["id"] == "pitchEnvelope1")["parameters"]["enabled"] = False
    assert_pitch_ablation(graph, candidate)
    manifest = json.loads((corpus / "download-manifest.json").read_text())
    selection = json.loads((corpus / "selection.json").read_text())
    if (manifest["status"] != "complete" or manifest["commit"] != COMMIT or selection["commit"] != COMMIT
            or manifest["selectionSha256"] != SELECTION_SHA
            or manifest["licence"]["identifier"] != "CC0-1.0"
            or manifest["licence"]["sha256"] != LICENSE_SHA
            or manifest["objectiveSourceIds"] != OBJECTIVE or manifest["reservedSourceIds"] != WITHHELD
            or [record["id"] for record in manifest["audioFiles"]] != OBJECTIVE + WITHHELD
            or [record["id"] for record in selection["files"]] != OBJECTIVE + WITHHELD):
        raise ValueError("Corpus provenance or predeclared parent split changed")
    stages = {}
    for stage, identifiers in (("objective", OBJECTIVE), ("withheld", WITHHELD)):
        records = [record for record in manifest["audioFiles"] if record["id"] in identifiers]
        if any(record["split"] != ("objective" if stage == "objective" else "reserved") for record in records):
            raise ValueError("Corpus parent changed partition")
        cases = [stage_case(record, corpus) for record in records]
        suite = validate_suite(path.parent / value[stage + "Suite"], cases)
        identities[suite["source"]["path"]] = suite["source"]["sha256"]
        stages[stage] = {"cases": cases, "suite": suite, "referenceStudy": reference_study(cases, stage)}
    if 2 * sum(stage["suite"]["renderCount"] for stage in stages.values()) > 48 or 2 * sum(
            stage["suite"]["audioSeconds"] for stage in stages.values()) > 160:
        raise ValueError("Calibration exceeds its total capture budget")
    return {"document": value, "path": str(path), "baselinePath": str(baseline), "corpusRoot": str(corpus),
            "identities": identities, "stages": stages}


def validate_stage_sources(plan, stage):
    if stage not in ("objective", "withheld"):
        raise ValueError("Expected objective or withheld source stage")
    identities = {}
    for case in plan["stages"][stage]["cases"]:
        if file_digest(case["sourcePath"]) != case["sourceSha256"]:
            raise ValueError("Calibration source recording changed: " + case["id"])
        identities[case["sourcePath"]] = case["sourceSha256"]
    return identities
