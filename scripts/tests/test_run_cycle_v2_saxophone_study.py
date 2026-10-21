"""Protect stage isolation and the pilot's frozen evidence boundary."""

import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import run_cycle_v2_saxophone_study as runner
from cycle_saxophone_study import DEFAULT_PLAN, OBJECTIVE_IDS, WITHHELD_IDS, load_plan


class SaxophoneStagesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        content = self.directory / "content"
        self.plan_path = content / "studies/expressive-sax/plan.json"
        shutil.copytree(DEFAULT_PLAN.parent, self.plan_path.parent)
        for name in ("presets/saxophone-3.cyclegraph", "references/sax-growl.wav"):
            path = content / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(DEFAULT_PLAN.parents[2] / name, path)
        self.plan = load_plan(self.plan_path)
        self.output = self.directory / "run"
        self.calls, self.renders, self.reports = [], [], []
        self.graph = None
        self.revision = 0
        self.undo = self.redo = None
        self.fail_command = None
        self.engine = {"executableSha256": "fixed-engine", "pid": 123}
        self.library_audio = Path(self.plan["referenceStudy"]["references"][0]["source"])
        self.addCleanup(patch.stopall)
        patch.object(runner, "render_suite", self.render).start()
        patch.object(runner, "report", self.report).start()

    def send(self, payload, **kwargs):
        self.calls.append(copy.deepcopy(payload))
        kind, data = payload["command"], {}
        if kind == self.fail_command:
            return {"ok": True, "result": {"ok": False, "message": "injected failure"}}
        if kind == "openGraph":
            self.graph = json.loads(Path(payload["path"]).read_text())
            self.revision += 1
            self.undo = self.redo = None
        elif kind in ("saveGraph", "exportGraph"):
            Path(payload["path"]).write_text(json.dumps(self.graph, sort_keys=True))
        elif kind == "inspectNodeControls":
            node = next(node for node in self.graph["nodes"] if node["id"] == payload["nodeId"])
            data = {"nodeId": node["id"], "model": copy.deepcopy(node["model"]), "documentRevision": self.revision}
        elif kind == "applyNodeEdits":
            self.assertEqual(payload["expectedDocumentRevision"], self.revision)
            self.undo = copy.deepcopy(self.graph)
            for edit in payload["edits"]:
                node = next(node for node in self.graph["nodes"] if node["id"] == edit["nodeId"])
                self.assertEqual(edit["expectedModelRevision"], node["model"]["revision"])
                node["model"] = copy.deepcopy(edit["model"])
            self.revision += 1
            data = {"changed": True, "documentRevision": self.revision}
        elif kind == "undo":
            self.graph, self.redo = self.undo, self.graph
        elif kind == "redo":
            self.graph = self.redo
        elif kind == "listReferences":
            data = {"references": [{"referenceId": "factory:sax-growl", "audioPath": str(self.library_audio)}]}
        elif kind == "selectReference":
            pass
        elif kind == "setExplanation":
            self.graph.setdefault("presetPresentation", {})["description"] = payload["text"]
            self.revision += 1
        elif kind == "exportReferenceAnalysis":
            data = {"referenceId": "factory:sax-growl", "audioPath": str(self.library_audio),
                    "durationSeconds": 173270 / 44100, "pitchFrames": [], "cyclogram": {"columns": 1},
                    "lowConfidence": False, "trackingUsedFallback": False}
            Path(payload["path"]).write_text(json.dumps(data))
        else:
            self.fail("Unexpected app command: " + kind)
        return {"ok": True, "result": {"ok": True, "data": data}}

    def render(self, suite, preset, destination, send, engine):
        identifiers = [case["id"] for case in suite["cases"]]
        self.renders.append((preset.name, identifiers))
        destination.mkdir()
        result = {"ok": True, "preset": {"effectiveGraphSha256": runner.content_hash(json.loads(preset.read_text()))},
                  "cases": [{"id": key, "repeatabilityVerified": True,
                             "renders": [{"capture": {"rms": .125, "peak": .125}}]} for key in identifiers]}
        runner.write(destination / "results.json", result)
        for key in identifiers:
            with wave.open(str(destination / (key + "-01.wav")), "wb") as target:
                target.setparams((1, 2, 44100, 88200, "NONE", "not compressed"))
                target.writeframes(b"\x00\x10" * 88200)
        return result

    def report(self, destination, name, cases):
        self.reports.append((name, copy.deepcopy(cases)))
        folder = destination / name
        folder.mkdir()
        value = {"objectiveSummary": {} if name == "withheld" else {"rms": .1}, "cases": [
            {"id": case["id"], "intervals": [{"raw": {"meanAbsoluteDifference": {
                "rms": .1 if case["id"] == "cc2-response" else 0}}} for phase in case["intervals"]]} for case in cases]}
        runner.write(folder / "report.json", value)
        (folder / "index.html").write_text("audition owner stub")
        return value

    def objective(self):
        result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "objective-complete", result.get("error"))
        return result

    def test_objective_cannot_render_reserved_cases_and_audit_cannot_edit(self):
        result = self.objective()
        self.assertEqual([set(ids) for _, ids in self.renders], [OBJECTIVE_IDS, OBJECTIVE_IDS])
        self.assertFalse(result["acousticModelGate"])
        self.assertFalse(result["searchGate"])
        self.assertEqual(result["economy"]["baseline"], result["economy"]["candidate"])
        self.assertEqual([call["command"] for call in self.calls].count("applyNodeEdits"), 1)
        self.assertTrue(all(case["partition"] == "objective" and "pitch" not in case
                            for _, cases in self.reports for case in cases))
        frozen_candidate = (self.output / "candidate.cyclegraph").read_bytes()
        objective_files = json.loads((self.output / "freeze.json").read_text())["artifacts"]
        self.calls.clear()
        audited = runner.run_stage(self.plan, self.output, "audit", self.send, {**self.engine, "pid": 456})
        self.assertEqual(audited["status"], "complete", audited.get("error"))
        self.assertEqual([set(ids) for _, ids in self.renders[2:]], [WITHHELD_IDS, WITHHELD_IDS])
        self.assertEqual(self.calls, [])
        self.assertEqual(frozen_candidate, (self.output / "candidate.cyclegraph").read_bytes())
        self.assertTrue(all(case["partition"] == "withheld" for case in self.reports[-1][1]))
        for name, digest in objective_files.items():
            self.assertEqual(runner.file_digest(self.output / name), digest)

    def test_changed_sources_reports_candidate_or_engine_reject_before_commands(self):
        self.objective()
        paths = [Path(name) for name in self.plan["identities"]]
        paths += [self.output / name for name in ("candidate.cyclegraph", "intervention/report.json",
                  "baseline-renders/results.json", "controls/index.html")]
        self.calls.clear()
        original_results = (self.output / "results.json").read_bytes()
        for path in paths:
            original = path.read_bytes()
            try:
                path.write_bytes(original + b"changed")
                with self.subTest(path=path), self.assertRaises((ValueError, OSError)):
                    runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
                self.assertEqual(self.calls, [])
                self.assertFalse((self.output / "audit").exists())
                self.assertEqual((self.output / "results.json").read_bytes(), original_results)
            finally:
                path.write_bytes(original)
        with self.assertRaises(ValueError):
            runner.run_stage(self.plan, self.output, "audit", self.send, {"executableSha256": "changed"})
        self.assertEqual(self.calls, [])

    def test_command_failure_persists_evidence_and_cannot_be_audited(self):
        self.fail_command = "applyNodeEdits"
        result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["failedStage"], "objective")
        self.assertIn("Automation failed", result["error"])
        self.assertEqual(json.loads((self.output / "results.json").read_text())["status"], "failed")
        self.assertTrue((self.output / "split.json").is_file())
        self.assertFalse((self.output / "freeze.json").exists())
        self.assertEqual(self.renders, [])
        self.calls.clear()
        with self.assertRaises(OSError):
            runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
        self.assertEqual(self.calls, [])

    def test_wrong_library_recording_fails_before_rendering(self):
        self.library_audio = self.directory / "wrong-recording.wav"
        self.library_audio.write_bytes(b"a stale factory reference")
        result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertIn("differs from the declared source", result["error"])
        self.assertEqual(self.renders, [])

    def test_partial_audit_failure_is_saved_and_never_resumed(self):
        self.objective()
        with patch.object(runner, "render_suite", side_effect=RuntimeError("capture timed out")):
            result = runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["failedStage"], "audit")
        self.assertIn("capture timed out", result["error"])
        self.assertEqual(json.loads((self.output / "results.json").read_text())["failedStage"], "audit")
        with self.assertRaisesRegex(ValueError, "cannot be overwritten"):
            runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)

    def test_library_audio_change_after_report_cannot_be_frozen_as_observed(self):
        copied_audio = self.directory / "library-copy.wav"
        shutil.copy2(self.library_audio, copied_audio)
        self.library_audio = copied_audio
        def report_then_change(destination, name, cases):
            value = self.report(destination, name, cases)
            if name == "reference":
                self.library_audio.write_bytes(b"changed after collected observation")
            return value
        with patch.object(runner, "report", side_effect=report_then_change):
            result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertIn("Frozen input changed", result["error"])
        self.assertFalse((self.output / "freeze.json").exists())

    def test_successful_or_partial_audit_and_objective_outputs_are_never_reused(self):
        self.objective()
        self.calls.clear()
        with self.assertRaises(FileExistsError):
            runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        (self.output / "audit").mkdir()
        with self.assertRaisesRegex(ValueError, "cannot be overwritten"):
            runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
        self.assertEqual(self.calls, [])


if __name__ == "__main__":
    unittest.main()
