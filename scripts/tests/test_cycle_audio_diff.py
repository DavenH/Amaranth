#!/usr/bin/env python3

import math
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import cycle_audio_diff
import compare_cycle_audio


class CycleAudioDiffTest(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
