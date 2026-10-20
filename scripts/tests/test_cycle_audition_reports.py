import copy
import json
import math
from pathlib import Path
import random
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_audition_analysis import analyze_case, level, rms_gains
from cycle_audition_study import load_study
from cycle_performance_suite import content_hash
from cycle_reference_study import file_digest
from report_cycle_auditions import generate_report, objective_summary, write_wav


class AuditionReportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.rate = 8000
        self.tone = [0.4 * math.sin(2 * math.pi * 500 * index / self.rate) for index in range(4000)]
        self.pitch = {"midiNote": 69 + 12 * math.log2(500 / 440), "origin": "observed",
                      "confidence": 0.99, "monophonic": True}

    def source(self, name, channels):
        path = self.root / f"{name}.wav"
        write_wav(path, channels, self.rate)
        return {"audioPath": str(path), "audioSha256": file_digest(path), "sampleRate": self.rate,
                "channels": len(channels), "crop": {"startSeconds": 0, "endSeconds": len(channels[0]) / self.rate},
                "lowConfidence": False, "trackingUsedFallback": False, "provenance": {}}

    def case(self, left=None, right=None, phases=None, pitch=True):
        return {"id": "compare", "partition": "objective",
                "a": self.source("a", left or [self.tone]), "b": self.source("b", right or [self.tone]),
                "pitch": self.pitch if pitch else None,
                "intervals": phases or [{"id": "whole", "role": "trajectory", "startSeconds": 0, "endSeconds": 0.5}]}

    def suite(self, name="captures", channels=None):
        directory = self.root / name
        directory.mkdir()
        wav = directory / "note-01.wav"
        write_wav(wav, channels or [self.tone], self.rate)
        (directory / "source-preset.cyclegraph").write_text('{"source":"saved"}')
        graph = {"graph": "effective"}
        (directory / "effective-preset.cyclegraph").write_text(json.dumps(graph))
        performance = {"sampleRate": self.rate, "durationMs": 500, "events": []}
        case = {"id": "note", "labels": {"articulation": "short"}, "performance": performance}
        document = {"schema": "cycle-performance-suite.v1", "cases": [{"id": "note"}]}
        value = {"schema": "cycle-performance-results.v1", "status": "complete", "ok": True,
                 "suiteSha256": content_hash(document), "suite": {"source": {"document": document}, "cases": [case]},
                 "engine": {"executableSha256": "historical-engine", "processIdentity": {"executable": "/gone/old-app"}},
                 "preset": {"sourcePath": "/gone/original-preset", "snapshot": "source-preset.cyclegraph",
                            "sourceSha256": file_digest(directory / "source-preset.cyclegraph"),
                            "effectiveGraphSha256": content_hash(graph), "finalGraphSha256": content_hash(graph)},
                 "cases": [{"id": "note", "labels": case["labels"], "performanceSha256": content_hash(performance),
                            "renders": [{"repetition": 1, "path": wav.name, "wavSha256": file_digest(wav),
                                         "sampleRate": self.rate, "channels": len(channels or [self.tone]), "frames": 4000}]}]}
        path = directory / "results.json"
        path.write_text(json.dumps(value))
        return path, value

    def reference(self, insecure=False):
        source = self.source("reference", [self.tone])
        analysis = {"referenceId": "test", "audioPath": source["audioPath"], "pitchState": "effective path",
                    "lowConfidence": insecure, "trackingUsedFallback": insecure,
                    "trackingInsecurity": 10 if insecure else 0, "durationSeconds": 0.5}
        analysis_path = self.root / "analysis.json"
        analysis_path.write_text(json.dumps(analysis))
        labels = {"technique": {"value": "growl", "origin": "inferred", "confidence": 0.4}}
        declared = {"id": "sample", "referenceId": "test", "labels": labels,
                    "crop": {"startSeconds": 0.05, "endSeconds": 0.45}}
        study = {"schemaVersion": 1, "references": [declared]}
        observed = {"id": "sample", "referenceId": "test", "labels": labels,
                    "observation": declared["crop"], "analysisPath": "analysis.json",
                    "analysisSha256": file_digest(analysis_path), "sourceAudioSha256": source["audioSha256"],
                    "lowConfidence": insecure, "trackingUsedFallback": insecure}
        report = {"schemaVersion": 1, "study": study, "studySha256": content_hash(study), "observations": [observed]}
        path = self.root / "reference-report.json"
        path.write_text(json.dumps(report))
        return path, report

    def manifest(self, a, b=None):
        value = {"schema": "cycle-audition-study.v1", "cases": [{"id": "evidence", "partition": "objective",
                 "a": a, "b": b or a, "pitch": self.pitch,
                 "intervals": [{"id": "body", "role": "sustain", "startSeconds": 0.05, "endSeconds": 0.3}]}]}
        path = self.root / "study.json"
        path.write_text(json.dumps(value))
        return path, value

    def test_slow_attack_and_release_are_visible_in_their_explicit_phases(self):
        def envelope(attack, release):
            return [value * min(1, index / self.rate / attack) * min(1, max(0, (0.5 - index / self.rate) / release))
                    for index, value in enumerate(self.tone)]
        phases = [{"id": "attack", "role": "attack", "startSeconds": 0, "endSeconds": 0.2},
                  {"id": "release", "role": "release", "startSeconds": 0.35, "endSeconds": 0.5}]
        evidence, _ = analyze_case(self.case([envelope(0.02, 0.02)], [envelope(0.2, 0.15)], phases))
        for phase in evidence["intervals"]:
            self.assertGreater(phase["raw"]["meanAbsoluteDifference"]["rms"], 0.05)
        self.assertEqual(evidence["alignment"]["mode"], "none")

    def test_identical_endpoints_do_not_hide_a_different_transition(self):
        changed = [value * (0.2 if 0.15 < index / self.rate < 0.35 else 1)
                   for index, value in enumerate(self.tone)]
        phases = [{"id": name, "role": "trajectory", "startSeconds": start, "endSeconds": end}
                  for name, start, end in (("start", 0, 0.1), ("middle", 0.15, 0.35), ("end", 0.4, 0.5))]
        evidence, _ = analyze_case(self.case(right=[changed], phases=phases))
        deltas = [phase["raw"]["meanAbsoluteDifference"]["rms"] for phase in evidence["intervals"]]
        self.assertEqual(deltas[0], 0)
        self.assertGreater(deltas[1], 0.2)
        self.assertEqual(deltas[2], 0)

    def test_added_harmonic_and_noise_have_distinct_spectral_evidence(self):
        harmonic = [value + 0.2 * math.sin(2 * math.pi * 1000 * index / self.rate)
                    for index, value in enumerate(self.tone)]
        evidence, _ = analyze_case(self.case(right=[harmonic]))
        difference = evidence["intervals"][0]["raw"]["meanAbsoluteDifference"]
        self.assertGreater(difference["centroidHz"], 50)
        self.assertGreater(difference["upperHarmonicBandFraction"], 0.1)
        rng = random.Random(29)
        noisy = [value + 0.2 * rng.uniform(-1, 1) for value in self.tone]
        evidence, _ = analyze_case(self.case(right=[noisy]))
        difference = evidence["intervals"][0]["raw"]["meanAbsoluteDifference"]
        self.assertGreater(difference["remainingBandFraction"], 0.05)
        self.assertGreater(difference["flatness"], 0.01)

    def test_antiphase_stereo_retains_energy_spectra_and_matching(self):
        evidence, _ = analyze_case(self.case(left=[self.tone, [-value for value in self.tone]]))
        self.assertAlmostEqual(evidence["levels"]["a"]["rms"], 0.4 / math.sqrt(2), places=6)
        self.assertTrue(evidence["rmsMatching"]["available"])
        self.assertAlmostEqual(evidence["intervals"][0]["raw"]["a"][0]["centroidHz"], 500, delta=1)
        self.assertEqual(evidence["intervals"][0]["raw"]["meanAbsoluteDifference"]["rms"], 0)

    def test_fixed_lag_is_bounded_and_raw_discrepancy_remains(self):
        rng = random.Random(21)
        signal = [rng.uniform(-0.4, 0.4) for _ in self.tone]
        delayed = [0] * 7 + signal[:-7]
        case = self.case([signal], [delayed], pitch=False)
        policy = {"maxLagMs": 1, "calibration": {"startSeconds": 0.002, "endSeconds": 0.025}}
        evidence, _ = analyze_case(case, policy)
        self.assertEqual(evidence["alignment"]["lagSamples"], 7)
        phase = evidence["intervals"][0]
        self.assertGreater(phase["raw"]["meanAbsoluteDifference"]["rms"], 0)
        self.assertEqual(phase["aligned"]["meanAbsoluteDifference"]["rms"], 0)
        self.assertLess(evidence["alignment"]["overlapSeconds"][1], 0.5)
        self.assertEqual(analyze_case(case)[0]["alignment"]["lagSamples"], 0)
        with self.assertRaises(ValueError):
            analyze_case(self.case(left=[[0] * 4000], right=[[0] * 4000]), policy)

    def test_global_rms_matching_preserves_attack_ratio_and_silence_is_unmatched(self):
        case = self.case(right=[[value * 0.5 for value in self.tone]])
        evidence, channels = analyze_case(case)
        gains = evidence["rmsMatching"]["gains"]
        self.assertAlmostEqual(gains[0], 0.5, places=5)
        self.assertAlmostEqual(gains[1], 1)
        before = level([channels[0][0][:500]])["rms"] / level([channels[0][0][500:1000]])["rms"]
        matched = [[value * gains[0] for value in channels[0][0]]]
        after = level([matched[0][:500]])["rms"] / level([matched[0][500:1000]])["rms"]
        self.assertAlmostEqual(before, after)
        silent = analyze_case(self.case(left=[[0] * 4000], right=[[0] * 4000]))[0]
        self.assertFalse(silent["rmsMatching"]["available"])
        self.assertTrue(all(math.isfinite(row["flatness"]) for row in silent["intervals"][0]["raw"]["a"]))

    def test_uncertain_inferred_polyphonic_or_unknown_pitch_stays_unpitched(self):
        variants = [None, {**self.pitch, "origin": "inferred"}, {**self.pitch, "confidence": 0.79},
                    {**self.pitch, "monophonic": False}]
        for pitch in variants:
            case = self.case()
            case["pitch"] = pitch
            evidence, _ = analyze_case(case)
            self.assertFalse(evidence["pitch"]["used"])
            self.assertNotIn("harmonicBandFraction", evidence["intervals"][0]["raw"]["a"][0])
        case = self.case()
        case["a"]["trackingUsedFallback"] = True
        self.assertFalse(analyze_case(case)[0]["pitch"]["used"])

    def test_legal_full_scale_pcm_quantization_and_unresolved_low_pitch(self):
        destination = self.root / "near-full-scale.wav"
        write_wav(destination, [[(2 ** 31 - 1) / 2 ** 31, -1]], self.rate)
        self.assertTrue(destination.is_file())
        with self.assertRaises(ValueError):
            write_wav(self.root / "clipped.wav", [[1.01]], self.rate)
        case = self.case()
        case["pitch"] = {**self.pitch, "midiNote": 0}
        evidence, _ = analyze_case(case)
        self.assertFalse(evidence["pitch"]["used"])
        self.assertIn("resolution", evidence["pitch"]["reason"])
        self.assertIn("centroidHz", evidence["intervals"][0]["raw"]["a"][0])

    def test_saved_suite_provenance_survives_without_live_engine_or_original_preset(self):
        path, _ = self.suite()
        study, _ = self.manifest({"performanceResults": str(path), "caseId": "note"})
        plan = load_study(study)
        source = plan["cases"][0]["a"]
        self.assertEqual(source["provenance"]["engine"]["executableSha256"], "historical-engine")
        self.assertEqual(source["provenance"]["performance"]["durationMs"], 500)
        self.assertEqual(source["provenance"]["case"]["labels"]["articulation"], "short")

    def test_artifact_hash_changes_and_incomplete_results_are_rejected(self):
        path, saved = self.suite()
        study, _ = self.manifest({"performanceResults": str(path), "caseId": "note"})
        for target in (path.parent / "source-preset.cyclegraph", path.parent / "effective-preset.cyclegraph",
                       path.parent / "note-01.wav"):
            original = target.read_bytes()
            target.write_bytes(original + b"changed")
            with self.subTest(target=target), self.assertRaises((ValueError, json.JSONDecodeError)):
                load_study(study)
            target.write_bytes(original)
        saved["status"] = "running"
        path.write_text(json.dumps(saved))
        with self.assertRaises(ValueError):
            load_study(study)

    def test_reference_crop_labels_and_authoritative_insecure_pitch_are_preserved(self):
        path, _ = self.reference(insecure=True)
        study, _ = self.manifest({"referenceStudy": str(path), "observationId": "sample"})
        plan = load_study(study)
        source = plan["cases"][0]["a"]
        self.assertEqual(source["crop"]["startSeconds"], 0.05)
        self.assertEqual(source["provenance"]["observation"]["labels"]["technique"]["origin"], "inferred")
        self.assertFalse(analyze_case(plan["cases"][0])[0]["pitch"]["used"])
        analysis_path = self.root / "analysis.json"
        analysis_path.write_text(analysis_path.read_text() + " ")
        with self.assertRaises(ValueError):
            load_study(study)

    def test_reference_copied_metadata_corruption_cannot_enable_harmonics(self):
        path, report = self.reference(insecure=True)
        study, _ = self.manifest({"referenceStudy": str(path), "observationId": "sample"})
        for change in (lambda v: v["observations"][0].update(lowConfidence=False, trackingUsedFallback=False),
                       lambda v: v["observations"][0].update(labels={}),
                       lambda v: v["study"].update(name="tampered")):
            corrupted = copy.deepcopy(report)
            change(corrupted)
            path.write_text(json.dumps(corrupted))
            with self.assertRaises(ValueError):
                load_study(study)

    def test_withheld_cases_never_enter_objective_summary_and_report_has_actual_players(self):
        path, _ = self.suite(channels=[self.tone, [-value for value in self.tone]])
        study, value = self.manifest({"performanceResults": str(path), "caseId": "note"})
        withheld = copy.deepcopy(value["cases"][0])
        withheld.update(id="reserved", partition="withheld")
        value["cases"].append(withheld)
        study.write_text(json.dumps(value))
        output = self.root / "report"
        destination = generate_report(load_study(study), output)
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(report["status"], "complete")
        self.assertIn("Withheld cases", destination.read_text())
        for case in report["cases"]:
            self.assertTrue((output / f"{case['id']}.svg").is_file())
            for paths in case["audio"].values():
                for name in paths.values():
                    self.assertEqual(file_digest(output / name), case["artifacts"][name])
                    self.assertIn(f"src='{name}'", destination.read_text())
        changed = copy.deepcopy(report["cases"])
        changed[1]["intervals"][0]["raw"]["meanAbsoluteDifference"]["rms"] = 99
        self.assertEqual(objective_summary(changed), report["objectiveSummary"])
        with self.assertRaises(ValueError):
            generate_report(load_study(study), output)

    def test_invalid_windows_partitions_alignment_and_budget_fail_before_report(self):
        path, _ = self.suite()
        study, original = self.manifest({"performanceResults": str(path), "caseId": "note"})
        changes = [lambda v: v["cases"][0]["intervals"][0].update(endSeconds=0.6),
                   lambda v: v["cases"][0].update(partition="maybe"),
                   lambda v: v.update(alignment={"maxLagMs": 11, "calibration": {"startSeconds": 0, "endSeconds": 0.1}}),
                   lambda v: v["cases"][0].update(pitch={**self.pitch, "confidence": float("nan")})]
        def exceed_window_budget(value):
            phase = {"id": "long", "role": "trajectory", "startSeconds": 0, "endSeconds": 0.5}
            value["cases"] = [{**copy.deepcopy(original["cases"][0]), "id": f"case-{index}",
                               "intervals": [{**phase, "id": f"phase-{phase_index}"} for phase_index in range(16)]}
                              for index in range(32)]
        changes.append(exceed_window_budget)
        for change in changes:
            value = copy.deepcopy(original)
            change(value)
            study.write_text(json.dumps(value))
            with self.assertRaises(ValueError):
                load_study(study)


if __name__ == "__main__":
    unittest.main()
