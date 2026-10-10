import io
import json
from pathlib import Path
import struct
import sys
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import numpy as np
from PIL import Image

from surface_colour_engine import (
    SurfaceEngine, checked_grid, default_recipe, gradient, linear_to_oklab,
    linear_to_srgb, load_grid, new_layer, oklab_to_linear, png_bytes,
    srgb_to_linear, validate_recipe, layer_alpha, blend_colour, mapped_coordinate, gamut_safe_oklab,
    colour_coordinate,
)
from surface_colour_lab import LabServer, SAMPLE


class EngineTests(unittest.TestCase):
    def setUp(self):
        y, x = np.mgrid[0:64, 0:96]
        self.source = .7 * np.sin(y * .15) + .03 * np.sin(x * 1.3)
        self.engine = SurfaceEngine(self.source)

    def test_cycle_orientation(self):
        data = struct.pack("<ii6f", 2, 3, 0, 1, 2, 3, 4, 5)
        np.testing.assert_array_equal(load_grid(data, "fixture.f32"), [[2, 5], [1, 4], [0, 3]])

    def test_invalid_grids(self):
        for data in (b"", struct.pack("<ii", -1, 8), struct.pack("<ii", 2000, 2000),
                     struct.pack("<ii3f", 2, 2, 1, 2, 3),
                     struct.pack("<ii4f", 2, 2, 1, 2, 3, float("nan"))):
            with self.assertRaises(ValueError):
                load_grid(data, "bad.f32")
        with self.assertRaises(ValueError):
            checked_grid(np.zeros((1, 4)))

    def test_csv_orientation(self):
        np.testing.assert_array_equal(load_grid(b"1,2\n3,4\n", "x.csv"), [[1, 2], [3, 4]])

    def test_raw_is_straight_mapping_and_source_is_immutable(self):
        recipe = default_recipe()
        result, reference, _ = self.engine.render(recipe)
        np.testing.assert_array_equal(result, reference)
        np.testing.assert_array_equal(self.engine.source, self.source.astype(np.float32))
        self.assertFalse(self.engine.source.flags.writeable)

    def test_constant_has_no_detail_at_any_boundary(self):
        engine = SurfaceEngine(np.full((32, 48), .7))
        height = engine.height("unit")
        for kind in ("highpass", "bandpass"):
            np.testing.assert_allclose(engine.field(height, "unit", new_layer(kind)), 0, atol=1.e-7)

    def test_low_and_high_reconstruct_original(self):
        height = self.engine.height("spy")
        low = self.engine.field(height, "spy", new_layer("lowpass"))
        high = self.engine.field(height, "spy", new_layer("highpass"))
        np.testing.assert_allclose(low + high, height, atol=1.e-7)

    def test_axis_and_zero_radius(self):
        engine = SurfaceEngine(np.tile(np.linspace(0, 1, 96), (64, 1)))
        layer = new_layer("highpass")
        layer["axis"] = "y"
        np.testing.assert_allclose(engine.field(engine.height("unit"), "unit", layer), 0, atol=1.e-7)
        layer["axis"], layer["sigma"] = "x", 0
        np.testing.assert_array_equal(engine.field(engine.height("unit"), "unit", layer), 0)

    def test_band_is_difference_of_lowpasses(self):
        height = self.engine.height("spy")
        layer = new_layer()
        band = self.engine.field(height, "spy", layer)
        expected = self.engine.blur(height, "spy", 2, "both") - self.engine.blur(height, "spy", 8, "both")
        np.testing.assert_array_equal(band, expected)

    def test_disabled_layer_and_zero_opacity_are_identity(self):
        recipe = default_recipe()
        reference = self.engine.render(recipe)[0]
        detail = new_layer()
        recipe["layers"].append(detail)
        detail["enabled"] = False
        np.testing.assert_array_equal(self.engine.render(recipe)[0], reference)
        detail["enabled"], detail["opacity"] = True, 0
        np.testing.assert_array_equal(self.engine.render(recipe)[0], reference)

    def test_zero_detail_mask_does_not_paint_midpoint(self):
        engine = SurfaceEngine(np.full((16, 16), .3))
        recipe = default_recipe()
        expected = engine.render(recipe)[0]
        recipe["layers"].append(new_layer())
        np.testing.assert_array_equal(engine.render(recipe)[0], expected)

    def test_colour_edits_reuse_blurs_and_cache_is_bounded(self):
        recipe = default_recipe()
        recipe["layers"].append(new_layer())
        self.engine.render(recipe)
        count = self.engine.blur_count
        recipe["layers"][1]["stops"][0][1] = "#ffff00"
        self.engine.render(recipe)
        self.assertEqual(count, self.engine.blur_count)
        for sigma in range(20):
            self.engine.blur(self.engine.height("spy"), "spy", sigma, "both")
        self.assertLessEqual(len(self.engine.cache), 12)

    def test_colour_spaces_round_trip(self):
        values = np.random.default_rng(4).random((10, 10, 3))
        np.testing.assert_allclose(linear_to_srgb(srgb_to_linear(values)), values, atol=1.e-6)
        np.testing.assert_allclose(oklab_to_linear(linear_to_oklab(values)), values, atol=2.e-7)

    def test_gradient_endpoints_in_all_spaces(self):
        stops = [[0, "#123456"], [1, "#fedcba"]]
        expected = srgb_to_linear(np.array([[18, 52, 86], [254, 220, 186]]) / 255)
        for space in ("srgb", "linear", "oklab"):
            np.testing.assert_allclose(gradient(np.array([0, 1]), stops, space), expected, atol=1.e-6)

    def test_recipe_reload_and_export(self):
        recipe = default_recipe()
        recipe["layers"].append(new_layer())
        result = self.engine.render(recipe)[0]
        np.testing.assert_array_equal(result, self.engine.render(json.loads(json.dumps(recipe)))[0])
        image = Image.open(io.BytesIO(png_bytes(result)))
        self.assertEqual(image.size, (96, 64))

    def test_invalid_recipes(self):
        for key, value in (("sigma", -1), ("opacity", 2), ("gain", float("nan")),
                           ("filter", "mystery"), ("stops", [[0, "#ffffff"], [0, "#000000"]])):
            recipe = default_recipe()
            recipe["layers"][0][key] = value
            with self.assertRaises(ValueError):
                validate_recipe(recipe)
        recipe = default_recipe()
        recipe["layers"] = [None]
        with self.assertRaises(ValueError):
            validate_recipe(recipe)

    def test_stengah_fixture(self):
        field = load_grid(SAMPLE.read_bytes(), SAMPLE.name)
        self.assertEqual(field.shape, (512, 512))
        self.assertGreater(float(np.ptp(field)), .1)

    def test_alpha_is_proportional_symmetric_and_smoothly_boosted(self):
        layer = new_layer("highpass")
        layer["opacity"] = .7
        values = np.array([0., .25, .49, .5, .51, .75, 1.])
        alpha = layer_alpha(values, layer)[..., 0]
        np.testing.assert_allclose(alpha, .7 * 2 * np.abs(values - .5))
        np.testing.assert_allclose(alpha, alpha[::-1])
        layer["alpha_boost"] = 4
        boosted = layer_alpha(values, layer)[..., 0]
        self.assertEqual(boosted[3], 0)
        self.assertTrue(np.all(boosted >= alpha))
        self.assertLess(boosted[1], .7)
        layer["gain"] = 2
        layer["offset"] = .1
        mapped = mapped_coordinate(np.array([-.1, 0., .1]), layer)
        np.testing.assert_allclose(mapped, [.4, .6, .8])

    def test_every_blend_preserves_base_at_zero_detail(self):
        engine = SurfaceEngine(np.full((16, 16), .3))
        recipe = default_recipe()
        expected = engine.render(recipe)[0]
        detail = new_layer("highpass")
        recipe["layers"].append(detail)
        for mode in ("normal", "add", "multiply", "screen", "hue", "colour"):
            detail["blend"] = mode
            np.testing.assert_array_equal(engine.render(recipe)[0], expected)

    def test_inverse_alpha_complements_detail_coverage(self):
        layer = new_layer("highpass")
        layer["opacity"] = .6
        values = np.linspace(-.2, 1.2, 101)
        for boost in (1, 2, 16):
            layer["alpha_boost"] = boost
            layer["mask"] = "amplitude"
            ordinary = layer_alpha(values, layer)
            layer["mask"] = "inverse_amplitude"
            inverse = layer_alpha(values, layer)
            np.testing.assert_allclose(ordinary + inverse, .6, atol=1.e-7)
            np.testing.assert_allclose(inverse, inverse[::-1], atol=1.e-7)
            np.testing.assert_allclose(layer_alpha(np.array([0, .5, 1]), layer)[..., 0], [0, .6, 0])

    def test_original_colour_is_independent_of_residual_and_alpha(self):
        layer = new_layer("highpass")
        height = np.array([.25, .25, .75, .75])
        filtered = np.array([.1, .8, .2, .9])
        expected_alpha = layer_alpha(filtered, layer)
        np.testing.assert_array_equal(colour_coordinate(height, filtered, layer), filtered)
        layer.update(colour_source="original", colour_gain=1.1)
        mapped = colour_coordinate(height, filtered, layer)
        np.testing.assert_allclose(mapped, [.225, .225, .775, .775])
        np.testing.assert_array_equal(layer_alpha(filtered, layer), expected_alpha)

    def test_greyscale_icy_hot_recipe_retains_neutral_regions(self):
        recipe = json.loads((SAMPLE.parent / "greyscale-icy-hot.json").read_text())
        engine = SurfaceEngine(np.full((16, 16), .3))
        actual, reference, _ = engine.render(recipe)
        np.testing.assert_array_equal(actual, reference)
        result = self.engine.render(recipe)[0]
        self.assertTrue(np.any(np.ptp(result, axis=-1) > .01))

    def test_perceptual_blends_transfer_hue_without_dimming_base(self):
        base_lab = np.array([[[.6, .035, .02], [.6, .035, .02]]])
        detail_lab = np.array([[[.4, -.04, .03], [.8, .02, -.04]]])
        base = oklab_to_linear(base_lab)
        detail = oklab_to_linear(detail_lab)
        for mode in ("hue", "colour"):
            result = linear_to_oklab(blend_colour(base, detail, mode))
            np.testing.assert_allclose(result[..., 0], base_lab[..., 0], atol=1.e-6)
            direction = result[..., 1:] / np.linalg.norm(result[..., 1:], axis=-1, keepdims=True)
            expected = detail_lab[..., 1:] / np.linalg.norm(detail_lab[..., 1:], axis=-1, keepdims=True)
            np.testing.assert_allclose(direction, expected, atol=1.e-5)
            chroma_source = base_lab if mode == "hue" else detail_lab
            np.testing.assert_allclose(np.linalg.norm(result[..., 1:], axis=-1),
                                       np.linalg.norm(chroma_source[..., 1:], axis=-1), atol=1.e-6)

    def test_old_recipe_migrates_without_mutating_user_settings(self):
        recipe = default_recipe()
        recipe["version"] = 1
        recipe["layers"].append(new_layer())
        detail = recipe["layers"][1]
        detail.update(mask="energy", mask_gain=12, gain=3)
        detail.pop("alpha_boost")
        migrated = validate_recipe(recipe)
        self.assertEqual(migrated["version"], 2)
        self.assertEqual(migrated["layers"][1]["mask"], "amplitude")
        self.assertEqual(migrated["layers"][1]["alpha_boost"], 1)
        self.assertEqual(migrated["layers"][1]["gain"], 3)
        self.assertEqual(recipe["layers"][1]["mask"], "energy")
        np.testing.assert_array_equal(self.engine.render(recipe)[0], self.engine.render(migrated)[0])

    def test_gamut_reduction_retains_lightness_and_hue(self):
        lab = np.array([[[.25, .4, .3], [.85, -.4, -.3]]])
        rgb = gamut_safe_oklab(lab)
        self.assertTrue(np.all((rgb >= 0) & (rgb <= 1)))
        actual = linear_to_oklab(rgb)
        np.testing.assert_allclose(actual[..., 0], lab[..., 0], atol=1.e-6)
        np.testing.assert_allclose(actual[..., 1] / actual[..., 2], lab[..., 1] / lab[..., 2], atol=1.e-5)

    def test_add_and_multiply_are_gated_by_mapped_detail_alpha(self):
        recipe = default_recipe()
        base = self.engine.render(recipe)[0]
        detail = new_layer("highpass")
        detail.update(gain=1.5, alpha_boost=2)
        recipe["layers"].append(detail)
        field = self.engine.field(self.engine.height(recipe["input"]), recipe["input"], detail)
        coordinate = mapped_coordinate(field, detail)
        colour = gradient(coordinate, detail["stops"], recipe["space"])
        alpha = layer_alpha(coordinate, detail)
        for mode in ("add", "multiply"):
            detail["blend"] = mode
            combined = base + colour if mode == "add" else base * colour
            expected = np.clip(base * (1 - alpha) + combined * alpha, 0, 1)
            np.testing.assert_allclose(self.engine.render(recipe)[0], expected, atol=1.e-7)


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.server = LabServer(("127.0.0.1", 0), np.zeros((8, 8)), "test")
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def post(self, path, body, authenticated=True):
        headers = {"X-Lab-Token": self.server.token} if authenticated else {}
        return urlopen(Request(self.url + path, body, headers), timeout=5)

    def test_render_export_and_rejected_request(self):
        recipe = json.dumps(default_recipe()).encode()
        with self.post("/api/render", recipe) as response:
            self.assertIn("image", json.load(response))
        with self.post("/api/export", recipe) as response:
            self.assertEqual(response.headers["Content-Type"], "image/png")
        with self.assertRaises(HTTPError) as caught:
            self.post("/api/render", recipe, False)
        self.assertEqual(caught.exception.code, 403)

    def test_invalid_load_preserves_source_and_returns_error(self):
        with self.assertRaises(HTTPError) as caught:
            self.post("/api/load?name=x.f32", b"bad")
        self.assertEqual(caught.exception.code, 400)
        self.assertEqual(self.server.name, "test")


if __name__ == "__main__":
    unittest.main()
