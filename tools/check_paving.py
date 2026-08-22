#!/usr/bin/env python3
#
# Created: 22:08:2026 - 19:30:00
# Last updated: 22:08:2026 - 19:30:00
# File: tools/check_paving.py
#
# Responsibility:
# - ПРИЁМКА МОЩЕНИЯ ЧИСЛОМ, а не на глаз. Критерий архитектора (22.08):
#   процессионная ось обязана проходиться ИЗ КОНЦА В КОНЕЦ, не сходя с
#   мощения. Метраж критерием НЕ является: 1030 м² звучат приёмкой ровно до
#   вопроса, где они лежат — замер дал 35.3% покрытия и разрыв 33 м.
# - Две проверки: (1) покрытие каменных трактов PLAN["roads"] с длиннейшим
#   разрывом; (2) пары плит в ТРИ корзины — наложение / стык / врозь — с Δy.
#
# Usage:
#     python3 tools/check_paving.py [<scene>] [<plan.json>]
#     (умолчания: assets/scenes/whiterun.scene, docs/WHITERUN_PLAN.json)
#     Ненулевой выход, если приёмка не пройдена.
#
# Dependencies:
# - Uses: Python stdlib. Габариты рецептов — таблица RECIPE ниже.
# - Used by: раскладчик города (gen_whiterun.py, check_layout) и приёмка.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ТРИ КОРЗИНЫ ОБЯЗАТЕЛЬНЫ. Без деления «наложение / стык / врозь» прибор
#   врёт систематически в сторону паники: терраса с Δy 3.48 м попадёт в
#   дефекты, а «врозь» даст число, которое есть арифметика цепи, а не
#   находка (в цепи из 17 звеньев дальние пары дальние — это не дефект).
# - ГАБАРИТ БЕРЁТСЯ ИЗ РЕЦЕПТА, а не из имени файла: имя врёт. Наклонные
#   куски -sNN имеют РОДИТЕЛЬСКОЕ пятно в плане (у них меняется только y),
#   иначе они молча читаются как «непокрыто».
#
# UPD:
# - 22:08:2026 - 19:30:00: Создан. Прибор написан wr-forge при замере мощения
#   Вайтрана; вынесен из черновиков в конвейер по доводу архитектора: критерий
#   приёмки, живущий в чужой голове, проверяется один раз.
#
from __future__ import annotations

import json
import math
import sys

# Габариты рецептов мостовой (w вдоль локального +X, d поперёк), метры.
# Наклонные варианты -sNN наследуют пятно родителя.
RECIPE = {
    "city-cobble24x6": (24.0, 6.0), "city-cobble12x6": (12.0, 6.0),
    "city-cobble8x6": (8.0, 6.0),   "city-cobble6x6": (6.0, 6.0),
    "city-cobble24x4": (24.0, 4.0), "city-cobble16": (16.0, 16.0),
    "city-cobble14x11": (14.0, 11.0),
}
STEP_M = 0.5        # шаг пробы вдоль тракта
CONTACT_M = 0.6     # зазор, ниже которого пара считается СТЫКОМ
STEP_LIMIT_M = 0.20 # порог ступени на стыке


def recipe_size(stem: str):
    """Пятно рецепта. Наклонный -sNN — родительское: меняется только y."""
    base = stem.split("-s")[0] if "-s" in stem else stem
    return RECIPE.get(base)


def read_slabs(path: str):
    """Плиты мостовой из .scene: (стем, x, y, z, yaw_deg, w, d)."""
    import re
    blocks = re.split(r"\n(?=\[)", open(path, encoding="utf-8").read())
    out = []
    for b in blocks:
        m = re.search(r"(city-cobble[0-9x]+(?:-s\d+)?)\.dfh", b)
        if not m:
            continue
        size = recipe_size(m.group(1))
        if size is None:
            print(f"  ВНИМАНИЕ: нет габарита для {m.group(1)} — пропущена")
            continue
        p = re.search(r"\bpos\s*=\s*([-\d.]+)[ ,]+([-\d.]+)[ ,]+([-\d.]+)", b)
        y_ = re.search(r"\byaw\s*=\s*([-\d.]+)", b)
        if not p:
            continue
        x, y, z = (float(v) for v in p.groups())
        # yaw в сцене — радианы; локальный +X = (cos, -sin), начало в углу.
        yaw = math.degrees(float(y_.group(1))) if y_ else 0.0
        out.append((m.group(1), x, y, z, yaw, size[0], size[1]))
    return out


def poly(s):
    _, x, _y, z, a, w, d = s
    r = math.radians(a)
    c, si = math.cos(r), math.sin(r)
    return [(x + lx * c - lz * si, z - lx * si - lz * c)
            for lx, lz in ((0, 0), (w, 0), (w, d), (0, d))]


def inside(pt, P) -> bool:
    c = False
    for i in range(len(P)):
        a, b = P[i], P[(i + 1) % len(P)]
        if (a[1] > pt[1]) != (b[1] > pt[1]):
            if pt[0] < a[0] + (pt[1] - a[1]) / (b[1] - a[1]) * (b[0] - a[0]):
                c = not c
    return c


def clip(sub, cl):
    """Сазерленд-Ходжман: пересечение выпуклых контуров."""
    def ins(p, a, b):
        return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]) >= 0

    def itr(p, q, a, b):
        d1 = (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0])
        d2 = (b[0]-a[0])*(q[1]-a[1]) - (b[1]-a[1])*(q[0]-a[0])
        t = d1 / (d1 - d2)
        return (p[0] + t*(q[0]-p[0]), p[1] + t*(q[1]-p[1]))

    out = sub
    if (cl[1][0]-cl[0][0])*(cl[2][1]-cl[0][1]) \
            - (cl[1][1]-cl[0][1])*(cl[2][0]-cl[0][0]) < 0:
        cl = cl[::-1]
    for i in range(len(cl)):
        a, b = cl[i], cl[(i + 1) % len(cl)]
        inp, out = out, []
        for j in range(len(inp)):
            p, q = inp[j], inp[(j + 1) % len(inp)]
            if ins(p, a, b):
                out.append(p)
                if not ins(q, a, b):
                    out.append(itr(p, q, a, b))
            elif ins(q, a, b):
                out.append(itr(p, q, a, b))
        if not out:
            return []
    return out


def area(p) -> float:
    if len(p) < 3:
        return 0.0
    return abs(sum(p[i][0]*p[(i+1) % len(p)][1] - p[(i+1) % len(p)][0]*p[i][1]
                   for i in range(len(p)))) / 2


def gap(A, B) -> float:
    """Минимальный зазор между кромками; 0 при перекрытии."""
    if any(inside(p, B) for p in A) or any(inside(p, A) for p in B):
        return 0.0

    def seg(p, q, a, b):
        def d(px, py, ax, ay, bx, by):
            vx, vy = bx-ax, by-ay
            wx, wy = px-ax, py-ay
            L = vx*vx + vy*vy
            t = 0 if L == 0 else max(0, min(1, (wx*vx + wy*vy)/L))
            return math.hypot(px-(ax+t*vx), py-(ay+t*vy))
        return min(d(*p, *a, *b), d(*q, *a, *b), d(*a, *p, *q), d(*b, *p, *q))
    return min(seg(A[i], A[(i+1) % 4], B[j], B[(j+1) % 4])
               for i in range(4) for j in range(4))


def check(scene: str, plan: str) -> int:
    slabs = read_slabs(scene)
    polys = [poly(s) for s in slabs]
    roads = [r for r in json.load(open(plan, encoding="utf-8"))["roads"]
             if r["mat"] == "stone"]
    print(f"плит мощения: {len(slabs)}; каменных трактов: {len(roads)}\n")

    # --- (1) СВЯЗНОСТЬ: пройти ось из конца в конец, не сходя с мощения ----
    worst_gap = 0.0
    hit = tot = 0
    for k, rd in enumerate(roads, 1):
        P = rd["pts"]
        pts = []
        for (ax, az), (bx, bz) in zip(P, P[1:]):
            L = math.hypot(bx-ax, bz-az)
            n = max(1, int(L / STEP_M))
            for i in range(n):
                pts.append((ax + (bx-ax)*i/n, az + (bz-az)*i/n))
        pts.append(tuple(P[-1]))
        cov = [any(inside(p, q) for q in polys) for p in pts]
        hit += sum(cov)
        tot += len(cov)
        run = best = 0
        for c in cov:
            run = 0 if c else run + 1
            best = max(best, run)
        worst_gap = max(worst_gap, best * STEP_M)
        print(f"тракт {k}: {P[0]} -> {P[-1]}, длина {len(pts)*STEP_M:5.0f} м"
              f"   покрыто {100*sum(cov)/len(cov):5.1f}%"
              f"   длиннейший разрыв {best*STEP_M:5.1f} м")

    # --- (2) ТРИ КОРЗИНЫ -------------------------------------------------
    over, butt = [], []
    for i in range(len(slabs)):
        for j in range(i + 1, len(slabs)):
            a = area(clip(polys[i], polys[j]))
            dy = abs(slabs[i][2] - slabs[j][2])
            if a > 0.5:
                over.append((slabs[i], slabs[j], a, dy))
            elif gap(polys[i], polys[j]) < CONTACT_M:
                butt.append((slabs[i], slabs[j], dy))
    bad_steps = [b for b in butt if b[2] > STEP_LIMIT_M]

    print(f"\nпокрытие каменных трактов: {100*hit/max(tot,1):.1f}%,"
          f" длиннейший разрыв {worst_gap:.0f} м")
    print(f"наложений (>0.5 м2): {len(over)}"
          f"   стыков: {len(butt)}, из них со ступенью >{STEP_LIMIT_M} м:"
          f" {len(bad_steps)}")
    for a, b, ar, dy in over:
        print(f"  НАЛОЖЕНИЕ {a[0]}@({a[1]:.0f},{a[3]:.0f}) x"
              f" {b[0]}@({b[1]:.0f},{b[3]:.0f}): {ar:.1f} м2, dy {dy:.3f}")
    for a, b, dy in bad_steps:
        print(f"  СТУПЕНЬ {a[0]}@({a[1]:.0f},{a[3]:.0f}) x"
              f" {b[0]}@({b[1]:.0f},{b[3]:.0f}): dy {dy:.2f} м")

    ok = worst_gap == 0.0 and not over and not bad_steps
    print(f"\nПРИЁМКА «ось проходится из конца в конец»:"
          f" {'ПРОЙДЕНА' if ok else 'НЕ ПРОЙДЕНА'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(check(sys.argv[1] if len(sys.argv) > 1
                   else "assets/scenes/whiterun.scene",
                   sys.argv[2] if len(sys.argv) > 2
                   else "docs/WHITERUN_PLAN.json"))
