#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path

from assert_png_stats import region_stats
from compare_png_psnr import read_png_rgb


def bounds_tuple(bounds):
    return (
        float(bounds["x"]),
        float(bounds["y"]),
        float(bounds["width"]),
        float(bounds["height"]),
    )


def find_diagnostics(report):
    for result in report.get("results", []):
        if result.get("type") == "inspectOpenGLDiagnostics" and result.get("ok"):
            return result.get("data", {})

    raise ValueError("report does not contain a successful inspectOpenGLDiagnostics result")


def panel_regions(diagnostics):
    canvas_x, canvas_y, _, _ = bounds_tuple(diagnostics["canvasScreenBounds"])

    for panel in diagnostics.get("panels", []):
        if not panel.get("created") or not panel.get("showing") or not panel.get("nonEmpty"):
            raise ValueError(f"GL panel is not ready: {panel.get('id')}")

        screen_x, screen_y, width, height = bounds_tuple(panel["screenBounds"])
        yield panel["id"], (screen_x - canvas_x, screen_y - canvas_y, width, height)


def main():
    parser = argparse.ArgumentParser(description="Assert Cycle2 GL panel luma stats from automation diagnostics.")
    parser.add_argument("png_path")
    parser.add_argument("report_path")
    parser.add_argument("--min-mean", type=float, default=0.02)
    parser.add_argument("--max-mean", type=float, default=1.0)
    parser.add_argument("--min-stddev", type=float, default=0.015)
    args = parser.parse_args()

    width, height, rgb = read_png_rgb(Path(args.png_path))

    with open(args.report_path, "r", encoding="utf-8") as handle:
        diagnostics = find_diagnostics(json.load(handle))

    ok = True
    for label, region in panel_regions(diagnostics):
        left = max(0, min(width, int(round(region[0]))))
        top = max(0, min(height, int(round(region[1]))))
        right = max(left, min(width, int(round(region[0] + region[2]))))
        bottom = max(top, min(height, int(round(region[1] + region[3]))))

        if right <= left or bottom <= top:
            raise ValueError(f"empty discovered region for {label}: {region}")

        mean, stddev = region_stats(rgb, width, (left, top, right, bottom))
        print(f"{label} {left},{top},{right - left},{bottom - top} mean={mean:.5f} stddev={stddev:.5f}")

        if mean < args.min_mean or mean > args.max_mean or stddev < args.min_stddev:
            ok = False

    return 0 if ok else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(2)
