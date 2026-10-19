import json
import multiprocessing
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from cycle_automation_support.identity import socket_fingerprint, socket_path, valid_instance, workspace
from cycle_automation_support.lifecycle import ensure, prepare_socket
from cycle_automation_support.profiles import PRODUCTS
from cycle_automation_support.operations import capture, collect_crash, run_fixture
from cycle_automation_support.smokes import selected_fixtures
from cycle_automation_support.registry import Conflict, acquire, lease, recover, registry


def hold_lease(root, ready, release):
    owner = {"workspace_id": "first", "workspace_path": "/work/first",
             "run_id": "first-run", "app_pid": 123}
    with patch("cycle_automation_support.registry.state_root", return_value=root):
        with lease("foreground", owner, "capture"):
            ready.set()
            release.wait(5)


class CycleAutomationTest(unittest.TestCase):
    def test_product_required_before_build_or_launch(self):
        result = subprocess.run([sys.executable, str(SCRIPTS / "cycle_automation.py"),
                                 "run", "--fixture", "missing.json"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--product", result.stderr)

    def test_product_profiles_are_distinct(self):
        self.assertEqual(PRODUCTS["cycle-v2"].target, "CycleV2")
        self.assertEqual(PRODUCTS["cycle1"].target, "Cycle")
        self.assertNotEqual(PRODUCTS["cycle1"].app, PRODUCTS["cycle-v2"].app)

    def test_workspace_identity_prefers_local_conductor_id(self):
        with patch.dict(os.environ, {"CONDUCTOR_IS_LOCAL": "1", "CONDUCTOR_WORKSPACE_ID": "workspace-one"}):
            self.assertEqual(workspace()[0], "workspace-one")
        with patch.dict(os.environ, {"CONDUCTOR_IS_LOCAL": "0", "CONDUCTOR_WORKSPACE_ID": "workspace-one"}):
            self.assertEqual(workspace()[0], workspace()[1])

    def test_socket_namespace_and_length(self):
        first = socket_path("a" * 12, "cycle1")
        second = socket_path("b" * 12, "cycle1")
        third = socket_path("a" * 12, "cycle-v2")
        self.assertEqual(len({first, second, third}), 3)
        self.assertLess(len(os.fsencode(third)), 104)

    def test_reused_pid_with_new_start_identity_is_invalid(self):
        record = {"pid": 123, "process_identity": {
            "executable": "/tmp/Cycle", "started": "first"}}
        with patch("cycle_automation_support.identity.process_identity", return_value={
                "executable": "/tmp/Cycle", "started": "second"}):
            self.assertFalse(valid_instance(record))

    def test_live_lease_reports_owner(self):
        data = {"leases": {}}
        owner = {"workspace_id": "one", "workspace_path": "/work/one",
                 "run_id": "run-one", "app_pid": 123}
        acquire(data, "foreground", owner, "capture")
        with self.assertRaisesRegex(Conflict, "one.*123.*run-one.*capture"):
            acquire(data, "foreground", owner, "capture")

    def test_invalid_registry_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "registry.json"
            path.write_text("{broken")
            with patch("cycle_automation_support.registry.state_root", return_value=root):
                with self.assertRaisesRegex(RuntimeError, "Invalid registry preserved"):
                    with registry():
                        pass
            self.assertEqual(path.read_text(), "{broken")

    def test_concurrent_process_cannot_acquire_foreground_lease(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            context = multiprocessing.get_context("fork")
            ready = context.Event()
            release = context.Event()
            worker = context.Process(target=hold_lease, args=(root, ready, release))
            worker.start()
            try:
                self.assertTrue(ready.wait(5))
                owner = {"workspace_id": "second", "workspace_path": "/work/second",
                         "run_id": "second-run", "app_pid": 456}
                with patch("cycle_automation_support.registry.state_root", return_value=root):
                    with self.assertRaisesRegex(Conflict, "first.*first-run"):
                        with lease("foreground", owner, "capture"):
                            pass
            finally:
                release.set()
                worker.join(5)
            self.assertEqual(worker.exitcode, 0)

    def test_bounded_wait_releases_and_times_out(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = {"workspace_id": "first", "workspace_path": "/first",
                     "run_id": "first", "app_pid": 1}
            second = {"workspace_id": "second", "workspace_path": "/second",
                      "run_id": "second", "app_pid": 2}
            elapsed = []

            def waiter():
                began = time.monotonic()
                with lease("foreground", second, "capture", wait=1):
                    elapsed.append(time.monotonic() - began)

            with patch("cycle_automation_support.registry.state_root", return_value=root):
                with lease("foreground", first, "capture"):
                    with self.assertRaises(Conflict):
                        with lease("foreground", second, "capture", wait=0.1):
                            pass
                    worker = threading.Thread(target=waiter)
                    worker.start()
                    time.sleep(0.2)
                worker.join(2)
            self.assertEqual(len(elapsed), 1)
            self.assertGreaterEqual(elapsed[0], 0.2)

    def test_stale_recovery_only_removes_owned_socket_without_signalling(self):
        with tempfile.TemporaryDirectory() as directory:
            socket = Path(directory) / "old.sock"
            socket.touch()
            data = {"instances": {"one/cycle1": {
                "pid": 123, "process_identity": {"started": "old"},
                "coordinator_pid": 456, "coordinator_identity": {"started": "old"},
                "socket": str(socket), "socket_identity": socket_fingerprint(socket)}}, "leases": {}}
            with patch("cycle_automation_support.registry.valid_instance", return_value=False), \
                    patch("cycle_automation_support.registry.process_identity", return_value=None), \
                    patch("os.kill") as kill:
                recover(data)
            self.assertEqual(data["instances"], {})
            self.assertFalse(socket.exists())
            kill.assert_not_called()

    def test_launch_preserves_replaced_socket_and_removes_owned_socket(self):
        with tempfile.TemporaryDirectory() as directory:
            socket = Path(directory) / "cycle.sock"
            socket.touch()
            existing = {"socket": str(socket), "socket_identity": socket_fingerprint(socket)}
            prepare_socket(socket, existing)
            self.assertFalse(socket.exists())

            socket.touch()
            replacement = {"socket": str(socket), "socket_identity": [0, 0]}
            with self.assertRaisesRegex(RuntimeError, "Socket collision"):
                prepare_socket(socket, replacement)
            self.assertTrue(socket.exists())

    def test_build_and_instance_selection_hold_one_build_lease(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = {"workspace_id": "work", "workspace_path": "/work",
                     "run_id": "run", "app_pid": None}
            profile = PRODUCTS["cycle-v2"]
            resource = "build:work/cycle-v2"
            observed = []

            def check_lease(*args):
                with registry() as data:
                    observed.append(resource in data["leases"])
                return "selected"

            with patch("cycle_automation_support.registry.state_root", return_value=root), \
                    patch("cycle_automation_support.lifecycle.build", side_effect=check_lease), \
                    patch("cycle_automation_support.lifecycle.ensure_built", side_effect=check_lease):
                self.assertEqual(ensure(profile, owner, "work"), "selected")
                with lease(resource, {**owner, "run_id": "other"}, "build"):
                    with self.assertRaises(Conflict):
                        ensure(profile, owner, "work")
            self.assertEqual(observed, [True, True])

    def test_region_capture_uses_recorded_pid_window(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            destination = root / "region.png"
            owner = {"workspace_id": "work", "workspace_path": "/work",
                     "run_id": "run", "app_pid": 123}

            def write_image(command, **kwargs):
                destination.write_bytes(b"\x89PNG\r\n\x1a\n" + b"\0" * 8
                                        + (20).to_bytes(4, "big") + (10).to_bytes(4, "big"))
                return subprocess.CompletedProcess(command, 0,
                        "pid=123 window=42 x=10 y=20 width=20 height=10")

            with patch("cycle_automation_support.registry.state_root", return_value=root), \
                    patch("cycle_automation_support.operations.preflight"), \
                    patch("cycle_automation_support.operations.valid_instance", return_value=True), \
                    patch("cycle_automation_support.operations.compiled_helper", return_value="/helper"), \
                    patch("cycle_automation_support.operations.subprocess.run", side_effect=write_image) as run:
                result = capture({"pid": 123}, owner, destination, "10,20,20,10")
            self.assertEqual(run.call_args.args[0],
                             ["/helper", "123", str(destination), "10,20,20,10"])
            self.assertEqual(result["method"], "pid-window-region")
            self.assertEqual(result["window_id"], 42)

    def test_crash_report_must_match_pid_and_start(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reports = root / "Library" / "Logs" / "DiagnosticReports"
            reports.mkdir(parents=True)
            record = {"pid": 123, "launched": 0,
                      "executable": "/work/Cycle.app/Contents/MacOS/Cycle",
                      "process_identity": {"started": [1000, 250000]}}
            for name, pid in (("wrong", 124), ("right", 123)):
                path = reports / f"{name}.ips"
                path.write_text(json.dumps({"app_name": "Cycle"}) + "\n" +
                                json.dumps({"pid": pid,
                                            "procLaunch": "1970-01-01 00:16:40.250000 +0000"}))
            with patch("pathlib.Path.home", return_value=root):
                found = collect_crash(record, root)
            self.assertEqual(found, root / "crash.ips")
            self.assertIn('"pid": 123', found.read_text())

    def test_smoke_manifest_selects_named_fixture_and_visual_variant(self):
        manifest = json.loads((SCRIPTS / "fixtures/cycle-v2-smoke-suite.json").read_text())
        selected = selected_fixtures(manifest, ["readonly"], include_visual=False)
        self.assertEqual([item["name"] for item in selected], ["readonly"])
        visual = selected_fixtures(manifest, [], include_visual=True)
        self.assertIn("mesh-controls-os-screenshot", [item["name"] for item in visual])
        with self.assertRaisesRegex(ValueError, "Unknown smoke fixtures"):
            selected_fixtures(manifest, ["not-a-fixture"], include_visual=False)

    def test_app_screenshot_conflict_prevents_session_command(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / "cycle-agent-screenshot.json"
            fixture.write_text('{"commands":[{"command":"screenshot","area":"window"}]}')
            held = {"workspace_id": "other", "workspace_path": "/other",
                    "run_id": "other", "app_pid": 55}
            caller = {"workspace_id": "this", "workspace_path": "/this",
                      "run_id": "this", "app_pid": 66}
            with patch("cycle_automation_support.registry.state_root", return_value=root), \
                    patch("cycle_automation_support.operations.command") as send:
                with lease("foreground", held, "native-input"):
                    with self.assertRaises(Conflict):
                        run_fixture({"pid": 66, "socket": "unused"}, fixture,
                                    root / "report.json", caller)
            send.assert_not_called()


if __name__ == "__main__":
    unittest.main()
