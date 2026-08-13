#!/usr/bin/env python3
"""
Created: 13:08:2026 - 16:28:00
Last updated: 13:08:2026 - 20:05:00
Module: tools
File: tools/measure_text_separation.py

Responsibility:
- Answer "is this on-screen text separated from what it sits on?" as a NUMBER,
  in the ruler the project already owns: 2 * PALETTE_SHADE_STEP_REF = 0.157 of
  quantizer brightness (weights 0.30/0.59/0.11, fs_upscale.sc:47). Two readings,
  because they answer different questions:
    pixels -- each ink pixel against the world pixel it COVERS (the control
              arm's value at that coordinate). This is the reading that shows
              where a defect lives: split it by background brightness and the
              distribution talks.
    edges  -- each ink pixel against every 4-neighbour that is not ink, AS
              PRESENTED. This is the reading a fix must move: a plate, a shadow
              or an outline all change what the letter abuts, and the reader
              reads edges, not covered pixels.

Key items:
- ink_mask(): membership from the ARM, with a per-pixel noise exclusion.
- report_pixels() / report_edges(): the two readings above.
- main(): control.png test.png [--noise noise.png] [--gate N]

Dependencies:
- Uses: python3 stdlib only (zlib, struct). No Pillow, no numpy.
- Used by: zone ui at Rule 27 acceptance; anyone shipping HUD/menu text.

Notes:
- THE INSTRUMENT MUST NOT FIND ITS SUBJECT BY THE PROPERTY UNDER TEST, and that
  is why membership is never "looks like ink" (bright, or pale-ish, or inside
  the HUD rectangle). A pale glyph over a white cloud is exactly the case such a
  rule loses, which is the case the instrument exists for. Membership comes from
  an ARM -- the same world rendered with the text off and with it on -- and the
  VALUE comes from the frames. If you are tempted to simplify this into a colour
  test, you are about to build a metric that passes precisely when the defect is
  worst.
- THE ARM NEEDS A NOISE FLOOR, because two runs of the same binary are not
  bit-equal: foliage, clouds and streaming move a little. Shoot a THIRD run with
  the control's own keys and pass it as --noise; every pixel where the two
  controls disagree is dropped BY NAME, not by raising the difference threshold.
  Raising the threshold instead would silently drop the weakest ink -- the very
  pixels the measurement is about.
- SAME-COLOURED NEIGHBOURS ARE NOT EDGES. Two adjacent pixels of one colour are
  one region; counting the boundary between them as a lost edge inflates the
  failure count with pixels no reader can see as an edge at all.
- A run that owns NOTHING says so and exits non-zero. A measurement of nothing
  must never present as a measurement of zero (this project has already paid for
  that lesson twice in the capture path).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not "improve" the mask into a colour or rectangle test (see Notes).
- Frames must be at the internal resolution (box-average the 4x capture first,
  tools/archive_frame.py) -- averaging blends ink into background and both
  readings drift toward passing.
"""
"""
UPD:
- 13:08:2026 - 16:28:00: Created by zone ui, at the lead's approval, out of the
  readout-plate acceptance (docs/acceptance/README.md, the ui section).
- 13:08:2026 - 17:05:00: Header timestamps corrected -- they were written ahead of
  the clock, and this file's own subject is measurements that must not flatter.
- 13:08:2026 - 19:36:00: --either: ownership without the "drawn lighter" clause,
  for the crosshair. Its outline is BLACK, so over a bright sky the arm draws the
  mark darker and the default mask owned nothing -- on precisely the ground the
  outline exists for. The default is unchanged, so every earlier reading stands.
- 13:08:2026 - 20:05:00: Метка времени записи выше приведена к часам — была написана
  вперёд, а этот файл о замерах, которые не вправе льстить.
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

# The ruler. Two quantizer steps, the same threshold the moon row uses
# (MOON_SKY_LUMA_SEPARATION_MIN in docs/NUMBERS.md).
SHADE_STEP = 0.0784
SEPARATION_MIN = 2 * SHADE_STEP

# Quantizer brightness weights (fs_upscale.sc:47), NOT Euclidean RGB and NOT
# sRGB luminance: the palette pass is what decides whether two values collapse
# into one entry, so it is the metric that decides whether text survives.
W_R, W_G, W_B = 0.30, 0.59, 0.11


def read_png(path):
    """8-bit RGB/RGBA PNG -> (width, height, channels, bytearray)."""
    data = Path(path).read_bytes()
    pos, idat = 8, b""
    w = h = ch = 0
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            w, h, depth, colour, _, _, _ = struct.unpack(">IIBBBBB", chunk)
            if depth != 8:
                raise SystemExit(f"{path}: only 8-bit PNGs are handled")
            ch = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * ch
    out = bytearray(h * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, ch, out


def luma(pix, i):
    return (W_R * pix[i] + W_G * pix[i + 1] + W_B * pix[i + 2]) / 255.0


def chan_diff(a, b, i):
    return max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))


def ink_mask(w, h, ch, ctrl, test, noise, gate, either=False):
    """Pixels the mark OWNS, decided by the arm alone.

    By default ownership also requires the pixel to be drawn LIGHTER, because
    every text in this project is pale ink and that condition throws out the
    world's own darkening. --either drops it, for a mark that is legitimately
    DARKER than what it covers: the crosshair carries a black outline, and over
    a bright sky the outline is the half of it that does the work. Without the
    flag such a mark owns nothing over bright ground -- and "nothing owned" on
    exactly the background the mark exists to survive would read as a passing
    frame while measuring an empty set.
    """
    ink = set()
    unstable = 0
    for y in range(h):
        row = y * w
        for x in range(w):
            i = (row + x) * ch
            if noise is not None and chan_diff(ctrl, noise, i) >= gate:
                unstable += 1
                continue
            if chan_diff(ctrl, test, i) < gate:
                continue
            if either or luma(test, i) > luma(ctrl, i):
                ink.add((x, y))
    return ink, unstable


def report_pixels(w, ch, ctrl, test, ink):
    rows = [(abs(luma(test, (y * w + x) * ch) - luma(ctrl, (y * w + x) * ch)),
             luma(ctrl, (y * w + x) * ch)) for (x, y) in ink]
    def line(name, sel):
        if not sel:
            print(f"  {name}: none")
            return
        d = sorted(v for v, _ in sel)
        fail = sum(1 for v in d if v < SEPARATION_MIN)
        print(f"  {name}: n={len(d)}  median {d[len(d) // 2]:.3f}  "
              f"below {SEPARATION_MIN:.3f}: {fail} = {100.0 * fail / len(d):.1f}%")
    print("ink against the pixels it covers:")
    line("all", rows)
    line("over BRIGHT background (>0.55)", [r for r in rows if r[1] > 0.55])
    line("over DARK background (<=0.55)", [r for r in rows if r[1] <= 0.55])


def report_edges(w, h, ch, test, ink):
    edges = []
    worst = {}
    for (x, y) in ink:
        i = (y * w + x) * ch
        li = luma(test, i)
        col = (test[i], test[i + 1], test[i + 2])
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h) or (nx, ny) in ink:
                continue
            j = (ny * w + nx) * ch
            if (test[j], test[j + 1], test[j + 2]) == col:
                continue  # one region, not an edge
            v = abs(li - luma(test, j))
            edges.append((v, col))
            worst[(x, y)] = min(worst.get((x, y), 9.0), v)
    if not edges:
        print("glyph edges: none")
        return
    d = sorted(v for v, _ in edges)
    fail = [c for v, c in edges if v < SEPARATION_MIN]
    lost = sum(1 for v in worst.values() if v < SEPARATION_MIN)
    print("ink against what it abuts, as presented:")
    print(f"  edges n={len(d)}  median {d[len(d) // 2]:.3f}  p10 {d[len(d) // 10]:.3f}  "
          f"below {SEPARATION_MIN:.3f}: {len(fail)} = {100.0 * len(fail) / len(d):.1f}%")
    print(f"  ink pixels with at least one lost edge: {lost} = "
          f"{100.0 * lost / len(worst):.1f}%")
    if fail:
        # WHOSE failures they are decides whether a fix is due: a plate outline
        # or a stray world pixel in the mask is not a letter.
        tally = {}
        for c in fail:
            tally[c] = tally.get(c, 0) + 1
        top = sorted(tally.items(), key=lambda kv: -kv[1])[:4]
        print("  failing edges by the colour that owns them: "
              + ", ".join(f"{c}x{n}" for c, n in top))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[7])
    ap.add_argument("control", help="frame with the text OFF")
    ap.add_argument("test", help="same frame with the text ON")
    ap.add_argument("--noise", help="third run with the CONTROL's own keys")
    ap.add_argument("--gate", type=int, default=16,
                    help="per-channel difference that counts as drawn (default 16)")
    ap.add_argument("--either", action="store_true",
                    help="own pixels drawn DARKER too (an outlined mark, not text)")
    args = ap.parse_args()

    w, h, ch, ctrl = read_png(args.control)
    w2, h2, _, test = read_png(args.test)
    if (w, h) != (w2, h2):
        raise SystemExit("control and test differ in size -- not one arm pair")
    noise = None
    if args.noise:
        w3, h3, _, noise = read_png(args.noise)
        if (w, h) != (w3, h3):
            raise SystemExit("noise arm differs in size")

    ink, unstable = ink_mask(w, h, ch, ctrl, test, noise, args.gate, args.either)
    print(f"frame {w}x{h}  gate {args.gate}  "
          f"unstable pixels dropped by name: {unstable}")
    if not ink:
        print("NOTHING OWNED -- this run measured nothing, not zero.")
        return 2
    print(f"ink pixels owned by the arm: {len(ink)}")
    report_pixels(w, ch, ctrl, test, ink)
    report_edges(w, h, ch, test, ink)
    return 0


if __name__ == "__main__":
    sys.exit(main())
