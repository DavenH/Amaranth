#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

from compare_png_psnr import read_png_rgb


def parse_region(value, width, height):
    parts = [float(part) for part in value.split(",")]

    if len(parts) != 4:
        raise ValueError(f"region must be x,y,width,height: {value}")

    x, y, w, h = parts

    if 0.0 <= x <= 1.0 and 0.0 <= y <= 1.0 and 0.0 < w <= 1.0 and 0.0 < h <= 1.0:
        x *= width
        y *= height
        w *= width
        h *= height

    left = max(0, min(width, int(round(x))))
    top = max(0, min(height, int(round(y))))
    right = max(left, min(width, int(round(x + w))))
    bottom = max(top, min(height, int(round(y + h))))

    if right <= left or bottom <= top:
        raise ValueError(f"empty region after clipping: {value}")

    return left, top, right, bottom


def region_stats(rgb, width, region):
    left, top, right, bottom = region
    count = 0
    total = 0.0
    total_squared = 0.0

    for y in range(top, bottom):
        row = y * width * 3

        for x in range(left, right):
            offset = row + x * 3
            r = rgb[offset]
            g = rgb[offset + 1]
            b = rgb[offset + 2]
            luma = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0
            total += luma
            total_squared += luma * luma
            count += 1

    mean = total / count
    variance = max(0.0, total_squared / count - mean * mean)
    return mean, variance ** 0.5


def main():
    parser = argparse.ArgumentParser(description="Assert simple luma stats for PNG regions.")
    parser.add_argument("path")
    parser.add_argument("--region", action="append", default=["0,0,1,1"],
                        help="Region as x,y,width,height in pixels or normalized 0..1 values")
    parser.add_argument("--min-mean", type=float, default=0.0)
    parser.add_argument("--max-mean", type=float, default=1.0)
    parser.add_argument("--min-stddev", type=float, default=0.0)
    parser.add_argument("--label", default="")
    args = parser.parse_args()

    path = Path(args.path)
    width, height, rgb = read_png_rgb(path)
    ok = True

    for index, raw_region in enumerate(args.region):
        region = parse_region(raw_region, width, height)
        mean, stddev = region_stats(rgb, width, region)
        label = args.label or f"region-{index}"
        print(f"{label} {raw_region} mean={mean:.5f} stddev={stddev:.5f}")

        if mean < args.min_mean or mean > args.max_mean or stddev < args.min_stddev:
            ok = False

    return 0 if ok else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(2)
