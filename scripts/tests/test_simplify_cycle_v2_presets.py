import copy
import json
import sys
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

import simplify_cycle_v2_presets as simplify


def node(node_id, kind, vertices=None):
    result = {"id": node_id, "kind": kind, "position": {"x": 0.0, "y": 0.0}}
    if vertices is not None:
        result["model"] = {
            "mesh": {
                "vertices": list(vertices),
                "cubes": [1] if vertices else [],
            }
        }
    return result


def edge(source, source_port, destination, destination_port):
    return {
        "sourceNodeId": source,
        "sourcePortId": source_port,
        "destNodeId": destination,
        "destPortId": destination_port,
        "connectionKind": "signal",
        "attachmentType": "none",
    }


def graph(nodes, edges):
    return {
        "nodes": nodes,
        "edges": edges,
        "guides": [],
        "guideHeatmaps": [],
        "guideAssignments": [],
        "probes": [],
    }


class SimplifyCycleV2PresetsTest(unittest.TestCase):
    def test_neutral_pan_is_bypassed_and_probe_source_follows(self):
        document = graph([
            {
                **node("mesh", "trilinearMesh", [1]),
                "parameters": {"range": 0.5},
            },
            {
                "id": "pan",
                "kind": "spectralLayer",
                "parameters": {"pan": 0.5, "range": 0.625},
            },
            node("out", "output"),
        ], [
            edge("mesh", "out", "pan", "in"),
            edge("pan", "out", "out", "time"),
        ])
        document["probes"] = [{
            "sourceNodeId": "pan",
            "sourcePortId": "out",
            "anchorDestNodeId": "out",
        }]

        report = simplify.simplify_graph(document)

        self.assertEqual(report["neutralPan"], 1)
        self.assertEqual([item["id"] for item in document["nodes"]], ["mesh", "out"])
        self.assertEqual(document["edges"][0]["sourceNodeId"], "mesh")
        self.assertEqual(document["probes"][0]["sourceNodeId"], "mesh")
        self.assertEqual(document["nodes"][0]["parameters"]["range"], 0.625)

    def test_empty_spectral_layer_and_operation_are_bypassed(self):
        document = graph([
            node("fft", "fft"),
            node("phaseLayer1", "trilinearMesh", []),
            {"id": "phaseProcess", "kind": "spectralLayer", "parameters": {"pan": 0.2}},
            node("phaseOp1", "add"),
            node("ifft", "ifft"),
        ], [
            edge("phaseLayer1", "out", "phaseProcess", "in"),
            edge("phaseProcess", "out", "phaseOp1", "right"),
            edge("fft", "phase", "phaseOp1", "left"),
            edge("phaseOp1", "out", "ifft", "phase"),
        ])

        report = simplify.simplify_graph(document)

        self.assertEqual(report["emptySpectralLayer"], 1)
        self.assertEqual(
            {item["id"] for item in document["nodes"]},
            {"fft", "ifft"},
        )
        self.assertEqual(document["edges"], [
            edge("fft", "phase", "ifft", "phase")
        ])

    def test_direct_transform_round_trip_is_removed(self):
        document = graph([
            node("mesh", "trilinearMesh", [1]),
            node("fft", "fft"),
            node("ifft", "ifft"),
            node("out", "output"),
        ], [
            edge("mesh", "out", "fft", "time"),
            edge("fft", "mag", "ifft", "mag"),
            edge("fft", "phase", "ifft", "phase"),
            edge("ifft", "time", "out", "time"),
        ])

        report = simplify.simplify_graph(document)

        self.assertEqual(report["transformPair"], 1)
        self.assertEqual(
            {item["id"] for item in document["nodes"]},
            {"mesh", "out"},
        )
        self.assertEqual(document["edges"], [edge("mesh", "out", "out", "time")])

    def test_unused_nodes_and_guides_are_removed(self):
        document = graph([
            node("mesh", "trilinearMesh", [1]),
            node("unused", "envelope"),
            node("out", "output"),
        ], [edge("mesh", "out", "out", "time")])
        document["guides"] = [
            {"id": "used"},
            {"id": "unused", "heatmapAssetId": "unused-map"},
        ]
        document["guideAssignments"] = [{
            "guideId": "used",
            "targetNodeId": "mesh",
        }]
        document["guideHeatmaps"] = [{"id": "unused-map"}]

        report = simplify.simplify_graph(document)

        self.assertEqual(report["isolatedNode"], 1)
        self.assertEqual(report["unusedGuide"], 1)
        self.assertEqual([item["id"] for item in document["guides"]], ["used"])
        self.assertEqual(document["guideHeatmaps"], [])

    def test_wholly_silent_empty_time_graph_collapses_to_output(self):
        document = graph([
            node("voice", "voiceContext"),
            node("timeLayer1", "trilinearMesh", []),
            node("envelope", "envelope"),
            node("multiply", "multiply"),
            node("out", "output"),
        ], [
            edge("voice", "context", "timeLayer1", "context"),
            edge("timeLayer1", "out", "multiply", "left"),
            edge("envelope", "env", "multiply", "right"),
            edge("multiply", "out", "out", "time"),
        ])

        report = simplify.simplify_graph(document)

        self.assertEqual(report["silentGraph"], 1)
        self.assertEqual([item["id"] for item in document["nodes"]], ["out"])
        self.assertEqual(document["edges"], [])
        self.assertEqual(document["nodes"][0]["position"], {"x": 100.0, "y": 100.0})

    def test_empty_time_seed_is_retained_when_spectral_content_exists(self):
        document = graph([
            node("timeLayer1", "trilinearMesh", []),
            node("fft", "fft"),
            node("magnitudeLayer1", "trilinearMesh", [1]),
            node("magnitudeOp1", "add"),
            node("ifft", "ifft"),
            node("out", "output"),
        ], [
            edge("timeLayer1", "out", "fft", "time"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
            edge("fft", "phase", "ifft", "phase"),
            edge("ifft", "time", "out", "time"),
        ])
        original = copy.deepcopy(document)

        report = simplify.simplify_graph(document)

        self.assertFalse(report)
        self.assertEqual(document, original)

    def test_canonical_writer_preserves_checked_in_graph_format(self):
        preset = SCRIPTS.parent / "cycle-v2" / "content" / "presets" / "Warmth.cyclegraph"
        encoded = preset.read_text(encoding="utf-8")

        self.assertEqual(simplify.canonical_json(json.loads(encoded)) + "\n", encoded)


if __name__ == "__main__":
    unittest.main()
