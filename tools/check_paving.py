#!/usr/bin/env python3
#
# File: tools/check_paving.py
#
# Responsibility:
# - ПРИЁМКА МОЩЕНИЯ ЧИСЛОМ, а не на глаз. Критерий архитектора (22.08):
#   процессионная ось обязана проходиться ИЗ КОНЦА В КОНЕЦ, не сходя с
#   мощения. Метраж критерием НЕ является: 1030 м² звучат приёмкой ровно до
#   вопроса, где они лежат.
# - ПРЕДМЕТ ЗАМЕРА — ВЫПУЩЕННЫЙ ФАЙЛ <город>.relief, а не память генератора.
#   Мощение с 24.08 живёт СТРОКАМИ `path`/`pp` рельефа (класс полотна, третье
#   число строки path), а не телами плит в .scene.
#
# Usage:
#     python3 tools/check_paving.py [<relief>] [<plan.json>]
#     (умолчания: assets/scenes/whiterun.relief, docs/WHITERUN_PLAN.json)
#     Ненулевой выход, если приёмка не пройдена.
#
# Dependencies:
# - Uses: Python stdlib и tools/gen_city.py (Fabric + path_polyline + path_class
#   — зеркало движка, ОДНО на дерево, см. «ГРАНИЦА ПРИБОРА» ниже).
# - Used by: ctest (paving_whiterun, paving_cornhall, paving_bare_rejected,
#   paving_blot_rejected), раскладчик города и приёмка.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ДАННЫЕ БЕРУТСЯ ИЗ ВЫПУСКА, ГЕОМЕТРИЯ — ИЗ ЗЕРКАЛА ДВИЖКА. Это разные вещи,
#   и смешивать их нельзя: свою копию формул износа и класса прибор НЕ ДЕРЖИТ
#   (правило 39 — теневая копия цепочки расходится с оригиналом молча), но и
#   ленты из памяти генератора НЕ БЕРЁТ (иначе он мерил бы намерение, а не
#   выпуск, и сегодняшний дефект — файл без мощения — прошёл бы зелёным).
# - УМОЛЧАНИЕ КЛАССА — «НЕ ТВЁРДОЕ». Строка path без третьего числа читается
#   движком как класс 1 (укатанный грунт), и прибор читает её так же.
from __future__ import annotations

import json
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import gen_city as G  # noqa: E402  (путь к пакету приходится дописать)

STEP_M = 0.25       # шаг пробы вдоль оси (критерий архитектора)
GAP_LIMIT_M = 0.0   # длиннейший разрыв полотна на оси, метры

# ЧТО СЧИТАЕТСЯ ТВЁРДОЙ ПОВЕРХНОСТЬЮ, ПО КЛАССАМ ПУТЕВОГО АТЛАСА (core
# PathClass, ordinals — прибитый межзонный контракт world::PathClassTests):
#   0 мостовая, 1 укатанный грунт, 2 протоптанная стёжка, 3 тёсаные плиты.
HARD_CLASSES = (0, 3)
CLASS_NAME = {0: "мостовая", 1: "грунт", 2: "стёжка", 3: "тёсаный камень"}

def read_relief_paths(path: str):
    """Ленты полотна из ВЫПУЩЕННОГО .relief: (ломаная, полуширина, soft, класс,
    имя). Формат — ReliefLayer.cpp::read_relief: строка `path <half> <soft>
    [<класс>]`, за ней точки `pp <x> <z>`; ТРЕТЬЕГО ЧИСЛА МОЖЕТ НЕ БЫТЬ, и
    тогда движок читает класс 1 (укатанный грунт) — прибор читает так же.
    Лента короче двух точек это МЕСТО, а не путь (движок возвращает сами
    точки и ничего не штампует); такие сюда не попадают."""
    out, cur = [], None
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            tok = line.split()
            if not tok:
                continue
            if tok[0] == "path":
                cur = ([], float(tok[1]),
                       float(tok[2]) if len(tok) > 2 else 1.0,
                       int(tok[3]) if len(tok) > 3 else 1,
                       "лента #%d" % len(out))
                out.append(cur)
            elif tok[0] == "pp" and cur is not None:
                cur[0].append((float(tok[1]), float(tok[2])))
            elif tok[0] != "pp":
                cur = None
    return [p for p in out if len(p[0]) >= 2]

def axis_samples(pts):
    """Пробы по ЛОМАНОЙ ЧЕРТЕЖА, а не по кривой полотна, и это выбор.

    Кривая полотна — это ТА ЖЕ ломаная, растянутая центростремительным
    Catmull-Rom, и лента кладётся ПО НЕЙ. Проба, поставленная на кривую,
    поэтому лежит на осевой ленты по построению: износ там равен единице
    всегда, при любом состоянии выпуска, и такой замер не способен провалиться
    ни от чего (правило 27). Ломаная чертежа — линия, которую провёл
    архитектор; кривая отходит от неё до 1.86 м, и вопрос «накрыта ли она
    полотном» имеет обе стороны."""
    sm = []
    for a, b in zip(pts, pts[1:]):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(L / STEP_M))
        for i in range(n):
            sm.append((a[0] + (b[0] - a[0]) * i / n,
                       a[1] + (b[1] - a[1]) * i / n))
    sm.append(tuple(pts[-1]))
    return sm

def check(relief: str, plan_path: str) -> int:
    plan = json.load(open(plan_path, encoding="utf-8"))
    # path_class() спрашивает у чертежа кольцо стен — связываем ЕГО, а не
    # паспорт города: прибору нужен только контур, и оснастка обязана уметь
    # обойтись контуром (правило 30a — у теста должен быть проходимый случай).
    G.PLAN = plan
    paths = read_relief_paths(relief)
    fab = G.Fabric(paths)

    from collections import Counter
    kinds = Counter(p[3] for p in paths)
    print("полотно в выпуске: " + (", ".join(
        f"кл.{k} ({CLASS_NAME.get(k, '?')}) x{v}"
        for k, v in sorted(kinds.items())) or "НЕТ НИ ОДНОЙ ЛЕНТЫ"))
    roads = [r for r in plan["roads"] if r["mat"] == "stone"]
    print(f"всего лент полотна: {len(paths)}; каменных трактов: {len(roads)}\n")

    worst_gap, n_bare, n_blot, n_seam, n_all = 0.0, 0, 0, 0, 0
    blots = []
    for k, rd in enumerate(roads, 1):
        want = G.path_class(rd)
        ends = (tuple(rd["pts"][0]), tuple(rd["pts"][-1]))
        sm = axis_samples(rd["pts"])
        own, bare, blot, seam, run, best = 0, 0, 0, 0, 0, 0
        for x, z in sm:
            cls, _wear, who = fab.sample(x, z)
            if cls is None:
                bare += 1
                run += 1
                best = max(best, run)
                continue
            run = 0
            if cls == want:
                own += 1
                continue
            # СТЫК ДВУХ ПОЛОТЕН — НЕ КЛЯКСА, И РАЗДЕЛЯЮТСЯ ОНИ ПО ТОПОЛОГИИ,
            # А НЕ ПО ВЕЛИЧИНЕ (правило 47). Там, где мостовая внутри стен
            # встречает загородную стёжку, переходная полоса шириной с радиус
            # класса неизбежна: у движка побеждает ПОСЛЕДНИЙ накрывший мазок,
            # слоя приоритетов нет. Законна встреча КОНЦОМ В КОНЕЦ; чужая
            # лента поперёк улицы, ни к чему не примыкающая, — клякса.
            if who is not None and any(fab.touches(who, ex, ez)
                                       for ex, ez in ends):
                seam += 1
            else:
                blot += 1
                if len(blots) < 8:
                    blots.append((k, x, z, cls, want,
                                  fab.st[who][4] if who is not None else "?"))
        gap = best * STEP_M
        worst_gap = max(worst_gap, gap)
        n_bare += bare
        n_blot += blot
        n_seam += seam
        n_all += len(sm)
        print(f"тракт {k} ({CLASS_NAME.get(want, '?')}, кл.{want}):"
              f" {rd['pts'][0]} -> {rd['pts'][-1]},"
              f" длина {len(sm)*STEP_M:5.0f} м"
              f"   своё полотно {100*own/len(sm):5.1f}%"
              f"   голо {100*bare/len(sm):5.1f}%"
              f"   разрыв {gap:5.2f} м"
              + (f"   стык {seam}" if seam else "")
              + (f"   КЛЯКСА {blot}" if blot else ""))

    print(f"\n(1) СВЯЗНОСТЬ: полотно своего класса на"
          f" {100*(n_all-n_bare-n_blot-n_seam)/max(n_all,1):.1f}% проб оси,"
          f" голо {100*n_bare/max(n_all,1):.1f}%,"
          f" длиннейший разрыв {worst_gap:.2f} м (порог {GAP_LIMIT_M:.2f} м)")
    print(f"(2) КЛАССОВАЯ ЧИСТОТА: клякс {n_blot} (порог 0);"
          f" стыков с соседним полотном {n_seam} — законны")
    for k, x, z, cls, want, nm in blots:
        print(f"  КЛЯКСА тракт {k} у ({x:.1f}, {z:.1f}): кл.{cls}"
              f" ({CLASS_NAME.get(cls, '?')}) на полотне кл.{want}"
              f" — «{nm}»")

    ok = worst_gap <= GAP_LIMIT_M and n_bare == 0 and n_blot == 0
    print(f"\nПРИЁМКА «ось проходится из конца в конец»:"
          f" {'ПРОЙДЕНА' if ok else 'НЕ ПРОЙДЕНА'}")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(check(sys.argv[1] if len(sys.argv) > 1
                   else "assets/scenes/whiterun.relief",
                   sys.argv[2] if len(sys.argv) > 2
                   else "docs/WHITERUN_PLAN.json"))
