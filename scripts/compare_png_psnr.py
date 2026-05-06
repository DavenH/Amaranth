#!/usr/bin/env python3
import argparse
import math
import struct
import sys
import zlib


def read_png_rgb(path):
    with open(path, "rb") as handle:
        data = handle.read()

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG file")

    offset = 8
    width = None
    height = None
    bit_depth = None
    color_type = None
    interlace = None
    compressed = bytearray()

    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        chunk_type = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        offset += 12 + length

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", payload)
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise ValueError(f"{path}: missing IHDR")

    if bit_depth != 8 or color_type not in (2, 6) or interlace != 0:
        raise ValueError(f"{path}: only 8-bit non-interlaced RGB/RGBA PNGs are supported")

    channels = 3 if color_type == 2 else 4
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    rows = []
    previous = bytearray(stride)
    pos = 0

    for _ in range(height):
        filter_type = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + stride])
        pos += stride

        def left(i):
            return row[i - channels] if i >= channels else 0

        def up(i):
            return previous[i]

        def upper_left(i):
            return previous[i - channels] if i >= channels else 0

        if filter_type == 1:
            for i in range(stride):
                row[i] = (row[i] + left(i)) & 0xff
        elif filter_type == 2:
            for i in range(stride):
                row[i] = (row[i] + up(i)) & 0xff
        elif filter_type == 3:
            for i in range(stride):
                row[i] = (row[i] + ((left(i) + up(i)) // 2)) & 0xff
        elif filter_type == 4:
            for i in range(stride):
                a = left(i)
                b = up(i)
                c = upper_left(i)
                p = a + b - c
                pa = abs(p - a)
                pb = abs(p - b)
                pc = abs(p - c)
                predictor = a if pa <= pb and pa <= pc else b if pb <= pc else c
                row[i] = (row[i] + predictor) & 0xff
        elif filter_type != 0:
            raise ValueError(f"{path}: unsupported PNG filter {filter_type}")

        if channels == 4:
            rgb = bytearray()
            for i in range(0, len(row), 4):
                rgb.extend(row[i:i + 3])
            rows.append(bytes(rgb))
        else:
            rows.append(bytes(row))

        previous = row

    return width, height, b"".join(rows)


def psnr(reference, candidate):
    ref_width, ref_height, ref = read_png_rgb(reference)
    cand_width, cand_height, cand = read_png_rgb(candidate)

    if (ref_width, ref_height) != (cand_width, cand_height):
        raise ValueError(
                f"dimension mismatch: reference={ref_width}x{ref_height}, "
                f"candidate={cand_width}x{cand_height}")

    squared_error = 0
    for a, b in zip(ref, cand):
        diff = a - b
        squared_error += diff * diff

    if squared_error == 0:
        return math.inf

    mse = squared_error / len(ref)
    return 20.0 * math.log10(255.0 / math.sqrt(mse))


def main():
    parser = argparse.ArgumentParser(description="Compare two PNGs with RGB PSNR.")
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--min-psnr", type=float)
    args = parser.parse_args()

    value = psnr(args.reference, args.candidate)
    printable = "inf" if math.isinf(value) else f"{value:.4f}"
    print(f"psnr={printable}dB")

    if args.min_psnr is not None and value < args.min_psnr:
        return 1

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(2)
