#!/usr/bin/env python3

import argparse
import json
import re
from collections import Counter
from pathlib import Path


MAXIMUM_LINE_LENGTH = 140
SPECTRAL_START_OFFSET = 280.0
SPECTRAL_LAYER_PREFIXES = ("magnitudeLayer", "phaseLayer")
SILENT_GRAPH_NODE_KINDS = {
    "voiceContext",
    "modulationTriple",
    "trilinearMesh",
    "envelope",
    "multiply",
    "output",
}


def scalar_json(value):
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    if value is True:
        return "true"
    if value is False:
        return "false"
    if value is None:
        return "null"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return re.sub(r"e([+-])0+(\d+)", r"e\1\2", repr(value))
    raise TypeError(f"Unsupported JSON scalar: {type(value)}")


def canonical_json(value, depth=0):
    if isinstance(value, dict):
        scalar = all(not isinstance(item, (dict, list)) for item in value.values())
        compact = ""
        if scalar:
            compact = "{}" if not value else "{ " + ", ".join(
                f"{scalar_json(key)}: {scalar_json(item)}"
                for key, item in value.items()
            ) + " }"
        edge = "sourceNodeId" in value and "destNodeId" in value
        if scalar and (edge or depth * 4 + len(compact) <= MAXIMUM_LINE_LENGTH):
            return compact
        lines = [
            "    " * (depth + 1)
            + scalar_json(key)
            + ": "
            + canonical_json(item, depth + 1)
            for key, item in value.items()
        ]
        return "{\n" + ",\n".join(lines) + "\n" + "    " * depth + "}"
    if isinstance(value, list):
        scalar = all(not isinstance(item, (dict, list)) for item in value)
        compact = ""
        if scalar:
            compact = "[]" if not value else "[ " + ", ".join(
                scalar_json(item) for item in value
            ) + " ]"
        if scalar and depth * 4 + len(compact) <= MAXIMUM_LINE_LENGTH:
            return compact
        lines = [
            "    " * (depth + 1) + canonical_json(item, depth + 1)
            for item in value
        ]
        return "[\n" + ",\n".join(lines) + "\n" + "    " * depth + "]"
    return scalar_json(value)


def mesh_is_empty(node):
    mesh = node.get("model", {}).get("mesh", {})
    return not mesh.get("vertices")


def node_references(document, node_ids):
    probes = [
        probe for probe in document.get("probes", [])
        if probe.get("sourceNodeId") in node_ids
        or probe.get("anchorDestNodeId") in node_ids
    ]
    assignments = [
        assignment for assignment in document.get("guideAssignments", [])
        if assignment.get("targetNodeId") in node_ids
    ]
    bindings = [
        binding for binding in document.get("audioResourceBindings", [])
        if binding.get("nodeId") in node_ids
    ]
    return probes, assignments, bindings


def remove_nodes(document, node_ids):
    document["nodes"] = [
        node for node in document.get("nodes", []) if node["id"] not in node_ids
    ]
    document["edges"] = [
        edge for edge in document.get("edges", [])
        if edge["sourceNodeId"] not in node_ids
        and edge["destNodeId"] not in node_ids
    ]
    document["guideAssignments"] = [
        assignment for assignment in document.get("guideAssignments", [])
        if assignment.get("targetNodeId") not in node_ids
    ]
    document["audioResourceBindings"] = [
        binding for binding in document.get("audioResourceBindings", [])
        if binding.get("nodeId") not in node_ids
    ]
    if not document.get("audioResourceBindings"):
        document.pop("audioResourceBindings", None)


def retarget_probe_sources(document, removed_node_id, source_edge):
    for probe in document.get("probes", []):
        if probe.get("sourceNodeId") != removed_node_id:
            continue
        probe["sourceNodeId"] = source_edge["sourceNodeId"]
        probe["sourcePortId"] = source_edge["sourcePortId"]


def bypass_neutral_pan(document, report):
    while True:
        nodes = {node["id"]: node for node in document.get("nodes", [])}
        candidate = next((
            node for node in nodes.values()
            if node.get("kind") == "spectralLayer"
            and abs(float(node.get("parameters", {}).get("pan", 0.5)) - 0.5) < 1.0e-6
        ), None)
        if candidate is None:
            return
        node_id = candidate["id"]
        incoming = [edge for edge in document["edges"] if edge["destNodeId"] == node_id]
        outgoing = [edge for edge in document["edges"] if edge["sourceNodeId"] == node_id]
        probes, assignments, bindings = node_references(document, {node_id})
        anchored = [probe for probe in probes if probe.get("anchorDestNodeId") == node_id]
        if len(incoming) != 1 or not outgoing or anchored or assignments or bindings:
            report["ambiguousNeutralPan"] += 1
            return
        source = incoming[0]
        source_node = nodes.get(source["sourceNodeId"])
        legacy_range = candidate.get("parameters", {}).get("range")
        if legacy_range is not None:
            if source_node is None or source_node.get("kind") != "trilinearMesh":
                report["ambiguousNeutralPan"] += 1
                return
            source_node.setdefault("parameters", {})["range"] = legacy_range
        spectral_mode = candidate.get("parameters", {}).get("mode")
        if spectral_mode is not None and source_node is not None:
            source_node.setdefault("parameters", {})["spectralMode"] = spectral_mode
        for edge in outgoing:
            edge["sourceNodeId"] = source["sourceNodeId"]
            edge["sourcePortId"] = source["sourcePortId"]
        retarget_probe_sources(document, node_id, source)
        remove_nodes(document, {node_id})
        report["neutralPan"] += 1


def bypass_empty_spectral_layers(document, report):
    while True:
        nodes = {node["id"]: node for node in document.get("nodes", [])}
        candidate = next((
            node for node in nodes.values()
            if node.get("kind") == "trilinearMesh"
            and node["id"].startswith(SPECTRAL_LAYER_PREFIXES)
            and mesh_is_empty(node)
        ), None)
        if candidate is None:
            return
        layer_id = candidate["id"]
        layer_out = [edge for edge in document["edges"] if edge["sourceNodeId"] == layer_id]
        chain_ids = {layer_id}
        if len(layer_out) != 1:
            report["ambiguousEmptySpectralLayer"] += 1
            return
        operation_edge = layer_out[0]
        process = nodes.get(operation_edge["destNodeId"])
        if process is not None and process.get("kind") == "spectralLayer":
            chain_ids.add(process["id"])
            process_out = [
                edge for edge in document["edges"]
                if edge["sourceNodeId"] == process["id"]
            ]
            if len(process_out) != 1:
                report["ambiguousEmptySpectralLayer"] += 1
                return
            operation_edge = process_out[0]
        operation = nodes.get(operation_edge["destNodeId"])
        if operation is None or operation.get("kind") not in ("add", "multiply"):
            report["ambiguousEmptySpectralLayer"] += 1
            return
        chain_ids.add(operation["id"])
        operation_inputs = [
            edge for edge in document["edges"] if edge["destNodeId"] == operation["id"]
        ]
        base_inputs = [
            edge for edge in operation_inputs
            if edge["sourceNodeId"] not in chain_ids
        ]
        operation_outputs = [
            edge for edge in document["edges"] if edge["sourceNodeId"] == operation["id"]
        ]
        probes, assignments, bindings = node_references(document, chain_ids)
        if (len(operation_inputs) != 2 or len(base_inputs) != 1
                or not operation_outputs or probes or assignments or bindings):
            report["ambiguousEmptySpectralLayer"] += 1
            return
        source = base_inputs[0]
        for edge in operation_outputs:
            edge["sourceNodeId"] = source["sourceNodeId"]
            edge["sourcePortId"] = source["sourcePortId"]
        remove_nodes(document, chain_ids)
        report["emptySpectralLayer"] += 1


def bypass_redundant_transform_pair(document, report):
    nodes = {node["id"]: node for node in document.get("nodes", [])}
    fft_nodes = [node for node in nodes.values() if node.get("kind") == "fft"]
    for fft in fft_nodes:
        incoming = [edge for edge in document["edges"] if edge["destNodeId"] == fft["id"]]
        outgoing = [edge for edge in document["edges"] if edge["sourceNodeId"] == fft["id"]]
        if len(incoming) != 1 or len(outgoing) != 2:
            continue
        ifft_ids = {edge["destNodeId"] for edge in outgoing}
        if len(ifft_ids) != 1:
            continue
        ifft = nodes.get(next(iter(ifft_ids)))
        if ifft is None or ifft.get("kind") != "ifft":
            continue
        branch_ports = {
            (edge["sourcePortId"], edge["destPortId"]) for edge in outgoing
        }
        if branch_ports != {("mag", "mag"), ("phase", "phase")}:
            continue
        ifft_incoming = [
            edge for edge in document["edges"] if edge["destNodeId"] == ifft["id"]
        ]
        ifft_outgoing = [
            edge for edge in document["edges"] if edge["sourceNodeId"] == ifft["id"]
        ]
        pair_ids = {fft["id"], ifft["id"]}
        probes, assignments, bindings = node_references(document, pair_ids)
        if (len(ifft_incoming) != 2 or not ifft_outgoing
                or probes or assignments or bindings):
            report["ambiguousTransformPair"] += 1
            continue
        source = incoming[0]
        for edge in ifft_outgoing:
            edge["sourceNodeId"] = source["sourceNodeId"]
            edge["sourcePortId"] = source["sourcePortId"]
        remove_nodes(document, pair_ids)
        report["transformPair"] += 1


def direct_scratch_fanouts(document, nodes, trimesh_ids):
    fanouts = {}
    for edge_index, edge in enumerate(document.get("edges", [])):
        source = nodes.get(edge["sourceNodeId"])
        if (source is None
                or source.get("kind") != "envelope"
                or source.get("parameters", {}).get("purpose") != "scratch"
                or edge["destNodeId"] not in trimesh_ids
                or edge["destPortId"] != "scratch"):
            continue
        fanout = fanouts.setdefault(source["id"], {"targets": set(), "indices": []})
        fanout["targets"].add(edge["destNodeId"])
        fanout["indices"].append(edge_index)
    return fanouts


def collapse_complete_scratch_fanout(document, report):
    nodes = {node["id"]: node for node in document.get("nodes", [])}
    voice_contexts = [
        node for node in nodes.values() if node.get("kind") == "voiceContext"
    ]
    trimesh_ids = {
        node["id"] for node in nodes.values()
        if node.get("kind") == "trilinearMesh"
    }
    if len(voice_contexts) != 1 or len(trimesh_ids) < 2:
        return

    voice_context = voice_contexts[0]
    if any(
            edge["destNodeId"] == voice_context["id"]
            and edge["destPortId"] == "scratch"
            for edge in document.get("edges", [])):
        return

    candidates = [
        fanout for fanout in direct_scratch_fanouts(
            document, nodes, trimesh_ids).values()
        if fanout["targets"] == trimesh_ids and len(fanout["indices"]) >= 2
    ]
    if len(candidates) != 1:
        if candidates:
            report["ambiguousScratchDefault"] += 1
        return

    candidate = candidates[0]
    retained_index = candidate["indices"][0]
    removed_indices = set(candidate["indices"][1:])
    retained = document["edges"][retained_index]
    retained["destNodeId"] = voice_context["id"]
    retained["destPortId"] = "scratch"
    document["edges"] = [
        edge for index, edge in enumerate(document["edges"])
        if index not in removed_indices
    ]
    report["scratchDefault"] += 1
    report["scratchEdgesRemoved"] += len(removed_indices)


def remove_port_override(node, group, port_id):
    port_sides = node.get("portSides")
    if port_sides is None or group not in port_sides:
        return
    port_sides[group].pop(port_id, None)
    if not port_sides[group]:
        port_sides.pop(group)
    if not port_sides:
        node.pop("portSides")


def spectral_context_target(nodes, edges, source_edge):
    source = nodes.get(source_edge["sourceNodeId"])
    if source is None:
        return None
    if source.get("kind") == "trilinearMesh" and not mesh_is_empty(source):
        return source
    if source.get("kind") != "spectralLayer":
        return None
    incoming = [
        edge for edge in edges
        if edge["destNodeId"] == source["id"] and edge["destPortId"] == "in"
    ]
    if len(incoming) != 1:
        return None
    mesh = nodes.get(incoming[0]["sourceNodeId"])
    if (mesh is None or mesh.get("kind") != "trilinearMesh"
            or mesh_is_empty(mesh)):
        return None
    return mesh


def compact_promoted_spectral_graph(nodes, voice, context_targets):
    positioned_targets = [
        target for target in context_targets if "position" in target
    ]
    if not positioned_targets or "position" not in voice:
        return 0.0
    first_spectral_x = min(
        float(target["position"]["x"]) for target in positioned_targets
    )
    desired_x = float(voice["position"]["x"]) + SPECTRAL_START_OFFSET
    shift = first_spectral_x - desired_x
    if shift <= 0.0:
        return 0.0
    for node in nodes.values():
        position = node.get("position")
        if position is not None and float(position["x"]) >= first_spectral_x:
            position["x"] = float(position["x"]) - shift
    return shift


def promote_empty_time_seed_to_spectral_context(document, report):
    nodes = {node["id"]: node for node in document.get("nodes", [])}
    voice_contexts = [
        node for node in nodes.values() if node.get("kind") == "voiceContext"
    ]
    if len(voice_contexts) != 1:
        return
    voice = voice_contexts[0]
    if voice.get("parameters", {}).get("domain") != "waveform":
        return

    edges = document.get("edges", [])
    candidates = []
    for time_mesh in nodes.values():
        if (time_mesh.get("kind") != "trilinearMesh"
                or not time_mesh["id"].startswith("timeLayer")
                or not mesh_is_empty(time_mesh)):
            continue
        outgoing = [
            edge for edge in edges
            if edge["sourceNodeId"] == time_mesh["id"]
            and edge["sourcePortId"] == "out"
        ]
        if len(outgoing) != 1:
            continue
        fft = nodes.get(outgoing[0]["destNodeId"])
        if fft is not None and fft.get("kind") == "fft":
            candidates.append((time_mesh, fft))
    if len(candidates) != 1:
        return

    time_mesh, fft = candidates[0]
    fft_inputs = [edge for edge in edges if edge["destNodeId"] == fft["id"]]
    fft_outputs = [edge for edge in edges if edge["sourceNodeId"] == fft["id"]]
    if (len(fft_inputs) != 1 or fft_inputs[0]["sourceNodeId"] != time_mesh["id"]
            or any(edge["sourcePortId"] not in ("mag", "phase") for edge in fft_outputs)):
        return

    removed_ids = {time_mesh["id"], fft["id"]}
    bypasses = []
    context_targets = []
    for port_id, destination_port in (("mag", "mag"), ("phase", "phase")):
        branch = [edge for edge in fft_outputs if edge["sourcePortId"] == port_id]
        if len(branch) > 1:
            return
        if not branch:
            continue
        destination = nodes.get(branch[0]["destNodeId"])
        if (destination is not None and destination.get("kind") == "ifft"
                and branch[0]["destPortId"] == destination_port):
            continue
        if destination is None or destination.get("kind") != "add":
            return
        operation_inputs = [
            edge for edge in edges if edge["destNodeId"] == destination["id"]
        ]
        content_inputs = [
            edge for edge in operation_inputs if edge["sourceNodeId"] != fft["id"]
        ]
        operation_outputs = [
            edge for edge in edges if edge["sourceNodeId"] == destination["id"]
        ]
        if (len(operation_inputs) != 2 or len(content_inputs) != 1
                or not operation_outputs):
            return
        context_target = spectral_context_target(nodes, edges, content_inputs[0])
        if context_target is None:
            return
        removed_ids.add(destination["id"])
        bypasses.append((content_inputs[0], operation_outputs))
        context_targets.append(context_target)

    if not context_targets:
        return
    probes, assignments, bindings = node_references(document, removed_ids)
    if probes or assignments or bindings:
        report["ambiguousEmptyTimeSeed"] += 1
        return

    for source, outgoing in bypasses:
        for edge in outgoing:
            edge["sourceNodeId"] = source["sourceNodeId"]
            edge["sourcePortId"] = source["sourcePortId"]
    horizontal_shift = compact_promoted_spectral_graph(nodes, voice, context_targets)
    remove_nodes(document, removed_ids)
    voice.setdefault("parameters", {})["domain"] = "spectral"
    for target in context_targets:
        remove_port_override(target, "outputs", "out")
        if any(
                edge["sourceNodeId"] == voice["id"]
                and edge["sourcePortId"] == "context"
                and edge["destNodeId"] == target["id"]
                and edge["destPortId"] == "context"
                for edge in document["edges"]):
            continue
        document["edges"].append({
            "sourceNodeId": voice["id"],
            "sourcePortId": "context",
            "destNodeId": target["id"],
            "destPortId": "context",
            "connectionKind": "signal",
            "attachmentType": "none",
        })
    report["emptyTimeSeed"] += 1
    report["emptyTimeSeedNodes"] += len(removed_ids)
    if horizontal_shift > 0.0:
        report["spectralLayoutCompaction"] += 1


def collapse_silent_empty_time_graph(document, report):
    nodes = document.get("nodes", [])
    meshes = [node for node in nodes if node.get("kind") == "trilinearMesh"]
    if not meshes or any(not mesh_is_empty(node) for node in meshes):
        return
    if any(not node["id"].startswith("timeLayer") for node in meshes):
        return
    if any(node.get("kind") not in SILENT_GRAPH_NODE_KINDS for node in nodes):
        return
    if document.get("probes") or document.get("audioResources"):
        return
    outputs = [node for node in nodes if node.get("kind") == "output"]
    if len(outputs) != 1:
        return
    removed = {node["id"] for node in nodes if node.get("kind") != "output"}
    output = outputs[0]
    output["position"] = {"x": 100.0, "y": 100.0}
    remove_nodes(document, removed)
    document["edges"] = []
    document["probes"] = []
    report["silentGraph"] += 1
    report["silentGraphNodes"] += len(removed)


def prune_isolated_nodes(document, report):
    while True:
        incident = {
            endpoint
            for edge in document.get("edges", [])
            for endpoint in (edge["sourceNodeId"], edge["destNodeId"])
        }
        removable = {
            node["id"] for node in document.get("nodes", [])
            if node.get("kind") != "output" and node["id"] not in incident
        }
        removable = {
            node_id for node_id in removable
            if not any(node_references(document, {node_id}))
        }
        if not removable:
            return
        remove_nodes(document, removable)
        report["isolatedNode"] += len(removable)


def simplify_graph(document):
    report = Counter()
    collapse_silent_empty_time_graph(document, report)
    bypass_empty_spectral_layers(document, report)
    bypass_neutral_pan(document, report)
    bypass_redundant_transform_pair(document, report)
    promote_empty_time_seed_to_spectral_context(document, report)
    collapse_complete_scratch_fanout(document, report)
    prune_isolated_nodes(document, report)
    return report


def graph_paths(path):
    if path.is_dir():
        return sorted(path.glob("*.cyclegraph"))
    return [path]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    totals = Counter()
    changed_files = 0
    for requested in args.paths:
        for path in graph_paths(requested):
            original = path.read_text(encoding="utf-8")
            document = json.loads(original)
            report = simplify_graph(document)
            encoded = canonical_json(document) + "\n"
            if encoded == original:
                continue
            changed_files += 1
            totals.update(report)
            if args.write:
                path.write_text(encoded, encoding="utf-8")
            print(f"{path}: {dict(report)}")
    print(f"changedFiles={changed_files} totals={dict(totals)}")
    if changed_files and not args.write:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
