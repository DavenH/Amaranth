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


SPECTRAL_ROLES = {
    **ROLES,
    "reed": ("trilinearMesh", 286, 269, 340, 600),
    "fft": ("fft", 144, 142, 706, 600),
    "color": ("trilinearMesh", 286, 269, 862, 250),
    "colorMultiply": ("multiply", 150, 118, 930, 600),
    "ifft": ("ifft", 144, 142, 1160, 600),
    "body": ("impulseResponse", 256, 210, 1384, 600),
    "breathMultiply": ("multiply", 150, 118, 1720, 600),
    "volumeMultiply": ("multiply", 150, 118, 1950, 600),
    "voiceOutput": ("voiceOutput", 116, 116, 2180, 600),
    "instability": ("modulationSource", 250, 48, 0, 702),
    "pressure": ("modulationSource", 250, 48, 1545, 840),
    "volume": ("envelope", 295.2, 244, 1877.4, 830),
    "globalInput": ("globalInput", 116, 116, 1440, 40),
    "output": ("output", 190, 320, 1640, 40),
}

SPECTRAL_OUTPUT_SPY = (1920, 40, 300, 206)


EXPRESSIVE_ROLES = {
    **SPECTRAL_ROLES,
    "body": ("impulseResponse", 256, 210, 1614, 600),
    "breathMultiply": ("multiply", 150, 118, 1950, 600),
    "volumeMultiply": ("multiply", 150, 118, 2180, 600),
    "voiceOutput": ("voiceOutput", 116, 116, 2410, 600),
    "pressure": ("modulationSource", 250, 48, 1775, 840),
    "volume": ("envelope", 295.2, 244, 2107.4, 830),
    "key": ("modulationSource", 250, 48, 570, 352),
    "articulationControls": ("modulationTriple", 280, 126, -340, 264),
    "pitch": ("envelope", 295.2, 244, 0, 200),
    "air": ("trilinearMesh", 286, 269, 1316, 250),
    "airAdd": ("add", 150, 118, 1384, 600),
}


def layout_nodes(graph, ids, roles=ROLES):
    by_id = {node["id"]: node for node in graph["nodes"]}
    nodes = {role: by_id[ids.get(role, role)] for role in roles
        if ids.get(role, role) in by_id}
    missing = set(roles).difference(nodes, {"instability"})
    unknown = set(by_id).difference(node["id"] for node in nodes.values())
    if missing or unknown:
        raise ValueError("Incomplete reed layout: missing roles " + str(sorted(missing))
            + "; nodes without a position " + str(sorted(unknown)))
    for role, node in nodes.items():
        if node["kind"] != roles[role][0]:
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


def apply_spectral_reed_layout(graph, ids):
    """Keep the spectral branch local and its phase bypass below the audio row.

    Adjacent audio ports share y=658 with 80 world-unit joins. Color enters
    the magnitude multiplier from above; both phase ports share y=742 below
    that multiplier. Shared context enters both meshes from above. Global I/O
    and its reserved spy occupy the independent upper-right row.
    """
    nodes = layout_nodes(graph, ids, SPECTRAL_ROLES)
    for role, node in nodes.items():
        _, _, _, x, y = SPECTRAL_ROLES[role]
        node["position"] = {"x": x, "y": y}
        node.pop("portSides", None)
        node.pop("reverseInputPortOrder", None)
    nodes["voice"]["portSides"] = {"outputs": {"context": "bottom"}}
    for role in ("reed", "color"):
        nodes[role]["portSides"] = {"inputs": {"context": "top"}}
    nodes["color"]["portSides"]["outputs"] = {"out": "bottom"}
    nodes["fft"]["portSides"] = {"outputs": {"phase": "bottom"}}
    nodes["ifft"]["portSides"] = {"inputs": {"phase": "bottom"}}
    nodes["colorMultiply"]["portSides"] = {"inputs": {"right": "top"}}
    for role in ("breathMultiply", "volumeMultiply"):
        nodes[role]["portSides"] = {"inputs": {"right": "bottom"}}
    nodes["volume"]["portSides"] = {"outputs": {"env": "top"}}
    graph.pop("nodeGroups", None)
    presentation = graph.setdefault("presetPresentation", {"version": 1})
    presentation["outputSpyX"], presentation["outputSpyY"] = SPECTRAL_OUTPUT_SPY[:2]
    report = reed_layout_report(graph, ids, SPECTRAL_ROLES, SPECTRAL_OUTPUT_SPY)
    if report["overlaps"]:
        raise ValueError("Spectral reed preset cards overlap: " + str(report["overlaps"]))
    return report



def apply_expressive_reed_layout(graph, ids):
    """Keep the main audio row clear and controls beside their destinations.

    A dedicated CC3 source joins the reed locally; the vibrato triple and
    pitch envelope form an adjacent pair above it. Air sits above Add so its
    signal drops vertically and all mesh-context fan-outs stay above cards.
    Existing frozen layouts remain available for earlier studies.
    """
    nodes = layout_nodes(graph, ids, EXPRESSIVE_ROLES)
    for role, node in nodes.items():
        _, _, _, x, y = EXPRESSIVE_ROLES[role]
        node["position"] = {"x": x, "y": y}
        node.pop("portSides", None)
        node.pop("reverseInputPortOrder", None)
    nodes["voice"]["portSides"] = {"outputs": {"context": "bottom"}}
    for role in ("reed", "color", "air"):
        nodes[role]["portSides"] = {"inputs": {"context": "top"}}
    nodes["color"]["portSides"]["outputs"] = {"out": "bottom"}
    nodes["air"]["portSides"]["outputs"] = {"out": "bottom"}
    nodes["fft"]["portSides"] = {"outputs": {"phase": "bottom"}}
    nodes["ifft"]["portSides"] = {"inputs": {"phase": "bottom"}}
    for role in ("colorMultiply", "airAdd"):
        nodes[role]["portSides"] = {"inputs": {"right": "top"}}
    for role in ("breathMultiply", "volumeMultiply"):
        nodes[role]["portSides"] = {"inputs": {"right": "bottom"}}
    nodes["volume"]["portSides"] = {"outputs": {"env": "top"}}
    graph.pop("nodeGroups", None)
    presentation = graph.setdefault("presetPresentation", {"version": 1})
    presentation["outputSpyX"], presentation["outputSpyY"] = SPECTRAL_OUTPUT_SPY[:2]
    report = reed_layout_report(graph, ids, EXPRESSIVE_ROLES, SPECTRAL_OUTPUT_SPY)
    if report["overlaps"]:
        raise ValueError("Expressive reed preset cards overlap: " + str(report["overlaps"]))
    return report


def reed_layout_report(graph, ids, roles=ROLES, output_spy=OUTPUT_SPY):
    """Audit every actual node card, endpoint caption and reserved output spy."""
    rectangles = {"outputSpy": output_spy}
    for role, node in layout_nodes(graph, ids, roles).items():
        _, width, height, _, _ = roles[role]
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
