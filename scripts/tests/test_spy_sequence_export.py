import argparse
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from export_cycle_v2_spy_sequence import arguments, export_sequence, frame_states, main


class SpySequenceExportTest(unittest.TestCase):
    def test_exact_frame_times_consume_scheduled_values_and_ignore_notes(self):
        schedule = {"durationMs": 1000, "events": [
            {"timeMs": 0, "type": "controller", "controller": 1, "value": 10},
            {"timeMs": 0, "type": "noteOn", "note": 60},
            {"timeMs": 5, "type": "controller", "controller": 1, "value": 11},
            {"timeMs": 250, "type": "controller", "controller": 74, "value": 30},
            {"timeMs": 500, "type": "noteOff", "note": 60},
            {"timeMs": 500, "type": "controller", "controller": 1, "value": 90},
            {"timeMs": 1000, "type": "controller", "controller": 1, "value": 127}]}
        frames = list(frame_states(schedule, 4))
        self.assertEqual([frame[1] for frame in frames], [0, 0.25, 0.5, 0.75])
        self.assertEqual([frame[2][1] for frame in frames], [10, 11, 90, 90])
        self.assertEqual([frame[2][74] for frame in frames], [0, 30, 30, 30])
        self.assertEqual([frame[2][125] for frame in frames], [0, 0, 0, 0])
        self.assertEqual(len(list(frame_states({"durationMs": 1001, "events": []}, 30))), 31)
        self.assertEqual(len(list(frame_states({"durationMs": 1000, "events": []}, 29.97))), 30)

    def run_export(self, root, failure=None):
        preset = root / "test.cyclegraph"
        pattern = root / "test.cyclepattern"
        preset.write_text("{}")
        pattern.write_text(json.dumps({"version": 1, "sequence": {"durationSeconds": 1}}))
        args = argparse.Namespace(output_dir=root / "frames", preset=preset, pattern=pattern,
                                  spy="probe1", fps=4, width=160, height=96, note=48, gradient="Bullion")
        commands = []

        def send(command):
            commands.append(command)
            if command["command"] == "exportPatternSchedule":
                return {"durationMs": 1000, "scheduler": "PatternControlScheduler", "events": [
                    {"type": "controller", "timeMs": 0, "controller": 1, "value": 12},
                    {"type": "controller", "timeMs": 500, "controller": 1, "value": 99}]}
            if command["command"] == "openGraph":
                return {}
            if failure is not None and len(list(args.output_dir.glob("frame-*.png"))) == 2:
                raise failure
            Path(command["path"]).write_bytes(b"png")
            return {"gradient": "bullion", "midiNote": 48, "noteOffTime": 0.6, "contentRevision": "123"}

        result = export_sequence(args, send)
        return result, json.loads((args.output_dir / "manifest.json").read_text()), commands

    def test_export_retains_sources_resolved_settings_and_consecutive_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result, manifest, commands = self.run_export(root)
            self.assertEqual(result, 0)
            self.assertEqual(manifest["status"], "complete")
            self.assertEqual(manifest["gradient"], "bullion")
            self.assertEqual(manifest["completedFrames"], 4)
            self.assertEqual([frame["path"] for frame in manifest["frames"]],
                             [f"frame-{i:06d}.png" for i in range(4)])
            journal = [json.loads(line) for line in (root / "frames/frames.jsonl").read_text().splitlines()]
            self.assertEqual(journal, manifest["frames"])
            self.assertEqual((root / "frames/source-preset.cyclegraph").read_bytes(),
                             (root / "test.cyclegraph").read_bytes())
            render = [item for item in commands if item["command"] == "renderSpyFrame"]
            self.assertEqual([item["controllers"][1] for item in render], [12, 12, 99, 99])
            self.assertTrue(all(item["gradient"] == "Bullion" and item["midiNote"] == 48 for item in render))

    def test_failure_and_interrupt_keep_finished_frames_and_identify_failed_frame(self):
        for error, status, code in [(RuntimeError("write failed"), "failed", 1),
                                    (KeyboardInterrupt(), "interrupted", 130)]:
            with self.subTest(status=status), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result, manifest, _ = self.run_export(root, error)
                self.assertEqual(result, code)
                self.assertEqual(manifest["status"], status)
                self.assertEqual(manifest["failedFrame"], 2)
                self.assertEqual(manifest["completedFrames"], 2)
                self.assertEqual(len(list((root / "frames").glob("frame-*.png"))), 2)

    def test_listing_needs_no_export_arguments_and_cli_rejects_invalid_settings(self):
        self.assertTrue(arguments(["--list-gradients"]).list_gradients)
        for settings in [["--fps", "nan"], ["--fps", "0"], ["--width", "0"], ["--note", "128"]]:
            with self.subTest(settings=settings), self.assertRaises(SystemExit):
                arguments(["--list-gradients", *settings])

    def test_command_launches_scoped_coordinator_and_preserves_gradient_argument(self):
        with patch("export_cycle_v2_spy_sequence.subprocess.run") as run:
            run.return_value.returncode = 0
            self.assertEqual(main(["--list-gradients", "--no-build"]), 0)
            command = run.call_args.args[0]
            self.assertIn("--clean", command)
            self.assertIn("--no-build", command)
            self.assertIn("--list-gradients", command)


if __name__ == "__main__":
    unittest.main()
