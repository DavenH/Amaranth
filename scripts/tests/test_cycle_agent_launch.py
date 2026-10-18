#!/usr/bin/env python3
"""Compatibility aliases must select a product without lifecycle policy."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]


class CycleAgentAliasTest(unittest.TestCase):
    def test_fixture_aliases_pass_explicit_product(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake_python = root / "python3"
            fake_python.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$ARGS_FILE"\n')
            fake_python.chmod(0o755)
            arguments = root / "args.txt"
            environment = os.environ.copy()
            environment.update({"PATH": f"{root}:{environment['PATH']}",
                                "ARGS_FILE": str(arguments)})
            for script, expected in (("run_cycle_agent.sh", "cycle1"),
                                     ("run_cycle_v2_agent.sh", "cycle-v2")):
                subprocess.run(["/bin/zsh", str(SCRIPTS / script), "fixture.json"],
                               env=environment, check=True)
                passed = arguments.read_text().splitlines()
                self.assertEqual(passed[1:4], ["run", "--product", expected])
                self.assertEqual(passed[4:], ["--fixture", "fixture.json"])

            for script, expected in (("run_cycle_agent_smokes.sh", "cycle1"),
                                     ("run_cycle_v2_agent_smokes.sh", "cycle-v2")):
                subprocess.run(["/bin/zsh", str(SCRIPTS / script), "readonly"],
                               env=environment, check=True)
                passed = arguments.read_text().splitlines()
                self.assertEqual(passed[1:4], ["suite", "--product", expected])
                self.assertEqual(passed[-1], "readonly")


if __name__ == "__main__":
    unittest.main()
