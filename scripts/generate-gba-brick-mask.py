#!/usr/bin/env python3
import struct
import sys
import zlib
from pathlib import Path


WIDTH = 1024
HEIGHT = 768
MASK_CONFIGS = {
    "GB": (112, 24, 800, 720, "assets/ui/logo-gb.png", 160, 29, 41, 304, True),
    "GBC": (112, 24, 800, 720, "assets/ui/logo-gbc.png", 145, 60, 26, 311, True),
    "GBA": (32, 64, 960, 640, "assets/ui/logo-gba.png", 240, 28, 392, 722, False),
    "SFC": (128, 48, 768, 672, "assets/ui/logo-snes.png", 260, 75, 26, 254, True),
    "MD": (32, 48, 960, 672, "assets/ui/logo-genesis.png", 142, 34, 441, 729, False),
    "GG": (32, 24, 960, 720, "assets/ui/logo-game-gear.png", 0, 0, 0, 0, False),
}
MASK_COLORS = {
    "GB": ((48, 38, 89, 255), (129, 112, 213, 140)),
    "GBC": ((48, 38, 89, 255), (129, 112, 213, 140)),
    "GBA": ((48, 38, 89, 255), (129, 112, 213, 140)),
    "SFC": ((201, 199, 193, 255), (103, 78, 139, 210)),
    "MD": ((24, 24, 26, 255), (72, 72, 76, 255)),
    "GG": ((20, 21, 23, 255), (70, 72, 76, 255)),
}


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: %s" % path)
    pos = 8
    idat = bytearray()
    width = height = depth = color_type = interlace = None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
            if (depth, color_type, compression, filter_method, interlace) != (8, 6, 0, 0, 0):
                raise ValueError("unsupported PNG format in %s" % path)
        elif kind == b"IDAT":
            idat.extend(chunk)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    rows = []
    offset = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        encoded = raw[offset:offset + stride]
        offset += stride
        row = bytearray(stride)
        for i, value in enumerate(encoded):
            left = row[i - 4] if i >= 4 else 0
            up = previous[i]
            upper_left = previous[i - 4] if i >= 4 else 0
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
                raise ValueError("unsupported PNG filter")
            row[i] = (value + prediction) & 0xff
        rows.append(row)
        previous = row
    pixels = bytearray().join(rows)
    return width, height, pixels


def write_png(path, width, height, pixels):
    def chunk(kind, payload):
        header = struct.pack(">I", len(payload)) + kind + payload
        crc = struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)
        return header + crc

    scanlines = bytearray()
    stride = width * 4
    for y in range(height):
        scanlines.append(0)
        scanlines.extend(pixels[y * stride:(y + 1) * stride])
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def fill_rect(pixels, x0, y0, x1, y1, color):
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(WIDTH, x1), min(HEIGHT, y1)
    for y in range(y0, y1):
        for x in range(x0, x1):
            pixels[(y * WIDTH + x) * 4:(y * WIDTH + x + 1) * 4] = bytes(color)


def fill_circle(pixels, center_x, center_y, radius, color):
    radius_squared = radius * radius
    for y in range(center_y - radius, center_y + radius + 1):
        for x in range(center_x - radius, center_x + radius + 1):
            if 0 <= x < WIDTH and 0 <= y < HEIGHT and (x - center_x) ** 2 + (y - center_y) ** 2 <= radius_squared:
                pixels[(y * WIDTH + x) * 4:(y * WIDTH + x + 1) * 4] = bytes(color)


def add_matte_texture(pixels):
    # Deterministic one-step luminance variation: visible up close, but the
    # four borders still read as one solid injection-moulded plastic color.
    for y in range(HEIGHT):
        for x in range(WIDTH):
            index = (y * WIDTH + x) * 4
            if pixels[index + 3] != 255:
                continue
            variation = ((x * 17 + y * 31 + (x * y) % 7) % 5) - 2
            for channel in range(3):
                pixels[index + channel] = max(0, min(255, pixels[index + channel] + variation))


def blend(dst, src):
    alpha = src[3] / 255.0
    inverse = 1.0 - alpha
    return tuple(int(src[i] * alpha + dst[i] * inverse + 0.5) for i in range(3)) + (255,)


def main():
    system = sys.argv[3].upper() if len(sys.argv) > 3 else "GBA"
    if system not in MASK_CONFIGS:
        raise ValueError("unsupported mask system: %s" % system)
    screen_x, screen_y, screen_width, screen_height, default_logo, target_width, target_height, logo_x, logo_y, rotate_logo = MASK_CONFIGS[system]
    body_color, edge_color = MASK_COLORS[system]
    project_dir = Path(__file__).resolve().parent.parent
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else project_dir / "vendor" / "tg5040" / "res" / ("%s-brick-mask.png" % system.lower())
    logo_path = sys.argv[2] if len(sys.argv) > 2 else default_logo
    if not Path(logo_path).is_absolute():
        logo_path = project_dir / logo_path
    pixels = bytearray(WIDTH * HEIGHT * 4)

    # The transparent window matches each system's integer Native scaling,
    # centered in the Brick's 1024x768 framebuffer.
    screen_right = screen_x + screen_width
    screen_bottom = screen_y + screen_height
    fill_rect(pixels, 0, 0, WIDTH, screen_y, body_color)
    fill_rect(pixels, 0, screen_bottom, WIDTH, HEIGHT, body_color)
    fill_rect(pixels, 0, screen_y, screen_x, screen_bottom, body_color)
    fill_rect(pixels, screen_right, screen_y, WIDTH, screen_bottom, body_color)
    if screen_y:
        fill_rect(pixels, 0, screen_y - 2, WIDTH, screen_y, edge_color)
    if screen_bottom < HEIGHT:
        fill_rect(pixels, 0, screen_bottom, WIDTH, screen_bottom + 2, edge_color)
    if screen_x:
        fill_rect(pixels, screen_x - 2, screen_y, screen_x, screen_bottom, edge_color)
    if screen_right < WIDTH:
        fill_rect(pixels, screen_right, screen_y, screen_right + 2, screen_bottom, edge_color)

    if system in ("SFC", "MD", "GG"):
        add_matte_texture(pixels)
    if system == "SFC":
        # A restrained echo of the North American controller's two-tone
        # purple face buttons makes the mask identifiable without decoration.
        fill_circle(pixels, 947, 618, 14, (84, 57, 125, 255))
        fill_circle(pixels, 979, 650, 14, (84, 57, 125, 255))
        fill_circle(pixels, 979, 618, 14, (155, 135, 185, 255))
        fill_circle(pixels, 947, 650, 14, (155, 135, 185, 255))

    if target_width == 0 or target_height == 0:
        write_png(output, WIDTH, HEIGHT, pixels)
        print(output)
        return

    logo_width, logo_height, logo = read_png(logo_path)
    render_width = target_height if rotate_logo else target_width
    render_height = target_width if rotate_logo else target_height
    for y in range(render_height):
        for x in range(render_width):
            if rotate_logo:
                scaled_x = y
                scaled_y = target_height - 1 - x
            else:
                scaled_x = x
                scaled_y = y
            source_x = min(logo_width - 1, scaled_x * logo_width // target_width)
            source_y = min(logo_height - 1, scaled_y * logo_height // target_height)
            source = tuple(logo[(source_y * logo_width + source_x) * 4:(source_y * logo_width + source_x + 1) * 4])
            if source[3] == 0:
                continue
            if system == "MD":
                # The black Genesis wordmark remains readable on the charcoal
                # bezel as the restrained Model 1 red accent.
                source = (220, 38, 51, source[3])
            index = ((logo_y + y) * WIDTH + logo_x + x) * 4
            destination = tuple(pixels[index:index + 4])
            pixels[index:index + 4] = bytes(blend(destination, source))

    write_png(output, WIDTH, HEIGHT, pixels)
    print(output)


if __name__ == "__main__":
    main()
