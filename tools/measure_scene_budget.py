#!/usr/bin/env python3
#
# File: tools/measure_scene_budget.py
#
# Responsibility:
# - СЧЁТ БЮДЖЕТА КОМПОЗИЦИОННОЙ СЦЕНЫ: треугольники и ДРО, посчитанные ровно
#   тем правилом, каким приложение раскладывает сцену по плиткам
#   (AppWorld.cpp: одна плитка 32 м, потоки wood/cards/bark/ground — разные
#   партии, объекты kind == "emissive" в плитки не попадают вовсе).
#
# Usage:
#     python3 tools/measure_scene_budget.py <файл.scene> <полка>[;<полка>...]
#
# ЗАЧЕМ. Куртинный посев и ковёр меняют число дро и треугольников на кадр —
# это назвала ресёрчер, и это единственная строка её записки, которую нельзя
# закрыть кадром. Замерять «на глаз по FPS» здесь нечего: узкое место сцены —
# не пиксели, а СКОЛЬКО ПАРТИЙ уходит в кадр, и это чистая функция файла.
#
# ЧЕГО ОН НЕ МЕРИТ, ЧЕСТНО: отсечение по пирамиде и лестницу дальних форм. Это
# ПОТОЛОК — «сколько всего в сцене», а не «сколько в кадре». Потолок и есть та
# величина, которая обязана остаться управляемой: кадр меняется от того, куда
# смотришь, а файл — нет.
#
# Dependencies:
# - Uses: tools/dfo_read.py, Python stdlib.
# - Used by: рука и отчёты волны ярусов.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ПРАВИЛО ПЛИТКИ ЖИВЁТ В ДВУХ МЕСТАХ (здесь и в AppWorld.cpp), и это
#   ИЗМЕРИТЕЛЬ, а не второй движок: если приложение сменит размер плитки,
#   этот счёт станет неверным МОЛЧА. Поэтому размер вынесен в TILE_M с
#   явной ссылкой на источник.

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dfo_read  # noqa: E402

# ИСТОЧНИК: engine/app/sources/AppWorld.cpp, «The tile is 32 m».
TILE_M = 32.0
STREAMS = ("WOOD", "CARD", "GRND", "BARK")

def read_places(path):
    out = []
    cur = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("["):
                if cur.get("object") and cur.get("section") == "place":
                    out.append(cur)
                cur = {"section": line[1:-1]}
            elif "=" in line and not line.startswith("#"):
                k, v = line.split("=", 1)
                cur[k.strip()] = v.strip()
    if cur.get("object") and cur.get("section") == "place":
        out.append(cur)
    return out

def main():
    if len(sys.argv) < 3:
        print("usage: measure_scene_budget.py <scene> <shelf>[;<shelf>...]",
              file=sys.stderr)
        return 2
    scene = sys.argv[1]
    shelves = [d for d in sys.argv[2].split(";") if d]
    objects = dfo_read.shelf(shelves)
    places = read_places(scene)

    tiles = {}
    tris = 0
    tris_far = 0
    missing = set()
    by_kind = {}
    for p in places:
        name = p["object"]
        o = objects.get(name)
        if o is None:
            missing.add(name)
            continue
        px, _py, pz = (float(v) for v in p["pos"].split())
        if o.kind == "emissive":
            continue          # самосветящиеся в плитки не входят (AppWorld)
        key = (int(px // TILE_M), int(pz // TILE_M))
        t = tiles.setdefault(key, {s: 0 for s in STREAMS})
        for s in STREAMS:
            st = o.streams.get(s)
            if st is not None:
                t[s] += st.tris
        tris += o.tris
        far = objects.get(name + "-far")
        tris_far += (far or o).tris
        k = by_kind.setdefault(o.kind, [0, 0])
        k[0] += 1
        k[1] += o.tris

    draws = sum(1 for t in tiles.values() for s in STREAMS if t[s] > 0)
    print("сцена            : %s" % scene)
    print("размещений       : %d%s" % (len(places),
          ("  (нет на полке: %d)" % len(missing)) if missing else ""))
    print("треугольников    : %d (ближняя форма)" % tris)
    print("                 : %d (если бы ВСЁ стояло дальней формой, -%.0f%%)"
          % (tris_far, 100.0 * (1.0 - tris_far / max(1.0, float(tris)))))
    print("плиток 32 м      : %d" % len(tiles))
    print("ДРО (потоков)    : %d" % draws)
    if tiles:
        worst = max(tiles.items(), key=lambda kv: sum(kv[1].values()))
        print("худшая плитка    : %s — %d трис, %d дро"
              % (worst[0], sum(worst[1].values()),
                 sum(1 for s in STREAMS if worst[1][s] > 0)))
        print("трис на плитку   : среднее %d"
              % (tris // max(1, len(tiles))))
    for k, (n, t) in sorted(by_kind.items()):
        print("  вид %-9s: %5d шт, %8d трис" % (k, n, t))
    if missing:
        print("  НЕТ НА ПОЛКЕ  : " + ", ".join(sorted(missing)[:8]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
