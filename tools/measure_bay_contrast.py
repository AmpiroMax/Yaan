# Created: 16:08:2026 - 22:31:18
# Last updated: 16:08:2026 - 22:31:18
# Module: tools
# File: tools/measure_bay_contrast.py
#
# Responsibility:
# - The DARK-PIT instrument, second half of the sealed-hull rule: a wall bay
#   that lets no ray through can still READ as a hole when it sits much darker
#   than its own frame (user's word was «дырки», and it covered both). Measures
#   mean luminance of named rectangles in a frame and prints every rect's mean
#   plus each bay/frame ratio.
#
# Usage:
#   python3 tools/measure_bay_contrast.py <frame.png> frame=X,Y,W,H bay1=X,Y,W,H ...
#   The rect named "frame" is the denominator; every other rect is reported as
#   a ratio against it. Rects are chosen by GEOMETRY from a frozen camera
#   recipe (Rule 47: the metric may not locate its subject by the property
#   under test — these boxes are fixed pixels, not "the dark part").
#
# Dependencies:
# - Uses: tools/pngdiff.py read_png (stdlib-only PNG reader). No Pillow (Rule 24).
# - Used by: houses zone acceptance (bay-contrast rule).
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - Luminance is the PIPELINE's own 0.30/0.59/0.11 weighting (Rule 36: the
#   pipeline's metric belongs in the design vocabulary), on the sRGB bytes as
#   shown — this measures what the eye is shown, not linear light.
# - The threshold does NOT live here. An instrument reports; the acceptance
#   names its number and where it came from (both arms, Rule 45).
#
# UPD:
# - 16:08:2026 - 22:31:18: Создан для второй половины жалобы про дырки (провал
#   тёмного пролёта в раме); обе руки — из ОДНОГО кадра (правило 47).

import sys
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from pngdiff import read_png  # noqa: E402


def mean_luma(px, w, nch, rect):
    x0, y0, rw, rh = rect
    total = 0.0
    n = 0
    for y in range(y0, y0 + rh):
        row = y * w * nch
        for x in range(x0, x0 + rw):
            o = row + x * nch
            r, g, b = px[o], px[o + 1], px[o + 2]
            total += 0.30 * r + 0.59 * g + 0.11 * b
            n += 1
    return total / max(1, n)


def main():
    if len(sys.argv) < 3:
        print(__doc__ or "usage: measure_bay_contrast.py <png> frame=X,Y,W,H bay=X,Y,W,H ...")
        return 2
    w, h, nch, px = read_png(sys.argv[1])
    assert nch >= 3, "need an RGB frame"
    rects = {}
    for arg in sys.argv[2:]:
        name, _, spec = arg.partition("=")
        x, y, rw, rh = (int(v) for v in spec.split(","))
        assert 0 <= x and 0 <= y and x + rw <= w and y + rh <= h, f"{name} outside {w}x{h}"
        rects[name] = (x, y, rw, rh)
    assert "frame" in rects, "one rect must be named frame= (the denominator)"
    means = {name: mean_luma(px, w, nch, r) for name, r in rects.items()}
    fm = means["frame"]
    for name, m in means.items():
        if name == "frame":
            print(f"[bay] frame       mean {m:7.2f}  (denominator)")
        else:
            print(f"[bay] {name:<11} mean {m:7.2f}  ratio vs frame {m / fm:.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
