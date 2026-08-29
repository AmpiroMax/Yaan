#!/usr/bin/env python3
"""
Module: tools
File: tools/measure_ground_junction.py

Responsibility:
- §5.12b acceptance instrument: the longest contiguous run of columns over
  which the lowest visible massif pixel meets GROUND rather than a canopy edge,
  measured on a 640x360 acceptance frame.

Key items:
- classify(): pixel class from the shipped palette, calibrated on a real frame.
- measure(): longest ground-junction run + the massif's angular extent.

Dependencies:
- Uses: python3 stdlib + tools/archive_frame.py's PNG reader. No Pillow.
- Used by: core/design, the §5.12 acceptance.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE CLASSIFIER IS THE INSTRUMENT. Its first cut called rock "canopy" and
  grass "canopy" and returned 1 px on BOTH arms — identical readings on two
  different worlds is the signature of an instrument measuring itself rather
  than the scene. If this is ported to another palette, re-sample the colours
  before trusting a number out of it.
"""

import sys, importlib.util
spec = importlib.util.spec_from_file_location("af", "tools/archive_frame.py")
af = importlib.util.module_from_spec(spec); spec.loader.exec_module(af)

def classify(r, g, b):
    # CALIBRATED AGAINST THE ACTUAL FRAME, not guessed. The first cut called
    # rock "canopy" (rock reads v 40-78 grey) and grass "canopy" too (grass is
    # (52,71,28), v 71) -- both thresholds were invented rather than sampled,
    # and the measurement came back 1 px on BOTH arms, which is the signature
    # of an instrument reporting itself.
    mx, mn = max(r,g,b), min(r,g,b)
    v = mx; sat = 0 if mx == 0 else (mx-mn)/mx
    if v < 30:                       return 'canopy'   # trunk / deep shadow
    if v > 140 and b >= g:           return 'sky'
    if b > r and b > g and v > 90:   return 'sky'
    if g > r + 8 and g > b + 15:     return 'grass' if v >= 45 else 'canopy'
    if sat < 0.15:                   return 'rock'
    return 'other'

def measure(path):
    w,h,ch,px = af.read_png(path)
    cols = []
    for x in range(w):
        rock_lo = None
        for y in range(h):
            i = (y*w + x)*ch
            if classify(px[i],px[i+1],px[i+2]) == 'rock':
                rock_lo = y
        cols.append(rock_lo)
    # for each column with rock, what is just BELOW the lowest rock pixel?
    verdict = []
    for x,ylo in enumerate(cols):
        if ylo is None or ylo >= h-3:
            verdict.append(None); continue
        below = []
        for dy in (1,2,3):
            i = ((ylo+dy)*w + x)*ch
            below.append(classify(px[i],px[i+1],px[i+2]))
        verdict.append('ground' if 'grass' in below else 'curtained')
    width_cols = sum(1 for v in verdict if v is not None)
    best = cur = 0
    for v in verdict:
        cur = cur+1 if v == 'ground' else 0
        best = max(best, cur)
    return best, width_cols, w

for label, p in (("apron OFF (control)", "docs/acceptance/core-apron-west300-BEFORE-687f152.png"),
                 ("apron ON  (shipped)", "docs/acceptance/core-apron-west300-AFTER-687f152.png")):
    run, extent, w = measure(p)
    print(f"{label}: longest massif-meets-GROUND run = {run:3d} px   "
          f"(massif angular extent {extent:3d} px of {w}, run/extent {run/max(extent,1):.2f})")
