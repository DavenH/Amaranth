#!/usr/bin/env python3
"""Collect labelled Cycle V2 reference analyses through the existing coordinator."""

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
from pathlib import Path

from cycle_automation_support.session import command


ROOT = Path(__file__).resolve().parents[1]


def finite_number(value, field, minimum=0.0, maximum=None):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{field} must be a finite number")
    if not math.isfinite(value) or value < minimum or (maximum is not None and value > maximum):
        raise ValueError(f"{field} is outside its supported range")
    return value


def read_study(path):
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError("Study must be an object")
    study = value.get("study", value)
    if (not isinstance(study, dict) or type(study.get("schemaVersion")) is not int
            or study["schemaVersion"] != 1):
        raise ValueError("Study requires schemaVersion 1")
    entries = study.get("references")
    if not isinstance(entries, list) or not 1 <= len(entries) <= 64:
        raise ValueError("Study requires between 1 and 64 references")
    result = {"schemaVersion": 1, "name": study.get("name", path.stem), "references": []}
    if not isinstance(result["name"], str):
        raise ValueError("Study name must be text")
    identifiers = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("Each reference must be an object")
        identifier = entry.get("id")
        if not isinstance(identifier, str) or not identifier.strip() or identifier in identifiers:
            raise ValueError("Reference entries require distinct nonempty ids")
        identifiers.add(identifier)
        normalized = {"id": identifier}
        reference_id = entry.get("referenceId")
        source = entry.get("source")
        if reference_id is not None:
            if not isinstance(reference_id, str) or not reference_id:
                raise ValueError("referenceId must be nonempty text")
            normalized["referenceId"] = reference_id
        if source is not None:
            if not isinstance(source, str) or not source:
                raise ValueError("source must be a nonempty path")
            resolved = Path(source).expanduser()
            if not resolved.is_absolute():
                resolved = path.parent / resolved
            resolved = resolved.resolve()
            if reference_id is None and not resolved.is_file():
                raise ValueError(f"Reference source is missing: {resolved}")
            normalized["source"] = str(resolved)
        if reference_id is None and source is None:
            raise ValueError("Each reference needs referenceId or source")
        labels = entry.get("labels", {})
        if not isinstance(labels, dict) or len(labels) > 64:
            raise ValueError("labels must be an object with at most 64 entries")
        normalized["labels"] = {}
        for name, label in labels.items():
            if not name or not isinstance(label, dict) or label.get("origin") not in ("observed", "inferred"):
                raise ValueError("Each label needs an observed or inferred origin")
            content = label.get("value")
            if not isinstance(content, (str, bool, int, float)):
                raise ValueError("Label values must be scalar text, boolean or numbers")
            if isinstance(content, (int, float)) and not math.isfinite(content):
                raise ValueError("Numeric label values must be finite")
            clean = {"value": content, "origin": label["origin"]}
            if "confidence" in label:
                clean["confidence"] = finite_number(label["confidence"], "label confidence", 0, 1)
            if "unit" in label:
                if not isinstance(label["unit"], str):
                    raise ValueError("Label unit must be text")
                clean["unit"] = label["unit"]
            normalized["labels"][name] = clean
        crop = entry.get("crop", {})
        if not isinstance(crop, dict):
            raise ValueError("crop must be an object")
        if set(crop) - {"startSeconds", "endSeconds"}:
            raise ValueError("crop accepts only startSeconds and endSeconds")
        start = finite_number(crop.get("startSeconds", 0), "crop start")
        normalized["crop"] = {"startSeconds": start}
        if "endSeconds" in crop:
            end = finite_number(crop["endSeconds"], "crop end")
            if end <= start:
                raise ValueError("Crop end must follow its start")
            normalized["crop"]["endSeconds"] = end
        result["references"].append(normalized)
    return result


def file_digest(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def observation(analysis, crop):
    duration = analysis["durationSeconds"]
    start = crop["startSeconds"]
    end = crop.get("endSeconds", duration)
    if not 0 <= start < end <= duration:
        raise ValueError("Crop lies outside the analyzed recording interval")
    frames = analysis["pitchFrames"]
    indices = [index for index, frame in enumerate(frames) if start <= frame["timeSeconds"] < end]
    columns = analysis["cyclogram"]["columns"]
    first = math.ceil(start / duration * (columns - 1))
    stop = min(columns, math.ceil(end / duration * (columns - 1)))
    if end == duration:
        stop = columns
    return {"startSeconds": start, "endSeconds": end,
            "pitchFrameIndices": indices, "cyclogramColumnRange": [first, stop],
            "analysisScope": "full decoded recording; crop selects observations without retracking"}


def collect_study(study, output, request, include_grid=False):
    if output.exists() and any(output.iterdir()):
        raise ValueError("Output directory must be empty; use a fresh directory for each study run")
    output.mkdir(parents=True, exist_ok=True)
    analysis_directory = output / "analyses"
    analysis_directory.mkdir(exist_ok=True)
    catalogue = request({"command": "listReferences"})
    sources = {str(Path(record["audioPath"]).resolve()): record["referenceId"]
               for record in catalogue["references"]}
    source_hashes = {}
    content_ids = {}
    analyses = {}
    observations = []
    resolved_study = json.loads(json.dumps(study))
    for entry in resolved_study["references"]:
        reference_id = entry.get("referenceId") or sources.get(entry.get("source"))
        if reference_id is None:
            source = Path(entry["source"])
            digest = file_digest(source)
            reference_id = content_ids.get(digest)
            if reference_id is None:
                for library_path, library_id in sources.items():
                    library_file = Path(library_path)
                    if library_file.stat().st_size != source.stat().st_size:
                        continue
                    if library_path not in source_hashes:
                        source_hashes[library_path] = file_digest(library_file)
                    content_ids[source_hashes[library_path]] = library_id
                    if source_hashes[library_path] == digest:
                        reference_id = library_id
                        break
        if reference_id is None:
            imported = request({"command": "importReference", "path": entry["source"]})
            reference_id = imported["referenceId"]
            content_ids[digest] = reference_id
            sources[entry["source"]] = reference_id
        entry["referenceId"] = reference_id
        if reference_id not in analyses:
            request({"command": "selectReference", "referenceId": reference_id})
            path = analysis_directory / f"{len(analyses):03d}.json"
            analysis = request({"command": "exportReferenceAnalysis", "path": str(path.resolve()),
                                "includeGrid": include_grid})
            if analysis["referenceId"] != reference_id:
                raise RuntimeError("Reference selection changed during analysis export")
            analyses[reference_id] = (analysis, path, file_digest(path),
                                      file_digest(analysis["audioPath"]))
        analysis, path, analysis_digest, audio_digest = analyses[reference_id]
        selected = observation(analysis, entry["crop"])
        observations.append({"id": entry["id"], "referenceId": reference_id,
                             "labels": entry["labels"], "observation": selected,
                             "analysisPath": str(path.relative_to(output)),
                             "analysisSha256": analysis_digest,
                             "sourceAudioSha256": audio_digest,
                             "lowConfidence": analysis["lowConfidence"],
                             "trackingUsedFallback": analysis["trackingUsedFallback"]})
    encoded = json.dumps(resolved_study, sort_keys=True, separators=(",", ":"), allow_nan=False)
    report = {"schemaVersion": 1, "study": resolved_study,
              "studySha256": hashlib.sha256(encoded.encode()).hexdigest(),
              "observations": observations}
    destination = output / "report.json"
    destination.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    return destination


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--session", default=os.environ.get("CYCLE_AGENT_SESSION"))
    parser.add_argument("--include-grid", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args(argv)
    try:
        study_path = args.study.resolve()
        study = read_study(study_path)
        finite_number(args.timeout, "timeout", 1, 120)
        if args.validate_only:
            print(json.dumps(study, indent=2, allow_nan=False))
            return 0
        if args.output is None:
            parser.error("--output is required when collecting a study")
        output = args.output.resolve()
        if not args.session:
            invocation = [sys.executable, str(ROOT / "scripts/cycle_automation.py"),
                          "exec", "--product", "cycle-v2"]
            if args.no_build:
                invocation.append("--no-build")
            invocation.extend(["--", sys.executable, str(Path(__file__).resolve()),
                               "--study", str(study_path), "--output", str(output),
                               "--timeout", str(args.timeout)])
            if args.include_grid:
                invocation.append("--include-grid")
            return subprocess.run(invocation, check=False).returncode

        def request(payload):
            return command(args.session, payload, timeout=args.timeout)["result"].get("data", {})

        print(collect_study(study, output, request, args.include_grid))
        return 0
    except (ValueError, OSError, RuntimeError, KeyError) as error:
        parser.exit(1, f"Reference study failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
