#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import migrate_cycle_v1_preset_library


class MigrateCycleV1PresetLibraryTest(unittest.TestCase):
    def test_kebab_case_separates_words_and_numbers(self):
        cases = {
            "Acidic3": "acidic-3",
            "AcidLoop": "acid-loop",
            "Punk2B": "punk-2-b",
            "Mouth Harp": "mouth-harp",
            "LPDist": "lp-dist",
            "already-kebab": "already-kebab",
        }

        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    migrate_cycle_v1_preset_library.kebab_case(source),
                    expected)


if __name__ == "__main__":
    unittest.main()
