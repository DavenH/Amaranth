#!/usr/bin/env python3
"""Audit Cycle 1 Envelope ownership against Cycle V2 factory graphs."""

import argparse
import json
from collections import defaultdict
from pathlib import Path

import port_cycle_v1_preset


PURPOSES = ("volume", "pitch", "scratch")
LEGACY_MORPH_NODE_ID = "legacyEnvelopeMorph"


def source_preset(path):
    document = json.loads(path.read_text(encoding="utf-8"))
    return document.get("preset", document)


def enabled(parameter):
    return bool(parameter.get("enabled", True))


def envelope_nodes_by_purpose(graph):
    result = defaultdict(list)
    for node in graph.get("nodes", []):
        if node.get("kind") != "envelope":
            continue
        result[node.get("parameters", {}).get("purpose", "control")].append(node)
    return result


def has_edge(edges, source_id, destination_port, destination_prefix=None):
    return any(
        edge.get("sourceNodeId") == source_id
        and edge.get("destPortId") == destination_port
        and (
            destination_prefix is None
            or edge.get("destNodeId", "").startswith(destination_prefix)
        )
        for edge in edges
    )


def has_voice_context(graph):
    return any(node.get("kind") == "voiceContext"
               for node in graph.get("nodes", []))


def expected_active_layer(preset, purpose):
    active = [
        layer
        for layer in port_cycle_v1_preset.envelope_layers(preset, purpose)
        if layer["properties"]["active"]
    ]
    return active[0] if len(active) == 1 else None, len(active)


def authored_morph_position(preset):
    return (preset.get("morphPanel") or {}).get("position")


def audit_pair(name, preset, graph):
    findings = []
    if not has_voice_context(graph):
        return {"preset": name, "applicable": False, "findings": findings}

    nodes_by_purpose = envelope_nodes_by_purpose(graph)
    edges = graph.get("edges", [])
    active_layers = {}
    if any(node.get("id") == LEGACY_MORPH_NODE_ID
           for node in graph.get("nodes", [])):
        findings.append({"kind": "legacyMorphNode"})
    legacy_edges = [
        edge for edge in edges
        if edge.get("sourceNodeId") == LEGACY_MORPH_NODE_ID
        or edge.get("destNodeId") == LEGACY_MORPH_NODE_ID
    ]
    if legacy_edges:
        findings.append({
            "kind": "legacyMorphRoute",
            "edgeCount": len(legacy_edges),
        })

    for purpose in PURPOSES:
        active_layer, active_count = expected_active_layer(preset, purpose)
        active_layers[purpose] = active_layer
        if active_count > 1:
            findings.append({
                "kind": "sourceGap",
                "purpose": purpose,
                "detail": f"Cycle 1 has {active_count} active layers",
            })
            continue

        active_nodes = [node for node in nodes_by_purpose[purpose]
                        if enabled(node.get("parameters", {}))]
        expected_count = 1 if active_layer is not None else 0
        if len(active_nodes) != expected_count:
            findings.append({
                "kind": "activePurpose",
                "purpose": purpose,
                "expectedCount": expected_count,
                "actualNodeIds": [node["id"] for node in active_nodes],
            })
            continue
        if active_layer is None:
            continue

        node = active_nodes[0]
        node_id = node["id"]
        parameters = node.get("parameters", {})
        expected_declick = (
            bool(preset.get("settings", {}).get("Declick", True))
            if purpose == "volume"
            else False
        )
        if bool(parameters.get("declick", False)) != expected_declick:
            findings.append({
                "kind": "declick",
                "purpose": purpose,
                "nodeId": node_id,
                "expected": expected_declick,
                "actual": bool(parameters.get("declick", False)),
            })

        if purpose == "volume":
            routed = has_edge(edges, node_id, "right", "volumeMultiply")
        elif purpose == "pitch":
            routed = has_edge(edges, node_id, "pitch")
        else:
            routed = has_edge(edges, node_id, "scratch")
        if not routed:
            findings.append({
                "kind": "ownerRoute",
                "purpose": purpose,
                "nodeId": node_id,
            })

    declick_enabled = bool(preset.get("settings", {}).get("Declick", True))
    if active_layers.get("volume") is None and declick_enabled:
        fallback_ids = {
            node["id"]
            for node in nodes_by_purpose["volume"]
            if bool(node.get("parameters", {}).get("declick", False))
        }
        routed = any(
            has_edge(edges, node_id, "right", "volumeMultiply")
            for node_id in fallback_ids
        )
        if not fallback_ids or not routed:
            findings.append({
                "kind": "declickFallback",
                "purpose": "volume",
                "actualNodeIds": sorted(fallback_ids),
                "routed": routed,
            })

    return {"preset": name, "applicable": True, "findings": findings}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cycle1_exports", type=Path)
    parser.add_argument("cycle2_presets", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    cycle1 = {
        path.stem.lower(): path
        for path in args.cycle1_exports.glob("*.json")
    }
    cycle2 = {
        path.stem.lower(): path
        for path in args.cycle2_presets.glob("*.cyclegraph")
    }
    paired_names = cycle1.keys() & cycle2.keys()
    pairs = []
    counts = defaultdict(int)
    skipped = []
    for name in sorted(paired_names):
        pair = audit_pair(
            name,
            source_preset(cycle1[name]),
            json.loads(cycle2[name].read_text(encoding="utf-8")),
        )
        if not pair["applicable"]:
            skipped.append(name)
            continue
        if not pair["findings"]:
            continue
        pairs.append(pair)
        for finding in pair["findings"]:
            counts[finding["kind"]] += 1

    report = {
        "pairedPresetCount": len(paired_names),
        "eligiblePresetCount": len(paired_names) - len(skipped),
        "skippedPresetCount": len(skipped),
        "skippedPresets": skipped,
        "mismatchedPresetCount": len(pairs),
        "findingCounts": dict(sorted(counts.items())),
        "presets": pairs,
    }
    rendered = json.dumps(report, indent=4) + "\n"
    if args.report is not None:
        args.report.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
