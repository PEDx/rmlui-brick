#!/usr/bin/env python3
import struct
import sys
import zlib
from pathlib import Path


GBA_BRIGHTNESS = (
    (0.76, 0.86, 0.86, 0.76),
    (0.86, 1.00, 1.00, 0.86),
    (0.86, 1.00, 1.00, 0.86),
    (0.76, 0.86, 0.86, 0.76),
)
GB_BRIGHTNESS = (
    (0.76, 0.86, 0.86, 0.86, 0.76),
    (0.86, 1.00, 1.00, 1.00, 0.86),
    (0.86, 1.00, 1.00, 1.00, 0.86),
    (0.86, 1.00, 1.00, 1.00, 0.86),
    (0.76, 0.86, 0.86, 0.86, 0.76),
)
SFC_BRIGHTNESS = (
    (0.97, 0.96, 0.97),
    (0.97, 0.96, 0.97),
    (0.82, 0.80, 0.82),
)
GG_BRIGHTNESS = (
    (0.80, 0.90, 1.00, 1.00, 0.90, 0.80),
    (0.84, 0.94, 1.00, 1.00, 0.94, 0.84),
    (0.84, 0.94, 1.00, 1.00, 0.94, 0.84),
    (0.84, 0.94, 1.00, 1.00, 0.94, 0.84),
    (0.80, 0.90, 1.00, 1.00, 0.90, 0.80),
)
LCD_CONFIGS = {
    "GBA": (960, 640, GBA_BRIGHTNESS, "gba-lcd-4x.png"),
    "GB": (800, 720, GB_BRIGHTNESS, "gb-gbc-lcd-5x.png"),
    "GBC": (800, 720, GB_BRIGHTNESS, "gb-gbc-lcd-5x.png"),
    "SFC": (768, 672, SFC_BRIGHTNESS, "sfc-crt-3x.png"),
    "MD": (960, 672, SFC_BRIGHTNESS, "md-crt-3x.png"),
    "GG": (960, 720, GG_BRIGHTNESS, "gg-lcd-6x5.png"),
}


def png_chunk(kind, payload):
    header = struct.pack(">I", len(payload)) + kind + payload
    return header + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)


def main():
    system = sys.argv[2].upper() if len(sys.argv) > 2 else "GBA"
    if system not in LCD_CONFIGS:
        raise ValueError("unsupported LCD system: %s" % system)
    width, height, brightness, default_name = LCD_CONFIGS[system]
    project_dir = Path(__file__).resolve().parent.parent
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else project_dir / "vendor" / "tg5040" / "res" / default_name
    period = len(brightness)

    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            alpha = round((1.0 - brightness[y % period][x % period]) * 255)
            rows.extend((0, 0, 0, alpha))

    png = b"\x89PNG\r\n\x1a\n"
    png += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += png_chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += png_chunk(b"IEND", b"")
    with open(output, "wb") as handle:
        handle.write(png)
    print(output)


if __name__ == "__main__":
    main()
