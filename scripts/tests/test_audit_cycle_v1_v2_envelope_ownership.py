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
    return {"meshLibrary": {"groups": groups}, "settings": {"Declick": declick}}


def envelope(node_id, purpose, enabled=True, declick=False):
    return {
        "id": node_id,
        "kind": "envelope",
        "parameters": {
            "enabled": enabled,
            "purpose": purpose,
            "declick": declick,
        },
    }


def edge(source, destination, port):
    return {
        "sourceNodeId": source,
        "sourcePortId": "env",
        "destNodeId": destination,
        "destPortId": port,
    }


class EnvelopeOwnershipAuditTest(unittest.TestCase):
    def test_dynamic_envelopes_accept_semantic_owner_routes(self):
        source = preset(
            volume=[layer(True, True)],
            pitch=[layer(True, True)],
            scratch=[layer(True, True)],
            declick=False,
        )
        graph = {
            "nodes": [
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

    def test_static_volume_reports_declick_and_both_missing_morph_inputs(self):
        source = preset(volume=[layer(True, False)], declick=True)
        graph = {
            "nodes": [envelope("volume", "volume")],
            "edges": [edge("volume", "volumeMultiply", "right")],
        }

        findings = audit.audit_pair("missing", source, graph)["findings"]

        self.assertEqual([finding["kind"] for finding in findings], [
            "declick",
            "staticMorphOverride",
        ])
        self.assertEqual(findings[1]["missingPorts"], ["red", "blue"])

    def test_inactive_volume_reports_missing_declick_fallback(self):
        findings = audit.audit_pair(
            "fallback",
            preset(volume=[layer(False, False)], declick=True),
            {"nodes": [], "edges": []},
        )["findings"]

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0]["kind"], "declickFallback")


if __name__ == "__main__":
    unittest.main()
