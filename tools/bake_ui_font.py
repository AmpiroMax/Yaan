#
# Module: tools
# File: tools/bake_ui_font.py
#
# Responsibility:
# - ПЕЧЁТ ГЛИФОВЫЙ АТЛАС НАСТОЯЩЕГО ШРИФТА для интерфейса игры (заказ владельца
#   27.08: «шрифт интерфейса слишком угловатый, нужен нормальный шрифт»). На
#   выходе — assets/fonts/ui_serif.fnt (метрики всех размеров одним файлом) и
#   assets/fonts/ui_serif_<px>.png (по атласу на ступень). Рантайм ничего не
#   растеризует: он читает PNG уже имеющимся своим читателем и кладёт покрытие
#   на холст.
#
# ПОЧЕМУ ОФЛАЙН-ЗАПЕЧКА, А НЕ БИБЛИОТЕКА ШРИФТОВ В ИГРЕ. FreeType в рантайме —
# это сторонняя зависимость в engine/app ради текста, который не меняется между
# запусками: набор строк у нас конечен и известен на сборке. Правило 1 держит
# сторонние включения за границей платформы, а игровое меню рисуется своим
# холстом (ImGui — только редактору). Запечка отдаёт то же качество и стоит
# рантайму ноль.
#
# ПОЧЕМУ СТУПЕНИ РАЗМЕРОВ, А НЕ ОДИН АТЛАС С МАСШТАБИРОВАНИЕМ. Масштабирование
# готового покрытия — это второе размытие поверх сглаживания, то есть ровно то
# «мыло», которого заказ требует избежать. Каждая ступень запекается ИЗ КРИВЫХ,
# и рантайм рисует её один к одному. Ступени геометрические (шаг ~1.27), потому
# что глаз читает размер логарифмически: линейная лестница дала бы толпу
# неотличимых крупных ступеней и дыру среди мелких.
#
# ШРИФТ: PT Serif Regular (ParaType, SIL OFL 1.1). Выбран за ПОЛНУЮ кириллицу,
# нарисованную для русского текста, а не долепленную к латинице, и за характер
# антиквы, который просил владелец. Лицензия лежит рядом с активом
# (assets/fonts/OFL.txt), строка атрибуции — в титрах игры (menu.credits.font):
# OFL требует сохранять уведомление об авторских правах там, где шрифт
# используется.
#
# Usage:
#     python3 tools/bake_ui_font.py
#     python3 tools/bake_ui_font.py --sizes 11,14,18
#
# Dependencies:
# - Uses: Pillow (офлайн), assets/fonts/PT_Serif-Web-Regular.ttf.
# - Used by: engine/app/sources/UiFont.cpp читает .fnt и .png.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ЛЕСТНИЦА РАЗМЕРОВ — ЭТО КОНТРАКТ С РАНТАЙМОМ. Убрав ступень, вы не сделаете
#   текст меньше — вы заставите рантайм взять СОСЕДНЮЮ, и раскладка поедет.
import argparse
import os
import sys

try:
    from PIL import Image, ImageFont
except ImportError as exc:  # pragma: no cover
    sys.stderr.write("bake_ui_font: нужен Pillow: %s\n" % exc)
    raise SystemExit(2)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF = os.path.join(REPO, "assets/fonts/PT_Serif-Web-Regular.ttf")
OUT_DIR = os.path.join(REPO, "assets/fonts")
STEM = "ui_serif"

# СТУПЕНИ. Нижняя (11) — самая мелкая, на которой антиква ещё читается: ниже
# засечка тоньше пикселя и шрифт превращается в кашу, и там рантайм честно
# остаётся на нижней ступени, а не печатает нечитаемое. Верхняя (96) — крупный
# первый пункт стартового экрана на холсте 1920×1080.
SIZES = [11, 14, 18, 23, 29, 37, 47, 60, 76, 96]

# ЧТО ПЕЧЁМ. Набор — это ровно то, что умеет блочный шрифт (BitmapFont.h:
# U+0020..U+007E, U+0410..U+044F, U+0401, U+0451, U+00AB, U+00BB, U+2014), плюс
# знаки, которых ему не хватало и которые в наших строках уже встречаются.
# Расширение НАБОРА, а не сужение: экран, потерявший знак при смене шрифта, —
# это регресс, который никто не заметит до чужого перевода.
CODEPOINTS = (
    list(range(0x20, 0x7F))
    + [0x00AB, 0x00BB, 0x00B7, 0x00D7, 0x00A9]
    + [0x0401, 0x0451]
    + list(range(0x0410, 0x0450))
    + [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2116, 0x2192]
)

# ЗАПАС ВОКРУГ ГЛИФА В АТЛАСЕ. Один пиксель со всех сторон: точки выборки не
# ходят за край ячейки, но сосед, стоящий вплотную, всё равно даёт грязь на
# любой будущей фильтрации, и один пиксель стоит меньше, чем расследование.
PAD = 1

def bake(size, codepoints):
    font = ImageFont.truetype(TTF, size)
    ascent, descent = font.getmetrics()
    line = ascent + descent

    # Мера каждого глифа берётся у самого шрифта, а не считается по кегль-квадрату.
    boxes = {}
    for cp in codepoints:
        ch = chr(cp)
        mask = font.getmask(ch, mode="L")
        w, h = mask.size
        bbox = font.getbbox(ch)  # (x0, y0, x1, y1) от базовой линии сверху
        adv = int(round(font.getlength(ch)))
        boxes[cp] = (w, h, bbox, adv, mask)

    # РАСКЛАДКА ПОЛКАМИ. Атлас — не сетка одинаковых ячеек: у антиквы ширины
    # разные, и квадратная сетка по самому широкому знаку раздула бы файл втрое
    # ради воздуха.
    max_w = max(1, max(b[0] for b in boxes.values()))
    per_row = 16
    atlas_w = 1
    while atlas_w < (max_w + 2 * PAD) * per_row:
        atlas_w *= 2
    pen_x, pen_y, row_h = PAD, PAD, 0
    placed = {}
    for cp in codepoints:
        w, h, bbox, adv, mask = boxes[cp]
        if pen_x + w + PAD > atlas_w:
            pen_x = PAD
            pen_y += row_h + PAD
            row_h = 0
        placed[cp] = (pen_x, pen_y, w, h, bbox, adv)
        pen_x += w + PAD
        row_h = max(row_h, h)
    atlas_h = 1
    while atlas_h < pen_y + row_h + PAD:
        atlas_h *= 2

    img = Image.new("L", (atlas_w, atlas_h), 0)
    for cp in codepoints:
        x, y, w, h, bbox, adv = placed[cp]
        if w == 0 or h == 0:
            continue
        mask = boxes[cp][4]
        img.paste(Image.frombytes("L", mask.size, bytes(mask)), (x, y))

    png = os.path.join(OUT_DIR, "%s_%d.png" % (STEM, size))
    img.save(png, "PNG", optimize=True)

    lines = ["size %d atlas %s_%d.png line %d ascent %d" % (size, STEM, size, line, ascent)]
    for cp in codepoints:
        x, y, w, h, bbox, adv = placed[cp]
        # ox/oy — СМЕЩЕНИЕ ОТ ПЕРА ДО ЛЕВОГО ВЕРХА ЯЧЕЙКИ, где перо стоит на
        # ВЕРХНЕЙ линии строки (а не на базовой). Так рантайму не нужно знать
        # про базовую линию вовсе: он кладёт строку по её верху, как это делал
        # блочный шрифт, и раскладка страниц не переписывается заново.
        ox = bbox[0] if bbox else 0
        oy = bbox[1] if bbox else 0
        lines.append("g %d %d %d %d %d %d %d %d" % (cp, x, y, w, h, ox, oy, adv))
    return lines, os.path.getsize(png)

def main():
    ap = argparse.ArgumentParser(description="печёт атлас шрифта интерфейса")
    ap.add_argument("--sizes", default=None, help="список кеглей через запятую")
    args = ap.parse_args()
    sizes = [int(s) for s in args.sizes.split(",")] if args.sizes else SIZES

    if not os.path.exists(TTF):
        sys.stderr.write("bake_ui_font: нет %s\n" % TTF)
        raise SystemExit(2)

    out = [
        "# ui font metrics -- ПЕЧЁТСЯ tools/bake_ui_font.py, РУКАМИ НЕ ПРАВИТЬ",
        "# font PT Serif Regular (ParaType, SIL OFL 1.1, assets/fonts/OFL.txt)",
        "# size <px> atlas <file> line <px> ascent <px>",
        "# g <codepoint> <x> <y> <w> <h> <ox> <oy> <advance>",
    ]
    total = 0
    for size in sizes:
        lines, png_bytes = bake(size, CODEPOINTS)
        out += lines
        total += png_bytes
        sys.stderr.write("  ступень %2d px — %d КБ\n" % (size, png_bytes // 1024))
    fnt = os.path.join(OUT_DIR, "%s.fnt" % STEM)
    with open(fnt, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    sys.stderr.write("bake_ui_font: %d ступеней, %d знаков, атласы %.2f МБ, %s\n"
                     % (len(sizes), len(CODEPOINTS), total / (1024 * 1024), fnt))

if __name__ == "__main__":
    main()
