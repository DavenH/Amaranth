import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_performance_suite import load_suite
from render_cycle_v2_exemplars import (DEFAULT_BANK, assert_one_factor, assert_unused_cc118, audition_manifest,
                                       edited_model, load_bank, named, negative_control_suite, prefix_diagnostics, synthesis_identity)


class ExplainedExemplarTest(unittest.TestCase):
    def setUp(self):
        self.bank = load_bank(DEFAULT_BANK)

    def example(self, identifier):
        entry = named(self.bank["examples"], identifier)
        return entry, json.loads(Path(entry["sourcePath"]).read_text())

    def test_factory_explanations_leave_the_reviewed_synthesis_identities_unchanged(self):
        self.assertEqual(len(self.bank["examples"]), 4)
        for entry in self.bank["examples"]:
            graph = json.loads(Path(entry["sourcePath"]).read_text())
            self.assertEqual(synthesis_identity(graph), entry["synthesisSha256"])
            self.assertGreater(len(graph["presetPresentation"]["description"]), 80)
            for node_id in entry["annotationNodeIds"]:
                self.assertIn("\n", named(graph["nodes"], node_id)["rationale"])
            for control in entry["controls"]:
                self.assertEqual(control["status"], "starting-hypothesis")
            altered = copy.deepcopy(graph)
            altered["nodes"][0]["parameters"]["octave"] += 1
            self.assertNotEqual(synthesis_identity(altered), entry["synthesisSha256"])
            altered = copy.deepcopy(graph)
            altered["presetPresentation"]["description"] = "Edited notes only"
            self.assertEqual(synthesis_identity(altered), entry["synthesisSha256"])

    def test_transfer_recipe_changes_one_point_without_mutating_the_inspection(self):
        entry, graph = self.example("sat-bass")
        node = named(graph["nodes"], "waveshaper")
        controls = {"nodeId": node["id"], "model": node["model"]}
        before = copy.deepcopy(controls)
        edit = edited_model(controls, entry["intervention"])
        self.assertEqual(controls, before)
        altered = copy.deepcopy(graph)
        named(altered["nodes"], "waveshaper")["model"] = edit["model"]
        self.assertEqual(named(edit["model"]["state"]["vertices"], 5)["y"], 0.6)
        assert_one_factor(graph, altered, entry["intervention"])
        named(altered["nodes"], "waveshaper")["parameters"]["pre"] = 0.2
        with self.assertRaises(ValueError):
            assert_one_factor(graph, altered, entry["intervention"])

    def test_release_recipe_preserves_sustain_attack_markers_and_doubles_later_spans(self):
        entry, graph = self.example("filter-saw")
        node = named(graph["nodes"], "volumeEnvelope1")
        controls = {"nodeId": node["id"], "model": node["model"]}
        edit = edited_model(controls, entry["intervention"])
        original = node["model"]["state"]["mesh"]
        changed = edit["model"]["state"]["mesh"]
        self.assertEqual(changed["sustainIndices"], original["sustainIndices"])
        self.assertEqual(changed["loopIndices"], original["loopIndices"])
        for vertex in original["mainMesh"]["vertices"]:
            current = named(changed["mainMesh"]["vertices"], vertex["id"])
            if vertex["phase"] <= 1:
                self.assertEqual(current, vertex)
            else:
                self.assertAlmostEqual(current["phase"] - 1, 2 * (vertex["phase"] - 1))
                self.assertEqual(current["amp"], vertex["amp"])
        altered = copy.deepcopy(graph)
        named(altered["nodes"], node["id"])["model"] = edit["model"]
        assert_one_factor(graph, altered, entry["intervention"])
        named(altered["nodes"], node["id"])["model"]["state"]["mesh"]["sustainIndices"] = []
        with self.assertRaises(ValueError):
            assert_one_factor(graph, altered, entry["intervention"])

    def test_release_recipe_rejects_unreviewed_markers_instead_of_guessing_a_boundary(self):
        entry, graph = self.example("filter-saw")
        node = named(graph["nodes"], "volumeEnvelope1")
        controls = {"nodeId": node["id"], "model": copy.deepcopy(node["model"])}
        controls["model"]["state"]["mesh"]["sustainIndices"] = [0]
        with self.assertRaises(ValueError):
            edited_model(controls, entry["intervention"])
        controls["model"]["state"]["mesh"]["sustainIndices"] = [1]
        vertex_id = controls["model"]["state"]["mesh"]["mainMesh"]["cubes"][1]["vertexIds"][0]
        named(controls["model"]["state"]["mesh"]["mainMesh"]["vertices"], vertex_id)["phase"] = 0.9
        with self.assertRaises(ValueError):
            edited_model(controls, entry["intervention"])

    def test_guide_recipe_cannot_change_seed_curve_or_other_guides(self):
        entry, graph = self.example("reed")
        changed = copy.deepcopy(graph)
        guide = named(changed["guides"], "guide5")
        guide["noise"] = entry["intervention"]["noise"]
        guide["revision"] += 1
        assert_one_factor(graph, changed, entry["intervention"])
        guide["noiseSeed"] += 1
        with self.assertRaises(ValueError):
            assert_one_factor(graph, changed, entry["intervention"])
        self.assertTrue(any(item["guideId"] == "guide5" and item["targetNodeId"] == "timeLayer1"
                            and item["target"]["field"] == "phase" for item in graph["guideAssignments"]))

    def test_delay_recipe_retains_every_other_effect_and_voice_setting(self):
        entry, graph = self.example("warmth")
        changed = copy.deepcopy(graph)
        named(changed["nodes"], "delay")["parameters"]["enabled"] = False
        assert_one_factor(graph, changed, entry["intervention"])
        named(changed["nodes"], "reverb")["parameters"]["enabled"] = False
        with self.assertRaises(ValueError):
            assert_one_factor(graph, changed, entry["intervention"])

    def test_unused_controller_control_preserves_the_entire_schedule_except_its_target(self):
        source = load_suite(Path(self.bank["suitePath"]))
        original = copy.deepcopy(named(source["cases"], "middle-cc1")["performance"])
        with tempfile.TemporaryDirectory() as directory:
            control = negative_control_suite(Path(self.bank["suitePath"]), Path(directory))
        observed = control["cases"][0]["performance"]
        expected = copy.deepcopy(original)
        for event in expected["events"]:
            if event["type"] == "controller" and event["timeMs"] > 0:
                event["controller"] = 118
        self.assertEqual(observed, expected)
        self.assertEqual(observed["events"][0]["controller"], 1)
        self.assertEqual((len(control["cases"]), control["repetitions"]), (1, 2))
        for entry in self.bank["examples"]:
            graph = json.loads(Path(entry["sourcePath"]).read_text())
            assert_unused_cc118(graph)
            named(graph["nodes"], "morph")["parameters"]["blueController"] = 118
            with self.assertRaises(ValueError):
                assert_unused_cc118(graph)

    def test_full_prefix_diagnostics_retain_tiny_boundary_differences(self):
        source = {"sampleRate": 8000, "sampleWidth": 3, "frames": 2000,
                  "samplePayloadSha256": "source", "channels": [[0.1] * 2000, [0.1] * 2000]}
        changed = copy.deepcopy(source)
        changed["channels"][0][1598] += 1 / 8388608
        changed["channels"][1][1598] += 1 / 8388608
        evidence = prefix_diagnostics([source, changed])
        self.assertEqual(evidence["differingSamples"], 2)
        self.assertEqual(evidence["firstDifferenceFrame"], 1598)
        self.assertEqual(evidence["firstDifferenceSeconds"], 1598 / 8000)
        self.assertAlmostEqual(evidence["worstSampleError"], 1 / 8388608)
        self.assertEqual(source["channels"][0][:1440], changed["channels"][0][:1440])

    def test_suite_and_reports_use_corresponding_controlled_phases_without_alignment(self):
        suite = load_suite(Path(self.bank["suitePath"]))
        self.assertEqual((len(suite["cases"]), suite["repetitions"], suite["renderCount"]), (4, 2, 8))
        steady = named(suite["cases"], "middle-held")["performance"]
        moving = named(suite["cases"], "middle-cc1")["performance"]
        self.assertEqual([event for event in steady["events"] if event["timeMs"] < 200],
                         [event for event in moving["events"] if event["timeMs"] < 200])
        self.assertTrue(any(event["type"] == "noteOff" and event["timeMs"] == 600 for event in moving["events"]))
        with tempfile.TemporaryDirectory() as directory:
            value = json.loads(audition_manifest("reed", Path(directory)).read_text())
        self.assertNotIn("alignment", value)
        self.assertTrue(all("pitch" not in case for case in value["cases"]))
        self.assertEqual(len(value["cases"]), 6)
        self.assertEqual(value["cases"][0]["intervals"][-1]["startSeconds"], .6)
        self.assertEqual(value["cases"][4]["a"]["performanceResults"], "negative-renders/results.json")
        self.assertEqual(value["cases"][4]["b"]["performanceResults"], "baseline-renders/results.json")
        self.assertEqual(value["cases"][5]["a"]["performanceResults"], "baseline-renders/results.json")
        self.assertEqual(value["cases"][5]["b"]["performanceResults"], "negative-renders/results.json")
        self.assertEqual(value["cases"][-1]["intervals"][0]["endSeconds"], .18)
        self.assertEqual(value["cases"][-1]["intervals"][1]["startSeconds"], .2)


if __name__ == "__main__":
    unittest.main()
