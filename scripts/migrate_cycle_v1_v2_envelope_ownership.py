#!/usr/bin/env python3
"""Migrate Cycle 1 Envelope ownership into existing Cycle V2 factory graphs.

The converter remains the authority for newly required nodes and edges. Existing
Cycle V2 presentation and non-Envelope topology are retained unchanged.
"""

import argparse
import copy
import json
from pathlib import Path

import audit_cycle_v1_v2_envelope_ownership as audit
import port_cycle_v1_preset
from simplify_cycle_v2_presets import canonical_json


def node_with_id(graph, node_id):
    return next((node for node in graph.get("nodes", [])
                 if node.get("id") == node_id), None)


def purpose_nodes(graph, purpose):
    return [
        node
        for node in graph.get("nodes", [])
        if node.get("kind") == "envelope"
        and node.get("parameters", {}).get("purpose", "control") == purpose
    ]


def remove_obsolete_morph_sources(graph):
    node_ids = {
        node["id"]
        for node in graph.get("nodes", [])
        if node.get("id") in ("staticEnvelopeMorph", "legacyEnvelopeMorph")
    }
    if not node_ids:
        return False
    graph["nodes"] = [
        node for node in graph.get("nodes", [])
        if node.get("id") not in node_ids
    ]
    graph["edges"] = [
        edge for edge in graph.get("edges", [])
        if edge.get("sourceNodeId") not in node_ids
        and edge.get("destNodeId") not in node_ids
    ]
    return True


def signal_edge(source, source_port, destination, destination_port):
    return {
        "sourceNodeId": source,
        "sourcePortId": source_port,
        "destNodeId": destination,
        "destPortId": destination_port,
        "connectionKind": "signal",
        "attachmentType": "none",
    }


def append_missing_node(graph, converted, node_id):
    if node_with_id(graph, node_id) is not None:
        return False
    converted_node = node_with_id(converted, node_id)
    if converted_node is None:
        raise ValueError(f"Converted graph is missing required node: {node_id}")
    graph.setdefault("nodes", []).append(copy.deepcopy(converted_node))
    return True


def append_missing_edge(graph, converted_edge):
    identity = (
        converted_edge["sourceNodeId"],
        converted_edge["sourcePortId"],
        converted_edge["destNodeId"],
        converted_edge["destPortId"],
    )
    for existing in graph.get("edges", []):
        existing_identity = (
            existing.get("sourceNodeId"),
            existing.get("sourcePortId"),
            existing.get("destNodeId"),
            existing.get("destPortId"),
        )
        if existing_identity == identity:
            return False
    graph.setdefault("edges", []).append(copy.deepcopy(converted_edge))
    return True


def install_declick_fallback(graph, converted):
    converted_volume_nodes = purpose_nodes(converted, "volume")
    fallback = next((
        node for node in converted_volume_nodes
        if node.get("parameters", {}).get("declick", False)
    ), None)
    if fallback is None:
        raise ValueError("Converted graph is missing its declick fallback Envelope")

    changed = append_missing_node(graph, converted, fallback["id"])
    changed = append_missing_node(graph, converted, "volumeMultiply") or changed

    desired_edges = [
        edge
        for edge in converted.get("edges", [])
        if edge.get("destNodeId") == "volumeMultiply"
        or edge.get("sourceNodeId") == "volumeMultiply"
    ]
    if len(desired_edges) != 3:
        raise ValueError("Converted graph has an ambiguous declick fallback route")

    terminal_edges = [
        edge
        for edge in graph.get("edges", [])
        if edge.get("destNodeId") == "voiceOutput"
        and edge.get("sourceNodeId") != "volumeMultiply"
    ]
    desired_source_edges = [
        edge for edge in desired_edges
        if edge.get("destNodeId") == "volumeMultiply"
        and edge.get("destPortId") == "left"
    ]
    if len(desired_source_edges) != 1:
        raise ValueError("Converted graph has no unique declick signal source")
    desired_source = desired_source_edges[0]
    for terminal in terminal_edges:
        if (terminal.get("sourceNodeId"), terminal.get("sourcePortId")) != (
                desired_source.get("sourceNodeId"),
                desired_source.get("sourcePortId")):
            raise ValueError("Existing voice terminal differs from the converter")
    if terminal_edges:
        graph["edges"] = [
            edge for edge in graph.get("edges", [])
            if edge not in terminal_edges
        ]
        changed = True

    for desired_edge in desired_edges:
        changed = append_missing_edge(graph, desired_edge) or changed
    return changed


def migrate_pair(preset, graph, converted):
    if not audit.has_voice_context(graph):
        return False

    changed = remove_obsolete_morph_sources(graph)
    morph_position = audit.authored_morph_position(preset)
    active_layers = {}
    for purpose in audit.PURPOSES:
        active_layer, active_count = audit.expected_active_layer(preset, purpose)
        if active_count > 1:
            raise ValueError(
                f"Cycle 1 has {active_count} active {purpose} Envelopes")
        active_layers[purpose] = active_layer
        nodes = purpose_nodes(graph, purpose)

        if active_layer is None:
            for node in nodes:
                parameters = node.setdefault("parameters", {})
                if audit.enabled(parameters):
                    parameters["enabled"] = False
                    graph["edges"] = [
                        edge for edge in graph.get("edges", [])
                        if edge.get("sourceNodeId") != node["id"]
                    ]
                    changed = True
            continue

        active_nodes = [
            node for node in nodes if audit.enabled(node.get("parameters", {}))
        ]
        if len(active_nodes) != 1:
            raise ValueError(
                f"Cannot identify the active {purpose} Envelope in Cycle V2")
        parameters = active_nodes[0].setdefault("parameters", {})
        expected_declick = (
            bool(preset.get("settings", {}).get("Declick", True))
            if purpose == "volume"
            else False
        )
        expected_values = {"declick": expected_declick}
        authored_static_morph = (
            morph_position is not None
            and not active_layer["properties"].get("dynamic", False)
        )
        if authored_static_morph:
            expected_values.update({
                "red": morph_position["red"],
                "blue": morph_position["blue"],
            })
        for parameter, value in expected_values.items():
            if parameters.get(parameter) != value:
                parameters[parameter] = value
                changed = True
        model_state = active_nodes[0].get("model", {}).get("state")
        if model_state is not None and authored_static_morph:
            for parameter in ("red", "blue"):
                value = morph_position[parameter]
                if model_state.get(parameter) != value:
                    model_state[parameter] = value
                    changed = True

    declick = bool(preset.get("settings", {}).get("Declick", True))
    if active_layers["volume"] is None and declick:
        changed = install_declick_fallback(graph, converted) or changed
    return changed


def matching_files(directory, suffix):
    return {
        path.stem.lower(): path
        for path in directory.glob(f"*{suffix}")
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cycle1_exports", type=Path)
    parser.add_argument("cycle2_presets", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    cycle1 = matching_files(args.cycle1_exports, ".json")
    cycle2 = matching_files(args.cycle2_presets, ".cyclegraph")
    migrated = []
    skipped = []
    for name in sorted(cycle1.keys() & cycle2.keys()):
        source = json.loads(cycle1[name].read_text(encoding="utf-8"))
        preset = source.get("preset", source)
        graph = json.loads(cycle2[name].read_text(encoding="utf-8"))
        if not audit.has_voice_context(graph):
            skipped.append(name)
            continue
        active_volume, _ = audit.expected_active_layer(preset, "volume")
        needs_fallback = (
            active_volume is None
            and bool(preset.get("settings", {}).get("Declick", True))
        )
        converted = (
            port_cycle_v1_preset.convert({"preset": preset})
            if needs_fallback
            else {"nodes": [], "edges": []}
        )
        if not migrate_pair(preset, graph, converted):
            continue
        cycle2[name].write_text(canonical_json(graph) + "\n", encoding="utf-8")
        migrated.append(name)

    report = {
        "pairedPresetCount": len(cycle1.keys() & cycle2.keys()),
        "migratedPresetCount": len(migrated),
        "migratedPresets": migrated,
        "skippedPresetCount": len(skipped),
        "skippedPresets": skipped,
    }
    rendered = json.dumps(report, indent=4) + "\n"
    if args.report is not None:
        args.report.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
