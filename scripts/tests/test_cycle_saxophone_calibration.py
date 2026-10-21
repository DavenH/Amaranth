import copy
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import cycle_saxophone_calibration as calibration
from cycle_reference_study import file_digest, read_study


class SaxophoneCalibrationTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        content = self.root / "content"
        self.directory = content / "studies/expressive-sax"
        self.directory.mkdir(parents=True)
        (content / "presets").symlink_to(calibration.DEFAULT_CALIBRATION.parents[2] / "presets", target_is_directory=True)
        for name in ("vcsl-calibration.json", "vcsl-objective-suite.json", "vcsl-withheld-suite.json"):
            shutil.copyfile(calibration.DEFAULT_CALIBRATION.parent / name, self.directory / name)
        self.path = self.directory / "vcsl-calibration.json"
        self.corpus = self.root / "corpus"
        self.corpus.mkdir()
        # Opaque source bytes ensure validation never quietly becomes an audio decoder.
        records = []
        for identifier in calibration.OBJECTIVE + calibration.WITHHELD:
            token, suffix = identifier.split("_Main_")[1].split("_", 1)
            articulation = "Non-Vibrato" if "_NV_" in identifier else ("Vibrato" if "_Vib_" in identifier else "Staccato")
            layer = re.search(r"vl(\d+)", suffix)
            variation = re.search(r"var(\d+)", suffix)
            source = self.corpus / (identifier + ".wav")
            source.write_bytes(identifier.encode())
            records.append({"id": identifier, "split": "objective" if identifier in calibration.OBJECTIVE else "reserved",
                "localPath": source.name, "sha256": file_digest(source),
                "sourceLabels": {"noteToken": token, "articulationFolder": articulation,
                    "velocityLayer": int(layer[1]) if layer else None,
                    "variationToken": int(variation[1]) if variation else None, "performerToken": "Brett"},
                "wave": {"sampleRate": 48000, "channels": 2, "sampleWidth": 3, "durationSeconds": 10}})
        (self.corpus / "LICENSE").write_text("Opaque licence identity fixture")
        self.write(self.corpus / "selection.json", {"commit": calibration.COMMIT, "files": [{"id": row["id"]} for row in records]})
        selection_sha, license_sha = [file_digest(self.corpus / name) for name in ("selection.json", "LICENSE")]
        manifest = {"status": "complete", "commit": calibration.COMMIT, "selectionSha256": selection_sha,
            "licence": {"identifier": "CC0-1.0", "sha256": license_sha}, "audioFiles": records,
            "objectiveSourceIds": calibration.OBJECTIVE, "reservedSourceIds": calibration.WITHHELD}
        self.write(self.corpus / "download-manifest.json", manifest)
        corpus_sha = file_digest(self.corpus / "download-manifest.json")
        self.change(self.path, lambda value: value.update(corpusManifestSha256=corpus_sha, selectionSha256=selection_sha))
        # Substitute test trust anchors, preserving real byte hashing and rejection behavior.
        for name, value in (("CORPUS_SHA", corpus_sha), ("SELECTION_SHA", selection_sha), ("LICENSE_SHA", license_sha)):
            patch = mock.patch.object(calibration, name, value)
            patch.start()
            self.addCleanup(patch.stop)

    def write(self, path, value):
        path.write_text(json.dumps(value))

    def change(self, path, mutate):
        value = json.loads(path.read_text())
        mutate(value)
        self.write(path, value)

    def load(self):
        return calibration.load_calibration(self.path, self.corpus)

    def test_fixed_parent_splits_and_native_performances_fit_the_total_budget(self):
        plan = self.load()
        self.assertEqual(len(plan["identities"]), 7)
        objective, withheld = [plan["stages"][stage] for stage in ("objective", "withheld")]
        self.assertEqual([case["id"] for case in objective["cases"]], calibration.OBJECTIVE)
        self.assertEqual([case["id"] for case in withheld["cases"]], calibration.WITHHELD)
        self.assertFalse({case["noteToken"] for case in objective["cases"]} & {case["noteToken"] for case in withheld["cases"]})
        self.assertEqual(2 * sum(stage["suite"]["renderCount"] for stage in plan["stages"].values()), 48)
        self.assertEqual(2 * sum(stage["suite"]["audioSeconds"] for stage in plan["stages"].values()), 132)
        self.assertEqual({case["standardMidi"] for case in objective["cases"]}, {48, 60, 72})
        self.assertTrue(all(case["standardMidi"] == 62 for case in withheld["cases"]))
        json.dumps(plan, allow_nan=False)

    def test_metadata_load_never_hashes_wavs_and_source_validation_is_stage_scoped(self):
        with mock.patch.object(calibration, "file_digest", wraps=file_digest) as digest:
            plan = self.load()
            self.assertFalse(any(str(call.args[0]).endswith(".wav") for call in digest.call_args_list))
            digest.reset_mock()
            identities = calibration.validate_stage_sources(plan, "objective")
            self.assertEqual(len(identities), 8)
            self.assertEqual({str(call.args[0]) for call in digest.call_args_list}, set(identities))
            self.assertFalse(set(identities) & {case["sourcePath"] for case in plan["stages"]["withheld"]["cases"]})

    def test_reserved_recording_tampering_is_checked_when_audit_starts(self):
        plan = self.load()
        reserved = Path(plan["stages"]["withheld"]["cases"][0]["sourcePath"])
        reserved.write_bytes(b"changed reserved recording")
        calibration.validate_stage_sources(self.load(), "objective")
        with self.assertRaisesRegex(ValueError, "source recording changed"):
            calibration.validate_stage_sources(plan, "withheld")
        with self.assertRaises(ValueError):
            calibration.validate_stage_sources(plan, "unknown")

    def test_changed_objective_source_and_pinned_metadata_are_rejected(self):
        plan = self.load()
        Path(plan["stages"]["objective"]["cases"][0]["sourcePath"]).write_bytes(b"changed objective recording")
        with self.assertRaises(ValueError):
            calibration.validate_stage_sources(plan, "objective")
        for name in ("download-manifest.json", "selection.json", "LICENSE"):
            path = self.corpus / name
            original = path.read_bytes()
            path.write_bytes(original + b" ")
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Pinned corpus metadata"):
                self.load()
            path.write_bytes(original)

    def test_source_and_candidate_declarations_cannot_expand_the_fixed_ablation(self):
        original = self.path.read_bytes()
        for mutate in (lambda value: value.update(sourceSha256="0" * 64),
                       lambda value: value.update(baseline="../../presets/alto-sax.cyclegraph"),
                       lambda value: value.update(missingCoverage=[]),
                       lambda value: value["candidate"].update(parameterId="level"),
                       lambda value: value["candidate"].update(value=0)):
            self.path.write_bytes(original)
            self.change(self.path, mutate)
            with self.assertRaises(ValueError):
                self.load()

    def test_schedule_rejects_confounded_pitch_rate_seed_or_extra_controller_motion(self):
        path = self.directory / "vcsl-objective-suite.json"
        original = path.read_bytes()
        for mutate in (lambda value: value["render"].update(sampleRate=44100),
                       lambda value: value["render"].update(randomSeed=42),
                       lambda value: value.update(repetitions=1),
                       lambda value: value["cases"][0]["events"][3].update(note=36),
                       lambda value: value["cases"][0]["events"][3].update(velocity=.9),
                       lambda value: value["cases"][0]["events"].append({"type": "controller", "timeMs": 500, "controller": 2, "value": 127}),
                       lambda value: value["cases"][0]["events"][-1].update(timeMs=2000),
                       lambda value: value["cases"][0].update(id=calibration.WITHHELD[0])):
            path.write_bytes(original)
            self.change(path, mutate)
            with self.assertRaises(ValueError):
                self.load()

    def test_reference_payload_uses_the_canonical_schema_and_retains_unknown_dynamics(self):
        plan = self.load()
        for stage, value in plan["stages"].items():
            path = self.root / (stage + "-references.json")
            self.write(path, value["referenceStudy"])
            self.assertEqual(read_study(path), value["referenceStudy"])
            for reference in value["referenceStudy"]["references"]:
                self.assertEqual(reference["labels"]["dynamics"]["value"], "unknown")
                self.assertTrue(all(label["origin"] == "inferred" for label in reference["labels"].values()))
            for case in value["cases"]:
                self.assertEqual(case["crop"]["startSeconds"], 0)
                self.assertLess(case["pitchWindow"]["endSeconds"], case["crop"]["endSeconds"])
                self.assertEqual(case["pitchVarianceTrusted"], case["articulation"] != "Staccato")

    def test_source_paths_cannot_escape_the_corpus_and_native_format_is_explicit(self):
        record = json.loads((self.corpus / "download-manifest.json").read_text())["audioFiles"][0]
        for mutate in (lambda value: value.update(localPath="../outside.wav"),
                       lambda value: value["wave"].update(sampleRate=44100),
                       lambda value: value["wave"].update(durationSeconds=.1)):
            changed = copy.deepcopy(record)
            mutate(changed)
            with self.assertRaises(ValueError):
                calibration.stage_case(changed, self.corpus.resolve())

    def test_pitch_ablation_proves_exact_parameter_only_change_and_preserves_inputs(self):
        graph = json.loads((calibration.DEFAULT_CALIBRATION.parent / "../../presets/saxophone-3.cyclegraph").read_text())
        before = copy.deepcopy(graph)
        changed = copy.deepcopy(graph)
        pitch = next(node for node in changed["nodes"] if node["id"] == "pitchEnvelope1")
        pitch["parameters"]["enabled"] = False
        calibration.assert_pitch_ablation(graph, changed)
        self.assertEqual(graph, before)
        self.assertEqual(calibration.pitch_ablation_edit(), {"nodeId": "pitchEnvelope1", "parameters": [{"id": "enabled", "value": "false"}]})
        for mutate in (lambda value: value["edges"].pop(),
                       lambda value: value["guides"][0].update(noise=.9),
                       lambda value: next(node for node in value["nodes"] if node["id"] == "pitchEnvelope1")["parameters"].update(enabled=0),
                       lambda value: next(node for node in value["nodes"] if node["id"] == "pitchEnvelope1")["model"].update(revision=99),
                       lambda value: next(node for node in value["nodes"] if node["id"] == "pitchEnvelope1")["parameters"].update(level=0)):
            altered = copy.deepcopy(changed)
            mutate(altered)
            with self.assertRaises(ValueError):
                calibration.assert_pitch_ablation(graph, altered)


if __name__ == "__main__":
    unittest.main()
