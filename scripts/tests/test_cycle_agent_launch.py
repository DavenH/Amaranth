#!/usr/bin/env python3

import os
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
RUNNER = REPO_ROOT / "scripts" / "run_cycle_agent.sh"


class CycleAgentLaunchTest(unittest.TestCase):
    def run_launcher(self, focus_on_launch):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app = root / "Cycle.app"
            (app / "Contents").mkdir(parents=True)
            (app / "Contents" / "Info.plist").write_text("", encoding="utf-8")
            fixture = root / "fixture.json"
            fixture.write_text("[]\n", encoding="utf-8")
            report = root / "report.json"
            log = root / "commands.log"
            fake_bin = root / "bin"
            fake_bin.mkdir()

            self.write_executable(fake_bin / "plutil", """
                #!/bin/zsh
                if [[ "$*" == *CFBundleIdentifier* ]]; then
                    print -r -- "com.amaranthaudio.test-cycle"
                else
                    print -r -- "Cycle"
                fi
            """)
            self.write_executable(fake_bin / "osascript", """
                #!/bin/zsh
                print -r -- "$*" >> "$COMMAND_LOG"
                if [[ "$*" == *"exists process"* || "$*" == *"frontmost of process"* ]]; then
                    print -r -- "true"
                fi
            """)
            self.write_executable(fake_bin / "open", """
                #!/bin/zsh
                print -r -- "$*" >> "$COMMAND_LOG"
                while (( $# > 0 )); do
                    if [[ "$1" == "--agent-report" ]]; then
                        shift
                        print -r -- '{"results":[]}' > "$1"
                        exit 0
                    fi
                    shift
                done
                exit 1
            """)

            environment = os.environ.copy()
            environment.update({
                "PATH": f"{fake_bin}:{environment['PATH']}",
                "COMMAND_LOG": str(log),
                "CYCLE_APP_PATH": str(app),
                "CYCLE_PROCESS_NAME": "TestCycle",
                "CYCLE_FOCUS_ON_LAUNCH": "1" if focus_on_launch else "0",
                "CYCLE_REUSE_EXISTING": "1",
                "CYCLE_PREFLIGHT_PERMISSIONS": "0",
                "CYCLE_CAPTURE_CRASH_REPORTS": "0",
                "CYCLE_DISMISS_CRASH_DIALOG": "0",
                "CYCLE_SUPPRESS_CRASH_DIALOG": "0",
                "CYCLE_FILTER_LOGS": "0",
                "CYCLE_WAIT_SECONDS": "1",
            })

            result = subprocess.run(
                ["/bin/zsh", str(RUNNER), str(fixture), str(report), str(root / "run.log")],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            return log.read_text(encoding="utf-8")

    def write_executable(self, path, source):
        path.write_text(textwrap.dedent(source).lstrip(), encoding="utf-8")
        path.chmod(0o755)

    def test_semantic_launch_does_not_take_focus_by_default(self):
        commands = self.run_launcher(focus_on_launch=False)

        self.assertNotIn("to activate", commands)
        self.assertNotIn("set frontmost", commands)

    def test_launch_focus_remains_available_as_an_explicit_opt_in(self):
        commands = self.run_launcher(focus_on_launch=True)

        self.assertIn("to activate", commands)


if __name__ == "__main__":
    unittest.main()
