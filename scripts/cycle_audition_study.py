"""Validate bounded audition plans and resolve immutable saved audio artifacts."""

import math
from pathlib import Path
import wave

from cycle_performance_suite import content_hash, identifier, number, object_fields, read_json
from cycle_reference_study import file_digest


def seconds(value, location):
    return number(value, 0, 120, location)


def interval(value, location):
    object_fields(value, {"startSeconds", "endSeconds"}, location)
    start = seconds(value.get("startSeconds"), location)
    end = seconds(value.get("endSeconds"), location)
    if end <= start:
        raise ValueError(f"{location}: end must follow start")
    return {"startSeconds": start, "endSeconds": end}


def document(path):
    if Path(path).stat().st_size > 64 * 1024 * 1024:
        raise ValueError("JSON artifact exceeds 64 MiB")
    value, digest = read_json(Path(path))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object: {path}")
    return value, digest


def select(items, key, value):
    matches = [item for item in items if item.get(key) == value]
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one {key}={value}")
    return matches[0]


def resolve_selector(value, base):
    object_fields(value, {"performanceResults", "caseId", "repetition", "referenceStudy",
                          "observationId", "crop"}, "artifact selector")
    performance = "performanceResults" in value
    key = "performanceResults" if performance else "referenceStudy"
    allowed = {key, "caseId", "repetition", "crop"} if performance else {key, "observationId", "crop"}
    object_fields(value, allowed, "artifact selector")
    if not isinstance(value.get(key), str):
        raise ValueError(f"Artifact selector requires {key}")
    path = (base / value[key]).resolve(strict=True)
    saved, digest = document(str(path))
    provenance = {"kind": key, "reportPath": str(path), "reportSha256": digest}
    if performance:
        if saved.get("schema") != "cycle-performance-results.v1" or saved.get("ok") is not True or saved.get("status") != "complete":
            raise ValueError("Performance results must be complete and successful")
        preset = saved["preset"]
        snapshot = (path.parent / preset["snapshot"]).resolve(strict=True)
        effective, _ = document(str(path.parent / "effective-preset.cyclegraph"))
        if (file_digest(snapshot) != preset["sourceSha256"]
                or content_hash(effective) != preset["effectiveGraphSha256"]
                or preset["finalGraphSha256"] != preset["effectiveGraphSha256"]
                or content_hash(saved["suite"]["source"]["document"]) != saved["suiteSha256"]):
            raise ValueError("Captured preset or embedded suite identity changed")
        entry = select(saved["cases"], "id", value.get("caseId"))
        performance_case = select(saved["suite"]["cases"], "id", value.get("caseId"))
        if content_hash(performance_case["performance"]) != entry["performanceSha256"]:
            raise ValueError("Captured performance identity changed")
        repetition = number(value.get("repetition", 1), 1, 16, "repetition", True)
        rendered = select(entry["renders"], "repetition", repetition)
        audio = (path.parent / rendered["path"]).resolve(strict=True)
        expected = rendered["wavSha256"]
        provenance.update(case=entry, preset=saved["preset"], engine=saved["engine"],
                          suiteSha256=saved["suiteSha256"], repetition=repetition,
                          performance=performance_case["performance"])
        low_confidence = fallback = False
        crop = None
    else:
        if saved.get("schemaVersion") != 1 or "observations" not in saved:
            raise ValueError("Reference study report requires schemaVersion 1")
        if content_hash(saved["study"]) != saved["studySha256"]:
            raise ValueError("Embedded reference study identity changed")
        entry = select(saved["observations"], "id", value.get("observationId"))
        declared = select(saved["study"]["references"], "id", value.get("observationId"))
        if any(entry[key] != declared[key] for key in ("referenceId", "labels")):
            raise ValueError("Reference observation disagrees with resolved study")
        analysis_path = (path.parent / entry["analysisPath"]).resolve(strict=True)
        analysis, analysis_hash = document(str(analysis_path))
        if analysis_hash != entry["analysisSha256"] or analysis["referenceId"] != entry["referenceId"]:
            raise ValueError("Reference analysis identity changed")
        audio = Path(analysis["audioPath"]).resolve(strict=True)
        expected = entry["sourceAudioSha256"]
        crop = {key: entry["observation"][key] for key in ("startSeconds", "endSeconds")}
        provenance.update(observation=entry, studySha256=saved["studySha256"],
                          pitchState=analysis["pitchState"], trackingInsecurity=analysis["trackingInsecurity"])
        low_confidence, fallback = analysis["lowConfidence"], analysis["trackingUsedFallback"]
        if (type(low_confidence) is not bool or type(fallback) is not bool
                or low_confidence != entry["lowConfidence"] or fallback != entry["trackingUsedFallback"]):
            raise ValueError("Reference observation disagrees with authoritative tracking confidence")
        declared_crop = declared["crop"]
        if (crop["startSeconds"] != declared_crop["startSeconds"]
                or crop["endSeconds"] != declared_crop.get("endSeconds", analysis["durationSeconds"])):
            raise ValueError("Reference observation disagrees with declared crop")
    if audio.stat().st_size > 64 * 1024 * 1024 or file_digest(audio) != expected:
        raise ValueError(f"Audio exceeds 64 MiB or its saved hash changed: {audio}")
    with wave.open(str(audio), "rb") as source:
        rate, channels, frames, width = (source.getframerate(), source.getnchannels(),
                                        source.getnframes(), source.getsampwidth())
        if not (8000 <= rate <= 192000 and channels in (1, 2) and width in (1, 2, 3, 4)
                and 0 < frames <= 2_000_000 and source.getcomptype() == "NONE"):
            raise ValueError("Unsupported or oversized PCM WAV")
        source.setpos(frames - 1)
        if len(source.readframes(1)) != channels * width:
            raise ValueError("Truncated PCM WAV")
    if performance and any(rendered[key] != actual for key, actual in
                           (("sampleRate", rate), ("channels", channels), ("frames", frames))):
        raise ValueError("Saved capture dimensions changed")
    crop = crop or {"startSeconds": 0, "endSeconds": frames / rate}
    if "crop" in value:
        selected = interval(value["crop"], "selector.crop")
        if selected["endSeconds"] > crop["endSeconds"] - crop["startSeconds"]:
            raise ValueError("Selector crop exceeds its saved observation")
        crop = {key: crop["startSeconds"] + selected[key] for key in selected}
    if not 0 <= crop["startSeconds"] < crop["endSeconds"] <= frames / rate + 1 / rate:
        raise ValueError("Observation crop exceeds source audio")
    return {"audioPath": str(audio), "audioSha256": expected, "sampleRate": rate,
            "channels": channels, "sampleWidth": width, "frames": frames, "crop": crop, "provenance": provenance,
            "lowConfidence": low_confidence, "trackingUsedFallback": fallback}


def load_study(path):
    path = path.resolve(strict=True)
    value, digest = document(str(path))
    object_fields(value, {"schema", "name", "cases", "rmsMatch", "alignment"}, "audition study")
    if value.get("schema") != "cycle-audition-study.v1":
        raise ValueError("Expected cycle-audition-study.v1")
    cases = value.get("cases")
    if not isinstance(cases, list) or not 1 <= len(cases) <= 32:
        raise ValueError("Study requires 1–32 cases")
    if not isinstance(value.get("rmsMatch", True), bool) or not isinstance(value.get("name", path.stem), str):
        raise ValueError("Invalid rmsMatch or name")
    alignment = value.get("alignment")
    if alignment is not None:
        object_fields(alignment, {"maxLagMs", "calibration"}, "alignment")
        alignment = {"maxLagMs": number(alignment.get("maxLagMs"), 0, 10, "maxLagMs"),
                     "calibration": interval(alignment.get("calibration"), "calibration")}
    result = {"schema": value["schema"], "name": value.get("name", path.stem),
              "sourcePath": str(path), "sourceSha256": digest, "rmsMatch": value.get("rmsMatch", True),
              "alignment": alignment, "cases": []}
    identifiers, duration, windows = set(), 0, 0
    for case in cases:
        object_fields(case, {"id", "partition", "a", "b", "intervals", "pitch"}, "case")
        key = identifier(case.get("id"), "case.id")
        if key in identifiers or case.get("partition") not in ("objective", "withheld"):
            raise ValueError("Case ids must be distinct and partition explicit")
        identifiers.add(key)
        pair = [resolve_selector(case.get(side), path.parent) for side in ("a", "b")]
        if pair[0]["sampleRate"] != pair[1]["sampleRate"]:
            raise ValueError("Compared sample rates differ; resampling is not implicit")
        available = min(side["crop"]["endSeconds"] - side["crop"]["startSeconds"] for side in pair)
        phases, names = [], set()
        if not isinstance(case.get("intervals"), list) or not 1 <= len(case["intervals"]) <= 16:
            raise ValueError("Each case requires 1–16 explicit intervals")
        for phase in case["intervals"]:
            object_fields(phase, {"id", "role", "startSeconds", "endSeconds"}, "interval")
            phase_id = identifier(phase.get("id"), "interval.id")
            bounds = interval({key: phase.get(key) for key in ("startSeconds", "endSeconds")}, "interval")
            if (phase_id in names or phase.get("role") not in ("attack", "sustain", "release", "trajectory")
                    or bounds["endSeconds"] > available
                    or round(bounds["endSeconds"] * pair[0]["sampleRate"]) <= round(bounds["startSeconds"] * pair[0]["sampleRate"])):
                raise ValueError("Invalid, duplicate or out-of-range analysis interval")
            names.add(phase_id)
            phases.append({"id": phase_id, "role": phase["role"], **bounds})
            windows += (math.ceil((bounds["endSeconds"] - bounds["startSeconds"]) * pair[0]["sampleRate"] / 512) + 1) * (4 if alignment else 2)
        if alignment and alignment["calibration"]["endSeconds"] > available:
            raise ValueError("Calibration exceeds compared crops")
        pitch = case.get("pitch")
        if pitch is not None:
            object_fields(pitch, {"midiNote", "origin", "confidence", "monophonic"}, "pitch")
            number(pitch.get("midiNote"), 0, 127, "pitch.midiNote")
            number(pitch.get("confidence"), 0, 1, "pitch.confidence")
            if pitch.get("origin") not in ("observed", "inferred") or type(pitch.get("monophonic")) is not bool:
                raise ValueError("Pitch requires origin and explicit monophonic status")
        duration += sum(side["crop"]["endSeconds"] - side["crop"]["startSeconds"] for side in pair)
        result["cases"].append({"id": key, "partition": case["partition"], "a": pair[0], "b": pair[1],
                                "intervals": phases, "pitch": pitch})
    if duration > 120 or windows > 4096:
        raise ValueError("Study exceeds 120 audio seconds or 4096 descriptor windows")
    return result
