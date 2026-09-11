import copy
import json
import sys
import unittest
from collections import Counter
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


def scratch_edge(source, destination):
    result = edge(source, "env", destination, "scratch")
    result["connectionKind"] = "processingAttachment"
    result["attachmentType"] = "scratchEnvelope"
    return result


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
    def test_complete_scratch_fanout_becomes_voice_context_default(self):
        scratch = {
            **node("scratch", "envelope"),
            "parameters": {"purpose": "scratch"},
        }
        document = graph([
            node("voice", "voiceContext"),
            scratch,
            node("time", "trilinearMesh", [1]),
            node("magnitude", "trilinearMesh", [1]),
        ], [
            scratch_edge("scratch", "time"),
            scratch_edge("scratch", "magnitude"),
        ])

        report = Counter()
        simplify.collapse_complete_scratch_fanout(document, report)

        self.assertEqual(report["scratchDefault"], 1)
        self.assertEqual(report["scratchEdgesRemoved"], 1)
        self.assertEqual(document["edges"], [scratch_edge("scratch", "voice")])

        second_report = Counter()
        simplify.collapse_complete_scratch_fanout(document, second_report)
        self.assertFalse(second_report)

    def test_partial_scratch_fanout_remains_explicit(self):
        scratch = {
            **node("scratch", "envelope"),
            "parameters": {"purpose": "scratch"},
        }
        document = graph([
            node("voice", "voiceContext"),
            scratch,
            node("time", "trilinearMesh", [1]),
            node("magnitude", "trilinearMesh", [1]),
            node("phase", "trilinearMesh", [1]),
        ], [
            scratch_edge("scratch", "time"),
            scratch_edge("scratch", "magnitude"),
        ])
        original = copy.deepcopy(document)

        report = Counter()
        simplify.collapse_complete_scratch_fanout(document, report)

        self.assertFalse(report)
        self.assertEqual(document, original)

    def test_neutral_pan_is_bypassed_and_probe_source_follows(self):
        document = graph([
            {
                **node("mesh", "trilinearMesh", [1]),
                "parameters": {"range": 0.5},
            },
            {
                "id": "pan",
                "kind": "spectralLayer",
                "parameters": {
                    "pan": 0.5,
                    "range": 0.625,
                    "mode": "additive",
                },
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
        self.assertEqual(
            document["nodes"][0]["parameters"]["spectralMode"],
            "additive",
        )

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

    def test_empty_time_seed_promotes_populated_spectral_branches(self):
        document = graph([
            {
                **node("voice", "voiceContext"),
                "parameters": {"domain": "waveform"},
            },
            node("timeLayer1", "trilinearMesh", []),
            node("fft", "fft"),
            node("magnitudeLayer1", "trilinearMesh", [1]),
            node("magnitudeOp1", "add"),
            node("phaseLayer1", "trilinearMesh", [1]),
            {
                **node("phasePan", "spectralLayer"),
                "parameters": {"pan": 0.75},
            },
            node("phaseOp1", "add"),
            node("ifft", "ifft"),
            node("out", "output"),
        ], [
            edge("voice", "context", "timeLayer1", "context"),
            edge("timeLayer1", "out", "fft", "time"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
            edge("fft", "phase", "phaseOp1", "left"),
            edge("phaseLayer1", "out", "phasePan", "in"),
            edge("phasePan", "out", "phaseOp1", "right"),
            edge("phaseOp1", "out", "ifft", "phase"),
            edge("ifft", "time", "out", "time"),
        ])
        positions = {item["id"]: item["position"] for item in document["nodes"]}
        positions["voice"]["x"] = 100.0
        positions["magnitudeLayer1"]["x"] = 1200.0
        positions["phaseLayer1"]["x"] = 1200.0
        positions["phasePan"]["x"] = 1480.0
        positions["ifft"]["x"] = 2280.0
        positions["out"]["x"] = 3160.0

        report = simplify.simplify_graph(document)

        self.assertEqual(report["emptyTimeSeed"], 1)
        self.assertEqual(report["emptyTimeSeedNodes"], 4)
        self.assertEqual(report["spectralLayoutCompaction"], 1)
        self.assertEqual(
            {item["id"] for item in document["nodes"]},
            {"voice", "magnitudeLayer1", "phaseLayer1", "phasePan", "ifft", "out"},
        )
        self.assertEqual(document["nodes"][0]["parameters"]["domain"], "spectral")
        rewritten_positions = {
            item["id"]: item["position"]["x"] for item in document["nodes"]
        }
        self.assertEqual(rewritten_positions["magnitudeLayer1"], 380.0)
        self.assertEqual(rewritten_positions["phaseLayer1"], 380.0)
        self.assertEqual(rewritten_positions["phasePan"], 660.0)
        self.assertEqual(rewritten_positions["ifft"], 1460.0)
        self.assertEqual(rewritten_positions["out"], 2340.0)
        self.assertEqual(document["edges"], [
            edge("magnitudeLayer1", "out", "ifft", "mag"),
            edge("phaseLayer1", "out", "phasePan", "in"),
            edge("phasePan", "out", "ifft", "phase"),
            edge("ifft", "time", "out", "time"),
            edge("voice", "context", "magnitudeLayer1", "context"),
            edge("voice", "context", "phaseLayer1", "context"),
        ])

        second_report = simplify.simplify_graph(document)
        self.assertFalse(second_report)

    def test_empty_time_seed_with_multiply_branch_is_retained(self):
        document = graph([
            {
                **node("voice", "voiceContext"),
                "parameters": {"domain": "waveform"},
            },
            node("timeLayer1", "trilinearMesh", []),
            node("fft", "fft"),
            node("magnitudeLayer1", "trilinearMesh", [1]),
            node("magnitudeOp1", "multiply"),
            node("ifft", "ifft"),
        ], [
            edge("voice", "context", "timeLayer1", "context"),
            edge("timeLayer1", "out", "fft", "time"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
            edge("fft", "phase", "ifft", "phase"),
        ])
        original = copy.deepcopy(document)

        report = simplify.simplify_graph(document)

        self.assertFalse(report)
        self.assertEqual(document, original)

    def test_empty_time_seed_with_probe_reference_is_retained(self):
        document = graph([
            {
                **node("voice", "voiceContext"),
                "parameters": {"domain": "waveform"},
            },
            node("timeLayer1", "trilinearMesh", []),
            node("fft", "fft"),
            node("magnitudeLayer1", "trilinearMesh", [1]),
            node("magnitudeOp1", "add"),
            node("ifft", "ifft"),
        ], [
            edge("voice", "context", "timeLayer1", "context"),
            edge("timeLayer1", "out", "fft", "time"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
            edge("fft", "phase", "ifft", "phase"),
        ])
        document["probes"] = [{
            "sourceNodeId": "magnitudeOp1",
            "sourcePortId": "out",
            "anchorDestNodeId": "ifft",
        }]
        original = copy.deepcopy(document)

        report = simplify.simplify_graph(document)

        self.assertEqual(report["ambiguousEmptyTimeSeed"], 1)
        self.assertEqual(document, original)

    def test_nonempty_time_layer_is_not_promoted_to_spectral_context(self):
        document = graph([
            {
                **node("voice", "voiceContext"),
                "parameters": {"domain": "waveform"},
            },
            node("timeLayer1", "trilinearMesh", [1]),
            node("fft", "fft"),
            node("magnitudeLayer1", "trilinearMesh", [1]),
            node("magnitudeOp1", "add"),
            node("ifft", "ifft"),
        ], [
            edge("voice", "context", "timeLayer1", "context"),
            edge("timeLayer1", "out", "fft", "time"),
            edge("fft", "mag", "magnitudeOp1", "left"),
            edge("magnitudeLayer1", "out", "magnitudeOp1", "right"),
            edge("magnitudeOp1", "out", "ifft", "mag"),
            edge("fft", "phase", "ifft", "phase"),
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
