import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_saxophone_articulation import (CONTROL_MAP, DEFAULT_CONTROLLERS, articulation_plan,
                                         demonstration_sequence)
from cycle_saxophone_study import named


class SaxophoneArticulationTest(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[2]
        self.graph = json.loads((root / "cycle-v2/content/presets/saxophone-3.cyclegraph").read_text())
        self.voice_duration_seconds = 2.082
        self.plan = articulation_plan(self.graph, self.voice_duration_seconds)

    def edit(self, identifier):
        return next(item for item in self.plan["edits"] if item["nodeId"] == identifier)

    def test_planning_leaves_source_intact_and_returns_serializable_command_inputs(self):
        before = copy.deepcopy(self.graph)
        articulation_plan(self.graph, self.voice_duration_seconds)
        self.assertEqual(before, self.graph)
        json.dumps(self.plan, allow_nan=False)
        for edit in self.plan["edits"]:
            if "model" not in edit:
                continue
            source = named(self.graph["nodes"], edit["nodeId"])["model"]
            self.assertEqual(edit["expectedModelRevision"], source["revision"])
            self.assertEqual(edit["model"]["revision"], source["revision"] + 1)
            if edit["model"]["schema"] == "envelope":
                self.assertEqual(edit["model"]["revision"], edit["model"]["state"]["revision"])

    def test_expression_changes_only_volume_red_amplitudes_and_keeps_articulation(self):
        edit = self.edit("volumeEnvelope1")
        source = named(self.graph["nodes"], "volumeEnvelope1")["model"]
        actual = copy.deepcopy(edit["model"])
        for vertex in actual["state"]["mesh"]["mainMesh"]["vertices"]:
            original = named(source["state"]["mesh"]["mainMesh"]["vertices"], vertex["id"])
            self.assertAlmostEqual(vertex["amp"], original["amp"] * (.35 if vertex["key"] == 0 else 1))
            vertex["amp"] = original["amp"]
        actual["revision"] = actual["state"]["revision"] = source["revision"]
        self.assertEqual(actual, source)

    def test_pitch_zero_is_exactly_neutral_and_full_depth_is_thirty_cents(self):
        mesh = self.edit("pitchEnvelope1")["model"]["state"]["mesh"]
        self.assertEqual(mesh["loopIndices"], [1])
        self.assertEqual(mesh["sustainIndices"], [5])
        self.assertEqual(len(mesh["mainMesh"]["vertices"]), 56)
        vertices = mesh["mainMesh"]["vertices"]
        self.assertTrue(all(vertex["amp"] == .5 for vertex in vertices if vertex["mod"] == 0))
        offsets = [(vertex["amp"] - .5) * 2400 for vertex in vertices if vertex["mod"] == 1]
        self.assertAlmostEqual(max(offsets), 30)
        self.assertAlmostEqual(min(offsets), -30)
        self.assertEqual(self.edit("pitchEnvelope1")["parameters"],
                         [{"id": "enabled", "value": "true"}, {"id": "level", "value": "1"}])

    def test_pitch_rate_geometry_has_shared_delay_closed_loop_and_release(self):
        mesh = self.edit("pitchEnvelope1")["model"]["state"]["mesh"]["mainMesh"]
        seconds = self.voice_duration_seconds
        for red, rate in ((0, 4.5), (1, 7.5)):
            poles = [vertex for vertex in mesh["vertices"]
                     if vertex["time"] == 0 and vertex["key"] == red and vertex["mod"] == 1]
            self.assertAlmostEqual(poles[1]["phase"] * seconds, .2)
            self.assertAlmostEqual((poles[5]["phase"] - poles[1]["phase"]) * seconds, 1 / rate)
            self.assertEqual(poles[1]["amp"], poles[5]["amp"])
            self.assertAlmostEqual((poles[6]["phase"] - poles[5]["phase"]) * seconds, .05)
            self.assertEqual(poles[6]["amp"], .5)
            self.assertTrue(all(first["phase"] < second["phase"] for first, second in zip(poles, poles[1:])))

    def test_roughness_is_distinct_unity_to_am_loop_and_remains_compact(self):
        node = named(self.plan["addNodes"], "growlEnvelope")
        self.assertEqual(node["parameters"]["purpose"], "control")
        mesh = node["model"]["state"]["mesh"]["mainMesh"]
        self.assertLess(len(mesh["vertices"]), 100)
        self.assertTrue(all(vertex["amp"] == 1 for vertex in mesh["vertices"] if vertex["mod"] == 0))
        self.assertEqual(min(vertex["amp"] for vertex in mesh["vertices"]), .65)
        seconds = self.voice_duration_seconds
        for red in (0, 1):
            poles = [vertex for vertex in mesh["vertices"]
                     if vertex["time"] == 0 and vertex["key"] == red and vertex["mod"] == 1]
            self.assertAlmostEqual((poles[5]["phase"] - poles[1]["phase"]) * seconds, 1 / 25)
            self.assertEqual((poles[0]["amp"], poles[1]["amp"], poles[5]["amp"], poles[6]["amp"]), (1, 1, 1, 1))

    def test_air_changes_only_existing_component_weights_and_preserves_brightness(self):
        source = named(self.graph["nodes"], "timeLayer1")["model"]
        actual = copy.deepcopy(self.edit("timeLayer1")["model"])
        for vertex in actual["mesh"]["vertices"]:
            original = named(source["mesh"]["vertices"], vertex["id"])
            if vertex["id"] in range(8, 16):
                self.assertEqual(vertex["weight"], .20 if vertex["key"] == 1 else 0)
                vertex["weight"] = original["weight"]
        actual["revision"] = source["revision"]
        self.assertEqual(actual, source)
        routed = dict((item["id"], item["value"]) for item in self.edit("modulationOverride1")["parameters"])
        self.assertEqual(routed, {"redSource": "midiCC", "redController": "4"})
        self.assertFalse(any(edit["nodeId"] == "modulationOverride2" for edit in self.plan["edits"]))

    def test_new_connections_retain_pitch_attachment_and_put_am_after_volume(self):
        existing = [edge for edge in self.graph["edges"] if edge not in self.plan["removeEdges"]]
        edges = existing + self.plan["addEdges"]
        destination_ports = [(edge["destNodeId"], edge["destPortId"]) for edge in edges]
        self.assertEqual(len(destination_ports), len(set(destination_ports)))
        self.assertTrue(any(edge["sourceNodeId"] == "pitchEnvelope1"
                            and edge["destNodeId"] == "voice" and edge["destPortId"] == "pitch" for edge in edges))
        self.assertEqual(next(edge["sourceNodeId"] for edge in edges
                              if edge["destNodeId"] == "voiceOutput"), "growlMultiply")
        controls = named(self.plan["addNodes"], "expressionControls")["parameters"]
        self.assertEqual([controls[key] for key in ("blueController", "redController", "yellowController")], [5, 76, 77])
        self.assertEqual({item["controller"] for item in CONTROL_MAP}, set(DEFAULT_CONTROLLERS))

    def test_card_positions_extend_the_existing_signal_path_and_only_move_voice_output(self):
        self.assertEqual(self.plan["moves"], [{"nodeId": "voiceOutput", "position": {"x": 3175, "y": 886}}])
        self.assertEqual({node["id"]: node["position"] for node in self.plan["addNodes"]}, {
            "expressionControls": {"x": 104, "y": 930},
            "growlEnvelope": {"x": 2732, "y": 1204},
            "growlMultiply": {"x": 2945, "y": 886}})

    def test_demo_initializes_and_resets_every_used_lane_with_balanced_finite_notes(self):
        sequence = demonstration_sequence()
        self.assertEqual(set(sequence), {"durationSeconds", "notes", "controls"})
        self.assertLessEqual(len(sequence["controls"]), 512)
        self.assertLessEqual(len(sequence["notes"]), 512)
        self.assertTrue(20 <= sequence["durationSeconds"] <= 30)
        self.assertEqual({point["controller"] for point in sequence["controls"]}, set(DEFAULT_CONTROLLERS))
        for controller, default in DEFAULT_CONTROLLERS.items():
            lane = [point for point in sequence["controls"] if point["controller"] == controller]
            self.assertEqual((lane[0]["timeSeconds"], lane[0]["value"]), (0, default))
            self.assertEqual((lane[-1]["timeSeconds"], lane[-1]["value"]), (27.8, default))
            self.assertTrue(all(0 <= point["value"] <= 127 for point in lane))
            self.assertTrue(all(first["timeSeconds"] < second["timeSeconds"] for first, second in zip(lane, lane[1:])))
            # Each lane holds its starting value through the straight-tone introduction.
            self.assertGreaterEqual(lane[1]["timeSeconds"], 2.2)
            self.assertEqual(lane[1]["value"], default)
        for note in sequence["notes"]:
            self.assertTrue(0 <= note["pitch"] <= 127 and 0 < note["velocity"] <= 127)
            self.assertTrue(note["startSeconds"] >= 0 and note["durationSeconds"] > 0)
            self.assertLessEqual(note["startSeconds"] + note["durationSeconds"], 27)
        self.assertEqual(min(note["pitch"] for note in sequence["notes"]), 48)
        self.assertEqual(max(note["pitch"] for note in sequence["notes"]), 72)

    def test_demo_contains_separated_attacks_overlapping_runs_and_held_note_return(self):
        notes = demonstration_sequence()["notes"]
        short = [note for note in notes if 2 <= note["startSeconds"] < 4]
        self.assertEqual(len(short), 3)
        self.assertTrue(all(first["startSeconds"] + first["durationSeconds"] < second["startSeconds"]
                            for first, second in zip(short, short[1:])))
        run = [note for note in notes if 18 <= note["startSeconds"] < 21]
        self.assertTrue(all(first["startSeconds"] < second["startSeconds"] < first["startSeconds"] + first["durationSeconds"]
                            for first, second in zip(run, run[1:])))
        held, excursion = notes[-2:]
        self.assertLess(held["startSeconds"], excursion["startSeconds"])
        self.assertLess(excursion["startSeconds"] + excursion["durationSeconds"], held["startSeconds"] + held["durationSeconds"])

    def test_demo_integer_controller_variation_fits_shared_export_queue_before_expansion(self):
        sequence = demonstration_sequence()
        # A monotone segment cannot emit more distinct integer changes than its total variation.
        # This bounds authored content without copying the shared five-millisecond scheduler.
        maximum_controls = len(DEFAULT_CONTROLLERS)
        for controller in DEFAULT_CONTROLLERS:
            lane = [point for point in sequence["controls"] if point["controller"] == controller]
            maximum_controls += sum(abs(second["value"] - first["value"])
                                    for first, second in zip(lane, lane[1:]))
        self.assertLessEqual(maximum_controls + 2 * len(sequence["notes"]), 2048)

    def test_unexpected_source_shapes_ids_and_revisions_fail_before_any_authoring(self):
        for mutate in (
                lambda graph: graph["nodes"].append({"id": "growlEnvelope"}),
                lambda graph: named(graph["nodes"], "volumeEnvelope1")["model"]["state"].update(revision=0),
                lambda graph: named(graph["nodes"], "timeLayer1")["model"]["mesh"]["cubes"][1].update(vertexIds=[]),
                lambda graph: graph.update(guideAssignments=[]),
                lambda graph: graph.update(edges=[])):
            graph = copy.deepcopy(self.graph)
            mutate(graph)
            with self.assertRaises(ValueError):
                articulation_plan(graph, self.voice_duration_seconds)

    def test_physical_duration_is_required_validated_and_controls_authored_timing(self):
        for duration in (0, -1, float("nan"), float("inf"), True, "2.082", None):
            with self.subTest(duration=duration), self.assertRaises(ValueError):
                articulation_plan(self.graph, duration)
        changed = articulation_plan(self.graph, self.voice_duration_seconds * 2)
        before = self.edit("pitchEnvelope1")["model"]["state"]["mesh"]["mainMesh"]["vertices"]
        changed_pitch = next(edit for edit in changed["edits"] if edit["nodeId"] == "pitchEnvelope1")
        after = changed_pitch["model"]["state"]["mesh"]["mainMesh"]["vertices"]
        self.assertEqual([vertex["amp"] for vertex in before], [vertex["amp"] for vertex in after])
        for original, slower in zip(before, after):
            self.assertEqual(original["phase"], slower["phase"] * 2)


if __name__ == "__main__":
    unittest.main()
