#!/usr/bin/env python3
"""Audit or migrate Cycle 1 oscillator controls into Cycle V2 Voice Contexts."""

import argparse
import json
from pathlib import Path


def source_preset(path):
    document = json.loads(path.read_text(encoding="utf-8"))
    return document.get("preset", document)


def voice_node(graph):
    voices = [node for node in graph.get("nodes", []) if node.get("kind") == "voiceContext"]
    return voices[0] if len(voices) == 1 else None


def translated_octave(unit_value):
    return round(4.0 * unit_value - 2.0)


def expected_controls(preset):
    knobs = preset.get("oscControls", {}).get("knobs", [])
    if len(knobs) < 3:
        return None
    oversampling = max(1, int(preset.get("settings", {}).get("OversampleFactorRltm", 1)))
    return {
        "octave": translated_octave(knobs[1]),
        "voiceLength": knobs[2],
        "pitch": 0.0,
        "portamento": False,
        "oversampling": f"{oversampling}x",
    }


def replace_voice_parameters(text, node_id, parameters):
    id_marker = f'"id": {json.dumps(node_id)}'
    node_start = text.find(id_marker)
    if node_start < 0:
        raise ValueError(f"cannot locate Voice Context {node_id}")
    parameter_marker = '"parameters": {'
    parameter_start = text.find(parameter_marker, node_start)
    if parameter_start < 0:
        raise ValueError(f"cannot locate parameters for Voice Context {node_id}")
    object_start = text.find("{", parameter_start)
    depth = 0
    object_end = -1
    for index in range(object_start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                object_end = index + 1
                break
    if object_end < 0:
        raise ValueError(f"unterminated parameters for Voice Context {node_id}")

    existing = text[object_start:object_end]
    ordered = {}
    for key, value in json.loads(existing).items():
        ordered[key] = value
        if key == "octave":
            ordered["voiceLength"] = parameters["voiceLength"]
    if "voiceLength" not in ordered:
        ordered["voiceLength"] = parameters["voiceLength"]
    for key, value in parameters.items():
        ordered[key] = value

    compact = "{ " + json.dumps(ordered, separators=(", ", ": "))[1:-1] + " }"
    if "\n" not in existing and 12 + len(compact) <= 140:
        replacement = compact
    else:
        lines = json.dumps(ordered, indent=4).splitlines()
        replacement = "\n".join(
            lines[:1] + ["            " + line for line in lines[1:]])
    return text[:object_start] + replacement + text[object_end:]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cycle1_exports", type=Path)
    parser.add_argument("cycle2_presets", type=Path)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--default-missing", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    cycle1 = {path.stem.lower(): path for path in args.cycle1_exports.glob("*.json")}
    cycle2 = {path.stem.lower(): path for path in args.cycle2_presets.glob("*.cyclegraph")}
    report = {
        "pairedPresetCount": len(cycle1.keys() & cycle2.keys()),
        "eligiblePresetCount": 0,
        "migratedPresetCount": 0,
        "defaultedGraphCount": 0,
        "mismatches": [],
        "gaps": [],
    }
    for name in sorted(cycle1.keys() & cycle2.keys()):
        expected = expected_controls(source_preset(cycle1[name]))
        graph = json.loads(cycle2[name].read_text(encoding="utf-8"))
        voice = voice_node(graph)
        if expected is None:
            report["gaps"].append({"preset": name, "reason": "missing Cycle 1 oscillator controls"})
            continue
        if voice is None:
            report["gaps"].append({"preset": name, "reason": "Cycle V2 does not have exactly one Voice Context"})
            continue
        report["eligiblePresetCount"] += 1
        actual = voice.get("parameters", {})
        differences = {
            key: {"cycle1": value, "cycle2": actual.get(key)}
            for key, value in expected.items()
            if actual.get(key) != value
        }
        if differences:
            report["mismatches"].append({"preset": name, "controls": differences})
        if args.apply:
            text = cycle2[name].read_text(encoding="utf-8")
            migrated = replace_voice_parameters(text, voice["id"], expected)
            if migrated != text:
                cycle2[name].write_text(migrated, encoding="utf-8")
                report["migratedPresetCount"] += 1

    if args.apply and args.default_missing:
        for path in sorted(args.cycle2_presets.glob("*.cyclegraph")):
            graph = json.loads(path.read_text(encoding="utf-8"))
            voice = voice_node(graph)
            if voice is None or "voiceLength" in voice.get("parameters", {}):
                continue
            parameters = dict(voice["parameters"])
            parameters["voiceLength"] = 0.375
            text = path.read_text(encoding="utf-8")
            migrated = replace_voice_parameters(text, voice["id"], parameters)
            if migrated != text:
                path.write_text(migrated, encoding="utf-8")
                report["defaultedGraphCount"] += 1

    rendered = json.dumps(report, indent=4) + "\n"
    if args.report is not None:
        args.report.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
