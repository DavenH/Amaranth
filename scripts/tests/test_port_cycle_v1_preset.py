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
    groups[6]["layers"][0]["mesh"]["vertices"] = [1]
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
    def assert_compact_nodes_do_not_overlap(self, converted):
        nodes = [
            node for node in converted["nodes"]
            if node["kind"] != "spectralLayer"
        ]
        for index, left in enumerate(nodes):
            left_x = left["position"]["x"]
            left_y = left["position"]["y"]
            left_width, left_height = port_cycle_v1_preset.node_footprint(left)
            for right in nodes[index + 1:]:
                right_x = right["position"]["x"]
                right_y = right["position"]["y"]
                right_width, right_height = \
                    port_cycle_v1_preset.node_footprint(right)
                overlaps = (
                    left_x < right_x + right_width
                    and right_x < left_x + left_width
                    and left_y < right_y + right_height
                    and right_y < left_y + left_height
                )
                self.assertFalse(
                    overlaps,
                    f"{left['id']} overlaps {right['id']}",
                )

    def test_converter_preserves_layer_enablement_and_operation_order(self):
        source = convertible_source()
        for layer in source["preset"]["meshLibrary"]["groups"][5]["layers"]:
            layer["mesh"]["vertices"] = [1]
        converted = port_cycle_v1_preset.convert(source)
        nodes = {entry["id"]: entry for entry in converted["nodes"]}

        self.assertTrue(nodes["timeLayer1"]["parameters"]["enabled"])
        self.assertFalse(nodes["timeLayer2"]["parameters"]["enabled"])
        self.assertFalse(nodes["magnitudeLayer2"]["parameters"]["enabled"])
        self.assertEqual(
            nodes["magnitudeLayer1"]["parameters"]["spectralMode"],
            "additive",
        )
        self.assertEqual(
            nodes["magnitudeLayer2"]["parameters"]["spectralMode"],
            "multiplicative",
        )
        self.assertEqual(nodes["magnitudeOp1"]["kind"], "add")
        self.assertEqual(nodes["magnitudeOp2"]["kind"], "multiply")
        self.assertEqual(nodes["phaseOp1"]["kind"], "add")

    def test_converter_persists_cycle_one_guide_noise_seeds(self):
        self.assertEqual(
            port_cycle_v1_preset.resolved_guide_noise_seed({"noiseSeed": 77}, 0),
            77,
        )
        self.assertEqual(
            port_cycle_v1_preset.resolved_guide_noise_seed({"noiseSeed": -1}, 0),
            6585,
        )

    def test_document_declick_uses_the_volume_envelope_boundary(self):
        source = convertible_source()
        source["preset"]["settings"]["Declick"] = True

        converted = port_cycle_v1_preset.convert(source)
        nodes = {entry["id"]: entry for entry in converted["nodes"]}

        self.assertTrue(nodes["volumeEnvelope1"]["parameters"]["declick"])
        self.assertIn("volumeMultiply", nodes)
        self.assertTrue(any(
            edge["sourceNodeId"] == "volumeEnvelope1"
            and edge["destNodeId"] == "volumeMultiply"
            for edge in converted["edges"]
        ))

        source["preset"]["settings"]["Declick"] = False
        converted = port_cycle_v1_preset.convert(source)
        nodes = {entry["id"]: entry for entry in converted["nodes"]}
        self.assertNotIn("volumeEnvelope1", nodes)
        self.assertNotIn("volumeMultiply", nodes)

    def test_static_envelopes_keep_red_blue_modulation_without_a_time_input(self):
        source = convertible_source()
        volume = {
            "properties": {"active": True, "dynamic": False},
            "mesh": {
                "mainMesh": {"vertices": [], "cubes": []},
                "loopIndices": [],
                "sustainIndices": [],
            },
        }
        source["preset"]["meshLibrary"]["groups"][0]["layers"] = [volume]
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"])

        converted = port_cycle_v1_preset.convert(source)
        nodes = {entry["id"]: entry for entry in converted["nodes"]}

        self.assertNotIn("staticEnvelopeMorph", nodes)
        self.assertEqual(nodes["volumeEnvelope1"]["parameters"]["red"], 0.5)
        self.assertEqual(nodes["volumeEnvelope1"]["parameters"]["blue"], 0.75)

        volume["properties"]["dynamic"] = True
        converted = port_cycle_v1_preset.convert(source)
        nodes = {entry["id"]: entry for entry in converted["nodes"]}
        self.assertNotIn("staticEnvelopeMorph", nodes)
        self.assertEqual(nodes["volumeEnvelope1"]["parameters"]["red"], 0.5)
        self.assertEqual(nodes["volumeEnvelope1"]["parameters"]["blue"], 0.75)

    def test_generated_layout_is_aligned_compact_and_non_overlapping(self):
        source = convertible_source()
        magnitude = source["preset"]["meshLibrary"]["groups"][5]["layers"][0]
        magnitude["mesh"]["vertices"] = [1]
        source["preset"]["meshLibrary"]["groups"][5]["layers"] = [
            copy.deepcopy(magnitude) for _ in range(10)
        ]
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"])

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}
        self.assert_compact_nodes_do_not_overlap(converted)

        self.assertLess(nodes["voice"]["position"]["x"],
                        nodes["timeLayer1"]["position"]["x"])
        self.assertLess(nodes["timeLayer1"]["position"]["x"],
                        nodes["fft"]["position"]["x"])
        self.assertLess(nodes["fft"]["position"]["x"],
                        nodes["ifft"]["position"]["x"])
        voice_kinds = {
            "voiceContext", "modulationSource", "modulationTriple",
            "trilinearMesh", "spectralLayer", "fft", "ifft", "envelope",
            "add", "multiply", "unison",
        }
        voice_bottom = max(
            node["position"]["y"]
            + port_cycle_v1_preset.node_footprint(node)[1]
            for node in converted["nodes"] if node["kind"] in voice_kinds
        )
        self.assertGreaterEqual(
            nodes["globalInput"]["position"]["y"],
            voice_bottom + 96.0,
        )

        mesh = nodes["magnitudeLayer1"]
        operation = nodes["magnitudeOp1"]
        mesh_width, _ = port_cycle_v1_preset.node_footprint(mesh)
        operation_width, _ = port_cycle_v1_preset.node_footprint(operation)
        self.assertAlmostEqual(
            mesh["position"]["x"] + mesh_width / 2.0,
            operation["position"]["x"] + operation_width / 2.0,
        )
        self.assertEqual(
            nodes["magnitudeOp1"]["portSides"]["inputs"]["right"],
            "top")
        self.assertEqual(
            nodes["phaseOp1"]["portSides"]["inputs"]["right"],
            "bottom")
        self.assertNotIn("outputs", nodes["magnitudeOp1"].get("portSides", {}))
        self.assertNotIn("portSides", nodes["fft"])
        self.assertNotIn("portSides", nodes["ifft"])
        self.assertEqual(
            nodes["magnitudeOp1"]["position"]["y"],
            nodes["magnitudeOp10"]["position"]["y"])
        self.assertGreater(
            nodes["output"]["position"]["y"],
            nodes["ifft"]["position"]["y"])

    def test_presentation_reconciliation_does_not_preserve_semantics(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        existing = copy.deepcopy(converted)
        existing_node = existing["nodes"][0]
        existing_node["position"] = {"x": 123.0, "y": 456.0}
        existing_node["portSides"] = {"outputs": {"context": "bottom"}}
        existing_node["parameters"]["octave"] = 7
        existing["probes"] = [{
            "id": "probe",
            "sourceNodeId": "timeLayer1",
            "sourcePortId": "out",
        }]

        reconciled = port_cycle_v1_preset.preserve_presentation(
            converted, existing)
        node = reconciled["nodes"][0]

        self.assertEqual(node["position"], {"x": 123.0, "y": 456.0})
        self.assertEqual(node["portSides"], {"outputs": {"context": "bottom"}})
        self.assertNotEqual(node["parameters"]["octave"], 7)
        self.assertEqual(reconciled["probes"], existing["probes"])

    def test_converter_preserves_legacy_inverse_velocity_blue_source(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        morph = next(node for node in converted["nodes"] if node["id"] == "morph")

        self.assertEqual(morph["parameters"]["blueSource"], "inverseVelocity")

    def test_converter_preserves_mod_wheel_blue_modulation_source(self):
        source = convertible_source()
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"], 101)

        converted = port_cycle_v1_preset.convert(source)
        morph = next(node for node in converted["nodes"] if node["id"] == "morph")

        self.assertEqual(morph["parameters"]["blueSource"], "modWheel")

    def test_time_layer_pan_uses_the_inline_pan_operation(self):
        source = convertible_source()
        source["preset"]["meshLibrary"]["groups"][4]["layers"][0] \
            ["properties"]["pan"] = 1.0

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertEqual(nodes["timeLayer1Process"]["parameters"]["pan"], 1.0)
        self.assertTrue(any(
            edge["sourceNodeId"] == "timeLayer1"
            and edge["destNodeId"] == "timeLayer1Process"
            for edge in converted["edges"]
        ))

    def test_centered_time_layer_omits_the_no_op_pan(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertNotIn("timeLayer1Process", nodes)
        self.assertTrue(any(
            edge["sourceNodeId"] == "timeLayer1"
            and edge["destNodeId"] in ("timeOp1", "fft")
            for edge in converted["edges"]
        ))

    def test_spectral_range_is_mapped_even_when_pan_is_centered(self):
        source = convertible_source()
        properties = source["preset"]["meshLibrary"]["groups"][5] \
            ["layers"][0]["properties"]
        properties["pan"] = 0.5
        properties["range"] = 0.625
        source["preset"]["meshLibrary"]["groups"][5] \
            ["layers"][0]["mesh"]["vertices"] = [1]

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertEqual(
            nodes["magnitudeLayer1"]["parameters"]["range"],
            0.625)
        self.assertNotIn("magnitudeLayer1Process", nodes)

    def test_inactive_unconnected_pitch_envelopes_are_omitted(self):
        converted = port_cycle_v1_preset.convert(convertible_source())

        self.assertFalse(any(
            node["id"].startswith("pitchEnvelope")
            for node in converted["nodes"]
        ))

    def test_document_declick_retains_only_a_neutral_volume_envelope(self):
        converted = port_cycle_v1_preset.convert(convertible_source())

        envelopes = [
            node for node in converted["nodes"]
            if node["kind"] == "envelope"
        ]
        self.assertEqual(len(envelopes), 1)
        self.assertEqual(envelopes[0]["id"], "volumeEnvelope1")
        self.assertTrue(envelopes[0]["parameters"]["declick"])

    def test_unassigned_guides_are_omitted(self):
        converted = port_cycle_v1_preset.convert(convertible_source())

        self.assertEqual(converted["guides"], [])

    def test_empty_phase_layer_is_bypassed(self):
        source = convertible_source()
        phase = source["preset"]["meshLibrary"]["groups"][6]["layers"][0]
        phase["properties"]["active"] = True
        phase["mesh"] = {"vertices": [], "cubes": []}
        source["preset"]["meshLibrary"]["groups"][5] \
            ["layers"][0]["mesh"]["vertices"] = [1]

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertNotIn("phaseLayer1", nodes)
        self.assertNotIn("phaseLayer1Process", nodes)
        self.assertNotIn("phaseOp1", nodes)
        self.assertTrue(any(
            edge["sourceNodeId"] == "fft"
            and edge["sourcePortId"] == "phase"
            and edge["destNodeId"] == "ifft"
            and edge["destPortId"] == "phase"
            for edge in converted["edges"]
        ))

    def test_empty_magnitude_layer_is_bypassed(self):
        converted = port_cycle_v1_preset.convert(convertible_source())
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertNotIn("magnitudeLayer1", nodes)
        self.assertNotIn("magnitudeLayer1Process", nodes)
        self.assertNotIn("magnitudeOp1", nodes)
        self.assertTrue(any(
            edge["sourceNodeId"] == "fft"
            and edge["sourcePortId"] == "mag"
            and edge["destNodeId"] == "ifft"
            and edge["destPortId"] == "mag"
            for edge in converted["edges"]
        ))

    def test_transform_pair_is_omitted_without_nonempty_spectral_layers(self):
        source = convertible_source()
        source["preset"]["meshLibrary"]["groups"][6]["layers"][0]["mesh"] = {
            "vertices": [],
            "cubes": [],
        }

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertNotIn("fft", nodes)
        self.assertNotIn("ifft", nodes)
        self.assertFalse(any(
            node["id"].startswith(("magnitudeLayer", "phaseLayer"))
            for node in converted["nodes"]
        ))
        self.assertTrue(any(
            edge["sourceNodeId"] != "ifft"
            and edge["destNodeId"] == "output"
            for edge in converted["edges"]
        ))

    def test_empty_phase_layer_does_not_break_later_phase_layer(self):
        source = convertible_source()
        phase_layers = source["preset"]["meshLibrary"]["groups"][6]["layers"]
        nonempty = copy.deepcopy(phase_layers[0])
        empty = copy.deepcopy(nonempty)
        empty["mesh"] = {"vertices": [], "cubes": []}
        phase_layers[:] = [empty, nonempty]
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"])

        converted = port_cycle_v1_preset.convert(source)
        nodes = {node["id"]: node for node in converted["nodes"]}

        self.assertIn("phaseLayer1", nodes)
        self.assertNotIn("phaseLayer2", nodes)
        self.assertTrue(any(
            edge["sourceNodeId"] == "fft"
            and edge["destNodeId"] == "phaseOp1"
            for edge in converted["edges"]
        ))
        self.assertTrue(any(
            edge["sourceNodeId"] == "phaseOp1"
            and edge["destNodeId"] == "ifft"
            for edge in converted["edges"]
        ))

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
        output = next(node for node in converted["nodes"] if node["id"] == "output")

        self.assertEqual(voice["parameters"]["octave"], 0)
        self.assertEqual(output["parameters"]["gain"], 0.5)

    def test_master_volume_is_preserved_on_output(self):
        source = convertible_source()
        source["preset"]["oscControls"]["knobs"][0] = 0.23

        converted = port_cycle_v1_preset.convert(source)
        output = next(node for node in converted["nodes"] if node["id"] == "output")

        self.assertEqual(output["parameters"]["gain"], 0.23)

    def test_equivalence_manifest_separates_output_gain_from_fixed_headroom(self):
        source = convertible_source()
        source["preset"]["oscControls"]["knobs"][0] = 0.23
        repository = Path(__file__).resolve().parents[2]

        manifest = port_cycle_v1_preset.equivalence_manifest(
            source,
            repository / "cycle/content/presets/saw.cyc",
            repository / "cycle-v2/content/presets/saw.cyclegraph",
            "Saw",
        )
        translation = manifest["translation"]

        self.assertEqual(translation["v2OutputGainUnitValue"], 0.23)
        self.assertEqual(translation["v2OutputHeadroom"], 0.125)
        self.assertEqual(
            translation["constantGainPolicy"],
            "Cycle1 master gain maps to Output; Cycle2 fixed headroom remains separate",
        )

    def test_missing_guide_properties_use_cycle_defaults(self):
        source = convertible_source()
        source["preset"]["meshLibrary"]["groups"][3]["layers"] = [{
            "properties": {"active": True},
            "mesh": {"vertices": [], "cubes": []},
        }]
        source["preset"]["meshLibrary"]["groups"][4]["layers"][0]["mesh"]["cubes"] = [{
            "guides": {"phase": 0},
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

    def test_legacy_reverb_uses_the_effective_high_pass_default(self):
        source = convertible_source()
        source["preset"]["details"] = {"productVersion": 1.0}
        source["preset"]["effects"]["Reverb"]["enabled"] = True

        converted = port_cycle_v1_preset.convert(source)
        reverb = next(
            node for node in converted["nodes"]
            if node["id"] == "reverb")

        self.assertEqual(reverb["parameters"]["highPass"], 0.05)

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
            "processingScope": "global",
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

    def test_multiple_magnitude_layers_are_supported_for_strict_parity(self):
        source = supported_source()
        source["preset"]["meshLibrary"]["groups"][5]["layers"].append(
            copy.deepcopy(source["preset"]["meshLibrary"]["groups"][5]["layers"][0]))
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"])

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertEqual(issues, [])

    def test_strict_parity_requires_an_active_magnitude_layer(self):
        source = supported_source()
        source["preset"]["meshLibrary"]["groups"][5]["layers"][0] \
            ["properties"]["active"] = False

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn("magnitude requires at least one active layer; found 0", issues)

    def test_strict_parity_allows_no_active_phase_layer(self):
        source = supported_source()
        source["preset"]["meshLibrary"]["groups"][6]["layers"][0] \
            ["properties"]["active"] = False

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertEqual(issues, [])

    def test_every_active_magnitude_layer_must_have_neutral_legacy_gain(self):
        source = supported_source()
        second = copy.deepcopy(source["preset"]["meshLibrary"]["groups"][5]["layers"][0])
        second["properties"]["gain"] = 0.25
        source["preset"]["meshLibrary"]["groups"][5]["layers"].append(second)
        source["preset"]["modMatrix"]["mappings"] = \
            port_cycle_v1_preset.default_modulation_mappings_for_preset(
                source["preset"])

        issues = port_cycle_v1_preset.validate_audio_parity_subset(source)

        self.assertIn("magnitude layer gain and fine tune must be neutral", issues)

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
