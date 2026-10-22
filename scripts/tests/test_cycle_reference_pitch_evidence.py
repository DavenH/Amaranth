"""Protect pitch evidence as a reduction of exported, confidence-labelled frames."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_reference_pitch_evidence import MAX_FRAMES, pitch_evidence, plot_pitch_evidence


def analysis(cents=(0, 0, 0, 0, 0)):
    return {"durationSeconds": 2, "sampleRate": 48000, "trackingInsecurity": 2.5,
            "lowConfidence": False, "trackingUsedFallback": False,
            "pitchState": "effective path including library edits",
            "pitchFrames": [{"timeSeconds": index * .1, "frequencyHz": 440 * 2 ** (value / 1200)}
                            for index, value in enumerate(cents)]}


class ReferencePitchEvidenceTest(unittest.TestCase):
    def test_constant_pitch_has_zero_spread_and_keeps_exported_times_and_state(self):
        source = analysis()
        original = copy.deepcopy(source)
        summary = pitch_evidence(source, .1, .5, 69)
        self.assertTrue(summary["available"])
        self.assertEqual(summary["frameCount"], 4)
        self.assertEqual(summary["medianHz"], 440)
        for key in ("medianCents", "lowCents", "highCents", "spreadCents"):
            self.assertEqual(summary[key], 0)
        self.assertEqual([frame["timeSeconds"] for frame in summary["frames"]], [.1, .2, .30000000000000004, .4])
        self.assertEqual(summary["pitchState"], source["pitchState"])
        self.assertEqual(summary["trackingInsecurity"], 2.5)
        self.assertIn("not raw tracker", summary["evidence"])
        self.assertEqual(source, original)

    def test_offset_uses_standard_midi_and_does_not_remove_detuning(self):
        summary = pitch_evidence(analysis((50,) * 5), 0, 1, 69)
        self.assertAlmostEqual(summary["medianCents"], 50)
        self.assertAlmostEqual(summary["medianHz"], 440 * 2 ** (50 / 1200))
        octave = pitch_evidence(analysis(), 0, 1, 57)
        self.assertAlmostEqual(octave["medianCents"], 1200)

    def test_percentiles_measure_variability_without_changing_elapsed_time(self):
        source = analysis(range(-100, 101, 10))
        source["durationSeconds"] = 3
        summary = pitch_evidence(source, 0, 3, 69)
        self.assertAlmostEqual(summary["lowCents"], -90)
        self.assertAlmostEqual(summary["highCents"], 90)
        self.assertAlmostEqual(summary["spreadCents"], 180)
        self.assertAlmostEqual(summary["medianCents"], 0)
        self.assertEqual(summary["frames"][-1]["timeSeconds"], 2)

    def test_crop_is_start_inclusive_end_exclusive_and_never_invents_frames(self):
        selected = pitch_evidence(analysis(), .1, .4, 69)
        self.assertEqual(selected["frameCount"], 3)
        self.assertEqual(selected["frames"][0]["timeSeconds"], .1)
        self.assertLess(selected["frames"][-1]["timeSeconds"], .4)
        empty = pitch_evidence(analysis(), .8, 1, 69)
        self.assertFalse(empty["available"])
        self.assertEqual(empty["frameCount"], 0)
        self.assertIsNone(empty["spreadCents"])
        self.assertFalse(pitch_evidence(analysis(), 0, .2, 69)["available"])

    def test_low_confidence_and_fallback_keep_flags_and_frames_but_suppress_statistics(self):
        for flag in ("lowConfidence", "trackingUsedFallback"):
            source = analysis()
            source[flag] = True
            summary = pitch_evidence(source, 0, 1, 69)
            self.assertFalse(summary["available"])
            self.assertTrue(summary[flag])
            self.assertEqual(len(summary["frames"]), 5)
            self.assertIn("suppressed", summary["reason"])
            for key in ("medianHz", "medianCents", "lowCents", "highCents", "spreadCents"):
                self.assertIsNone(summary[key])

    def test_invalid_intervals_notes_frequencies_times_flags_and_oversized_exports_fail(self):
        for start, end, note in ((-.1, 1, 69), (0, 3, 69), (1, 1, 69), (0, 1, True),
                                 (0, 1, 128), (0, float("nan"), 69)):
            with self.subTest(interval=(start, end, note)), self.assertRaises(ValueError):
                pitch_evidence(analysis(), start, end, note)
        for mutate in (
                lambda source: source.update(lowConfidence=0),
                lambda source: source.pop("trackingUsedFallback"),
                lambda source: source.update(trackingInsecurity=float("inf")),
                lambda source: source.update(pitchFrames=[{}] * (MAX_FRAMES + 1)),
                lambda source: source["pitchFrames"][0].update(frequencyHz=0),
                lambda source: source["pitchFrames"][0].update(frequencyHz=float("nan")),
                lambda source: source["pitchFrames"][0].update(frequencyHz=48000),
                lambda source: source["pitchFrames"][1].update(timeSeconds=0),
                lambda source: source["pitchFrames"][-1].update(timeSeconds=float("inf")),
                lambda source: source.pop("pitchState")):
            source = analysis()
            mutate(source)
            with self.assertRaises(ValueError):
                pitch_evidence(source, .1, .4, 69)

    def test_plots_preserve_case_provenance_partitions_and_original_times(self):
        summary = pitch_evidence(analysis(), .1, .5, 69)
        flagged = pitch_evidence({**analysis(), "lowConfidence": True}, .1, .5, 69)
        case = {"id": "source-body", "partition": "withheld", "standardMidiInferred": 69,
                "source": flagged, "baseline": summary, "candidate": copy.deepcopy(summary),
                "provenance": {"sourceSha256": "source-hash", "sourceCropStartSeconds": .1}}
        from matplotlib.axes import Axes
        original_plot, plotted = Axes.plot, []
        def capture(axis, x, y, **kwargs):
            plotted.append((list(x), list(y)))
            return original_plot(axis, x, y, **kwargs)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "pitch"
            with patch.object(Axes, "plot", capture):
                result = plot_pitch_evidence([case], output)
            saved = json.loads((output / "pitch-evidence.json").read_text())
            self.assertEqual(saved, result)
            self.assertEqual(saved["cases"], [case])
            self.assertEqual(plotted[0], ([], []))
            self.assertEqual(plotted[1][0], [frame["timeSeconds"] for frame in summary["frames"]])
            for extension in ("png", "svg"):
                self.assertGreater((output / ("source-body." + extension)).stat().st_size, 100)
            before = (output / "pitch-evidence.json").read_bytes()
            with self.assertRaisesRegex(ValueError, "already exists"):
                plot_pitch_evidence([case], output)
            self.assertEqual((output / "pitch-evidence.json").read_bytes(), before)

    def test_invalid_plot_inputs_fail_before_creating_output(self):
        summary = pitch_evidence(analysis(), 0, 1, 69)
        case = {"id": "body", "partition": "objective", "standardMidiInferred": 69,
                "source": summary, "baseline": summary, "candidate": summary}
        for cases in ([], [case, case], [{**case, "id": "../outside"}],
                      [{**case, "partition": "unknown"}], [{**case, "standardMidiInferred": 68}],
                      [{**case, "provenance": {"invalid": float("nan")}}]):
            with tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "pitch"
                with self.assertRaises(ValueError):
                    plot_pitch_evidence(cases, output)
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
