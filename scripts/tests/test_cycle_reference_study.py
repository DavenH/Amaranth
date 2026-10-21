import json
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_reference_study import collect_study, observation, read_study


class ReferenceStudyTest(unittest.TestCase):
    def write_study(self, root, entries):
        path = root / "study.json"
        path.write_text(json.dumps({"schemaVersion": 1, "references": entries}))
        return path

    def test_manifest_preserves_provenance_and_unknown_controls(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_study(root, [{"id": "soft", "referenceId": "factory:sax-growl",
                    "labels": {"pressure": {"value": 0.2, "origin": "observed", "confidence": 0.8},
                               "technique": {"value": "growl", "origin": "inferred"}}}])
            entry = read_study(path)["references"][0]
            self.assertEqual(entry["labels"]["pressure"]["origin"], "observed")
            self.assertEqual(entry["labels"]["technique"]["origin"], "inferred")
            self.assertNotIn("brightness", entry["labels"])
            self.assertNotIn("confidence", entry["labels"]["technique"])

    def test_manifest_rejects_invalid_entries_before_execution(self):
        cases = [
            [{"id": "missing"}],
            [{"id": "missing", "source": "absent.wav"}],
            [{"id": "x", "referenceId": "a"}, {"id": "x", "referenceId": "b"}],
            [{"id": "x", "referenceId": "a", "crop": {"startSeconds": float("nan")}}],
            [{"id": "x", "referenceId": "a", "crop": {"startSeconds": 2, "endSeconds": 1}}],
            [{"id": "x", "referenceId": "a", "crop": {"endSecond": 2}}],
            [{"id": "x", "referenceId": "a", "labels": {"pitch": {"value": 60}}}],
            [{"id": "x", "referenceId": "a", "labels": {
                "pitch": {"value": 60, "origin": "inferred", "confidence": 1.1}}}],
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for entries in cases:
                with self.subTest(entries=entries):
                    with self.assertRaises(ValueError):
                        read_study(self.write_study(root, entries))

    def test_crop_selects_existing_analysis_without_retracking_or_inventing_frames(self):
        analysis = {"durationSeconds": 2, "cyclogram": {"columns": 5},
                    "pitchFrames": [{"timeSeconds": value} for value in (0, 0.5, 1, 1.5)]}
        selected = observation(analysis, {"startSeconds": 0.5, "endSeconds": 1.5})
        self.assertEqual(selected["pitchFrameIndices"], [1, 2])
        self.assertEqual(selected["cyclogramColumnRange"], [1, 3])
        self.assertEqual(observation(analysis, {"startSeconds": 0})["cyclogramColumnRange"], [0, 5])
        with self.assertRaises(ValueError):
            observation(analysis, {"startSeconds": 0, "endSeconds": 3})

    def test_collection_reuses_import_and_analysis_and_can_rerun_by_resolved_id(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.wav"
            source.write_bytes(b"source audio identity")
            study = read_study(self.write_study(root, [
                {"id": "attack", "source": "source.wav", "crop": {"endSeconds": 0.5}},
                {"id": "body", "source": "source.wav", "crop": {"startSeconds": 0.5}},
            ]))
            calls = []
            analysis = {"referenceId": "imported", "audioPath": str(source), "durationSeconds": 1,
                        "lowConfidence": True, "trackingUsedFallback": True,
                        "pitchFrames": [{"timeSeconds": 0}, {"timeSeconds": 0.5}],
                        "cyclogram": {"columns": 3}}

            def request(payload):
                calls.append(payload)
                if payload["command"] == "listReferences":
                    return {"references": []}
                if payload["command"] == "importReference":
                    return {"referenceId": "imported"}
                if payload["command"] == "exportReferenceAnalysis":
                    Path(payload["path"]).write_text(json.dumps(analysis))
                    return analysis
                return {}

            report_path = collect_study(study, root / "first", request, True)
            report = json.loads(report_path.read_text())
            self.assertEqual(sum(call["command"] == "importReference" for call in calls), 1)
            self.assertEqual(sum(call["command"] == "exportReferenceAnalysis" for call in calls), 1)
            self.assertEqual(report["observations"][0]["analysisPath"], report["observations"][1]["analysisPath"])
            self.assertTrue(report["observations"][0]["trackingUsedFallback"])
            self.assertEqual(report["observations"][0]["labels"], {})
            calls.clear()
            collect_study(read_study(report_path), root / "second", request)
            self.assertFalse(any(call["command"] == "importReference" for call in calls))

    def test_original_source_reuses_byte_identical_uuid_library_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "original.wav"
            library_copy = root / "uuid.wav"
            source.write_bytes(b"same original audio bytes")
            library_copy.write_bytes(source.read_bytes())
            study = read_study(self.write_study(root, [{"id": "original", "source": "original.wav"}]))
            analysis = {"referenceId": "uuid", "audioPath": str(library_copy), "durationSeconds": 1,
                        "lowConfidence": False, "trackingUsedFallback": False,
                        "pitchFrames": [], "cyclogram": {"columns": 3}}

            def request(payload):
                if payload["command"] == "listReferences":
                    return {"references": [{"audioPath": str(library_copy), "referenceId": "uuid"}]}
                self.assertNotEqual(payload["command"], "importReference")
                if payload["command"] == "exportReferenceAnalysis":
                    Path(payload["path"]).write_text(json.dumps(analysis))
                    return analysis
                return {}

            report = json.loads(collect_study(study, root / "output", request).read_text())
            self.assertEqual(report["study"]["references"][0]["referenceId"], "uuid")

    def test_existing_artifacts_are_untouched_when_output_is_reused(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            report = output / "report.json"
            report.write_text("previous successful report")

            def request(payload):
                self.fail("Existing output must fail before any app command")

            with self.assertRaisesRegex(ValueError, "must be empty"):
                collect_study({"references": []}, output, request)
            self.assertEqual(report.read_text(), "previous successful report")


if __name__ == "__main__":
    unittest.main()
