#!/usr/bin/env python3
# Module: tools
# File: tools/contact_sheet.py
#
# Responsibility:
# - ЛЕНТА КАДРОВ ПРОГОНА (LOCOMOTION_GROUNDED.md §16.8, фаза 7): берёт
#   rec_%05d.png + rec.log, снятые дверью DFN_RECORD_EVERY на прогоне
#   записанного ввода (DFN_TRAJ_PLAY), и при желании CSV телеметрии
#   (DFN_LOCO_CSV), и собирает одну PNG-ленту: N кадров в ряд, под каждым —
#   его строка состояния (t, позиция, скорость, аллюр) и строка приборов
#   (роль, фаза, ход опорной стопы, зазор, рыск, смены клипа). Это артефакт,
#   который смотрит владелец, а не пересказ по памяти (правило 18a).
#
# Usage: python3 tools/contact_sheet.py <dir_with_rec_pngs> <out.png>
#            [--every K] [--max N] [--csv loco.csv] [--record-every N] [--width W]
# Снимать фигуру снаружи: запись с DFN_STAND_CAM=<камера стенда> — глаз
# траектории и есть орбита, прогон воспроизводит её.
#
# Dependencies:
# - Uses: PIL (conda env, python-only-conda), rec.log формата AppAfterFrame,
#   CSV формата LocoTelemetry::csv_header().
# - Used by: агенты и владелец вручную; artifacts/reports/<тема>/.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - Лента — снимок ТОГО ЖЕ прогона, что и числа: rec.log и CSV из одного запуска.
import argparse
import csv
import os
import re
import sys

from PIL import Image, ImageDraw


def read_rec_log(path):
    rows = {}
    if not os.path.exists(path):
        return rows
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"(\d+) t=([\d.]+) pos=\(([^)]*)\) yaw=([-\d.]+) pitch=([-\d.]+) .* v=([\d.]+) gait=(\d+) ground=(\d)", line)
            if m:
                rows[int(m.group(1))] = {
                    "t": float(m.group(2)), "pos": m.group(3), "yaw": float(m.group(4)),
                    "v": float(m.group(6)), "gait": int(m.group(7)), "ground": int(m.group(8)),
                }
    return rows


def read_csv(path):
    if not path or not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return list(csv.DictReader(f))


def csv_row_for_frame(rows, frame_idx, record_every):
    """Строка телеметрии кадра: в счётном прогоне кадр = тик, а лента пишет
    каждый record_every-й кадр — тик кадра = frame_idx × record_every. Часы
    rec.log (игровые секунды) и CSV (секунды прибора с нуля) — разные, по ним
    не сопоставить."""
    if not rows:
        return None
    k = min(len(rows) - 1, frame_idx * record_every)
    return rows[k]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("out")
    ap.add_argument("--every", type=int, default=1, help="каждый K-й кадр ленты")
    ap.add_argument("--max", type=int, default=24, help="не больше N кадров")
    ap.add_argument("--csv", default="", help="CSV телеметрии (DFN_LOCO_CSV)")
    ap.add_argument("--width", type=int, default=320, help="ширина кадра в ленте, px")
    ap.add_argument("--record-every", type=int, default=1, help="DFN_RECORD_EVERY прогона (кадров на строку ленты)")
    a = ap.parse_args()
    pngs = sorted(p for p in os.listdir(a.dir) if re.match(r"rec_\d{5}\.png$", p))
    pngs = pngs[:: max(1, a.every)][: a.max]
    if not pngs:
        print("нет rec_*.png в", a.dir, file=sys.stderr)
        return 2
    log = read_rec_log(os.path.join(a.dir, "rec.log"))
    tele = read_csv(a.csv)
    cells = []
    for p in pngs:
        idx = int(re.search(r"(\d{5})", p).group(1))
        im = Image.open(os.path.join(a.dir, p)).convert("RGB")
        h = int(im.height * a.width / im.width)
        im = im.resize((a.width, h))
        info = log.get(idx, {})
        lines = ["#%d t=%.2f v=%.2f gait=%d" % (idx, info.get("t", 0.0), info.get("v", 0.0), info.get("gait", 0))]
        r = csv_row_for_frame(tele, idx, a.record_every)
        if r:
            lines.append("%s ph=%s stance=%s gap=%s/%s" % (r["role"], r["phase"], r["stance_mps"], r["gap_l_mm"], r["gap_r_mm"]))
            lines.append("turn=%s trans=%s planted=%s%s" % (r["turn_rate_dps"], r["trans_ps"], r["planted_l"], r["planted_r"]))
        cells.append((im, lines))
    text_h = 14 * 3 + 6
    cols = min(len(cells), 6)
    rows_n = (len(cells) + cols - 1) // cols
    cell_h = cells[0][0].height + text_h
    sheet = Image.new("RGB", (cols * a.width, rows_n * cell_h), (24, 24, 24))
    draw = ImageDraw.Draw(sheet)
    for i, (im, lines) in enumerate(cells):
        x = (i % cols) * a.width
        y = (i // cols) * cell_h
        sheet.paste(im, (x, y))
        for k, line in enumerate(lines):
            draw.text((x + 4, y + im.height + 2 + 14 * k), line, fill=(230, 230, 230))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    sheet.save(a.out)
    print("лента:", a.out, "кадров", len(cells))
    return 0


if __name__ == "__main__":
    sys.exit(main())
