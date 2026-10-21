import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import create_cycle_v2_saxophone_articulation as runner
from cycle_saxophone_articulation import articulation_plan, demonstration_sequence


class ArticulationDriverTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.output = Path(temporary.name)
        self.graph = json.loads(runner.BASELINE.read_text())
        self.plan = articulation_plan(self.graph, 2.082)
        self.identifiers = {"expressionControls": "generated17", "growlEnvelope": "generated23",
                            "growlMultiply": "generated31"}

    def test_edge_addresses_follow_generated_ids_and_use_current_unique_index(self):
        edge = self.plan["addEdges"][-1]
        mapped = runner.remap_edge(edge, self.identifiers)
        self.assertEqual(mapped["sourceNodeId"], "generated31")
        self.assertEqual(mapped["destNodeId"], "voiceOutput")
        self.assertEqual(edge["sourceNodeId"], "growlMultiply")
        graph = {"edges": [self.graph["edges"][0], mapped]}
        self.assertEqual(runner.edge_index(graph, mapped), 1)
        graph["edges"].reverse()
        self.assertEqual(runner.edge_index(graph, mapped), 0)
        for edges in ([], [mapped, mapped]):
            with self.assertRaises(ValueError):
                runner.edge_index({"edges": edges}, mapped)

    def test_new_model_revision_uses_current_created_node_and_does_not_mutate_plan(self):
        before = copy.deepcopy(self.plan)
        edits = runner.node_edits(self.plan, self.identifiers,
                                 lambda identifier: {"model": {"revision": 7}})
        new_envelope = next(edit for edit in edits if edit["nodeId"] == "generated23")
        self.assertEqual(new_envelope["expectedModelRevision"], 7)
        self.assertEqual(new_envelope["model"]["revision"], 8)
        self.assertEqual(new_envelope["model"]["state"]["revision"], 8)
        self.assertEqual(self.plan, before)
        for revision in (0, True, "7"):
            with self.assertRaises(ValueError):
                runner.node_edits(self.plan, self.identifiers,
                                  lambda identifier: {"model": {"revision": revision}})

    def fake_authoring(self, collide=False, reject=None):
        state = {"graph": None, "revision": 0, "added": [], "calls": []}
        def invoke(command):
            state["calls"].append(copy.deepcopy(command))
            kind = command["command"]
            if kind == reject:
                raise RuntimeError("Application rejected " + kind)
            if kind == "openGraph":
                state["graph"] = json.loads(Path(command["path"]).read_text())
            elif kind in ("exportGraph", "saveGraph"):
                Path(command["path"]).write_text(json.dumps(state["graph"], sort_keys=True))
            elif kind == "convertSynthesisParameter":
                self.assertEqual((command["typeId"], command["parameterId"], command["direction"]),
                                 ("voiceContext", "voiceLength", "toPhysical"))
                return {"physicalValue": 2.082}
            elif kind == "addNode":
                identifier = "voice" if collide else list(self.identifiers.values())[len(state["added"])]
                node = {"id": identifier, "kind": command["kind"], "parameters": {}}
                if node["kind"] == "envelope":
                    node["model"] = {"schema": "envelope", "revision": 7, "state": {"revision": 7}}
                state["graph"]["nodes"].append(node)
                state["added"].append(identifier)
                state["revision"] += 1
                return {"nodeId": identifier}
            elif kind == "inspectNodeControls":
                node = runner.named(state["graph"]["nodes"], command["nodeId"])
                return {"documentRevision": state["revision"], "model": node.get("model")}
            elif kind == "applyNodeEdits":
                self.assertEqual(command["expectedDocumentRevision"], state["revision"])
                state["undo"] = copy.deepcopy(state["graph"])
                for edit in command["edits"]:
                    node = runner.named(state["graph"]["nodes"], edit["nodeId"])
                    if "model" in edit:
                        self.assertEqual(edit["expectedModelRevision"], node["model"]["revision"])
                        node["model"] = copy.deepcopy(edit["model"])
                    node["parameters"].update({item["id"]: item["value"] for item in edit.get("parameters", [])})
                state["revision"] += 1
                return {"changed": True, "documentRevision": state["revision"]}
            elif kind == "undo":
                state["redo"], state["graph"] = state["graph"], state["undo"]
            elif kind == "redo":
                state["graph"] = state["redo"]
            elif kind == "deleteEdge":
                state["graph"]["edges"].pop(command["edgeIndex"])
            elif kind == "connectPorts":
                state["graph"]["edges"].append({key: value for key, value in command.items() if key != "command"})
            elif kind == "setExplanation":
                state["graph"].setdefault("testAnnotations", []).append(command["text"])
            elif kind == "setPresetSequence":
                state["graph"].setdefault("presetPresentation", {})["sequence"] = command["sequence"]
            else:
                self.fail("Unexpected host command: " + kind)
            return {}
        return state, invoke

    def test_authoring_persists_generated_identity_shapes_and_embedded_sequence(self):
        original_digest = runner.file_digest(runner.BASELINE)
        state, invoke = self.fake_authoring()
        candidate, plan, evidence = runner.author_candidate(runner.BASELINE, self.output, invoke)
        saved = json.loads(candidate.read_text())
        self.assertEqual(saved["presetPresentation"]["sequence"], demonstration_sequence())
        self.assertEqual(runner.file_digest(runner.BASELINE), original_digest)
        self.assertTrue(evidence["atomicShapeUndoRedoReload"])
        self.assertEqual(evidence["logicalNodeIds"], self.identifiers)
        ids = {node["id"] for node in saved["nodes"]}
        self.assertTrue(set(self.identifiers.values()).issubset(ids))
        self.assertTrue(all(edge["sourceNodeId"] in ids and edge["destNodeId"] in ids for edge in saved["edges"]))
        self.assertEqual(evidence["economy"]["candidate"]["nodeCount"], 26)
        self.assertEqual(evidence["economy"]["candidate"]["edgeCount"], 33)
        self.assertEqual(sum(command["command"] == "applyNodeEdits" for command in state["calls"]), 1)
        self.assertEqual((self.output / "reloaded.cyclegraph").read_bytes(),
                         (self.output / "candidate-inspection.cyclegraph").read_bytes())

    def test_collision_or_rejected_semantic_edit_cannot_save_a_candidate(self):
        for collide, rejected in ((True, None), (False, "applyNodeEdits")):
            with self.subTest(collide=collide, rejected=rejected):
                state, invoke = self.fake_authoring(collide, rejected)
                with self.assertRaises(RuntimeError):
                    runner.author_candidate(runner.BASELINE, self.output, invoke)
                self.assertFalse(any(command["command"] == "saveGraph" for command in state["calls"]))

    def test_suite_keeps_budget_repeats_explicit_controls_and_shared_pattern_expansion(self):
        sequence = demonstration_sequence()
        runner.write(self.output / "demonstration.cyclepattern", {"version": 1, "sequence": sequence})
        path = self.output / "suite.json"
        runner.write(path, runner.control_suite(2.082))
        suite = runner.load_suite(path)
        self.assertEqual((suite["renderCount"], suite["audioSeconds"], suite["repetitions"]), (26, 152, 2))
        for case in suite["cases"][:-1]:
            self.assertEqual(case["performance"]["sampleRate"], 48000)
            self.assertEqual(case["performance"]["ratePolicy"], "native")
            initial = case["performance"]["events"][:len(runner.DEFAULT_CONTROLLERS)]
            self.assertEqual({item["controller"]: item["value"] for item in initial}, runner.DEFAULT_CONTROLLERS)
        self.assertTrue(all(case["performance"]["voiceDurationSeconds"] == 2.082 for case in suite["cases"]))
        pattern = suite["cases"][-1]
        self.assertEqual(pattern["pattern"]["document"]["sequence"], sequence)
        self.assertNotIn("events", pattern["performance"])

    def test_axis_auditions_share_notes_and_control_timestamps_with_unused_schedule_control(self):
        document = runner.control_suite(2.082)
        anchor = runner.named(document["cases"], "straight")["events"]
        for identifier in ("brightness", "vibrato-depth", "air", "roughness"):
            events = copy.deepcopy(runner.named(document["cases"], identifier)["events"])
            moving = [event for event in events if event["type"] == "controller" and event["timeMs"] > 0]
            self.assertEqual(len(moving), 1)
            moving[0].update(controller=118, value=127)
            self.assertEqual(events, anchor)


if __name__ == "__main__":
    unittest.main()
