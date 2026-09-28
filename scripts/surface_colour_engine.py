"""Vectorized, display-only scalar colour experiments. Arrays use image y/x order."""

import base64
from collections import OrderedDict
import io
import math
import struct

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

MAX_PIXELS = 1024 * 1024
PALETTES = {
    "Blue": [[0, "#041020"], [.3, "#244663"], [.5, "#7d919c"], [.75, "#b1dce7"], [1, "#f4fcff"]],
    "Copper / ice": [[0, "#031426"], [.35, "#23475f"], [.5, "#53616b"], [.65, "#b9663e"], [.85, "#f7ae64"], [1, "#fff3bc"]],
    "Inferno-like": [[0, "#08051d"], [.25, "#57106e"], [.5, "#bb3754"], [.75, "#f98d35"], [1, "#fcf9b7"]],
    "Bipolar": [[0, "#153366"], [.25, "#8baadc"], [.5, "#6c6a70"], [.75, "#db8562"], [1, "#ffe4b5"]],
    "Greyscale": [[0, "#000000"], [1, "#ffffff"]],
}


def default_recipe():
    return {"version": 1, "input": "spy", "space": "oklab", "aspect": 1.64,
            "layers": [new_layer("raw", "Blue")]}


def new_layer(kind="bandpass", palette="Copper / ice"):
    return {"name": "Base" if kind == "raw" else "Detail", "enabled": True,
            "filter": kind, "sigma": 2., "outer": 8., "axis": "both",
            "gain": 1. if kind in ("raw", "lowpass") else 8., "offset": 0.,
            "opacity": 1. if kind == "raw" else .35, "blend": "normal",
            "mask": "none" if kind == "raw" else "energy", "mask_gain": 12.,
            "stops": [list(stop) for stop in PALETTES[palette]]}


def number(value, minimum, maximum, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be a number")
    result = float(value)
    if not math.isfinite(result) or not minimum <= result <= maximum:
        raise ValueError(f"{name} must be between {minimum} and {maximum}")
    return result


def validate_recipe(recipe):
    if not isinstance(recipe, dict) or recipe.get("version") != 1:
        raise ValueError("Unsupported recipe version")
    if recipe.get("input") not in ("spy", "peak", "unit"):
        raise ValueError("Unknown input scaling")
    if recipe.get("space") not in ("srgb", "linear", "oklab"):
        raise ValueError("Unknown gradient colour space")
    number(recipe.get("aspect", 1.64), .1, 10, "Aspect")
    layers = recipe.get("layers")
    if not isinstance(layers, list) or not 1 <= len(layers) <= 12:
        raise ValueError("Use 1–12 layers")
    for layer in layers:
        if not isinstance(layer, dict):
            raise ValueError("Each layer must be an object")
        if layer.get("filter") not in ("raw", "lowpass", "highpass", "bandpass"):
            raise ValueError("Unknown filter")
        if layer.get("axis") not in ("both", "x", "y"):
            raise ValueError("Unknown filter axis")
        if layer.get("blend") not in ("normal", "add", "multiply", "screen"):
            raise ValueError("Unknown blend mode")
        if layer.get("mask") not in ("none", "energy"):
            raise ValueError("Unknown mask")
        for key, lo, hi in (("sigma", 0, 64), ("outer", 0, 128), ("gain", 0, 100),
                            ("offset", -1, 1), ("opacity", 0, 1), ("mask_gain", 0, 100)):
            number(layer.get(key), lo, hi, key)
        if layer["filter"] == "bandpass" and layer["outer"] <= layer["sigma"]:
            raise ValueError("Band-pass outer sigma must exceed inner sigma")
        validate_stops(layer.get("stops"))
    return recipe


def validate_stops(stops):
    if not isinstance(stops, list) or not 2 <= len(stops) <= 32:
        raise ValueError("A gradient needs 2–32 stops")
    if any(not isinstance(stop, list) or len(stop) != 2 for stop in stops):
        raise ValueError("Each stop must contain a position and a #RRGGBB colour")
    positions = [number(stop[0], 0, 1, "Stop position") for stop in stops]
    if any(b <= a for a, b in zip(positions, positions[1:])):
        raise ValueError("Gradient positions must be strictly increasing")
    for _, colour in stops:
        if not isinstance(colour, str) or len(colour) != 7 or not colour.startswith("#"):
            raise ValueError("Colours must use #RRGGBB")
        int(colour[1:], 16)


def load_grid(data, name):
    """Cycle f32: two LE int32 dimensions then column-major floats, bottom-up y."""
    if name.lower().endswith(".f32"):
        if len(data) < 8:
            raise ValueError("Truncated f32 header")
        columns, rows = struct.unpack("<ii", data[:8])
        if min(columns, rows) < 2 or columns * rows > MAX_PIXELS:
            raise ValueError("Invalid dimensions (maximum 1 megapixel)")
        if len(data) != 8 + columns * rows * 4:
            raise ValueError("f32 dimensions do not match the file length")
        values = np.frombuffer(data, dtype="<f4", offset=8).reshape(columns, rows).T[::-1].copy()
    elif name.lower().endswith(".csv"):
        values = np.loadtxt(io.BytesIO(data), delimiter=",")
    else:
        raise ValueError("Load a Cycle .f32 grid or a numeric .csv matrix")
    return checked_grid(values)


def checked_grid(values):
    values = np.asarray(values, dtype=np.float32)
    if values.ndim != 2 or min(values.shape) < 2 or values.size > MAX_PIXELS:
        raise ValueError("Grid must be a 2D matrix, 2+ samples per axis, at most 1 megapixel")
    if not np.isfinite(values).all():
        raise ValueError("Grid contains NaN or infinity")
    values = values.copy()
    values.flags.writeable = False
    return values


def demo_grid():
    y, x = np.mgrid[0:512, 0:768].astype(np.float32)
    x /= 767
    y /= 511
    return .7 * np.sin(2 * np.pi * (4 * y + .3 * np.sin(5 * x))) + .04 * np.sin(2 * np.pi * 90 * y) * (1 - x)


def srgb_to_linear(rgb):
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)


def linear_to_srgb(rgb):
    rgb = np.maximum(rgb, 0)
    return np.where(rgb <= .0031308, rgb * 12.92, 1.055 * rgb ** (1 / 2.4) - .055)


def linear_to_oklab(rgb):
    lms = rgb @ np.array([[.4122214708, .5363325363, .0514459929],
                         [.2119034982, .6806995451, .1073969566],
                         [.0883024619, .2817188376, .6299787005]]).T
    return np.cbrt(lms) @ np.array([[.2104542553, .793617785, -.0040720468],
                                  [1.9779984951, -2.428592205, .4505937099],
                                  [.0259040371, .7827717662, -.808675766]]).T


def oklab_to_linear(lab):
    lms = lab @ np.array([[1, .3963377774, .2158037573],
                         [1, -.1055613458, -.0638541728],
                         [1, -.0894841775, -1.291485548]]).T
    return (lms ** 3) @ np.array([[4.0767416621, -3.3077115913, .2309699292],
                                [-1.2684380046, 2.6097574011, -.3413193965],
                                [-.0041960863, -.7034186147, 1.707614701]]).T


def gradient(values, stops, space):
    """Return linear RGB; interpolation is performed in the requested space."""
    colours = np.array([[int(c[i:i + 2], 16) / 255 for i in (1, 3, 5)] for _, c in stops])
    positions = [p for p, _ in stops]
    if space != "srgb":
        colours = srgb_to_linear(colours)
    if space == "oklab":
        colours = linear_to_oklab(colours)
    mapped = np.stack([np.interp(values, positions, colours[:, i]) for i in range(3)], axis=-1)
    if space == "srgb":
        mapped = srgb_to_linear(mapped)
    elif space == "oklab":
        mapped = oklab_to_linear(mapped)
    return np.clip(mapped, 0, 1).astype(np.float32)


def png_bytes(rgb):
    pixels = np.rint(np.clip(linear_to_srgb(rgb), 0, 1) * 255).astype(np.uint8)
    stream = io.BytesIO()
    Image.fromarray(pixels).save(stream, format="PNG")
    return stream.getvalue()


def image_url(rgb):
    return "data:image/png;base64," + base64.b64encode(png_bytes(rgb)).decode("ascii")


class SurfaceEngine:
    def __init__(self, source):
        self.source = checked_grid(source)
        self.cache = OrderedDict()
        self.blur_count = 0

    def height(self, scaling):
        peak = float(np.max(np.abs(self.source)))
        if scaling == "unit":
            return np.clip(self.source, 0, 1)
        value = self.source / max(peak, 1.e-20)
        if scaling == "spy":
            value = value * 3
            value = value / (1 + np.abs(value))
        return .5 + .5 * value

    def blur(self, height, scaling, sigma, axis):
        key = (scaling, sigma, axis)
        if key not in self.cache:
            radii = (sigma if axis != "x" else 0, sigma if axis != "y" else 0)
            self.cache[key] = gaussian_filter(height, sigma=radii, mode="reflect")
            self.blur_count += 1
            if len(self.cache) > 12:
                self.cache.popitem(last=False)
        self.cache.move_to_end(key)
        return self.cache[key]

    def field(self, height, scaling, layer):
        kind = layer["filter"]
        if kind == "raw":
            return height
        low = self.blur(height, scaling, layer["sigma"], layer["axis"])
        if kind == "lowpass":
            return low
        if kind == "highpass":
            return height - low
        return low - self.blur(height, scaling, layer["outer"], layer["axis"])

    def render(self, recipe):
        validate_recipe(recipe)
        height = self.height(recipe["input"])
        result = np.zeros((*height.shape, 3), dtype=np.float32)
        reference = gradient(height, recipe["layers"][0]["stops"], recipe["space"])
        diagnostics = []
        for layer in recipe["layers"]:
            if not layer.get("enabled", True):
                diagnostics.append(None)
                continue
            field = self.field(height, recipe["input"], layer)
            signed = layer["filter"] in ("highpass", "bandpass")
            centred = field if signed else field - .5
            coordinate = .5 + centred * layer["gain"] + layer["offset"]
            colour = gradient(coordinate, layer["stops"], recipe["space"])
            alpha = np.full((*height.shape, 1), layer["opacity"], dtype=np.float32)
            if layer["mask"] == "energy":
                alpha *= np.clip(np.abs(centred) * layer["mask_gain"], 0, 1)[..., None]
            mode = layer["blend"]
            if mode == "add":
                combined = result + colour
            elif mode == "multiply":
                combined = result * colour
            elif mode == "screen":
                combined = 1 - (1 - result) * (1 - colour)
            else:
                combined = colour
            result = np.clip(result * (1 - alpha) + combined * alpha, 0, 1)
            diagnostics.append({"min": float(field.min()), "max": float(field.max()),
                                "clipped": float(np.mean((coordinate < 0) | (coordinate > 1)))})
        return result, reference, diagnostics

    def inspect_layer(self, recipe, index):
        layer = recipe["layers"][index]
        field = self.field(self.height(recipe["input"]), recipe["input"], layer)
        centred = field if layer["filter"] in ("highpass", "bandpass") else field - .5
        values = .5 + centred * layer["gain"] + layer["offset"]
        return {
            "field": image_url(gradient(values, PALETTES["Greyscale"], "srgb")),
            "gradient": image_url(gradient(np.linspace(0, 1, 512)[None, :], layer["stops"], recipe["space"]))
        }
