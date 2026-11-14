import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from align_cycle_v2_scratch_envelopes import align_preset_text, align_scratch_envelopes


def node(identifier, kind, y):
    return {"id": identifier, "kind": kind, "position": {"x": 100, "y": y},
            "parameters": {"keep": True}, "model": {"vertices": [1, 2, 3]}}


def scratch_edge(destination):
    return {"sourceNodeId": "scratch", "sourcePortId": "env", "destNodeId": destination,
            "destPortId": "scratch", "connectionKind": "processingAttachment",
            "attachmentType": "scratchEnvelope"}


def graph():
    return {"formatVersion": 7, "presetPresentation": {"title": "Keep me"},
            "nodes": [node("scratch", "envelope", 140), node("mesh", "trilinearMesh", 100)],
            "edges": [scratch_edge("mesh")]}


class AlignScratchEnvelopesTest(unittest.TestCase):
    def test_text_migration_changes_only_y_without_reformatting_mesh_or_preview(self):
        document = graph()
        document["metadata"] = {"smallNumber": 1e-8, "preview": "Keep\nme"}
        original = json.dumps(document, indent=3) + "\n"
        expected = original.replace('"y": 140', '"y": 100')
        updated, moved = align_preset_text(original)
        self.assertEqual(moved, ["scratch"])
        self.assertEqual(updated, expected)
        self.assertEqual(align_preset_text(updated), (updated, []))

    def test_only_the_envelope_y_changes_and_repeating_is_a_noop(self):
        document = graph()
        expected = copy.deepcopy(document)
        expected["nodes"][0]["position"]["y"] = 100
        self.assertEqual(align_scratch_envelopes(document), ["scratch"])
        self.assertEqual(document, expected)
        self.assertEqual(align_scratch_envelopes(document), [])

    def test_shared_envelopes_and_voice_defaults_stay_in_place(self):
        for kind in ("trilinearMesh", "voiceContext"):
            with self.subTest(kind=kind):
                document = graph()
                document["nodes"].append(node("second", kind, 200))
                document["edges"].append(scratch_edge("second"))
                expected = copy.deepcopy(document)
                self.assertEqual(align_scratch_envelopes(document), [])
                self.assertEqual(document, expected)
        document = graph()
        document["nodes"][1]["kind"] = "voiceContext"
        self.assertEqual(align_scratch_envelopes(document), [])

    def test_other_envelope_uses_stay_in_place(self):
        document = graph()
        document["edges"][0].update(destPortId="red", attachmentType="none")
        self.assertEqual(align_scratch_envelopes(document), [])
        document = graph()
        document["edges"].clear()
        self.assertEqual(align_scratch_envelopes(document), [])

    def test_two_routes_to_the_same_consumer_still_align(self):
        document = graph()
        extra = copy.deepcopy(document["edges"][0])
        extra.update(destPortId="red", attachmentType="none")
        document["edges"].append(extra)
        self.assertEqual(align_scratch_envelopes(document), ["scratch"])


if __name__ == "__main__":
    unittest.main()
