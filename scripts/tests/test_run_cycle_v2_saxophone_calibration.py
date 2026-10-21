"""Protect reserved-source isolation and reproducible calibration lineage."""

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

import run_cycle_v2_saxophone_calibration as runner


class CalibrationStagesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.output = self.directory / "output"
        baseline = self.directory / "factory.cyclegraph"
        shutil.copy2(runner.ROOT / "cycle-v2/content/presets/saxophone-3.cyclegraph", baseline)
        manifest = self.directory / "plan.json"
        manifest.write_text('{"experiment":"fixed-ablation"}')
        self.plan = {"path": str(manifest), "baselinePath": str(baseline),
                     "document": {"missingCoverage": ["No independent performer"]},
                     "identities": {str(path): runner.file_digest(path) for path in (manifest, baseline)}, "stages": {}}
        for stage, entries in (("objective", (("objective-nv", "Non-Vibrato"), ("objective-vib", "Vibrato"))),
                               ("withheld", (("reserved-nv", "Non-Vibrato"),))):
            cases = []
            for key, articulation in entries:
                source = self.directory / (key + ".wav")
                self.wav(source)
                crop = {"startSeconds": 0, "endSeconds": 2.1}
                cases.append({"id": key, "sourcePath": str(source), "sourceSha256": runner.file_digest(source),
                    "articulation": articulation, "standardMidi": 60, "crop": crop,
                    "pitchWindow": {"startSeconds": .4, "endSeconds": 2}, "pitchVarianceTrusted": True,
                    "intervals": [{"id": "body", "role": "sustain", "startSeconds": .08, "endSeconds": 2.1}]})
            self.plan["stages"][stage] = {"cases": cases, "suite": {"cases": [{"id": c["id"]} for c in cases]},
                "referenceStudy": {"schemaVersion": 1, "name": stage, "references": [
                    {"id": case["id"], "source": case["sourcePath"], "labels": {}, "crop": case["crop"]} for case in cases]}}
        self.calls, self.collected, self.renders, self.auditions = [], [], [], []
        self.graph = self.undo = self.redo = None
        self.revision = 0
        self.engine = {"executableSha256": "fixed-engine", "pid": 1}
        self.low_confidence = False
        self.change_after_collect = False
        self.addCleanup(patch.stopall)
        patch.object(runner, "render_suite", self.render).start()
        patch.object(runner, "collect_study", self.collect).start()
        patch.object(runner, "audition", self.audition).start()
        patch.object(runner, "plot_pitch_evidence", self.plot).start()

    def wav(self, path):
        with wave.open(str(path), "wb") as target:
            target.setparams((1, 2, 48000, 144000, "NONE", "not compressed"))
            target.writeframes(b"\x00\x10" * 144000)

    def send(self, payload, **kwargs):
        self.calls.append(copy.deepcopy(payload))
        kind, data = payload["command"], {}
        if kind == "openGraph":
            self.graph = json.loads(Path(payload["path"]).read_text())
            self.revision += 1
        elif kind == "setExplanation":
            self.graph.setdefault("presetPresentation", {})["description"] = payload["text"]
            self.revision += 1
        elif kind in ("saveGraph", "exportGraph"):
            Path(payload["path"]).write_text(json.dumps(self.graph, sort_keys=True))
        elif kind == "inspectNodeControls":
            data = {"documentRevision": self.revision}
        elif kind == "applyNodeEdits":
            self.assertEqual(payload["expectedDocumentRevision"], self.revision)
            self.assertEqual(payload["edits"], [runner.pitch_ablation_edit()])
            self.undo = copy.deepcopy(self.graph)
            next(node for node in self.graph["nodes"] if node["id"] == "pitchEnvelope1")["parameters"]["enabled"] = False
            self.revision += 1
            data = {"changed": True, "documentRevision": self.revision}
        elif kind == "undo":
            self.graph, self.redo = self.undo, self.graph
        elif kind == "redo":
            self.graph = self.redo
        else:
            self.fail("Unexpected command: " + kind)
        return {"ok": True, "result": {"ok": True, "data": data}}

    def render(self, suite, preset, destination, send, engine):
        self.renders.append([case["id"] for case in suite["cases"]])
        destination.mkdir()
        result = {"ok": True, "preset": {"effectiveGraphSha256": runner.content_hash(json.loads(preset.read_text()))}, "cases": []}
        for case in suite["cases"]:
            captures = []
            for repetition in (1, 2):
                path = destination / f"{case['id']}-{repetition:02}.wav"
                self.wav(path)
                captures.append({"repetition": repetition, "path": path.name, "wavSha256": runner.file_digest(path),
                                 "capture": {"rms": .125, "peak": .125}})
            result["cases"].append({"id": case["id"], "repeatabilityVerified": True, "renders": captures})
        runner.write(destination / "results.json", result)
        return result

    def collect(self, study, destination, request):
        destination.mkdir()
        observations = []
        self.collected.extend(entry["id"] for entry in study["references"])
        for index, entry in enumerate(study["references"]):
            frequencies = (262, 260, 264, 261) if entry["id"].startswith("baseline-") else (261.62,) * 4
            evidence = {"sampleRate": 48000, "durationSeconds": 3, "trackingInsecurity": 0,
                "lowConfidence": self.low_confidence, "trackingUsedFallback": False, "pitchState": "effective path",
                "referenceId": entry["id"], "audioPath": entry["source"], "pitchFrames": [
                    {"timeSeconds": time, "frequencyHz": frequency} for time, frequency in zip((.4, .8, 1.2, 1.6), frequencies)]}
            path = destination / f"{index}.json"
            runner.write(path, evidence)
            observations.append({"id": entry["id"], "referenceId": entry["id"], "analysisPath": path.name,
                "analysisSha256": runner.file_digest(path), "sourceAudioSha256": runner.file_digest(entry["source"])})
        path = destination / "report.json"
        runner.write(path, {"observations": observations})
        if self.change_after_collect and destination.name == "source-analysis":
            report = json.loads(path.read_text())
            report["observations"][0]["sourceAudioSha256"] = "wrong-library-audio"
            runner.write(path, report)
        return path

    def audition(self, destination, name, cases):
        self.auditions.append(copy.deepcopy(cases))
        folder = destination / name
        folder.mkdir()
        runner.write(folder / "report.json", {"cases": cases})
        (folder / "index.html").write_text("owned report stub")

    def plot(self, cases, output):
        output.mkdir()
        runner.write(output / "pitch-evidence.json", {"cases": cases})

    def objective(self):
        result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "objective-complete", result.get("error"))
        return result

    def test_objective_never_collects_reserved_audio_and_audit_never_edits(self):
        result = self.objective()
        self.assertFalse(any("reserved" in key for key in self.collected))
        self.assertEqual(self.renders, [["objective-nv", "objective-vib"]] * 2)
        self.assertTrue(result["objective"]["pitchComparisons"][0]["trackedNvSpreadCloser"])
        self.assertIsNone(result["objective"]["pitchComparisons"][1]["trackedNvSpreadCloser"])
        self.assertFalse(result["searchGate"])
        self.assertEqual(result["economy"]["baseline"], result["economy"]["candidate"])
        self.assertTrue(all(case["partition"] == "objective" and "pitch" not in case
                            for cases in self.auditions for case in cases))
        frozen = json.loads((self.output / "freeze.json").read_text())
        reserved = self.plan["stages"]["withheld"]["cases"][0]
        self.assertEqual(frozen["inputs"][reserved["sourcePath"]], reserved["sourceSha256"])
        candidate = (self.output / "candidate.cyclegraph").read_bytes()
        self.calls.clear()
        audited = runner.run_stage(self.plan, self.output, "audit", self.send, {**self.engine, "pid": 2})
        self.assertEqual(audited["status"], "complete", audited.get("error"))
        self.assertEqual(self.renders[2:], [["reserved-nv"]] * 2)
        self.assertTrue(any("reserved" in key for key in self.collected))
        self.assertEqual(self.calls, [])
        self.assertTrue(all(case["partition"] == "withheld" for case in self.auditions[-1]))
        self.assertEqual((self.output / "candidate.cyclegraph").read_bytes(), candidate)
        for name, digest in frozen["artifacts"].items():
            self.assertEqual(runner.file_digest(self.output / name), digest)

    def test_audit_rejects_reserved_parent_nested_artifacts_candidate_and_engine(self):
        self.objective()
        paths = [Path(self.plan["stages"]["withheld"]["cases"][0]["sourcePath"]),
                 self.output / "source-analysis/0.json", self.output / "candidate.cyclegraph",
                 self.output / "baseline-renders/results.json", self.output / "candidate-reference/index.html"]
        self.calls.clear()
        for path in paths:
            original = path.read_bytes()
            try:
                path.write_bytes(original + b"changed")
                with self.subTest(path=path), self.assertRaises(ValueError):
                    runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
                self.assertEqual(self.calls, [])
                self.assertFalse((self.output / "audit").exists())
            finally:
                path.write_bytes(original)
        with self.assertRaises(ValueError):
            runner.run_stage(self.plan, self.output, "audit", self.send, {"executableSha256": "changed"})

    def test_uncertain_tracking_suppresses_claim_instead_of_creating_success(self):
        self.low_confidence = True
        result = self.objective()
        self.assertTrue(all(case["trackedNvSpreadCloser"] is None for case in result["objective"]["pitchComparisons"]))
        self.assertFalse(result["acousticModelGate"])

    def test_wrong_library_source_fails_and_cannot_produce_a_freeze(self):
        self.change_after_collect = True
        result = runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertIn("differs from frozen parent", result["error"])
        self.assertFalse((self.output / "freeze.json").exists())
        self.assertEqual(self.renders, [])

    def test_synthesis_analysis_hash_and_reference_identity_are_checked_before_pitch_claims(self):
        for corruption in ("content", "reference"):
            def corrupt_analysis(study, destination, request):
                path = self.collect(study, destination, request)
                if destination.name == "synthesis-analysis":
                    report = json.loads(path.read_text())
                    observed = report["observations"][0]
                    source = destination / observed["analysisPath"]
                    evidence = json.loads(source.read_text())
                    evidence["referenceId"] = "different-reference"
                    runner.write(source, evidence)
                    if corruption == "reference":
                        observed["analysisSha256"] = runner.file_digest(source)
                        runner.write(path, report)
                return path
            output = self.directory / corruption
            with self.subTest(corruption=corruption), patch.object(runner, "collect_study", side_effect=corrupt_analysis):
                result = runner.run_stage(self.plan, output, "objective", self.send, self.engine)
                self.assertEqual(result["status"], "failed")
                self.assertIn("analysis identity changed", result["error"])
                self.assertFalse((output / "freeze.json").exists())
                self.assertNotIn("objective", result)

    def test_partial_audit_failure_persists_and_outputs_are_never_overwritten(self):
        self.objective()
        with self.assertRaises(FileExistsError):
            runner.run_stage(self.plan, self.output, "objective", self.send, self.engine)
        with patch.object(runner, "render_suite", side_effect=RuntimeError("capture timed out")):
            result = runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["failedStage"], "audit")
        self.assertEqual(json.loads((self.output / "results.json").read_text())["error"], "capture timed out")
        with self.assertRaisesRegex(ValueError, "cannot be overwritten"):
            runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)

    def test_candidate_changed_during_audit_cannot_be_certified_unchanged(self):
        self.objective()
        def render_changed_candidate(suite, preset, destination, send, engine):
            if preset.name == "candidate.cyclegraph":
                graph = json.loads(preset.read_text())
                next(node for node in graph["nodes"] if node["id"] == "voice")["parameters"]["octave"] = 1
                runner.write(preset, graph)
            return self.render(suite, preset, destination, send, engine)
        with patch.object(runner, "render_suite", side_effect=render_changed_candidate):
            result = runner.run_stage(self.plan, self.output, "audit", self.send, self.engine)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["failedStage"], "audit")
        self.assertIn("Frozen", result["error"])
        self.assertIn("candidate.cyclegraph", result["error"])
        self.assertNotIn("selectedCandidateUnchanged", result)
        with patch.object(runner, "render_suite", side_effect=render_changed_candidate):
            tampered = self.directory / "tampered-objective"
            objective = runner.run_stage(self.plan, tampered, "objective", self.send, self.engine)
        self.assertEqual(objective["status"], "failed")
        self.assertIn("candidate.cyclegraph", objective["error"])
        self.assertFalse((tampered / "freeze.json").exists())


if __name__ == "__main__":
    unittest.main()
