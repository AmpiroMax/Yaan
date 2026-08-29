#!/usr/bin/env python3
"""
Module: tools
File: tools/archive_frame.py

Responsibility:
- Copy a tour screenshot into docs/acceptance/ at the NATIVE internal
  resolution, by an EXACT integer box average of the upscaled framebuffer.

Key items:
- box_downscale(): NxN mean per output pixel, integer arithmetic.
- main(): src -> docs/acceptance/<name>, factor inferred from the size.

Dependencies:
- Uses: python3 stdlib only (zlib, struct). No Pillow, no sips.
- Used by: render's acceptance archive step (Rule 27).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- DO NOT USE `sips -z` FOR THIS. `save_screenshot` captures the FINAL upscaled
  backbuffer, so a 640x360 frame arrives as an exact 4x integer replication of
  itself. Undoing that is a box average and nothing else; `sips -z` resamples
  through a smoothing filter, which invents intermediate colours that the
  64-colour palette pass never produced and softens the pixel edges the whole
  look is built on. The archived frame must be the frame that was rendered.
"""

import struct
import sys
import zlib
from pathlib import Path

def read_png(path):
    data = Path(path).read_bytes()
    pos, idat = 8, b""
    width = height = channels = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, colour, _, _, _ = struct.unpack(">IIBBBBB", chunk)
            if depth != 8:
                raise SystemExit(f"{path}: only 8-bit PNGs are handled")
            channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = width * channels
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        filt = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if filt == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif filt == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif filt == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, channels, bytes(out)

def write_png(path, width, height, channels, pixels):
    colour = {1: 0, 2: 4, 3: 2, 4: 6}[channels]
    raw = bytearray()
    stride = width * channels
    for y in range(height):
        raw.append(0)  # filter: none — the archive is small and read by eye
        raw += pixels[y * stride:(y + 1) * stride]

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b""))

def box_downscale(width, height, channels, pixels, factor):
    ow, oh = width // factor, height // factor
    out = bytearray(ow * oh * channels)
    half = (factor * factor) // 2
    for y in range(oh):
        for x in range(ow):
            for c in range(channels):
                total = 0
                for dy in range(factor):
                    row = (y * factor + dy) * width
                    for dx in range(factor):
                        total += pixels[(row + x * factor + dx) * channels + c]
                # Round half up, in integers: the archive must be reproducible
                # bit for bit from the same source frame on any machine.
                out[(y * ow + x) * channels + c] = (total + half) // (factor * factor)
    return ow, oh, bytes(out)

def main(argv):
    if len(argv) < 3:
        raise SystemExit("usage: archive_frame.py <src.png> <dst.png> [native_w]")
    src, dst = argv[1], argv[2]
    native_w = int(argv[3]) if len(argv) > 3 else 640
    width, height, channels, pixels = read_png(src)
    if width % native_w != 0:
        raise SystemExit(f"{src}: {width} is not an integer multiple of {native_w}")
    factor = width // native_w
    if factor == 1:
        Path(dst).write_bytes(Path(src).read_bytes())
        print(f"{dst}: already native ({width}x{height})")
        return
    ow, oh, out = box_downscale(width, height, channels, pixels, factor)
    write_png(dst, ow, oh, channels, out)
    print(f"{dst}: {width}x{height} /{factor} box average -> {ow}x{oh}")

if __name__ == "__main__":
    main(sys.argv)
