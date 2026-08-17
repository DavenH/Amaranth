#!/usr/bin/env python3

"""Convert checked-in Cycle V2 Guide nodes to document Guide resources.

This is a one-shot repository migration, not a runtime compatibility layer.
It preserves each Guide's curve model and parameters, and translates every
synthetic Guide attachment edge into a typed cube-component assignment.
"""

import argparse
import json
from pathlib import Path


def guide_resource(node, order):
    parameters = node.get("parameters", {})
    return {
        "id": node["id"],
        "shortLabel": f"G{order + 1}",
        "name": "",
        "colourIndex": order,
        "shelfOrder": order,
        "enabled": parameters.get("enabled", True),
        "noise": parameters.get("noise", 0.5),
        "dcOffset": parameters.get("dcOffset", 0.5),
        "phase": parameters.get("phase", 0.5),
        "model": node["model"],
    }


def assignment(edge):
    prefix = "guide.cube."
    destination = edge["destPortId"]
    if not destination.startswith(prefix):
        raise ValueError(f"Invalid Guide target port: {destination}")
    cube_text, field = destination[len(prefix):].split(".", 1)
    if field not in {"time", "red", "blue", "phase", "amp", "curve"}:
        raise ValueError(f"Invalid Guide target field: {field}")
    return {
        "guideId": edge["sourceNodeId"],
        "targetNodeId": edge["destNodeId"],
        "target": {
            "kind": "trimeshCubeComponent",
            "cubeIndex": int(cube_text),
            "field": field,
        },
    }


def convert(document):
    nodes = document["nodes"]
    guide_nodes = [node for node in nodes if node.get("kind") == "guideCurve"]
    guide_ids = {node["id"] for node in guide_nodes}

    guide_edges = [
        edge for edge in document["edges"]
        if edge.get("attachmentType") == "guideCurve"
    ]
    if any(edge.get("sourceNodeId") not in guide_ids for edge in guide_edges):
        raise ValueError("Guide attachment edge does not originate at a Guide node")

    document["nodes"] = [node for node in nodes if node.get("id") not in guide_ids]
    document["edges"] = [edge for edge in document["edges"] if edge not in guide_edges]
    document["guides"] = [
        guide_resource(node, order) for order, node in enumerate(guide_nodes)
    ]
    document["guideAssignments"] = [assignment(edge) for edge in guide_edges]
    document["formatVersion"] = 3
    return document


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+", type=Path)
    args = parser.parse_args()

    for path in args.paths:
        with path.open(encoding="utf-8") as source:
            document = json.load(source)
        with path.open("w", encoding="utf-8") as destination:
            json.dump(convert(document), destination, indent=4)
            destination.write("\n")


if __name__ == "__main__":
    main()
