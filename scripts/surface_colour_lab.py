#!/usr/bin/env python3
"""Local Surface Colour Lab. See docs/surface-colour-lab.md."""

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import secrets
import threading
import time
from urllib.parse import parse_qs, urlsplit
import webbrowser

from surface_colour_engine import (
    PALETTES, SurfaceEngine, default_recipe, demo_grid, image_url, load_grid, png_bytes,
)

HERE = Path(__file__).resolve().parent
SAMPLE = HERE / "fixtures/surface-colour-lab/stengah-b0-spy1.f32"


class LabServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, source, name):
        super().__init__(address, LabHandler)
        self.engine = SurfaceEngine(source)
        self.name = name
        self.token = secrets.token_urlsafe(24)
        self.lock = threading.Lock()


class LabHandler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def reply(self, status, value, content_type="application/json"):
        body = json.dumps(value).encode() if content_type == "application/json" else value
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def info(self):
        source = self.server.engine.source
        return {"name": self.server.name, "width": source.shape[1], "height": source.shape[0],
                "min": float(source.min()), "max": float(source.max())}

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/":
            self.reply(200, (HERE / "surface_colour_lab.html").read_bytes(), "text/html; charset=utf-8")
        elif path == "/api/info":
            with self.server.lock:
                self.reply(200, {**self.info(), "token": self.server.token,
                                 "recipe": default_recipe(), "palettes": PALETTES})
        else:
            self.reply(404, {"error": "Not found"})

    def do_POST(self):
        if not secrets.compare_digest(self.headers.get("X-Lab-Token", ""), self.server.token):
            self.reply(403, {"error": "Reload the lab page to reconnect"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 24 * 1024 * 1024:
                raise ValueError("Request must be 1 byte–24 MB")
            data = self.rfile.read(length)
            url = urlsplit(self.path)
            with self.server.lock:
                if url.path == "/api/load":
                    name = parse_qs(url.query).get("name", ["surface.f32"])[0]
                    self.server.engine = SurfaceEngine(load_grid(data, name))
                    self.server.name = Path(name).name
                    self.reply(200, self.info())
                elif url.path in ("/api/render", "/api/export"):
                    started = time.perf_counter()
                    recipe = json.loads(data)
                    result, reference, diagnostics = self.server.engine.render(recipe)
                    if url.path == "/api/export":
                        self.reply(200, png_bytes(result), "image/png")
                    else:
                        index = max(0, min(len(recipe["layers"]) - 1, int(recipe.get("selected", 0))))
                        self.reply(200, {**self.server.engine.inspect_layer(recipe, index),
                                         "image": image_url(result), "reference": image_url(reference),
                                         "layers": diagnostics,
                                         "ms": round((time.perf_counter() - started) * 1000)})
                else:
                    self.reply(404, {"error": "Not found"})
        except (ValueError, TypeError, KeyError, IndexError, OverflowError) as error:
            self.reply(400, {"error": str(error)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, help="Cycle .f32 or row-major CSV")
    parser.add_argument("--demo", action="store_true", help="Use a synthetic sine/ripple field")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--recipe", type=Path, help="JSON recipe (headless render only)")
    parser.add_argument("--output", type=Path, help="Render PNG and exit instead of starting the UI")
    args = parser.parse_args()
    if args.recipe and not args.output:
        parser.error("Use --recipe with --output; in the UI use Load recipe")
    path = args.input or SAMPLE
    if args.demo:
        source, name = demo_grid(), "Sine + ripple demo"
    else:
        if not path.is_file():
            parser.error(f"No grid at {path}; provide --input or use --demo")
        source, name = load_grid(path.read_bytes(), path.name), path.name
    if args.output:
        recipe = json.loads(args.recipe.read_text()) if args.recipe else default_recipe()
        result, _, _ = SurfaceEngine(source).render(recipe)
        args.output.write_bytes(png_bytes(result))
        print(args.output)
        return
    server = LabServer(("127.0.0.1", args.port), source, name)
    url = f"http://127.0.0.1:{server.server_port}"
    print(f"Surface Colour Lab: {url}\nSource: {name}\nCtrl-C to stop.", flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
