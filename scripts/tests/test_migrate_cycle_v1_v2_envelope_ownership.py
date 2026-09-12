#!/usr/bin/env python3

import copy
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import migrate_cycle_v1_v2_envelope_ownership as migration


def preset(volume_active=True, scratch_active=False, declick=True):
    layer = lambda active: {
        "properties": {"active": active, "dynamic": False},
    }
    return {
        "meshLibrary": {
            "groups": [
                {"layers": [layer(volume_active)]},
                {"layers": []},
                {"layers": [layer(scratch_active)]},
            ],
        },
        "morphPanel": {
            "position": {"time": 0.25, "red": 0.4, "blue": 0.6},
        },
        "settings": {"Declick": declick},
    }


def envelope(node_id, purpose, enabled, declick=False):
    return {
        "id": node_id,
        "kind": "envelope",
        "position": {"x": 11.0, "y": 22.0},
        "parameters": {
            "enabled": enabled,
            "purpose": purpose,
            "declick": declick,
            "red": 0.0,
            "blue": 0.0,
        },
        "model": {
            "state": {
                "red": 0.0,
                "blue": 0.0,
            },
        },
    }


def signal_edge(source, source_port, destination, destination_port):
    return {
        "sourceNodeId": source,
        "sourcePortId": source_port,
        "destNodeId": destination,
        "destPortId": destination_port,
        "connectionKind": "signal",
        "attachmentType": "none",
    }


class EnvelopeOwnershipMigrationTest(unittest.TestCase):
    def test_updates_semantics_without_changing_existing_presentation_or_global_graph(self):
        source = preset(volume_active=True, scratch_active=False)
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                envelope("volume", "volume", True),
                envelope("scratch", "scratch", True),
                {
                    "id": "staticEnvelopeMorph",
                    "kind": "modulationSource",
                    "position": {"x": 90.0, "y": 91.0},
                },
                {"id": "globalInput", "kind": "globalInput"},
                {"id": "delay", "kind": "delay"},
            ],
            "edges": [
                signal_edge("volume", "env", "volumeMultiply", "right"),
                signal_edge("scratch", "env", "voice", "scratch"),
                signal_edge("staticEnvelopeMorph", "value", "volume", "red"),
                signal_edge("globalInput", "time", "delay", "time"),
            ],
        }
        original_global_edge = copy.deepcopy(graph["edges"][-1])

        changed = migration.migrate_pair(source, graph, {"nodes": [], "edges": []})

        self.assertTrue(changed)
        volume = migration.node_with_id(graph, "volume")
        scratch = migration.node_with_id(graph, "scratch")
        self.assertEqual(volume["position"], {"x": 11.0, "y": 22.0})
        self.assertEqual(volume["parameters"]["red"], 0.4)
        self.assertEqual(volume["parameters"]["blue"], 0.6)
        self.assertEqual(volume["model"]["state"]["red"], 0.4)
        self.assertEqual(volume["model"]["state"]["blue"], 0.6)
        self.assertTrue(volume["parameters"]["declick"])
        self.assertFalse(scratch["parameters"]["enabled"])
        self.assertIsNone(migration.node_with_id(graph, "staticEnvelopeMorph"))
        self.assertIn(original_global_edge, graph["edges"])

    def test_installs_converter_owned_declick_fallback_route(self):
        source = preset(volume_active=False, declick=True)
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                {"id": "wave", "kind": "trilinearMesh"},
                {"id": "voiceOutput", "kind": "voiceOutput"},
                {"id": "globalInput", "kind": "globalInput"},
            ],
            "edges": [
                signal_edge("wave", "out", "voiceOutput", "time"),
            ],
        }
        fallback = envelope("volumeEnvelope1", "volume", False, True)
        multiply = {
            "id": "volumeMultiply",
            "kind": "multiply",
            "position": {"x": 300.0, "y": 400.0},
            "parameters": {},
        }
        converted = {
            "nodes": [fallback, multiply],
            "edges": [
                signal_edge("wave", "out", "volumeMultiply", "left"),
                signal_edge("volumeEnvelope1", "env", "volumeMultiply", "right"),
                signal_edge("volumeMultiply", "out", "voiceOutput", "time"),
            ],
        }

        changed = migration.migrate_pair(source, graph, converted)

        self.assertTrue(changed)
        self.assertEqual(migration.node_with_id(
            graph, "volumeEnvelope1")["position"], {"x": 11.0, "y": 22.0})
        self.assertIsNotNone(migration.node_with_id(graph, "volumeMultiply"))
        self.assertNotIn(
            signal_edge("wave", "out", "voiceOutput", "time"),
            graph["edges"],
        )
        self.assertEqual(len(graph["edges"]), 3)

    def test_boundary_only_graph_is_unchanged(self):
        graph = {
            "nodes": [
                {"id": "voiceOutput", "kind": "voiceOutput"},
                {"id": "globalInput", "kind": "globalInput"},
            ],
            "edges": [],
        }
        original = copy.deepcopy(graph)

        changed = migration.migrate_pair(preset(), graph, {})

        self.assertFalse(changed)
        self.assertEqual(graph, original)

    def test_missing_source_morph_position_preserves_existing_values(self):
        source = preset(volume_active=True, declick=False)
        del source["morphPanel"]["position"]
        volume = envelope("volume", "volume", True)
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                volume,
            ],
            "edges": [
                signal_edge("volume", "env", "volumeMultiply", "right"),
            ],
        }

        migration.migrate_pair(source, graph, {"nodes": [], "edges": []})

        self.assertEqual(volume["parameters"]["red"], 0.0)
        self.assertEqual(volume["parameters"]["blue"], 0.0)
        self.assertEqual(volume["model"]["state"]["red"], 0.0)
        self.assertEqual(volume["model"]["state"]["blue"], 0.0)


if __name__ == "__main__":
    unittest.main()
