#!/usr/bin/env python3

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import create_cycle_v2_candidates as authoring


def curve_controls():
    return {"nodeId": "shape", "documentRevision": 12, "model": {
        "schema": "flatCurve", "version": 1, "revision": 3, "future": {"owner": "domain"},
        "state": {"revision": 3, "futureState": ["keep"], "vertices": [
            {"id": 7, "x": 0.25, "y": 0.8, "curve": 0.6, "futureVertex": True},
            {"id": 9, "x": 1.0, "y": 0.2, "curve": 0.4}]}}}


def mesh_controls():
    return {"nodeId": "timeLayer1", "documentRevision": 12, "model": {
        "schema": "trimesh", "version": 2, "revision": 5, "future": {"keep": 1},
        "mesh": {"name": "authored mesh", "futureMesh": [1, 2],
                 "vertices": [{"id": 0, "time": 1.03, "phase": -0.8, "amp": 0.1,
                               "key": 0.2, "mod": 0.4, "weight": 0.7, "futureVertex": "keep"}],
                 "cubes": [{"vertexIds": list(range(8)), "futureCube": ["keep"]}]}}}


class CandidateAuthoringTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.baseline = self.directory / "source.cyclegraph"
        self.baseline.write_bytes(b"immutable source graph\n")

    def test_curve_edit_changes_only_authored_values_and_preserves_source(self):
        controls = curve_controls()
        original = copy.deepcopy(controls)
        edit = authoring.curve_edit(controls, 0.65, 2)
        expected = copy.deepcopy(original["model"])
        expected["revision"] = expected["state"]["revision"] = 4
        expected["state"]["vertices"][0]["y"] = 0.0625
        expected["state"]["vertices"][1]["y"] = 1.0
        self.assertEqual(edit, {"nodeId": "shape", "parameters": [{"id": "pre", "value": "0.65"}],
                                "expectedModelRevision": 3, "model": expected})
        self.assertEqual(controls, original)
        edit["model"]["future"]["owner"] = "candidate"
        self.assertEqual(controls, original)

    def test_mesh_edit_preserves_geometry_topology_unknown_fields_and_source(self):
        controls = mesh_controls()
        original = copy.deepcopy(controls)
        edit = authoring.mesh_edit(controls, 0.5)
        expected = copy.deepcopy(original["model"])
        expected["revision"] = 6
        expected["mesh"]["vertices"][0]["amp"] = 0.3
        self.assertEqual(edit, {"nodeId": "timeLayer1", "expectedModelRevision": 5, "model": expected})
        self.assertEqual(controls, original)
        edit["model"]["mesh"]["cubes"][0]["futureCube"].append("candidate")
        self.assertEqual(controls, original)

    def session(self, reject_authoring=False):
        calls = []
        graph = self.baseline.read_bytes()
        revision = 1
        undo = redo = None
        dirty = False

        def send(payload, **kwargs):
            nonlocal graph, revision, undo, redo, dirty
            calls.append(copy.deepcopy(payload))
            command = payload["command"]
            data = {}
            if command == "inspectSynthesisCapabilities":
                data = {"authoring": {"command": "applyNodeEdits"}}
            elif command == "openGraph":
                graph = Path(payload["path"]).read_bytes()
                revision += 1
                undo = redo = None
                dirty = False
            elif command in ("saveGraph", "exportGraph"):
                Path(payload["path"]).write_bytes(graph)
                if command == "saveGraph":
                    dirty = False
            elif command == "inspectNodeControls":
                data = curve_controls() if payload["nodeId"] == "waveshaper" else mesh_controls()
                data["nodeId"] = payload["nodeId"]
                data["documentRevision"] = revision
            elif command == "snapshotState":
                data = {"documentRevision": revision, "documentDirty": dirty,
                        "canUndo": undo is not None, "canRedo": redo is not None}
            elif command == "applyNodeEdits":
                self.assertEqual(payload["expectedDocumentRevision"], revision)
                malformed = any(edit.get("model", {}).get("state", {}).get("vertices", [{}])[0]
                                .get("x") == "invalid" for edit in payload["edits"])
                if reject_authoring or malformed:
                    return {"ok": True, "result": {"ok": False, "message": "semantic batch rejected"}}
                undo, redo = graph, None
                graph = json.dumps(payload["edits"], sort_keys=True).encode()
                revision += 1
                dirty = True
                data = {"changed": True, "documentRevision": revision}
            elif command == "undo":
                graph, redo, undo = undo, graph, None
                revision += 1
            elif command == "redo":
                graph, undo, redo = redo, graph, None
                revision += 1
            else:
                self.fail(f"Unexpected automation command: {command}")
            return {"ok": True, "result": {"ok": True, "data": data}}

        return send, calls

    def test_rejected_batch_stops_before_saving_or_rendering_a_candidate(self):
        send, calls = self.session(reject_authoring=True)
        output = self.directory / "rejected"
        with patch.object(authoring, "render_suite") as render:
            result = authoring.create_candidates(self.baseline, output, {}, send, {})
        self.assertFalse(result["ok"])
        self.assertIn("semantic batch rejected", result["error"])
        self.assertEqual(result["candidates"], [])
        render.assert_not_called()
        saved = [Path(call["path"]).name for call in calls if call["command"] == "saveGraph"]
        self.assertEqual(saved, ["baseline.cyclegraph"])
        self.assertFalse((output / "warm.cyclegraph").exists())
        self.assertFalse((output / "lean.cyclegraph").exists())
        self.assertEqual(self.baseline.read_bytes(), b"immutable source graph\n")
        self.assertEqual(json.loads((output / "results.json").read_text()), result)

    def test_candidates_use_semantic_commands_and_existing_document_lifecycle(self):
        send, calls = self.session()
        output = self.directory / "completed"

        def render(suite, preset, render_output, render_send, engine):
            self.assertIs(render_send, send)
            self.assertTrue(preset.exists())
            return {"ok": True, "cases": [{"renders": [
                {"capture": {"rms": 0.05}, "pcmSha256": preset.stem}]}]}

        with patch.object(authoring, "render_suite", side_effect=render) as renderer:
            result = authoring.create_candidates(self.baseline, output, {}, send, {"pid": 123})
        self.assertTrue(result["ok"], result.get("error"))
        self.assertEqual(renderer.call_count, 2)
        self.assertEqual([item["id"] for item in result["candidates"]], ["warm", "lean"])
        batches = [call for call in calls if call["command"] == "applyNodeEdits"]
        self.assertEqual([len(call["edits"]) for call in batches], [1, 2, 2, 2])
        self.assertEqual([edit["nodeId"] for edit in batches[2]["edits"]], ["waveshaper", "timeLayer1"])
        for call in batches:
            self.assertNotIn("graph", call)
            self.assertNotIn("document", call)
        names = [call["command"] for call in calls]
        self.assertEqual(names.count("undo"), 2)
        self.assertEqual(names.count("redo"), 2)
        reopened = [Path(call["path"]).name for call in calls if call["command"] == "openGraph"]
        self.assertEqual(reopened, ["source.cyclegraph", "baseline.cyclegraph", "warm.cyclegraph",
                                    "baseline.cyclegraph", "lean.cyclegraph"])
        for name in ("warm", "lean"):
            self.assertEqual((output / f"{name}-undo.cyclegraph").read_bytes(),
                             (output / "baseline.cyclegraph").read_bytes())
            self.assertEqual((output / f"{name}-after-rejection.cyclegraph").read_bytes(),
                             (output / f"{name}.cyclegraph").read_bytes())
        self.assertEqual(self.baseline.read_bytes(), b"immutable source graph\n")


if __name__ == "__main__":
    unittest.main()
