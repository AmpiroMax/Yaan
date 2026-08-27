#!/usr/bin/env python3
"""
Created: 28:08:2026 - 00:41:00
Last updated: 28:08:2026 - 00:41:00
Module: tools
File: tools/measure_interior_light.py

Responsibility:
- Measure how DARK an interior frame is, in the two numbers the owner's
  complaint is actually about: "углы читаемы" (a corner that is dark but not
  black) and "очаг греет" (a hearth that is the brightest thing in the room).
  Reads the frame the app wrote; no Pillow, no numpy (Rule 24).

Key items:
- stats(): median / p10 / p90 / mean of luma over a pixel rectangle, plus the
  share of pixels below the visibility floor.
- pair(): the SAME rectangles on two frames, printed as before/after with the
  difference. The two frames are expected to be two DOSES OF ONE BINARY
  (Rule 47): DFN_INTERIOR_LIGHT=0 and =1.
- main(): whole-frame histogram + the named boxes given on the command line.

Dependencies:
- Uses: tools/archive_frame.read_png (stdlib zlib/struct) only.
- Used by: docs/reports/interior-light-28-08.html.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- RULE 47, AND IT IS THE WHOLE REASON THE BOXES ARE ARGUMENTS. A box may NOT
  be found by "the darkest region of the frame": the arm where the fix works
  has no such region, so the instrument would move its own sample to wherever
  the effect is weakest and report that nothing happened. Every box is set
  ONCE, by GEOMETRY, on the control arm, and the same pixels are read on both
  arms. The whole-frame histogram is legal for the same reason -- the sample is
  every pixel, and it cannot move.
- LUMA IS Rec.709 ON THE 8-BIT FRAME AS WRITTEN. The renderer applies no
  tonemap and no gamma encode (fs_prop writes lit albedo straight out), so the
  stored byte IS the linear quantity the shader computed, and a ratio of two
  bytes is a ratio of two light levels. If a tonemap is ever added, this
  sentence stops being true and this file has to be told.
- И РОВНО ПОЭТОМУ ЕСТЬ ВТОРАЯ ШКАЛА, «ПЕРЦЕПТИВНАЯ», И ОНА НЕ ДУБЛИКАТ.
  Ни один BGFX_TEXTURE_SRGB в бэкенде не выставлен (grep по всей зоне
  bgfx: ноль совпадений), то есть кадр уходит на экран БЕЗ гамма-кодирования,
  а монитор читает его КАК sRGB. Значит свет, доходящий до глаза, — это
  sRGB-декод нашего байта, и сравнивать наши числа со скриншотами Skyrim
  (которые СЖАТЫ в sRGB и потому декодируются обратно) можно только на этой
  шкале. Прямое сравнение байтов завысило бы нас втрое и объявило бы
  победу там, где её нет.
"""
"""
UPD:
- 28:08:2026 - 00:41:00: Создан вместе с волной света интерьеров: до этой
  волны «в домах слишком темно» не имело ни одного числа.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from archive_frame import read_png  # noqa: E402

# Порог читаемости: шаг 64-цветной палитры равен 255/12.75 ~= 20 люма
# (docs/FINDING_DUNGEON_DARK.md меряет ровно им). Пиксель ниже одного шага от
# нуля неотличим от чёрного на глаз и на любом мониторе игрока.
READABLE = 20.0

# ЗАМЕРЕННАЯ ПОЛОСА SKYRIM (docs/reports/interior-light-28-08.html §1): медиана
# ЯРКОСТИ КАДРА по десяти интерьерам Вайтрана после sRGB-декода — 0.024..0.100
# линейных, и НИ ОДИН из десяти не садится в настоящий чёрный (первая
# процентиль 0.001..0.007). Контраст p95/p05 в комнатах, где хозяин кадра
# очаг, — 15..40 крат; там, где в кадре есть окно, — 60..100.
SKYRIM_MEDIAN_LO = 0.024
SKYRIM_MEDIAN_HI = 0.100
SKYRIM_CONTRAST_LO = 15.0
SKYRIM_CONTRAST_HI = 40.0


def srgb_to_linear(v8):
    """Байт кадра -> свет, доходящий до глаза. Кадр пишется БЕЗ гамма-кодирования
    (в бэкенде нет ни одного BGFX_TEXTURE_SRGB), а монитор читает его как sRGB;
    декод — это и есть то, что видит игрок, и единственная шкала, на которой
    наши числа сравнимы со скриншотами Skyrim."""
    c = v8 / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def luma(px):
    return 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]


def box_luma(w, h, ch, pixels, x0, y0, x1, y1):
    """Люма по прямоугольнику [x0,x1) x [y0,y1) — список значений."""
    x0 = max(0, min(w, x0))
    x1 = max(0, min(w, x1))
    y0 = max(0, min(h, y0))
    y1 = max(0, min(h, y1))
    out = []
    for y in range(y0, y1):
        row = y * w * ch
        for x in range(x0, x1):
            i = row + x * ch
            out.append(luma(pixels[i:i + 3]))
    return out


def stats(vals):
    if not vals:
        return None
    v = sorted(vals)
    n = len(v)

    def pct(p):
        return v[min(n - 1, max(0, int(p * (n - 1))))]

    lo = srgb_to_linear(pct(0.05))
    hi = srgb_to_linear(pct(0.95))
    return {
        "n": n,
        "mean": sum(v) / n,
        "p10": pct(0.10),
        "median": pct(0.50),
        "p90": pct(0.90),
        "black": 100.0 * sum(1 for x in v if x < READABLE) / n,
        # ПЕРЦЕПТИВНАЯ ШКАЛА — та, на которой замерен Skyrim (см. шапку).
        "lin_p05": lo,
        "lin_p50": srgb_to_linear(pct(0.50)),
        "lin_p95": hi,
        "contrast": hi / max(lo, 1e-6),
    }


def fmt(s):
    return ("n=%7d  mean %6.2f  p10 %6.2f  median %6.2f  p90 %6.2f  "
            "ниже %.0f: %5.1f%%" % (s["n"], s["mean"], s["p10"], s["median"],
                                    s["p90"], READABLE, s["black"]))


def fmt_lin(s):
    band = "В ПОЛОСЕ" if SKYRIM_MEDIAN_LO <= s["lin_p50"] <= SKYRIM_MEDIAN_HI \
        else ("НИЖЕ полосы" if s["lin_p50"] < SKYRIM_MEDIAN_LO
              else "ВЫШЕ полосы")
    con = "в полосе" if SKYRIM_CONTRAST_LO <= s["contrast"] <= SKYRIM_CONTRAST_HI \
        else ("ниже" if s["contrast"] < SKYRIM_CONTRAST_LO else "выше")
    return ("p05 %.4f  p50 %.4f  p95 %.4f  контраст p95/p05 %5.1f  "
            "(медиана %s Skyrim 0.024..0.100; контраст %s 15..40)"
            % (s["lin_p05"], s["lin_p50"], s["lin_p95"], s["contrast"],
               band, con))


def load(path):
    w, h, ch, pixels = read_png(path)
    return w, h, ch, pixels


def main(argv):
    if len(argv) < 3:
        print("usage: measure_interior_light.py <before.png> <after.png> "
              "[name=x0,y0,x1,y1 ...]")
        return 2
    a = load(argv[1])
    b = load(argv[2])
    if a[0] != b[0] or a[1] != b[1]:
        print("КАДРЫ РАЗНОГО РАЗМЕРА — одни и те же пиксели прочесть нельзя")
        return 1
    w, h = a[0], a[1]
    boxes = [("ВЕСЬ КАДР", (0, 0, w, h))]
    for arg in argv[3:]:
        name, rect = arg.split("=", 1)
        x0, y0, x1, y1 = (int(t) for t in rect.split(","))
        boxes.append((name, (x0, y0, x1, y1)))
    print("кадр %dx%d" % (w, h))
    print("ДО:    %s" % argv[1])
    print("ПОСЛЕ: %s" % argv[2])
    rows = []
    for name, (x0, y0, x1, y1) in boxes:
        sa = stats(box_luma(a[0], a[1], a[2], a[3], x0, y0, x1, y1))
        sb = stats(box_luma(b[0], b[1], b[2], b[3], x0, y0, x1, y1))
        if sa is None or sb is None:
            continue
        rows.append((name, sa, sb))
        print("\n[%s]  %d,%d..%d,%d" % (name, x0, y0, x1, y1))
        print("  до:    %s" % fmt(sa))
        print("  после: %s" % fmt(sb))
        print("  Δ:     mean %+6.2f  p10 %+6.2f  median %+6.2f  (x%.2f по медиане)"
              % (sb["mean"] - sa["mean"], sb["p10"] - sa["p10"],
                 sb["median"] - sa["median"],
                 sb["median"] / max(sa["median"], 0.01)))
        print("  свет в глаз, до:    %s" % fmt_lin(sa))
        print("  свет в глаз, после: %s" % fmt_lin(sb))
    named = {n: (x, y) for n, x, y in rows}
    if "ОЧАГ" in named and "УГОЛ" in named:
        for i, tag in enumerate(("до", "после")):
            # ОТНОШЕНИЕ БЕРЁТСЯ НА ПЕРЦЕПТИВНОЙ ШКАЛЕ. Отношение сырых байтов
            # — это отношение ЛИНЕЙНЫХ величин, показанных как sRGB, и оно
            # занижает контраст ровно там, где он важен: 44.6/21.5 = 2.1
            # против настоящих 5.7 крат света, доходящего до глаза.
            hearth = named["ОЧАГ"][i]["lin_p50"]
            corner = named["УГОЛ"][i]["lin_p50"]
            print("\nконтраст очаг/угол (%s), свет в глаз: %.4f / %.4f = %.1fx"
                  % (tag, hearth, corner, hearth / max(corner, 1e-6)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
