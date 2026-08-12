#!/usr/bin/env python3
"""Convert a grayscale-on-white RGBA PNG to black artwork with real alpha."""

import struct
import sys
import zlib


def paeth(left, up, upper_left):
    estimate = left + up - upper_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= up_distance and left_distance <= upper_left_distance:
        return left
    if up_distance <= upper_left_distance:
        return up
    return upper_left


def read_rgba_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: %s" % path)

    position = 8
    compressed = bytearray()
    width = height = None
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        position += length + 12
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValueError("expected a non-interlaced 8-bit RGBA PNG")
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break

    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    previous = bytearray(stride)
    pixels = bytearray()
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        encoded = raw[offset:offset + stride]
        offset += stride
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - 4] if index >= 4 else 0
            up = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                prediction = 0
            elif filter_type == 1:
                prediction = left
            elif filter_type == 2:
                prediction = up
            elif filter_type == 3:
                prediction = (left + up) // 2
            elif filter_type == 4:
                prediction = paeth(left, up, upper_left)
            else:
                raise ValueError("unsupported PNG filter: %d" % filter_type)
            row[index] = (value + prediction) & 0xFF
        pixels.extend(row)
        previous = row
    return width, height, pixels


def chunk(kind, payload):
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_rgba_png(path, width, height, pixels):
    stride = width * 4
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        scanlines.extend(pixels[y * stride:(y + 1) * stride])
    output = b"\x89PNG\r\n\x1a\n"
    output += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    output += chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
    output += chunk(b"IEND", b"")
    open(path, "wb").write(output)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: convert-white-matte-to-alpha.py INPUT.png OUTPUT.png")
    width, height, pixels = read_rgba_png(sys.argv[1])
    for index in range(0, len(pixels), 4):
        matte = min(pixels[index], pixels[index + 1], pixels[index + 2])
        source_alpha = pixels[index + 3]
        pixels[index:index + 4] = bytes((0, 0, 0, (255 - matte) * source_alpha // 255))
    write_rgba_png(sys.argv[2], width, height, pixels)
    print("%s: %dx%d transparent matte converted" % (sys.argv[2], width, height))


if __name__ == "__main__":
    main()
