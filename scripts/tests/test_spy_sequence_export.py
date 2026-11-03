import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
from unittest.mock import patch
import zlib


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from export_cycle_v2_spy_sequence import arguments, encode_mp4, export_sequence, frame_states, main


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

    def run_export(self, root, failure=None, mp4=False):
        preset = root / "test.cyclegraph"
        pattern = root / "test.cyclepattern"
        preset.write_text("{}")
        pattern.write_text(json.dumps({"version": 1, "sequence": {"durationSeconds": 1}}))
        args = argparse.Namespace(output_dir=root / "frames", preset=preset, pattern=pattern,
                                  spy="probe1", fps=4, width=160, height=96, note=48, gradient="Bullion", mp4=mp4, mp4_bitrate=8, cursor="ripple", tail_seconds=0)
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

    def test_melody_captures_authored_pitches_with_fixed_reference_and_holds_tail_controllers(self):
        for override, cursor in [(None, "ripple"), (48, "none")]:
            with self.subTest(override=override), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                preset, pattern = root / "test.cyclegraph", root / "test.cyclepattern"
                preset.write_text("{}")
                pattern.write_text(json.dumps({"version": 1, "sequence": {}}))
                args = argparse.Namespace(output_dir=root / "frames", preset=preset, pattern=pattern,
                                          spy="default-output", fps=4, width=160, height=96,
                                          note=override, gradient=None, mp4=False, cursor=cursor, tail_seconds=0.5)
                commands = []
                events = [{"type": "noteOn", "timeMs": 0, "note": 60, "velocity": 90},
                          {"type": "controller", "timeMs": 0, "controller": 1, "value": 12},
                          {"type": "noteOn", "timeMs": 250, "note": 67, "velocity": 80},
                          {"type": "controller", "timeMs": 500, "controller": 1, "value": 99},
                          {"type": "noteOff", "timeMs": 750, "note": 60},
                          {"type": "noteOff", "timeMs": 1000, "note": 67}]
                voice = {"voiceId": "2", "midiNote": 67, "ageSeconds": 0.2,
                         "releaseAgeSeconds": 0, "durationSeconds": 1, "released": False}

                def send(command):
                    commands.append(command)
                    if command["command"] == "exportPatternSchedule":
                        return {"durationMs": 1000, "scheduler": "PatternControlScheduler", "events": events}
                    if command["command"] == "openGraph":
                        return {}
                    if command["command"] == "captureAudio":
                        Path(command["path"]).write_bytes(b"wav")
                        return {"path": command["path"], "sampleRate": 48000,
                                "playbackFrames": [{"sample": sample, "voices": [voice]}
                                                   for sample in command.get("playbackSamples", [])]}
                    Path(command["path"]).write_bytes(b"png")
                    return {"gradient": "bullion", "midiNote": override or 60,
                            "noteOffTime": 0.6, "contentRevision": "123", "playbackPositions": [0.2]}

                self.assertEqual(export_sequence(args, send), 0)
                capture = next(item for item in commands if item["command"] == "captureAudio")
                self.assertEqual(capture["keyScaleNote"], override or 60)
                self.assertEqual(capture["events"], events)
                self.assertEqual(capture["durationMs"], 1500)
                self.assertEqual(capture["voiceDurationSeconds"], 0)
                self.assertEqual(capture.get("playbackSamples", []),
                                 [0, 12000, 24000, 36000, 48000, 60000] if cursor != "none" else [])
                renders = [item for item in commands if item["command"] == "renderSpyFrame"]
                self.assertEqual(len(renders), 6)
                self.assertEqual([item["controllers"][1] for item in renders], [12, 12, 99, 99, 99, 99])
                self.assertTrue(all(item["midiNote"] == (override or 60) for item in renders))
                self.assertEqual(renders[1].get("playbackVoices", []), [voice] if cursor != "none" else [])
                manifest = json.loads((args.output_dir / "manifest.json").read_text())
                self.assertEqual(manifest["audio"]["path"], "audio.wav")
                self.assertEqual(manifest["keyScaleNote"], override or 60)

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
        self.assertEqual(arguments(["--list-gradients"]).cursor, "ripple")
        for settings in [["--fps", "nan"], ["--fps", "0"], ["--width", "0"], ["--note", "128"],
                         ["--mp4-bitrate", "nan"], ["--mp4-bitrate", "0"],
                         ["--tail-seconds", "-1"], ["--tail-seconds", "inf"]]:
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

    def test_mp4_success_and_failure_preserve_completed_frames_and_report_encoding_status(self):
        for error in [None, RuntimeError("ffmpeg failed"), KeyboardInterrupt()]:
            with self.subTest(error=error), tempfile.TemporaryDirectory() as directory:
                with patch("export_cycle_v2_spy_sequence.encode_mp4", side_effect=error) as encode:
                    result, manifest, _ = self.run_export(Path(directory), mp4=True)
                encode.assert_called_once_with(Path(directory) / "frames", 4, 4, 8)
                self.assertEqual(manifest["completedFrames"], 4)
                self.assertEqual(len(manifest["frames"]), 4)
                self.assertEqual(manifest["mp4"]["status"], manifest["status"])
                if error is None:
                    self.assertEqual(result, 0)
                    self.assertEqual(manifest["mp4"]["path"], "sequence.mp4")
                    self.assertEqual(manifest["mp4"]["targetBitrateMbps"], 8)
                else:
                    self.assertNotEqual(result, 0)
                    self.assertIsNone(manifest["failedFrame"])

    def test_mp4_flag_reaches_worker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            preset, pattern = root / "preset.cyclegraph", root / "pattern.cyclepattern"
            preset.touch()
            pattern.touch()
            with patch("export_cycle_v2_spy_sequence.subprocess.run") as run:
                run.return_value.returncode = 0
                main(["--preset", str(preset), "--pattern", str(pattern), "--output-dir", str(root / "frames"),
                      "--mp4", "--mp4-bitrate", "12", "--gradient", "Bullion"])
            worker = run.call_args.args[0]
            self.assertIn("--mp4", worker[worker.index("--"):])
            self.assertEqual(worker[worker.index("--mp4-bitrate") + 1], "12.0")
            self.assertEqual(worker[worker.index("--gradient") + 1], "Bullion")
            self.assertEqual(worker[worker.index("--cursor") + 1], "ripple")

    def test_failed_encoder_cleans_partial_video(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def fail(command, **_):
                self.assertEqual(command[command.index("-b:v") + 1], "8000000")
                self.assertNotIn("-crf", command)
                Path(command[-1]).write_bytes(b"partial")
                raise subprocess.CalledProcessError(1, command, stderr="encoder unavailable")

            with patch("export_cycle_v2_spy_sequence.subprocess.run", side_effect=fail):
                with self.assertRaisesRegex(RuntimeError, "encoder unavailable"):
                    encode_mp4(root, 30, 3)
            self.assertFalse((root / "sequence.partial.mp4").exists())
            self.assertFalse((root / "sequence.mp4").exists())

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "ffmpeg/ffprobe required")
    def test_real_mp4_keeps_frame_count_fractional_rate_and_pads_odd_dimensions(self):
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index in range(3):
                pixels = (b"\0" + bytes([index * 80, 40, 120]) * 3) * 5
                png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 3, 5, 8, 2, 0, 0, 0))
                       + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))
                (root / f"frame-{index:06d}.png").write_bytes(png)
            encode_mp4(root, 29.97, 3)
            result = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                                     "-show_entries", "stream=codec_name,width,height,r_frame_rate,nb_frames,pix_fmt",
                                     "-of", "json", str(root / "sequence.mp4")],
                                    check=True, capture_output=True, text=True)
            stream = json.loads(result.stdout)["streams"][0]
            self.assertEqual(stream["codec_name"], "h264")
            self.assertEqual((stream["width"], stream["height"]), (4, 6))
            self.assertEqual(stream["r_frame_rate"], "2997/100")
            self.assertEqual(stream["nb_frames"], "3")
            self.assertEqual(stream["pix_fmt"], "yuv420p")
            self.assertEqual(len(list(root.glob("frame-*.png"))), 3)
            with wave.open(str(root / "audio.wav"), "wb") as recording:
                recording.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
                recording.writeframes(b"\0" * (round(3 / 29.97 * 48000) * 4))
            (root / "sequence.mp4").unlink()
            encode_mp4(root, 29.97, 3)
            probe = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-of", "json",
                                    str(root / "sequence.mp4")], check=True, capture_output=True, text=True)
            streams = json.loads(probe.stdout)["streams"]
            self.assertEqual([stream["codec_name"] for stream in streams], ["h264", "aac"])
            self.assertAlmostEqual(float(streams[0]["duration"]), float(streams[1]["duration"]), delta=0.002)
            self.assertTrue((root / "audio.wav").is_file())


if __name__ == "__main__":
    unittest.main()
