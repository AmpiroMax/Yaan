#!/usr/bin/env python3
#
# Created: 22:08:2026 - 19:30:00
# Last updated: 22:08:2026 - 22:15:00
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
# - КОНВЕНЦИЯ ПОВОРОТА БЕРЁТСЯ ИЗ ДВИЖКА (AppHouse.cpp:790), а не из памяти и
#   не из шапки соседнего файла. Оба знака проверяются на несимметричном
#   случае: очаг внутри дома обязан оказаться ВНУТРИ пятна. Ошибка в ОДНОМ
#   знаке даёт зеркальное пятно и правдоподобные, но полностью ложные числа.
#
# UPD:
# - 22:08:2026 - 19:30:00: Создан. Прибор написан wr-forge при замере мощения
#   Вайтрана; вынесен из черновиков в конвейер по доводу архитектора: критерий
#   приёмки, живущий в чужой голове, проверяется один раз.
# - 22:08:2026 - 20:15:00: ДВА БАГА, оба нашёл wr-paver внешним замером.
#   (1) ЗЕРКАЛЬНОЕ ПЯТНО: у члена lz стоял минус, местный +Z шёл в (-sin,-cos)
#   вместо (+sin,+cos). Пятно уезжало на всю ширину поперёк улицы, осевая линия
#   тракта из него выпадала. Прибор показывал 3.6% там, где замощено сплошь, и
#   считал наложениями соседние по улице куски. После правки 88.6% против
#   88.7% у независимого замера wr-paver — сходится.
#   (2) СТУПЕНЬ ПО origin.y: у наклонного куска origin — нижняя кромка, поэтому
#   честно состыкованная пара s08 8x6 давала «ступень» 0.640 = 0.08*8, то есть
#   собственный подъём куска. Меряется top_at() в точке стыка; стало 0 ступеней
#   выше порога против 0.035 м максимума у wr-paver.
#   Урок в Notice выше: одна опечатка в знаке даёт числа, которые выглядят
#   правдоподобно и потому не вызывают подозрений.
# - 22:08:2026 - 21:20:00: Шаг пробы 0.5 -> 0.25 м: критерий приёмки архитектора
#   назван при шаге 0.25 (разрыв <= 1.0 м), и прибор обязан мерить в тех же
#   единицах, в каких сформулирован критерий, иначе полуметровый разрыв
#   округляется в ноль.
# - 22:08:2026 - 21:50:00: НАЛОЖЕНИЯ — ТРИ КОРЗИНЫ, порог площади СНЯТ. Прежний
#   отчёт давал МАКСИМУМ dy по наложениям, а для z-fighting важен МИНИМУМ:
#   «dy <= 0.056» включало ровный ноль и прятало два соплоскостных наложения.
#   Запретов на наложение два, и пороги у них противоположные: мерцание живёт
#   при dy ~ 0, ступень — при dy > 0.20, между ними полоса безвредного выступа.
#   Порог по ПЛОЩАДИ убран намеренно: 0.6 м2 соплоскостного мерцания под ногами
#   хуже, чем 15 м2 с честным сантиметром разницы. Указал архитектор.
# - 22:08:2026 - 22:15:00: КРИТЕРИЙ — ТВЁРДАЯ ПОВЕРХНОСТЬ, а не плита мощения
#   (поправка архитектора). Прибор считал только city-cobble и потому показывал
#   на МОСТУ 1.3 % — то есть звал замостить булыжником авторский настил. Список
#   покрытий заведён явно (мост, furn-walk2, плазы, марши), умолчание «не
#   считается». Пятна сняты прибором с самих .dfh и хранятся ПРЯМОУГОЛЬНИКОМ от
#   origin: у моста origin не в углу, настил идёт от -2.44 до 10.44 по X.
#   Стыки и наложения разбираются только между плитами мостовой (JOINT_KINDS):
#   у марша и моста своя геометрия верха, и мерить их формулой мостовой значит
#   врать. Тракт 1: 1.3 % -> 44.3 %, разрыв 20 -> 5.8 м; всего 88.6 -> 91.3 %.
#
from __future__ import annotations

import json
import math
import sys

# ЧТО СЧИТАЕТСЯ ТВЁРДОЙ ПОВЕРХНОСТЬЮ ПОД НОГАМИ (список архитектора, 22.08).
#
# Критерий приёмки — «ось проходится из конца в конец, не сходя с ТВЁРДОЙ
# ПОВЕРХНОСТИ», а не «по плите мощения». Авторский настил считается покрытием
# наравне с булыжником: мост несёт свой настил, и замостить его сверху было бы
# прямой ошибкой. Прибор, считавший только city-cobble, показывал на мосту
# 1.3 % и звал мостить мост.
#
# УМОЛЧАНИЕ — «НЕ СЧИТАЕТСЯ». Новый рецепт невидим прибору, пока его сюда не
# внесли сознательно. Обратное умолчание однажды засчитает покрытием крышу.
#
# Прямоугольник задан ОТНОСИТЕЛЬНО origin рецепта (x0, z0, x1, z1) и снят
# прибором с самого .dfh, а не выписан из имени: у моста origin вовсе не в
# углу — настил с аппарелями идёт от -2.44 до 10.44 по локальному X.
RECIPE = {
    # мостовая: origin в углу, пятно ровно w x d
    "city-cobble24x6": (0, 0, 24.0, 6.0), "city-cobble12x6": (0, 0, 12.0, 6.0),
    "city-cobble8x6": (0, 0, 8.0, 6.0),   "city-cobble6x6": (0, 0, 6.0, 6.0),
    "city-cobble24x4": (0, 0, 24.0, 4.0), "city-cobble16": (0, 0, 16.0, 16.0),
    "city-cobble14x11": (0, 0, 14.0, 11.0),
    # авторские настилы
    "city-bridge": (-2.44, -0.15, 10.44, 4.24),   # настил моста с аппарелями
    "furn-walk2": (0.0, 0.0, 2.00, 1.20),         # каменная дорожная плита
    "city-plaza12": (-0.12, -0.12, 12.12, 12.12),
    "city-plaza20": (-0.12, -0.12, 20.12, 20.12),
    "city-stairs": (-0.05, -0.02, 4.05, 6.37),    # марш: ступени тоже опора
    "city-stairs6": (-0.05, -0.02, 4.05, 12.37),
}
# Стыки/наложения разбираются ТОЛЬКО между плитами мостовой: у марша и моста
# своя геометрия верха, и мерить их «ступень» формулой мостовой значит врать.
JOINT_KINDS = ("city-cobble",)

STEP_M = 0.25       # шаг пробы вдоль тракта (критерий архитектора)
CONTACT_M = 0.6     # зазор, ниже которого пара считается СТЫКОМ
STEP_LIMIT_M = 0.20 # порог ступени под ногами
ZFIGHT_M = 0.01     # ниже этого наложение СОПЛОСКОСТНО и будет мерцать


def recipe_rect(stem: str):
    """Пятно рецепта (x0,z0,x1,z1) от origin. Наклонный -sNN — родительское."""
    base = stem
    if "-s" in stem and stem.rsplit("-s", 1)[1].isdigit():
        base = stem.rsplit("-s", 1)[0]
    return RECIPE.get(base)


def read_slabs(path: str):
    """Плиты мостовой из .scene: (стем, x, y, z, yaw_deg, w, d)."""
    import re
    blocks = re.split(r"\n(?=\[)", open(path, encoding="utf-8").read())
    out = []
    for b in blocks:
        m = re.search(r"([a-z0-9-]+?)\.dfh", b)
        if not m:
            continue
        rect = recipe_rect(m.group(1))
        if rect is None:
            continue  # умолчание: не покрытие
        p = re.search(r"\bpos\s*=\s*([-\d.]+)[ ,]+([-\d.]+)[ ,]+([-\d.]+)", b)
        y_ = re.search(r"\byaw\s*=\s*([-\d.]+)", b)
        if not p:
            continue
        x, y, z = (float(v) for v in p.groups())
        # yaw в сцене — радианы; локальный +X = (cos, -sin), начало в углу.
        yaw = math.degrees(float(y_.group(1))) if y_ else 0.0
        out.append((m.group(1), x, y, z, yaw, rect))
    return out


def poly(s):
    _, x, _y, z, a, (rx0, rz0, rx1, rz1) = s
    r = math.radians(a)
    c, si = math.cos(r), math.sin(r)
    # КОНВЕНЦИЯ ВЗЯТА ИЗ ДВИЖКА, а не из памяти. AppHouse.cpp:790, посадка
    # готовой постройки в мир:
    #     pos + {l.x*c + l.z*sn, l.y, -l.x*sn + l.z*c}
    # то есть местный +X -> (cos, -sin), местный +Z -> (+sin, +cos).
    # ЗДЕСЬ БЫЛ БАГ (22.08): знак у члена lz стоял минусом, пятно строилось
    # зеркально поперёк улицы. Осевая линия тракта из него выпадала, и прибор
    # показывал 3.6% покрытия там, где замощено сплошь, а соседние по улице
    # куски читались наложениями. Нашёл wr-paver внешним замером.
    return [(x + lx * c + lz * si, z - lx * si + lz * c)
            for lx, lz in ((rx0, rz0), (rx1, rz0), (rx1, rz1), (rx0, rz1))]


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


def slope_of(stem: str) -> float:
    """Уклон рецепта: суффикс -sNN это NN процентов вдоль локального +X."""
    if "-s" not in stem:
        return 0.0
    return int(stem.rsplit("-s", 1)[1]) / 100.0


PAVE_TOP = 0.087  # верх мостовой над origin у НИЖНЕЙ кромки


def top_at(s, pt) -> float:
    """Высота ХОДИБЕЛЬНОЙ ПОВЕРХНОСТИ плиты в мировой точке pt.

    СТУПЕНЬ НЕЛЬЗЯ МЕРИТЬ ПО origin.y: у наклонного куска origin — НИЖНЯЯ
    кромка, поэтому пара честно состыкованных s08 8x6 даёт разницу origin.y
    ровно 0.64 — это собственный подъём куска, а не ступень. Мерить надо верх
    в ТОЧКЕ СТЫКА. Ошибку нашёл wr-paver (22.08).
    """
    stem, x, y, z, a, _rect = s
    r = math.radians(a)
    # Проекция на локальный +X, который в мире идёт в (cos, -sin).
    lx = (pt[0] - x) * math.cos(r) - (pt[1] - z) * math.sin(r)
    return y + PAVE_TOP + slope_of(stem) * lx


def contact_point(A, B):
    """Представительная точка стыка: центр пересечения либо середина сближения."""
    inter = clip(A, B)
    if len(inter) >= 3:
        return (sum(p[0] for p in inter) / len(inter),
                sum(p[1] for p in inter) / len(inter))
    best, bp = 1e18, None
    for p in A:
        for q in B:
            d = (p[0]-q[0])**2 + (p[1]-q[1])**2
            if d < best:
                best, bp = d, ((p[0]+q[0])/2, (p[1]+q[1])/2)
    return bp


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
    from collections import Counter
    kinds = Counter(s[0].split("-s")[0] for s in slabs)
    print("покрытие в сцене: " + ", ".join(f"{k} x{v}" for k, v in
                                           sorted(kinds.items())))
    print(f"всего тел покрытия: {len(slabs)}; каменных трактов: {len(roads)}\n")

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
    # ТРИ КОРЗИНЫ. Порога по площади нет: соплоскостное наложение в полметра
    # хуже пятнадцати метров с честным сантиметром разницы.
    zfight, benign, over_step, butt = [], [], [], []
    for i in range(len(slabs)):
        if not slabs[i][0].startswith(JOINT_KINDS):
            continue
        for j in range(i + 1, len(slabs)):
            if not slabs[j][0].startswith(JOINT_KINDS):
                continue
            a = area(clip(polys[i], polys[j]))
            g = gap(polys[i], polys[j])
            if a <= 0.0 and g >= CONTACT_M:
                continue
            cp = contact_point(polys[i], polys[j])
            dy = abs(top_at(slabs[i], cp) - top_at(slabs[j], cp))
            if a > 0.0:
                rec = (slabs[i], slabs[j], a, dy)
                (zfight if dy < ZFIGHT_M
                 else (benign if dy <= STEP_LIMIT_M else over_step)).append(rec)
            else:
                butt.append((slabs[i], slabs[j], dy))
    bad_steps = [b for b in butt if b[2] > STEP_LIMIT_M]
    over = zfight + over_step

    print(f"\nпокрытие каменных трактов: {100*hit/max(tot,1):.1f}%,"
          f" длиннейший разрыв {worst_gap:.0f} м")
    print(f"наложений: z-fighting (dy<{ZFIGHT_M}) {len(zfight)},"
          f" безвредных {len(benign)}, со ступенью {len(over_step)}"
          f"   |   стыков {len(butt)}, из них со ступенью"
          f" >{STEP_LIMIT_M} м: {len(bad_steps)}")
    for a, b, ar, dy in zfight:
        print(f"  Z-FIGHTING {a[0]}@({a[1]:.0f},{a[3]:.0f}) x"
              f" {b[0]}@({b[1]:.0f},{b[3]:.0f}): {ar:.2f} м2, dy {dy:.4f}")
    for a, b, ar, dy in over_step:
        print(f"  НАЛОЖЕНИЕ СО СТУПЕНЬЮ {a[0]}@({a[1]:.0f},{a[3]:.0f}) x"
              f" {b[0]}@({b[1]:.0f},{b[3]:.0f}): {ar:.2f} м2, dy {dy:.3f}")
    if benign:
        dys = [r[3] for r in benign]
        print(f"  (безвредные наложения: dy {min(dys):.3f}..{max(dys):.3f}"
              f" — выступ на сантиметр-другой, справкой)")
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
