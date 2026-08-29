#!/usr/bin/env python3
#
# File: tools/check_flora_sow.py
#
# Responsibility:
# - ПРИБОР ЗАКОНА ПОСЕВА (tools/flora_sow.py). Четыре руки, и у каждой есть
#   КОНТРОЛЬ, который обязан провалиться (правило 30):
#     1. КУРТИННОСТЬ мерится индексом Кларка-Эванса R = среднее расстояние до
#        ближайшего соседа, делённое на пуассоновское ожидание. Куртинный посев
#        обязан дать R < 0.6; КОНТРОЛЬ — то же число растений равномерным
#        рассевом — обязан дать R около 1.0. Без контроля «R = 0.33» ничего не
#        доказывает: это могло быть свойством метрики, а не посева.
#     2. ПЕРЕКЛЮЧЕНИЕ ПО СВЕТУ: под сомкнутым пологом ковёр моховой, на открытом
#        месте травяной, и в переходной полосе есть И ТО И ДРУГОЕ (граница
#        рваная, а не контурная).
#     3. ОПУШКА: ни одно растение подлеска не ближе EDGE_UNDER_CLEAR_M к
#        полотну, ни одно деревце подроста — ближе SAPLING_CLEAR_M, и второй
#        порог БОЛЬШЕ первого («деревья держат дистанцию больше кустов»).
#     4. ДЕТЕРМИНИЗМ: два прогона с одним семенем совпадают до числа.
#
# Usage:
#     python3 tools/check_flora_sow.py
#
# ЗАЧЕМ ПРИБОР, А НЕ КАДР. Кадр показывает ОДНО место; закон посева — про всю
# землю. Куртинность, которая развалилась бы на другом семени, на кадре видна
# не была бы вовсе.
#
# Dependencies:
# - Uses: tools/flora_sow.py, Python stdlib. НИ ОДНОГО .dfo и ни одного
#   бинарника: закон — чистая функция, и мерить его надо без полки.
# - Used by: ctest (цель flora_sow_law).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.

import math
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flora_sow as fs  # noqa: E402

SPAN = 256.0
SEED = 20260828

class FakeShelf:
    """Полка без полки: закон не должен зависеть от того, что испекли."""

    def __init__(self, radii, solids):
        self.radii = radii
        self.solids = solids

    def radius(self, name):
        return self.radii.get(name, 0.5)

    def solid_radius(self, name):
        return self.radii.get(name, 0.5) if self.solids.get(name) else 0.0

    def is_solid(self, name):
        return bool(self.solids.get(name))

    def scale(self, name):
        return 1.0

SHELF = FakeShelf({"fern": 0.7, "bush": 1.6, "moss": 1.8, "sward": 1.4,
                   "grass": 0.9, "mushroom": 0.3, "sapling": 0.6},
                  {"fern": False, "bush": True, "moss": False, "sward": False,
                   "grass": False, "mushroom": False, "sapling": True})
PAL = {"shade": ["fern"], "light": ["fern"], "any": ["fern"]}

def clark_evans(pts):
    n = len(pts)
    if n < 30:
        return None
    xs = [p[0] for p in pts]
    zs = [p[1] for p in pts]
    area = (max(xs) - min(xs)) * (max(zs) - min(zs))
    cell = 8.0
    g = defaultdict(list)
    for i, (x, z) in enumerate(pts):
        g[(int(x // cell), int(z // cell))].append(i)
    total = 0.0
    for i, (x, z) in enumerate(pts):
        best = 1e18
        r = 1
        cx, cz = int(x // cell), int(z // cell)
        while True:
            for dz in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    for j in g.get((cx + dx, cz + dz), ()):
                        if j != i:
                            d = (x - pts[j][0]) ** 2 + (z - pts[j][1]) ** 2
                            if d < best:
                                best = d
            if best < (r * cell) ** 2 or r > 10:
                break
            r += 1
        total += math.sqrt(best)
    return (total / n) / (0.5 / math.sqrt(n / area))

class Grove:
    """Полог: плотная роща в середине карты и открытый луг вокруг."""

    def trees(self):
        out = []
        for i in range(7):
            for j in range(7):
                out.append((90.0 + i * 9.0, 90.0 + j * 9.0, 7.0, 20.0))
        return out

def main():
    bad = []
    canopy = fs.Canopy(Grove().trees())
    open_paths = fs.PathField()

    # --- 1. КУРТИННОСТЬ И ЕЁ КОНТРОЛЬ -----------------------------------
    clumped = fs.sow_undergrowth(SPAN, SEED, canopy, open_paths, SHELF, PAL)
    control = fs.sow_undergrowth(SPAN, SEED, canopy, open_paths, SHELF, PAL,
                                 uniform=True)
    rc = clark_evans([(p[1], p[2]) for p in clumped])
    ru = clark_evans([(p[1], p[2]) for p in control])
    if rc is None or ru is None:
        bad.append("посев дал слишком мало растений, чтобы мерить куртинность")
    else:
        if rc >= 0.6:
            bad.append("куртинность: R = %.3f, а куртина обязана дать < 0.6" % rc)
        if not 0.85 <= ru <= 1.15:
            bad.append("КОНТРОЛЬ равномерного рассева: R = %.3f, а должен быть "
                       "около 1.0 — метрика не отличает законы" % ru)
        print("[sow] куртинный R = %.3f, равномерный контроль R = %.3f "
              "(%d и %d растений)" % (rc, ru, len(clumped), len(control)))

    # --- 2. ПЕРЕКЛЮЧЕНИЕ КОВРА ПО СВЕТУ ---------------------------------
    carpet = fs.sow_carpet(SPAN, SEED, canopy, open_paths, SHELF,
                           ["moss"], ["sward"])
    dark = [p for p in carpet if canopy.openness(p[1], p[2]) < 0.2]
    lit = [p for p in carpet if canopy.openness(p[1], p[2]) > 0.9]
    band = [p for p in carpet
            if fs.CARPET_MOSS_BELOW < canopy.openness(p[1], p[2])
            < fs.CARPET_GRASS_ABOVE]
    if any(p[0] != "moss" for p in dark):
        bad.append("под сомкнутым пологом лежит не мох")
    if any(p[0] != "sward" for p in lit):
        bad.append("в световом пятне лежит не трава")
    kinds = {p[0] for p in band}
    if band and kinds != {"moss", "sward"}:
        bad.append("переходная полоса чистая (%s) — граница контурная, а "
                   "обязана быть рваной" % ", ".join(sorted(kinds)))
    print("[sow] ковёр: %d плат, из них в тени %d (все мох), на свету %d "
          "(вся трава), в полосе перехода %d обоих видов"
          % (len(carpet), len(dark), len(lit), len(band)))

    # --- 3. ОПУШКА -------------------------------------------------------
    # Тропа-полоса: полотно вдоль x = 128, поле расстояний считаем сами.
    step = 1.0
    vals = {}
    n = int(SPAN / step)
    for iz in range(n + 1):
        for ix in range(n + 1):
            d = abs(ix * step - 128.0) - 2.0   # полотно шириной 4 м
            if d < 8.0:
                vals[(ix, iz)] = d
    paths = fs.PathField(step, vals)
    near = fs.sow_undergrowth(SPAN, SEED, canopy, paths, SHELF, PAL)
    saplings = fs.sow_saplings(SPAN, SEED, canopy, paths, SHELF, ["sapling"],
                               Grove().trees())
    def worst(rows, name_r):
        w = 1e9
        for p in rows:
            r = SHELF.radius(p[0]) * fs.YAW_ENVELOPE
            w = min(w, abs(p[1] - 128.0) - 2.0 - r)
        return w
    wu = worst(near, None)
    ws = worst(saplings, None) if saplings else 1e9
    if wu < fs.EDGE_UNDER_CLEAR_M - 1e-3:
        bad.append("подлесок сел на полотно: ближайший край в %.2f м при "
                   "пороге %.2f" % (wu, fs.EDGE_UNDER_CLEAR_M))
    if ws < fs.SAPLING_CLEAR_M - 1e-3:
        bad.append("подрост ближе к полотну (%.2f м), чем его порог %.2f"
                   % (ws, fs.SAPLING_CLEAR_M))
    if fs.SAPLING_CLEAR_M <= fs.EDGE_UNDER_CLEAR_M:
        bad.append("деревья держат дистанцию НЕ больше кустов — правило "
                   "опушки перевёрнуто")
    print("[sow] опушка: подлесок не ближе %.2f м, подрост не ближе %.2f м "
          "(пороги %.2f и %.2f)"
          % (wu, ws, fs.EDGE_UNDER_CLEAR_M, fs.SAPLING_CLEAR_M))

    # --- 4. ДЕТЕРМИНИЗМ ---------------------------------------------------
    again = fs.sow_undergrowth(SPAN, SEED, canopy, open_paths, SHELF, PAL)
    if again != clumped:
        bad.append("два прогона одного семени разошлись")
    other = fs.sow_undergrowth(SPAN, SEED + 1, canopy, open_paths, SHELF, PAL)
    if other == clumped:
        bad.append("КОНТРОЛЬ: другое семя дало ТОТ ЖЕ посев — семя ни на что "
                   "не влияет")
    print("[sow] детерминизм: два прогона совпали, другое семя — нет")

    if bad:
        print("[sow] ЗАКОН ПОСЕВА НЕ ДЕРЖИТСЯ:", file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1
    print("[sow] четыре руки закона посева зелёные")
    return 0

if __name__ == "__main__":
    sys.exit(main())
