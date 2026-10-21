"""Validate and normalize performances for the existing Cycle V2 audio capture."""

import hashlib
import json
import math
import re
from pathlib import Path


SCHEMA = "cycle-performance-suite.v1"
RENDER_DEFAULTS = {
    "sampleRate": 44100, "blockSize": 512, "channels": 2,
    "voiceDurationSeconds": 7, "outputGain": 0.125, "controlNoteOffset": 0,
    "randomSeed": 1729, "ratePolicy": "native",
}
BUDGET_DEFAULTS = {"maxRenders": 64, "maxAudioSeconds": 120, "maxWallSeconds": 300}


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


def content_hash(value):
    return hashlib.sha256(canonical_json(value).encode("utf-8")).hexdigest()


def read_json(path):
    raw = path.read_bytes()
    value = json.loads(raw)
    return value, hashlib.sha256(raw).hexdigest()


def object_fields(value, allowed, location):
    if not isinstance(value, dict):
        raise ValueError(f"{location} must be an object")
    unknown = value.keys() - allowed
    if unknown:
        raise ValueError(f"{location}: unknown fields {sorted(unknown)}")
    return value


def number(value, low, high, location, integer=False):
    if (isinstance(value, bool) or not isinstance(value, (int, float))
            or not low <= value <= high or not math.isfinite(value)
            or (integer and not isinstance(value, int))):
        kind = "integer" if integer else "number"
        raise ValueError(f"{location} must be a finite {kind} in [{low}, {high}]")
    return value


def identifier(value, location):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,79}", value):
        raise ValueError(f"{location} must be a short identifier using letters, digits, _ or -")
    return value


def render_settings(value):
    object_fields(value, RENDER_DEFAULTS.keys(), "render")
    result = {**RENDER_DEFAULTS, **value}
    for key, low, high in (
            ("sampleRate", 8000, 192000), ("blockSize", 16, 8192),
            ("channels", 1, 2), ("controlNoteOffset", -127, 127),
            ("randomSeed", -(2 ** 63), 2 ** 63 - 1)):
        number(result[key], low, high, f"render.{key}", integer=True)
    number(result["voiceDurationSeconds"], 0.001, 120, "render.voiceDurationSeconds")
    number(result["outputGain"], 0, 16, "render.outputGain")
    if result["ratePolicy"] not in ("native", "legacyInternal44100"):
        raise ValueError("render.ratePolicy must be native or legacyInternal44100")
    return result


def event(value, duration_ms):
    if not isinstance(value, dict):
        raise ValueError("events must contain objects")
    kind = value.get("type")
    fields = {
        "noteOn": {"note", "velocity"}, "noteOff": {"note"},
        "controller": {"controller", "value"},
        "channelPressure": {"value"}, "allNotesOff": set(),
    }
    if not isinstance(kind, str) or kind not in fields:
        raise ValueError(f"Unsupported MIDI event type: {kind}")
    object_fields(value, fields[kind] | {"type", "timeMs", "channel"}, "event")
    time_ms = number(value.get("timeMs"), 0, duration_ms, "event.timeMs")
    if time_ms >= duration_ms:
        raise ValueError("event.timeMs must precede the end of the render")
    result = {"type": kind, "timeMs": time_ms,
              "channel": number(value.get("channel", 1), 1, 16, "event.channel", True)}
    for key in fields[kind]:
        default = 0.8 if key == "velocity" else None
        if key == "velocity":
            result[key] = number(value.get(key, default), 0, 1, "event.velocity")
            if result[key] == 0:
                raise ValueError("event.velocity must be positive; use noteOff to release a note")
        else:
            result[key] = number(value.get(key), 0, 127, f"event.{key}", True)
    return result


def pattern_source(path, channel, tail_ms):
    """Retain a shared pattern for expansion by the app's mature scheduler."""
    document, digest = read_json(path)
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError(f"Unsupported cyclepattern version: {path}")
    sequence = document.get("sequence")
    if not isinstance(sequence, dict):
        raise ValueError("pattern.sequence must be an object")
    seconds = number(sequence.get("durationSeconds"), 0.001, 120, "pattern duration")
    duration_ms = number(seconds * 1000 + tail_ms, 1, 60000, "pattern plus tail durationMs")
    source = {"path": str(path), "sha256": digest, "document": document, "channel": channel}
    return duration_ms, source


def normalize_case(value, defaults, base):
    object_fields(value, {"id", "labels", "render", "durationMs", "note", "velocity",
                          "noteDurationMs", "channel", "events", "pattern", "tailMs"}, "case")
    case_id = identifier(value.get("id"), "case.id")
    labels = value.get("labels", {})
    if not isinstance(labels, dict) or any(not isinstance(item, str) for item in labels.values()):
        raise ValueError("case.labels must map label names to strings")
    settings = render_settings({**defaults, **object_fields(
        value.get("render", {}), RENDER_DEFAULTS.keys(), f"{case_id}.render")})
    modes = [key for key in ("note", "events", "pattern") if key in value]
    if len(modes) != 1:
        raise ValueError(f"{case_id}: choose exactly one of note, events or pattern")
    mode = modes[0]
    if mode != "note" and any(key in value for key in ("velocity", "noteDurationMs")):
        raise ValueError(f"{case_id}: velocity and noteDurationMs require note")
    if mode != "pattern" and "tailMs" in value:
        raise ValueError(f"{case_id}: tailMs requires pattern")
    channel = number(value.get("channel", 1), 1, 16, "case.channel", True)
    pattern = None
    if mode == "pattern":
        if "durationMs" in value:
            raise ValueError(f"{case_id}: pattern duration comes from the library; use tailMs")
        if not isinstance(value["pattern"], str):
            raise ValueError("case.pattern must be a cyclepattern file path")
        tail = number(value.get("tailMs", 500), 0, 60000, "case.tailMs")
        duration, pattern = pattern_source((base / value["pattern"]).resolve(), channel, tail)
    else:
        duration = number(value.get("durationMs"), 1, 60000, f"{case_id}.durationMs")
        events = value.get("events")
        if mode == "note":
            length = number(value.get("noteDurationMs"), 0.001, duration, "case.noteDurationMs")
            events = [
                {"type": "noteOn", "timeMs": 0, "note": value["note"],
                 "velocity": value.get("velocity", 0.8), "channel": channel},
                {"type": "noteOff", "timeMs": length, "note": value["note"], "channel": channel},
            ]
        elif "channel" in value:
            raise ValueError(f"{case_id}: specify channels on individual events")
    performance = {**settings, "durationMs": duration}
    if pattern is None:
        if not isinstance(events, list) or len(events) > 2048:
            raise ValueError(f"{case_id}: events must be an array with at most 2048 entries")
        performance["events"] = sorted((event(item, duration) for item in events), key=lambda item: item["timeMs"])
    return {"id": case_id, "labels": labels, "performance": performance,
            "performanceSha256": content_hash(performance) if pattern is None else None, "pattern": pattern}


def load_suite(path, repetitions=None):
    path = Path(path).resolve()
    document, digest = read_json(path)
    object_fields(document, {"schema", "id", "render", "budget", "repetitions", "cases"}, "suite")
    if document.get("schema") != SCHEMA:
        raise ValueError(f"Expected suite schema {SCHEMA}")
    suite_id = identifier(document.get("id"), "suite.id")
    repeats = number(repetitions if repetitions is not None else document.get("repetitions", 1),
                     1, 16, "repetitions", True)
    defaults = render_settings(document.get("render", {}))
    budget = {**BUDGET_DEFAULTS, **object_fields(
        document.get("budget", {}), BUDGET_DEFAULTS.keys(), "budget")}
    number(budget["maxRenders"], 1, 1024, "budget.maxRenders", True)
    number(budget["maxAudioSeconds"], 0.001, 3600, "budget.maxAudioSeconds")
    number(budget["maxWallSeconds"], 0.001, 3600, "budget.maxWallSeconds")
    cases = document.get("cases")
    if not isinstance(cases, list) or not 1 <= len(cases) <= 128:
        raise ValueError("suite.cases must contain 1 to 128 cases")
    cases = [normalize_case(item, defaults, path.parent) for item in cases]
    if len({item["id"] for item in cases}) != len(cases):
        raise ValueError("case IDs must be unique")
    render_count = len(cases) * repeats
    audio_seconds = sum(item["performance"]["durationMs"] for item in cases) * repeats / 1000
    if render_count > budget["maxRenders"] or audio_seconds > budget["maxAudioSeconds"]:
        raise ValueError(f"Suite exceeds render budget: {render_count} renders, {audio_seconds:g} audio seconds")
    return {"schema": SCHEMA, "id": suite_id,
            "source": {"path": str(path), "sha256": digest, "document": document},
            "budget": budget, "repetitions": repeats, "cases": cases,
            "renderCount": render_count, "audioSeconds": audio_seconds}
