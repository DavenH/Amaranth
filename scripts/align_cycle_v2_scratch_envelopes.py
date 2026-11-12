#!/usr/bin/env python3
"""Align single-consumer scratch envelopes with their Trimesh, preserving other data."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re

from simplify_cycle_v2_presets import graph_paths, scalar_json


def align_scratch_envelopes(document):
    nodes = {node["id"]: node for node in document.get("nodes", [])}
    outgoing = defaultdict(list)
    for edge in document.get("edges", []):
        outgoing[edge["sourceNodeId"]].append(edge)
    moved = []
    for node in nodes.values():
        if node.get("kind") != "envelope":
            continue
        edges = outgoing[node["id"]]
        consumers = {edge["destNodeId"] for edge in edges}
        if len(consumers) != 1:
            continue
        consumer = nodes.get(next(iter(consumers)))
        if consumer is None or consumer.get("kind") != "trilinearMesh":
            continue
        if not any(edge.get("destPortId") == "scratch"
                   and edge.get("attachmentType") == "scratchEnvelope" for edge in edges):
            continue
        target_y = consumer["position"]["y"]
        if node["position"]["y"] != target_y:
            node["position"]["y"] = target_y
            moved.append(node["id"])
    return moved


def align_preset_text(text):
    document = json.loads(text)
    moved = align_scratch_envelopes(document)
    nodes = {node["id"]: node for node in document.get("nodes", [])}
    number = r"-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?"
    for identifier in moved:
        pattern = (r'("id"\s*:\s*' + re.escape(json.dumps(identifier))
                   + r'\s*,\s*"kind"\s*:\s*"envelope"[^{}]*"position"\s*:\s*\{[^{}]*"y"\s*:\s*)'
                   + number)
        value = scalar_json(nodes[identifier]["position"]["y"])
        text, count = re.subn(pattern, lambda match: match[1] + value, text, count=1)
        if count != 1:
            raise ValueError(f"Cannot locate the serialized position of {identifier}")
    return text, moved


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    changed_files = 0
    moved_nodes = 0
    for requested in args.paths:
        for path in graph_paths(requested):
            encoded, moved = align_preset_text(path.read_text(encoding="utf-8"))
            if not moved:
                continue
            changed_files += 1
            moved_nodes += len(moved)
            if args.write:
                path.write_text(encoded, encoding="utf-8")
            print(f"{path}: {', '.join(moved)}")
    print(f"changedFiles={changed_files} movedNodes={moved_nodes}")


if __name__ == "__main__":
    main()
