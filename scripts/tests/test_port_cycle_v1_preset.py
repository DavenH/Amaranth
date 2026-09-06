#!/usr/bin/env python3

import copy
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import port_cycle_v1_preset


def supported_source():
    layer = {
        "properties": {
            "active": True,
            "gain": 0.0,
            "fineTune": 0.0,
            "pan": 0.5,
            "mode": 0,
        },
        "mesh": {"vertices": [1]},
    }
    inactive_envelope = {"properties": {"active": False}}
    return {
        "preset": {
            "meshLibrary": {
                "groups": [
                    {}, {}, {}, {},
                    {"layers": [copy.deepcopy(layer)]},
                    {"layers": [copy.deepcopy(layer)]},
                    {"layers": [copy.deepcopy(layer)]},
                ],
            },
            "effects": {
                name: {"enabled": False}
                for name in ("ImpulseModeller", "Unison", "Delay", "Reverb", "EQ")
            },
            "envelopeProps": {
                "groups": {
                    purpose: {"layers": [copy.deepcopy(inactive_envelope)]}
                    for purpose in ("volume", "scratch", "pitch", "wavePitch")
                },
            },
            "multisample": {"samples": []},
            "modMatrix": {
                "mappings": copy.deepcopy(port_cycle_v1_preset.DEFAULT_MODULATION_MAPPINGS),
            },
            "settings": {"OversampleFactorRltm": 1},
            "guideCurveProps": {"guides": [{"noiseLevel": 0.0}]},
        },
    }


class PortCycleV1PresetTest(unittest.TestCase):
    def test_supported_subset_has_no_validation_issues(self):
        self.assertEqual(
            port_cycle_v1_preset.validate_audio_parity_subset(supported_source()),
            [],
        )

    def test_active_unsupported_effect_blocks_parity(self):
        source = supported_source()
        source["preset"]["effects"]["Delay"]["enabled"] = True

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn("active Delay is not supported by strict audio parity", issues)

    def test_guide_noise_blocks_deterministic_parity(self):
        source = supported_source()
        source["preset"]["guideCurveProps"]["guides"][0]["noiseLevel"] = 0.1

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn("Guide noise must be disabled for deterministic audio parity", issues)

    def test_multiple_magnitude_layers_report_validation_issue(self):
        source = supported_source()
        source["preset"]["meshLibrary"]["groups"][5]["layers"].append(
            copy.deepcopy(source["preset"]["meshLibrary"]["groups"][5]["layers"][0]))

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn("magnitude requires exactly one active layer; found 2", issues)

    def test_multiple_scratch_envelopes_report_validation_issue(self):
        source = supported_source()
        scratch = source["preset"]["envelopeProps"]["groups"]["scratch"]["layers"][0]
        scratch["properties"]["active"] = True
        source["preset"]["envelopeProps"]["groups"]["scratch"]["layers"].append(
            copy.deepcopy(scratch))

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn(
            "strict audio parity supports at most one scratch envelope",
            issues,
        )

    def test_octave_translation_includes_legacy_midi_reference(self):
        self.assertEqual(port_cycle_v1_preset.translated_octave(0.5), 0)
        self.assertEqual(port_cycle_v1_preset.translated_octave(0.198473282), -2)


if __name__ == "__main__":
    unittest.main()
