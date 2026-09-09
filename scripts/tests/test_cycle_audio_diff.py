#!/usr/bin/env python3

import hashlib
import json
import math
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import cycle_audio_diff
import compare_cycle_audio


class CycleAudioDiffTest(unittest.TestCase):
    def write_wav(self, path, samples, sample_rate=48000):
        with wave.open(str(path), "wb") as destination:
            destination.setnchannels(1)
            destination.setsampwidth(2)
            destination.setframerate(sample_rate)
            destination.writeframes(b"".join(
                int(sample).to_bytes(2, "little", signed=True)
                for sample in samples
            ))

    def write_stage_capture(self, path, stages):
        records = []
        for index, (stage, primary, secondary) in enumerate(stages):
            raw_path = path.parent / f"{path.stem}-{index}.f32le"
            payload = struct.pack(
                f"<{len(primary) + len(secondary)}f",
                *(primary + secondary),
            )
            raw_path.write_bytes(payload)
            records.append({
                "stage": stage,
                "frameIndex": 0,
                "frontier": 37,
                "midiNote": 48,
                "channel": 0,
                "primary": "magnitude" if secondary else "samples",
                "primaryValueCount": len(primary),
                "secondary": "phase" if secondary else "",
                "secondaryValueCount": len(secondary),
                "rawPath": str(raw_path),
                "sha256": hashlib.sha256(payload).hexdigest(),
            })
        path.write_text(json.dumps({
            "schema": "cycle-spectral-stage-capture.v1",
            "targetFrameIndex": 0,
            "records": records,
        }), encoding="utf-8")

    def test_exact_sample_comparison_reports_first_difference(self):
        with tempfile.TemporaryDirectory() as directory:
            reference_path = Path(directory) / "reference.wav"
            equal_path = Path(directory) / "equal.wav"
            different_path = Path(directory) / "different.wav"
            self.write_wav(reference_path, [0, 100, -200, 300])
            self.write_wav(equal_path, [0, 100, -200, 300])
            self.write_wav(different_path, [0, 100, -201, 300])

            reference = cycle_audio_diff.read_wav(reference_path)
            equal = cycle_audio_diff.exact_sample_comparison(
                reference, cycle_audio_diff.read_wav(equal_path))
            different = cycle_audio_diff.exact_sample_comparison(
                reference, cycle_audio_diff.read_wav(different_path))

        self.assertTrue(equal["samplesEqual"])
        self.assertEqual(equal["differingSamples"], 0)
        self.assertFalse(different["samplesEqual"])
        self.assertEqual(different["differingSamples"], 1)
        self.assertEqual(different["firstMismatch"]["frame"], 2)

    def test_raw_float_capture_is_channel_major_and_exact(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.f32le"
            values = [0.25, -0.5, 0.75, -1.0]
            path.write_bytes(struct.pack("<4f", *values))

            capture = cycle_audio_diff.read_raw_f32(path, 48000, 2, 2)

        self.assertEqual(capture["channels"], [[0.25, -0.5], [0.75, -1.0]])
        self.assertTrue(cycle_audio_diff.exact_sample_comparison(
            capture, capture)["samplesEqual"])

    def test_alignment_reports_candidate_latency_and_gain(self):
        reference = [math.sin(2.0 * math.pi * index / 32.0) for index in range(1024)]
        candidate = [0.0] * 7 + [0.5 * value for value in reference[:-7]]

        aligned = cycle_audio_diff.alignment(reference, candidate, 16)

        self.assertEqual(aligned["lagSamples"], 7)
        self.assertAlmostEqual(aligned["correlation"], 1.0, places=12)
        self.assertAlmostEqual(
            cycle_audio_diff.fit_gain(aligned["reference"], aligned["candidate"]),
            2.0,
            places=12,
        )

    def test_cyclogram_is_stable_for_an_exact_period_signal(self):
        sample_rate = 48000
        midi_note = 69
        signal = [math.sin(2.0 * math.pi * 440.0 * index / sample_rate)
                  for index in range(sample_rate)]

        metrics, _ = cycle_audio_diff.cyclogram_metrics(
            cycle_audio_diff.cyclogram(signal, sample_rate, midi_note))

        self.assertEqual(metrics["rows"], 16)
        self.assertLess(metrics["maxNormalizedRowError"], 0.001)

    def test_alignment_prefers_minimum_equivalent_periodic_lag(self):
        reference = [math.sin(2.0 * math.pi * index / 32.0) for index in range(1024)]
        candidate = [0.5 * value for value in reference]

        aligned = cycle_audio_diff.alignment(reference, candidate, 64)

        self.assertEqual(aligned["lagSamples"], 0)
        self.assertAlmostEqual(aligned["maximumCorrelation"], 1.0, places=12)

    def test_fft_places_a_bin_centered_sine_in_one_positive_bin(self):
        signal = [math.sin(2.0 * math.pi * 3.0 * index / 32.0) for index in range(32)]

        magnitudes = [abs(value) for value in cycle_audio_diff.fft(signal)]

        self.assertAlmostEqual(magnitudes[3], 16.0, places=10)
        self.assertLess(max(magnitudes[1:3] + magnitudes[4:16]), 1.0e-10)

    def test_pitch_signature_exposes_a_subharmonic(self):
        sample_rate = 48000
        midi_note = 69
        signal = [math.sin(2.0 * math.pi * 220.0 * index / sample_rate)
                  for index in range(sample_rate)]

        signature = cycle_audio_diff.pitch_signature(signal, sample_rate, midi_note)

        self.assertGreater(signature["halfFrequency"], 0.99)
        self.assertGreater(signature["halfToFundamental"], 1000.0)

    def test_spectrum_difference_ignores_subfloor_quantization(self):
        reference = [1.0, 0.1, 1.0e-6]
        candidate = [1.0, 0.1, 0.0]

        difference = cycle_audio_diff.spectrum_difference(reference, candidate)

        self.assertAlmostEqual(difference, 0.0, places=12)

    def test_threshold_verdict_keeps_each_failure_visible(self):
        analysis = {
            "alignment": {"correlation": 0.99},
            "gainFit": {"normalizedResidual": 0.3},
            "spectrum": {"logMagnitudeRmseDb": 2.0},
            "cyclogram": {"meanRowNormalizedDifference": 0.1},
        }
        thresholds = {
            "correlationMin": 0.95,
            "gainMatchedResidualMax": 0.2,
            "spectrumRmseDbMax": 4.0,
            "cyclogramMeanRowDifferenceMax": 0.2,
        }

        verdict = compare_cycle_audio.threshold_verdict(analysis, thresholds)

        self.assertFalse(verdict["passed"])
        self.assertFalse(verdict["checks"]["gainMatchedResidual"])
        self.assertTrue(verdict["checks"]["correlation"])

    def test_cycle_v1_note_compensates_legacy_reference_offset(self):
        manifest = {"translation": {"legacyMidiReferenceOffset": -12}}

        self.assertEqual(compare_cycle_audio.cycle_v1_note(manifest, 48), 60)

    def test_threshold_verdict_can_require_raw_exact_samples(self):
        analysis = {
            "alignment": {"correlation": 1.0},
            "gainFit": {"normalizedResidual": 0.0},
            "spectrum": {"logMagnitudeRmseDb": 0.0},
            "cyclogram": {"meanRowNormalizedDifference": 0.0},
            "rawExact": {"samplesEqual": False},
        }
        thresholds = {
            "correlationMin": 1.0,
            "gainMatchedResidualMax": 0.0,
            "spectrumRmseDbMax": 0.0,
            "cyclogramMeanRowDifferenceMax": 0.0,
            "exactSamplesRequired": True,
        }

        verdict = compare_cycle_audio.threshold_verdict(analysis, thresholds)

        self.assertFalse(verdict["passed"])
        self.assertFalse(verdict["checks"]["exactSamples"])

    def test_stage_capture_comparison_reports_first_divergent_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reference = root / "v1.json"
            candidate = root / "v2.json"
            self.write_stage_capture(reference, [
                ("time-frame", [0.0, 1.0, 0.0, -1.0], []),
                ("forward-fft", [0.5, 0.25], [0.0, 1.0]),
            ])
            self.write_stage_capture(candidate, [
                ("time-frame", [0.0, 1.0, 0.0, -1.0], []),
                ("forward-fft", [0.5, 0.125], [0.0, 1.0]),
            ])

            comparison = compare_cycle_audio.compare_stage_captures(
                reference, candidate)

        self.assertFalse(comparison["samplesEqual"])
        self.assertEqual(comparison["firstUnequalStage"], "forward-fft")
        self.assertTrue(comparison["records"][0]["samplesEqual"])
        self.assertEqual(
            comparison["records"][1]["primary"]["firstMismatch"]["index"],
            1,
        )


if __name__ == "__main__":
    unittest.main()
