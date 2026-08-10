#!/usr/bin/env python3
"""
Created: 10:08:2026 - 21:16:52
Last updated: 10:08:2026 - 21:16:52
Module: tools
File: tools/pngdiff.py

Responsibility:
- Pixel-difference instrument for before/after frame comparison: percentage of
  pixels that differ at all, and the worst per-channel delta, per frame and in
  total. Colour channels only; alpha is never compared.

Key items:
- read_png(): pure-stdlib PNG decode (8-bit, non-interlaced, all five filters).
- diff_dirs(): per-frame and total difference over two capture directories.

Dependencies:
- Uses: python3 stdlib only (zlib, struct). No Pillow, no numpy (Rule 24).
- Used by: render + any zone making a Rule 27 before/after pixel claim.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- READ THE LIMITATION BELOW BEFORE QUOTING A NUMBER OUT OF THIS.

  THIS TOOL IS SOUND. THE FULL SCREENSHOT TOUR IS NOT YET A VALID INPUT TO IT.
  Measured 10:08:2026 - 21:16:52, seed-1 testbed, 640x360 internal, DFN_PALETTE=0: two runs of
  the SAME binary at the SAME commit with NO change between them differ by
  17.448 % of pixels (max channel delta 247/255). Pinning the sky with
  DFN_TIME=0.5 does not help -- 21.748 % -- so it is not the day/night clock.
  Re-measured after the menu-skip fix (3903d69) at 34.660 %.

  Cause (Rule 42): Tour.cpp's testbed route settles each vantage with
  `constexpr uint32_t WAIT = 45`, denominated in RENDERED FRAMES. What it is
  waiting for is chunk streaming, denominated in SIM STEPS, driven off wall-
  clock frame_dt through the fixed-timestep catch-up loop. A different set of
  chunks is resident when the shutter opens on every run. That route also has
  no env override for WAIT (vantage_steps() has DFN_TOUR_WAIT; testbed_steps()
  does not).

  SO: a full-tour difference below ~20 % (35 % post-3903d69) certifies NOTHING.
  It is indistinguishable from two identical runs. What this tool CAN certify
  today is a single pinned-state probe (DFN_MASSIF_PROBE, DFN_CRAG_PROBE and
  friends) where nothing streams between the two arms -- run the control arm
  first, every time, and do not quote a number whose control you did not take.
  Rule 30 is not satisfied by the tool being correct; it is satisfied by the
  control coming back at zero.

Usage:
  tools/pngdiff.py DIR_A DIR_B
"""
"""
UPD:
- 10:08:2026 - 21:16:52: Created. Committed out of a scratchpad at the lead's instruction: an
  instrument living in a scratchpad is one the next agent rebuilds slightly
  differently, which is Rule 39 aimed at tooling. The tour-determinism
  limitation is recorded in the header rather than in a message, because the
  number it invalidates is the number someone will want to quote.
"""

import os
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos = 8
    idat = bytearray()
    w = h = bit = ctype = None
    palette = None
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if ctag == b"IHDR":
            w, h, bit, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            assert interlace == 0, "interlaced PNG not supported"
        elif ctag == b"PLTE":
            palette = body
        elif ctag == b"IDAT":
            idat += body
        elif ctag == b"IEND":
            break
        pos += 12 + length
    assert bit == 8, f"{path}: bit depth {bit} unsupported"
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    raw = zlib.decompress(bytes(idat))
    stride = w * nch
    out = bytearray(h * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        ft = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if ft == 1:
            for i in range(nch, stride):
                line[i] = (line[i] + line[i - nch]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                b = prev[i]
                c = prev[i - nch] if i >= nch else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    if ctype == 3:
        assert palette is not None
        rgb = bytearray(w * h * 3)
        for i, idx in enumerate(out):
            rgb[3 * i:3 * i + 3] = palette[3 * idx:3 * idx + 3]
        return w, h, 3, bytes(rgb)
    return w, h, nch, bytes(out)


def diff_dirs(da, db):
    names = sorted(set(os.listdir(da)) & set(os.listdir(db)))
    names = [n for n in names if n.endswith(".png")]
    if not names:
        print("NO MATCHING FRAMES", file=sys.stderr)
        return 1
    tot_px = tot_diff = 0
    gmax = 0
    for n in names:
        wa, ha, ca, pa = read_png(os.path.join(da, n))
        wb, hb, cb, pb = read_png(os.path.join(db, n))
        if (wa, ha, ca) != (wb, hb, cb):
            print(f"{n:34s} GEOMETRY MISMATCH {wa}x{ha}x{ca} vs {wb}x{hb}x{cb}")
            continue
        npx = wa * ha
        # compare only colour channels, never alpha
        cc = 3 if ca >= 3 else 1
        ndiff = 0
        mx = 0
        for i in range(npx):
            o = i * ca
            d = 0
            for k in range(cc):
                dd = abs(pa[o + k] - pb[o + k])
                if dd > d:
                    d = dd
            if d:
                ndiff += 1
                if d > mx:
                    mx = d
        tot_px += npx
        tot_diff += ndiff
        gmax = max(gmax, mx)
        print(f"{n:34s} {100.0 * ndiff / npx:7.3f} %   maxch {mx:3d}   ({ndiff}/{npx})")
    print(f"{'TOTAL':34s} {100.0 * tot_diff / tot_px:7.3f} %   maxch {gmax:3d}   "
          f"({tot_diff}/{tot_px})")
    return 0


if __name__ == "__main__":
    sys.exit(diff_dirs(sys.argv[1], sys.argv[2]))
