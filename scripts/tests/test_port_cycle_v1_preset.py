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
    source = {
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
    return source


def convertible_source():
    def mesh_layer(active=True, mode=0):
        return {
            "properties": {
                "active": active,
                "gain": 0.0,
                "fineTune": 0.0,
                "pan": 0.5,
                "range": 0.5,
                "mode": mode,
            },
            "mesh": {"vertices": [], "cubes": []},
        }

    groups = [{"layers": []} for _ in range(11)]
    groups[4]["layers"] = [mesh_layer(), mesh_layer(False)]
    groups[5]["layers"] = [mesh_layer(), mesh_layer(False, 1)]
    groups[6]["layers"] = [mesh_layer(False)]
    source = {
        "preset": {
            "meshLibrary": {"groups": groups},
            "morphPanel": {
                "position": {"time": 0.25, "red": 0.5, "blue": 0.75},
                "linking": {"red": False, "blue": False},
                "primaryAxis": 0,
            },
            "oscControls": {"knobs": [0.5, 0.5, 0.5]},
            "effects": {
                "ImpulseModeller": {"enabled": False},
                "Unison": {
                    "enabled": False,
                    "groupMode": True,
                    "knobs": [0.2, 0.3, 0.4, 0.5, 0.6],
                },
                "Delay": {
                    "enabled": False,
                    "knobs": [0.1, 0.2, 0.3, 0.4, 0.5],
                },
                "Reverb": {
                    "enabled": False,
                    "knobs": [0.1, 0.2, 0.3, 0.4, 0.5],
                },
                "EQ": {"enabled": False, "knobs": [0.1] * 10},
                "Waveshaper": {
                    "enabled": False,
                    "knobs": [0.5, 0.5],
                    "oversampleFactor": 1,
                },
            },
            "settings": {"OversampleFactorRltm": 1},
            "guideCurveProps": {"guides": []},
            "modMatrix": {"mappings": []},
            "multisample": {"samples": []},
        },
    }
    source["preset"]["modMatrix"]["mappings"] = \
        port_cycle_v1_preset.default_modulation_mappings_for_preset(
            source["preset"])
    return source


class PortCycleV1PresetTest(unittest.TestCase):
    def test_converter_preserves_layer_enablement_and_operation_order(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        nodes = {entry["id"]: entry for entry in converted["nodes"]}

        self.assertTrue(nodes["timeLayer1"]["parameters"]["enabled"])
        self.assertFalse(nodes["timeLayer2"]["parameters"]["enabled"])
        self.assertFalse(nodes["magnitudeLayer2"]["parameters"]["enabled"])
        self.assertEqual(nodes["magnitudeOp1"]["kind"], "add")
        self.assertEqual(nodes["magnitudeOp2"]["kind"], "multiply")
        self.assertEqual(nodes["phaseOp1"]["kind"], "add")

    def test_converter_preserves_velocity_blue_modulation_source(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        morph = next(node for node in converted["nodes"] if node["id"] == "morph")

        self.assertEqual(morph["parameters"]["blueSource"], "velocity")

    def test_converter_preserves_mod_wheel_blue_modulation_source(self):
        source = convertible_source()
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"], 101)

        converted = port_cycle_v1_preset.convert(source)
        morph = next(node for node in converted["nodes"] if node["id"] == "morph")

        self.assertEqual(morph["parameters"]["blueSource"], "modWheel")

    def test_time_layer_pan_is_rejected_without_a_time_domain_destination(self):
        source = convertible_source()
        source["preset"]["meshLibrary"]["groups"][4]["layers"][0] \
            ["properties"]["pan"] = 1.0

        issues = port_cycle_v1_preset.validate_conversion(source)

        self.assertIn("time layer 1 has unmapped pan 1.0", issues)

    def test_missing_realtime_oversampling_uses_cycle_default(self):
        source = convertible_source()
        del source["preset"]["settings"]["OversampleFactorRltm"]

        converted = port_cycle_v1_preset.convert(source)
        voice = next(node for node in converted["nodes"] if node["id"] == "voice")

        self.assertEqual(voice["parameters"]["oversampling"], "1x")

    def test_missing_oscillator_controls_use_cycle_defaults(self):
        source = convertible_source()
        source["preset"]["oscControls"]["knobs"] = []

        converted = port_cycle_v1_preset.convert(source)
        voice = next(node for node in converted["nodes"] if node["id"] == "voice")

        self.assertEqual(voice["parameters"]["octave"], 0)

    def test_missing_guide_properties_use_cycle_defaults(self):
        source = convertible_source()
        source["preset"]["meshLibrary"]["groups"][3]["layers"] = [{
            "properties": {"active": True},
            "mesh": {"vertices": [], "cubes": []},
        }]

        converted = port_cycle_v1_preset.convert(source)

        self.assertEqual(converted["guides"][0]["noise"], 0.0)
        self.assertEqual(converted["guides"][0]["dcOffset"], 0.0)
        self.assertEqual(converted["guides"][0]["phase"], 0.0)

    def test_missing_legacy_equalizer_is_disabled(self):
        source = convertible_source()
        source["preset"]["effects"]["EQ"] = None

        converted = port_cycle_v1_preset.convert(source)

        self.assertFalse(any(
            node["kind"] == "equalizer" for node in converted["nodes"]))

    def test_active_wave_pitch_envelope_is_reported(self):
        source = convertible_source()
        source["preset"]["effects"]["ImpulseModeller"]["waveLoaded"] = True
        source["preset"]["meshLibrary"]["groups"][8]["layers"] = [{
            "properties": {"active": True},
        }]

        issues = port_cycle_v1_preset.validate_conversion(source)

        self.assertIn(
            "active wave-pitch Envelope has no Cycle V2 destination",
            issues,
        )

    def test_delay_uses_the_shared_cycle_parameter_order(self):
        source = convertible_source()
        source["preset"]["effects"]["Delay"]["enabled"] = True

        converted = port_cycle_v1_preset.convert(source)
        delay = next(node for node in converted["nodes"] if node["id"] == "delay")

        self.assertEqual(delay["parameters"], {
            "enabled": True,
            "time": 0.1,
            "feedback": 0.2,
            "spinIters": 0.3,
            "spin": 0.4,
            "wet": 0.5,
        })

    def test_equalizer_and_reverb_keep_cycle_parameter_order(self):
        source = convertible_source()
        source["preset"]["effects"]["EQ"]["enabled"] = True
        source["preset"]["effects"]["Reverb"]["enabled"] = True

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertEqual(nodes["equalizer"]["parameters"]["band1Gain"], 0.1)
        self.assertEqual(nodes["equalizer"]["parameters"]["band1Frequency"], 0.1)
        self.assertEqual(nodes["reverb"]["parameters"], {
            "enabled": True,
            "size": 0.1,
            "damp": 0.2,
            "width": 0.3,
            "highPass": 0.4,
            "wet": 0.5,
        })

    def test_group_unison_uses_the_shared_cycle_mapping(self):
        source = convertible_source()
        source["preset"]["effects"]["Unison"]["enabled"] = True

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertEqual(nodes["unison"]["parameters"], {
            "enabled": True,
            "mode": "group",
            "order": 6,
            "width": 14.0,
            "panSpread": 0.3,
            "phase": 0.4,
            "jitter": 0.6,
        })
        self.assertEqual(nodes["unison"]["model"]["schema"], "unisonVoices")
        self.assertTrue(any(
            edge["sourceNodeId"] == "unison"
            and edge["destPortId"] == "unison"
            for edge in converted["edges"]
        ))

    def test_drawn_impulse_response_uses_the_shared_cycle_mapping(self):
        source = convertible_source()
        source["preset"]["effects"]["ImpulseModeller"].update({
            "enabled": True,
            "waveLoaded": False,
            "knobs": [0.2, 0.3, 0.4],
        })
        source["preset"]["meshLibrary"]["groups"][10]["layers"] = [{
            "properties": {"active": True},
            "mesh": {"vertices": [], "cubes": []},
        }]

        converted = port_cycle_v1_preset.convert(source)
        impulse = next(
            node for node in converted["nodes"]
            if node["id"] == "impulseResponse")

        self.assertEqual(impulse["parameters"], {
            "enabled": True,
            "size": 0.2,
            "post": 0.3,
            "highPass": 0.4,
        })

    def test_legacy_impulse_response_defaults_missing_high_pass(self):
        source = convertible_source()
        source["preset"]["effects"]["ImpulseModeller"].update({
            "enabled": True,
            "waveLoaded": False,
            "knobs": [0.2, 0.3],
        })
        source["preset"]["meshLibrary"]["groups"][10]["layers"] = [{
            "properties": {"active": True},
            "mesh": {"vertices": [], "cubes": []},
        }]

        converted = port_cycle_v1_preset.convert(source)
        impulse = next(
            node for node in converted["nodes"]
            if node["id"] == "impulseResponse")

        self.assertEqual(impulse["parameters"]["highPass"], 0.0)

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
