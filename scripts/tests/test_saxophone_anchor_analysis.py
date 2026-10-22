"""Protect harmonic authoring measurements and original stereo-level semantics."""

import sys
import json
import tempfile
import unittest
import wave
from pathlib import Path

try:
    import numpy as np
except ImportError:
    np = None

if np is not None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from cycle_reference_study import file_digest
    from cycle_saxophone_anchor_analysis import (bridge_measurement, compare_render, energy_envelope, half_lattice_profile, harmonic_phase,
                                                spectral_trace, summarize_interval)


@unittest.skipIf(np is None, "Optional offline analysis requires NumPy")
class AnchorMeasurementsTest(unittest.TestCase):
    def test_known_harmonics_preserve_stereo_energy_and_global_gain(self):
        rate, frequency = 48000, 293.7
        times = np.arange(rate) / rate
        signal = (np.sin(2 * np.pi * frequency * times)
                  + .5 * np.sin(4 * np.pi * frequency * times)
                  + .2 * np.sin(6 * np.pi * frequency * times))
        channels = np.array([signal, -signal])
        path = [{"timeSeconds": 0, "frequencyHz": frequency},
                {"timeSeconds": 1, "frequencyHz": frequency}]
        trace = spectral_trace(channels, rate, path)
        measured = summarize_interval(trace, .2, .8)
        scaled = summarize_interval(spectral_trace(channels * .25, rate, path), .2, .8)
        self.assertAlmostEqual(measured["harmonicRatiosToFundamental"][1], .5, delta=.002)
        self.assertAlmostEqual(measured["harmonicRatiosToFundamental"][2], .2, delta=.002)
        self.assertAlmostEqual(measured["spectralRms"], np.sqrt((1 + .25 + .04) / 2), delta=.002)
        self.assertLess(measured["remainingBandFraction"], .001)
        self.assertAlmostEqual(scaled["spectralRms"], measured["spectralRms"] * .25, places=10)
        np.testing.assert_allclose(scaled["harmonicRatiosToFundamental"], measured["harmonicRatiosToFundamental"])
        _, rms = energy_envelope(channels, rate)
        self.assertAlmostEqual(float(np.sqrt(np.mean(rms ** 2))), np.sqrt(np.mean(signal ** 2)), places=10)

    def test_phase_origin_is_removed_from_known_chirped_harmonics(self):
        rate = 48000
        times = np.arange(rate) / rate
        theta = 2 * np.pi * (293.7 * times + .025 * times ** 2)
        phases = np.array([.3, -.8, 1.2])
        harmonics = np.arange(1, 4)
        signal = np.sum(np.cos(theta[:, None] * harmonics + phases) * [1, .5, .2], axis=1)
        path = [{"timeSeconds": 0, "frequencyHz": 293.7},
                {"timeSeconds": 1, "frequencyHz": 293.75}]
        measured = harmonic_phase(signal, rate, path, .2, .8, harmonic_count=3)
        expected = np.angle(np.exp(1j * (phases - harmonics * phases[0])))
        np.testing.assert_allclose(measured["relativePhaseRadians"], expected, atol=1e-8)
        fundamental = np.angle(np.exp(1j * (theta[round(.2 * rate)] + phases[0])))
        self.assertAlmostEqual(measured["fundamentalPhaseRadians"], fundamental, places=8)

    def test_known_half_harmonics_are_distinct_from_main_integer_basis(self):
        rate, frequency = 48000, 293.7
        times = np.arange(round(.1 * rate)) / rate
        theta = 2 * np.pi * frequency * times
        signal = (.1 * np.cos(theta + .3) + .02 * np.cos(theta / 2 + .4)
                  + .03 * np.cos(theta * 1.5 - .2))
        path = [{"timeSeconds": 0, "frequencyHz": frequency},
                {"timeSeconds": .1, "frequencyHz": frequency}]
        measured = half_lattice_profile(signal[None, :], rate, path, 0, .05, harmonic_count=8)
        np.testing.assert_allclose(measured["channelPeakAmplitudes"][0][:3], [.02, .1, .03], atol=1e-10)
        self.assertAlmostEqual(measured["phaseRelativeToLeftMainRadians"][0][2], -.65, places=8)
        self.assertLess(measured["channelWeightedResidualRms"][0], 1e-10)

    def test_render_comparison_uses_one_global_gain_and_reports_missing_tail(self):
        rate = 48000
        times = np.arange(round(2.5 * rate)) / rate
        signal = (np.sin(2 * np.pi * 293.7 * times)
                  + .5 * np.sin(4 * np.pi * 293.7 * times)) * .1
        integers = np.round(signal * 32768).astype("<i2")
        path = [{"timeSeconds": 0, "frequencyHz": 293.7},
                {"timeSeconds": 2.5, "frequencyHz": 293.7}]
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, candidate, evidence = (directory / name for name in ("source.wav", "half.wav", "pitch.json"))
            for destination, values in ((source, integers), (candidate, (integers / 2).astype("<i2"))):
                with wave.open(str(destination), "wb") as output:
                    output.setparams((1, 2, rate, len(values), "NONE", "not compressed"))
                    output.writeframes(values.tobytes())
            evidence.write_text(json.dumps({"lowConfidence": False, "trackingUsedFallback": False,
                                            "pitchFrames": path}))
            decoded = integers.astype(float)[None, :] / 32768
            trace = spectral_trace(decoded, rate, path)
            anchor = {"id": "known-tones", "sourcePath": str(source), "sourceSha256": file_digest(source),
                      "pitchAnalysisPath": str(evidence), "pitchAnalysisSha256": file_digest(evidence),
                      "durationSeconds": 2.5,
                      "spectralTargets": {"body": summarize_interval(trace, .5, 1.5),
                                          "tail": summarize_interval(trace, 2.1, 2.4)},
                      "timings": {"attack10PercentSeconds": .005, "attack50PercentSeconds": .005,
                                  "attack90PercentSeconds": .005, "selectedBodyEndSeconds": 2,
                                  "selectedTailStartSeconds": 2.1, "tailMinus6DbSeconds": None,
                                  "tailMinus20DbSeconds": None, "tailMinus40DbSeconds": None}}
            identity = compare_render(anchor, source)
            self.assertAlmostEqual(identity["globalGain"], 1)
            self.assertAlmostEqual(identity["intervals"]["body"]["globalGainMatchedHarmonicMeanAbsoluteDbError"], 0)
            bridge = bridge_measurement(source, start=.4, end=2)
            self.assertAlmostEqual(bridge["channelEnergyRms"], np.sqrt(.1 ** 2 * (1 + .25) / 2), delta=.001)
            self.assertEqual(bridge["silent10msFrames"], 0)
            self.assertEqual(len(bridge["envelope10ms"]), 160)
            self.assertAlmostEqual(np.linalg.norm(bridge["normalizedHarmonicShape"]), 1)
            half = compare_render(anchor, candidate)
            self.assertAlmostEqual(half["renderRmsErrorDb"], -6.0206, delta=.003)
            self.assertAlmostEqual(half["globalGain"], 2, delta=.001)
            self.assertLess(half["intervals"]["body"]["globalGainMatchedHarmonicMeanAbsoluteDbError"], .003)
            with wave.open(str(candidate), "wb") as output:
                output.setparams((1, 2, rate, rate, "NONE", "not compressed"))
                output.writeframes(integers[:rate].tobytes())
            short = compare_render(anchor, candidate)
            self.assertFalse(short["intervals"]["tail"]["available"])


if __name__ == "__main__":
    unittest.main()
