#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import audit_cycle_v1_v2_envelope_ownership as audit


def layer(active, dynamic):
    return {"properties": {"active": active, "dynamic": dynamic}}


def preset(volume=None, pitch=None, scratch=None, declick=True):
    groups = [
        {"layers": volume or []},
        {"layers": pitch or []},
        {"layers": scratch or []},
    ]
    return {
        "meshLibrary": {"groups": groups},
        "morphPanel": {
            "position": {"time": 0.25, "red": 0.4, "blue": 0.6},
        },
        "settings": {"Declick": declick},
    }


def envelope(node_id, purpose, enabled=True, declick=False):
    return {
        "id": node_id,
        "kind": "envelope",
        "parameters": {
            "enabled": enabled,
            "purpose": purpose,
            "declick": declick,
            "red": 0.4,
            "blue": 0.6,
        },
    }


def edge(source, destination, port):
    return {
        "sourceNodeId": source,
        "sourcePortId": "env",
        "destNodeId": destination,
        "destPortId": port,
    }


def legacy_morph():
    return {
        "id": audit.LEGACY_MORPH_NODE_ID,
        "kind": "modulationSource",
        "parameters": {
            "source": "constant",
            "controller": 1,
            "constant": 0.0,
        },
    }


def legacy_edge(destination, port):
    return {
        "sourceNodeId": audit.LEGACY_MORPH_NODE_ID,
        "sourcePortId": "value",
        "destNodeId": destination,
        "destPortId": port,
    }


class EnvelopeOwnershipAuditTest(unittest.TestCase):
    def test_active_envelopes_require_owner_routes(self):
        source = preset(
            volume=[layer(True, True)],
            pitch=[layer(True, True)],
            scratch=[layer(True, True)],
            declick=False,
        )
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                envelope("volume", "volume"),
                envelope("pitch", "pitch"),
                envelope("scratch", "scratch"),
            ],
            "edges": [
                edge("volume", "volumeMultiply", "right"),
                edge("pitch", "voice", "pitch"),
                edge("scratch", "voice", "scratch"),
            ],
        }

        self.assertEqual(audit.audit_pair("complete", source, graph)["findings"], [])

    def test_active_volume_reports_declick_and_stale_legacy_morph_route(self):
        source = preset(volume=[layer(True, False)], declick=True)
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                envelope("volume", "volume"),
            ],
            "edges": [
                edge("volume", "volumeMultiply", "right"),
                legacy_edge("volume", "red"),
            ],
        }

        findings = audit.audit_pair("missing", source, graph)["findings"]

        self.assertEqual([finding["kind"] for finding in findings], [
            "legacyMorphRoute",
            "declick",
        ])
        self.assertEqual(findings[0]["edgeCount"], 1)

    def test_legacy_morph_node_and_routes_are_rejected(self):
        source = preset(volume=[layer(True, False)], declick=False)
        volume = envelope("volume", "volume")
        volume["parameters"]["red"] = 0.0
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                volume,
                legacy_morph(),
            ],
            "edges": [
                edge("volume", "volumeMultiply", "right"),
                legacy_edge("volume", "red"),
                legacy_edge("volume", "blue"),
            ],
        }

        findings = audit.audit_pair("morph", source, graph)["findings"]

        self.assertEqual([finding["kind"] for finding in findings], [
            "legacyMorphNode",
            "legacyMorphRoute",
        ])

    def test_inactive_volume_reports_missing_declick_fallback(self):
        findings = audit.audit_pair(
            "fallback",
            preset(volume=[layer(False, False)], declick=True),
            {
                "nodes": [{"id": "voice", "kind": "voiceContext"}],
                "edges": [],
            },
        )["findings"]

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0]["kind"], "declickFallback")

    def test_boundary_only_graph_is_not_a_semantic_envelope_target(self):
        result = audit.audit_pair(
            "empty",
            preset(volume=[layer(True, False)]),
            {
                "nodes": [
                    {"id": "voiceOutput", "kind": "voiceOutput"},
                    {"id": "globalInput", "kind": "globalInput"},
                    {"id": "output", "kind": "output"},
                ],
                "edges": [],
            },
        )

        self.assertFalse(result["applicable"])
        self.assertEqual(result["findings"], [])

    def test_missing_source_morph_position_needs_no_compatibility_node(self):
        source = preset(volume=[layer(True, False)], declick=False)
        del source["morphPanel"]["position"]
        volume = envelope("volume", "volume")
        volume["parameters"]["red"] = 0.2
        graph = {
            "nodes": [
                {"id": "voice", "kind": "voiceContext"},
                volume,
            ],
            "edges": [
                edge("volume", "volumeMultiply", "right"),
            ],
        }

        findings = audit.audit_pair("implicit-morph", source, graph)["findings"]

        self.assertEqual(findings, [])


if __name__ == "__main__":
    unittest.main()
