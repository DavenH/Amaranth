"""Compact presentation for the reed pulse -> voice body IR saxophone graph."""

from itertools import combinations


# Canonical natural world sizes: NodeDefinition.cpp / naturalSizeForNode.
ROLES = {
    "controls": ("modulationTriple", 280, 126, 0, 0),
    "voice": ("voiceContext", 280, 182, 340, 0),
    "reed": ("trilinearMesh", 286, 269, 340, 300),
    "body": ("impulseResponse", 256, 210, 720, 300),
    "breathMultiply": ("multiply", 150, 118, 1080, 300),
    "volumeMultiply": ("multiply", 150, 118, 1340, 300),
    "voiceOutput": ("voiceOutput", 116, 116, 1600, 300),
    "instability": ("modulationSource", 250, 48, 0, 402),
    "pressure": ("modulationSource", 250, 48, 905, 540),
    "volume": ("envelope", 295.2, 244, 1267.4, 640),
    "globalInput": ("globalInput", 116, 116, 84, 630),
    "output": ("output", 190, 320, 340, 630),
}

OUTPUT_SPY = (0, 760, 300, 206)


def layout_nodes(graph, ids):
    by_id = {node["id"]: node for node in graph["nodes"]}
    nodes = {role: by_id[ids.get(role, role)] for role in ROLES
        if ids.get(role, role) in by_id}
    missing = set(ROLES).difference(nodes, {"instability"})
    unknown = set(by_id).difference(node["id"] for node in nodes.values())
    if missing or unknown:
        raise ValueError("Incomplete reed layout: missing roles " + str(sorted(missing))
            + "; nodes without a position " + str(sorted(unknown)))
    for role, node in nodes.items():
        if node["kind"] != ROLES[role][0]:
            raise ValueError("Unexpected node kind for reed layout role " + role)
    return nodes


def apply_reed_layout(graph, ids):
    """Author only positions, port sides and the output spy on a complete graph.

    The main audio joins share y+58; the CC3 source meets reed.red at y+126.
    Context and volume enter vertically, keeping the signal row unobstructed.
    """
    nodes = layout_nodes(graph, ids)
    for role, node in nodes.items():
        _, _, _, x, y = ROLES[role]
        node["position"] = {"x": x, "y": y}
        node.pop("portSides", None)
        node.pop("reverseInputPortOrder", None)
    nodes["voice"]["portSides"] = {"outputs": {"context": "bottom"}}
    nodes["reed"]["portSides"] = {"inputs": {"context": "top"}}
    nodes["volumeMultiply"]["portSides"] = {"inputs": {"right": "bottom"}}
    nodes["breathMultiply"]["portSides"] = {"inputs": {"right": "bottom"}}
    nodes["volume"]["portSides"] = {"outputs": {"env": "top"}}
    graph.pop("nodeGroups", None)
    presentation = graph.setdefault("presetPresentation", {"version": 1})
    presentation["outputSpyX"], presentation["outputSpyY"] = OUTPUT_SPY[:2]
    report = reed_layout_report(graph, ids)
    if report["overlaps"]:
        raise ValueError("Reed preset cards overlap: " + str(report["overlaps"]))
    return report


def reed_layout_report(graph, ids):
    """Audit every actual node card, endpoint caption and reserved output spy."""
    rectangles = {"outputSpy": OUTPUT_SPY}
    for role, node in layout_nodes(graph, ids).items():
        _, width, height, _, _ = ROLES[role]
        x, y = (node["position"][axis] for axis in ("x", "y"))
        if role == "voiceOutput":
            width += 84
        elif role == "globalInput":
            x -= 84
            width += 84
        rectangles[role] = (x, y, width, height)
    overlaps = []
    for (first, a), (second, b) in combinations(rectangles.items(), 2):
        if a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]:
            overlaps.append([first, second])
    left = min(rect[0] for rect in rectangles.values())
    top = min(rect[1] for rect in rectangles.values())
    right = max(rect[0] + rect[2] for rect in rectangles.values())
    bottom = max(rect[1] + rect[3] for rect in rectangles.values())
    return {"cardBounds": rectangles, "overlaps": overlaps,
        "contentBounds": (left, top, right - left, bottom - top)}
