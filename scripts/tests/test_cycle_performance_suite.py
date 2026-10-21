#!/usr/bin/env python3

import copy
import hashlib
import json
import sys
import tempfile
import unittest
import wave
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from cycle_performance_suite import content_hash, load_suite
from render_cycle_v2_performance_suite import render_suite, wav_identity


ROOT = Path(__file__).resolve().parents[2]


class PerformanceSuiteTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.path = self.directory / "suite.json"
        self.preset = self.directory / "preset.cyclegraph"
        self.preset.write_text('{"format": "test-source"}\n', encoding="utf-8")
        self.value = {
            "schema": "cycle-performance-suite.v1", "id": "contract", "repetitions": 2,
            "cases": [{"id": "short", "note": 60, "velocity": 0.3,
                       "noteDurationMs": 30, "durationMs": 100}],
        }

    def plan(self, value=None, repetitions=None):
        self.path.write_text(json.dumps(self.value if value is None else value), encoding="utf-8")
        return load_suite(self.path, repetitions)

    def test_explicit_capture_settings_preserve_attack_release_and_seed(self):
        plan = self.plan()
        request = plan["cases"][0]["performance"]
        self.assertEqual((request["sampleRate"], request["channels"], request["randomSeed"]), (44100, 2, 1729))
        self.assertEqual(request["events"], [
            {"type": "noteOn", "timeMs": 0, "channel": 1, "note": 60, "velocity": 0.3},
            {"type": "noteOff", "timeMs": 30, "channel": 1, "note": 60},
        ])
        self.assertEqual(plan["renderCount"], 2)
        self.assertEqual(plan["audioSeconds"], 0.2)

    def test_performance_fingerprint_covers_sound_inputs_but_not_labels_or_paths(self):
        original = self.plan()["cases"][0]["performanceSha256"]
        for key, value in (("randomSeed", 42), ("sampleRate", 48000),
                           ("outputGain", 0.5), ("voiceDurationSeconds", 2), ("blockSize", 127)):
            with self.subTest(key=key):
                changed = copy.deepcopy(self.value)
                changed["render"] = {key: value}
                self.assertNotEqual(original, self.plan(changed)["cases"][0]["performanceSha256"])
        changed = copy.deepcopy(self.value)
        changed["cases"][0].update(id="renamed", labels={"intent": "another description"})
        self.assertEqual(original, self.plan(changed)["cases"][0]["performanceSha256"])

    def test_timed_controls_pressure_and_repeats_preserve_equal_time_order(self):
        events = [
            {"type": "noteOff", "timeMs": 90, "note": 60},
            {"type": "controller", "timeMs": 0, "controller": 74, "value": 3},
            {"type": "channelPressure", "timeMs": 0, "value": 10},
            {"type": "noteOn", "timeMs": 0, "note": 60, "velocity": 0.5},
            {"type": "controller", "timeMs": 40, "controller": 74, "value": 100},
            {"type": "noteOff", "timeMs": 50, "note": 60},
            {"type": "noteOn", "timeMs": 60, "note": 60, "velocity": 0.5},
        ]
        self.value["cases"] = [{"id": "expression", "durationMs": 100, "events": events}]
        normalized = self.plan()["cases"][0]["performance"]["events"]
        self.assertEqual([item["type"] for item in normalized[:3]],
                         ["controller", "channelPressure", "noteOn"])
        self.assertEqual([item["timeMs"] for item in normalized], [0, 0, 0, 40, 50, 60, 90])

    def test_shared_factory_pattern_retains_exact_input_for_the_app_scheduler(self):
        pattern = ROOT / "cycle-v2/content/patterns/factory-basic-keys.cyclepattern"
        self.value["cases"] = [{"id": "keys", "pattern": str(pattern), "tailMs": 500}]
        case = self.plan()["cases"][0]
        request = case["performance"]
        self.assertEqual(request["durationMs"], 4500)
        self.assertNotIn("events", request)
        self.assertIsNone(case["performanceSha256"])
        self.assertEqual(case["pattern"]["document"], json.loads(pattern.read_bytes()))
        self.assertEqual(case["pattern"]["sha256"], hashlib.sha256(pattern.read_bytes()).hexdigest())

    def test_invalid_values_fail_instead_of_being_clamped_by_capture(self):
        variants = []
        for render in ({"sampleRate": 0}, {"blockSize": 8}, {"channels": 3},
                       {"randomSeed": True}, {"outputGain": float("nan")},
                       {"voiceDurationSeconds": float("inf")}, {"sampleRate": 10 ** 400},
                       {"ratePolicy": "unknown"}, {"ouputGain": 0.5}):
            value = copy.deepcopy(self.value)
            value["render"] = render
            variants.append(value)
        for changes in ({"velocity": 0}, {"note": 128}, {"noteDurationMs": 100},
                        {"durationMs": 60001}, {"channel": 0}, {"duration": 100}):
            value = copy.deepcopy(self.value)
            value["cases"][0].update(changes)
            variants.append(value)
        for value in variants:
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    self.plan(value)

    def test_invalid_events_and_ambiguous_schedules_fail_before_dispatch(self):
        for bad in (
                {"type": "pitchWheel", "timeMs": 10, "value": 80},
                {"type": "noteOn", "timeMs": 100, "note": 60},
                {"type": "controller", "timeMs": 10, "controller": 74, "value": 128},
                {"type": "channelPressure", "timeMs": -1, "value": 60},
                {"type": "noteOff", "timeMs": 10, "note": 60, "time": 11}):
            self.value["cases"] = [{"id": "bad", "durationMs": 100, "events": [bad]}]
            with self.subTest(event=bad), self.assertRaises(ValueError):
                self.plan()
        self.value["cases"][0].update(note=60)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.plan()

    def test_aggregate_work_and_event_capacity_are_bounded(self):
        self.value["budget"] = {"maxRenders": 1}
        with self.assertRaisesRegex(ValueError, "budget"):
            self.plan()
        self.value["budget"] = {"maxAudioSeconds": 0.1}
        with self.assertRaisesRegex(ValueError, "budget"):
            self.plan()
        self.value.pop("budget")
        with self.assertRaises(ValueError):
            self.plan(repetitions=17)
        self.value["cases"] = [{"id": "overflow", "durationMs": 100,
                                "events": [{"type": "allNotesOff", "timeMs": 0}] * 2049}]
        with self.assertRaisesRegex(ValueError, "2048"):
            self.plan()

    def test_unique_safe_case_ids_prevent_artifact_collisions(self):
        self.value["cases"].append(copy.deepcopy(self.value["cases"][0]))
        with self.assertRaisesRegex(ValueError, "unique"):
            self.plan()
        self.value["cases"].pop()
        self.value["cases"][0]["id"] = "../escaped"
        with self.assertRaises(ValueError):
            self.plan()

    def test_long_shared_patterns_are_not_silently_cropped(self):
        pattern = self.directory / "too-long.cyclepattern"
        document = {"version": 1, "id": "test", "name": "test",
                    "sequence": {"durationSeconds": 90, "notes": [], "controls": []}}
        pattern.write_text(json.dumps(document), encoding="utf-8")
        self.value["cases"] = [{"id": "long", "pattern": str(pattern)}]
        with self.assertRaisesRegex(ValueError, "durationMs"):
            self.plan()

    def fake_session(self, change_audio=False, change_graph=False, fail_capture=False):
        calls = []
        capture_count = 0
        graph_count = 0

        def send(payload, **kwargs):
            nonlocal capture_count, graph_count
            calls.append(payload)
            kind = payload["command"]
            data = {}
            if kind == "exportGraph":
                graph_count += 1
                Path(payload["path"]).write_text(json.dumps({
                    "model": "effective graph", "revision": graph_count if change_graph else 1,
                }), encoding="utf-8")
            elif kind == "captureAudio":
                capture_count += 1
                if fail_capture:
                    return {"ok": True, "result": {"ok": False, "message": "prepare failed"}}
                frames = round(payload["durationMs"] * payload["sampleRate"] / 1000)
                sample = b"\x01\x00\x00" if change_audio and capture_count > 1 else b"\x00\x01\x00"
                with wave.open(payload["path"], "wb") as output:
                    output.setparams((payload["channels"], 3, payload["sampleRate"], 0, "NONE", "not compressed"))
                    output.writeframes(sample * frames * payload["channels"])
                data = {"peak": 0.1, "rms": 0.05, "renderer": "test-artifact-writer"}
            return {"ok": True, "result": {"ok": True, "data": data}}

        return send, calls

    def run_plan(self, **kwargs):
        send, calls = self.fake_session(**kwargs)
        output = self.directory / "artifacts"
        result = render_suite(self.plan(), self.preset, output, send, {"identity": "contract"})
        return result, calls, output

    def test_artifacts_identify_exact_source_effective_graph_and_repeated_pcm(self):
        self.value["cases"].append({"id": "second", "durationMs": 120, "note": 64, "noteDurationMs": 50})
        result, calls, output = self.run_plan()
        self.assertTrue(result["ok"])
        self.assertEqual(result["preset"]["sourceSha256"], hashlib.sha256(self.preset.read_bytes()).hexdigest())
        self.assertEqual((output / "source-preset.cyclegraph").read_bytes(), self.preset.read_bytes())
        graph = json.loads((output / "effective-preset.cyclegraph").read_bytes())
        self.assertEqual(result["preset"]["effectiveGraphSha256"], content_hash(graph))
        captures = [item for item in calls if item["command"] == "captureAudio"]
        self.assertEqual([Path(item["path"]).name for item in captures],
                         ["short-01.wav", "second-01.wav", "short-02.wav", "second-02.wav"])
        self.assertTrue(all(item["repeatabilityVerified"] for item in result["cases"]))
        self.assertEqual(result, json.loads((output / "results.json").read_bytes()))
        self.assertEqual(len((output / "commands.jsonl").read_text().splitlines()), len(calls))

    def test_runner_uses_the_exported_pattern_schedule_without_rebuilding_cc_points(self):
        pattern = self.directory / "expression.cyclepattern"
        sequence = {"durationSeconds": 0.1, "notes": [],
                    "controls": [{"controller": 74, "value": 0, "timeSeconds": 0},
                                 {"controller": 74, "value": 100, "timeSeconds": 0.1}]}
        pattern.write_text(json.dumps({"version": 1, "sequence": sequence}), encoding="utf-8")
        self.value["cases"] = [{"id": "pattern", "pattern": str(pattern)}]
        expanded = [{"type": "controller", "timeMs": 0, "channel": 1, "controller": 74, "value": 0},
                    {"type": "controller", "timeMs": 50, "channel": 1, "controller": 74, "value": 50},
                    {"type": "controller", "timeMs": 100, "channel": 1, "controller": 74, "value": 100}]
        send, calls = self.fake_session()

        def with_schedule(payload, **kwargs):
            if payload["command"] == "exportPatternSchedule":
                self.assertEqual(payload["sequence"], sequence)
                return {"ok": True, "result": {"ok": True, "data": {
                    "scheduler": "PatternControlScheduler", "events": expanded,
                }}}
            return send(payload, **kwargs)

        plan = self.plan()
        output = self.directory / "pattern-artifacts"
        result = render_suite(plan, self.preset, output, with_schedule, {})
        self.assertTrue(result["ok"])
        captures = [item for item in calls if item["command"] == "captureAudio"]
        self.assertTrue(all(item["events"] == expanded for item in captures))
        self.assertEqual(result["cases"][0]["performanceSha256"],
                         content_hash(result["suite"]["cases"][0]["performance"]))

    def test_changed_audio_is_failed_with_partial_evidence(self):
        result, calls, output = self.run_plan(change_audio=True)
        self.assertFalse(result["ok"])
        self.assertIn("Nonrepeatable PCM", result["error"])
        self.assertEqual(len(result["cases"][0]["renders"]), 2)
        self.assertTrue((output / "results.json").is_file())
        self.assertEqual(calls[-1]["command"], "captureAudio")

    def test_capture_failure_stops_and_keeps_command_diagnostic(self):
        result, calls, output = self.run_plan(fail_capture=True)
        self.assertFalse(result["ok"])
        self.assertIn("prepare failed", result["error"])
        self.assertEqual(sum(item["command"] == "captureAudio" for item in calls), 1)
        self.assertIn("prepare failed", (output / "commands.jsonl").read_text())

    def test_concurrent_document_change_invalidates_the_suite(self):
        result, _, output = self.run_plan(change_graph=True)
        self.assertFalse(result["ok"])
        self.assertIn("graph changed", result["error"])
        self.assertTrue((output / "verification-preset.cyclegraph").is_file())

    def test_output_directory_is_never_overwritten(self):
        send, calls = self.fake_session()
        with self.assertRaises(FileExistsError):
            render_suite(self.plan(), self.preset, self.directory, send, {})
        self.assertEqual(calls, [])

    def test_wall_budget_stops_new_commands_and_retains_failed_manifest(self):
        self.value["budget"] = {"maxWallSeconds": 1}
        elapsed = [0]
        calls = []

        def slow_send(payload, **kwargs):
            calls.append(payload)
            self.assertLessEqual(kwargs["timeout"], 1)
            elapsed[0] += 2
            return {"ok": True, "result": {"ok": True}}

        output = self.directory / "timed"
        result = render_suite(self.plan(), self.preset, output, slow_send, {}, lambda: elapsed[0])
        self.assertFalse(result["ok"])
        self.assertIn("wall budget", result["error"])
        self.assertEqual(len(calls), 1)

    def test_transport_failure_records_the_attempted_command(self):
        def unavailable(payload, **kwargs):
            raise TimeoutError("no reply")

        output = self.directory / "unavailable"
        result = render_suite(self.plan(), self.preset, output, unavailable, {})
        self.assertFalse(result["ok"])
        attempt = json.loads((output / "commands.jsonl").read_text())
        self.assertEqual(attempt["command"]["command"], "openGraph")
        self.assertEqual(attempt["error"], "no reply")

    def test_wav_dimensions_and_payload_are_checked_before_recording_success(self):
        performance = self.plan()["cases"][0]["performance"]
        path = self.directory / "wrong-length.wav"
        with wave.open(str(path), "wb") as output:
            output.setparams((2, 3, 44100, 0, "NONE", "not compressed"))
            output.writeframes(b"\x00" * 6)
        with self.assertRaisesRegex(RuntimeError, "dimensions"):
            wav_identity(path, performance)


if __name__ == "__main__":
    unittest.main()
