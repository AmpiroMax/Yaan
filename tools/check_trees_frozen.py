#!/usr/bin/env python3
#
# File: tools/check_trees_frozen.py
#
# Responsibility:
# - СТОРОЖ НЕПРИКОСНОВЕННОСТИ ПЕРВОЙ ИТЕРАЦИИ ДЕРЕВЬЕВ. Печёт полку dfn_forge
#   во временную папку и сверяет content_hash каждого рецепта со строкой в
#   assets/objects/trees/INDEX.md. Расхождение — отказ.
#
# Usage:
#     python3 tools/check_trees_frozen.py <путь к dfn_forge>
#
# ЗАЧЕМ ПРИБОР, А НЕ ОБЕЩАНИЕ. Волна второй итерации (28.08) поклялась не
# трогать вид старых деревьев. Клятву держит не аккуратность, а построение:
# новый строитель стоит отдельным файлом, новые ряды атласа — отдельными
# рядами. Но обе клятвы можно нарушить одной строкой в общем коде (FloraCards,
# ObjectRegistry, FloraBuild), и нарушение будет ТИХИМ: .dfo на полке не
# перепекаются сами, а игра читает их с диска — значит расхождение всплывёт
# только у того, кто в следующий раз запустит кузницу, через недели.
# Прибор ловит это в ctest, в тот же час.
#
# ЧТО ОН НЕ ЛОВИТ, ЧЕСТНО: он сверяет ХЭШ СОДЕРЖИМОГО, то есть геометрию и uv.
# Он НЕ видит атлас — тайл можно перерисовать, и хэши не дрогнут. Ряды 0..13
# сторожит отдельная рука в tests/render (LeafAtlasV2Tests): она проверяет,
# что новые ряды 14-15 не пусты, а старые ряды не изменились относительно
# зафиксированного отпечатка.
#
# Dependencies:
# - Uses: Python stdlib; бинарник dfn_forge.
# - Used by: ctest (цель trees_v1_frozen в корневом CMakeLists).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.

import os
import re
import subprocess
import sys
import tempfile

ROW = re.compile(r"^([\w.-]+) \| ([\w.-]+\.dfo) \| ([\w-]+) \| ([0-9a-f]{16}) \|")

def read_index(path):
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = ROW.match(line.strip())
            if m:
                out[m.group(1)] = (m.group(3), m.group(4))
    return out

def main():
    if len(sys.argv) < 2:
        print("usage: check_trees_frozen.py <dfn_forge>", file=sys.stderr)
        return 2
    forge = sys.argv[1]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    shipped_path = os.path.join(root, "assets", "objects", "trees", "INDEX.md")
    shipped = read_index(shipped_path)
    if not shipped:
        print(f"[frozen] {shipped_path}: ни одной строки рецепта — ОТКАЗ", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        out_dir = os.path.join(tmp, "trees")
        os.makedirs(out_dir, exist_ok=True)
        r = subprocess.run([forge, out_dir], cwd=root, capture_output=True, text=True)
        if r.returncode != 0:
            print("[frozen] кузница отказала:\n" + r.stderr, file=sys.stderr)
            return 1
        fresh = read_index(os.path.join(out_dir, "INDEX.md"))

    bad = []
    for name, (row, h) in sorted(shipped.items()):
        if name not in fresh:
            bad.append(f"{name}: рецепт исчез из кузницы")
        elif fresh[name] != (row, h):
            bad.append(f"{name}: полка {row}/{h} -> кузница {fresh[name][0]}/{fresh[name][1]}")
    for name in sorted(fresh):
        if name not in shipped:
            bad.append(f"{name}: кузница печёт рецепт, которого нет на полке")

    if bad:
        print("[frozen] ПЕРВАЯ ИТЕРАЦИЯ СДВИНУЛАСЬ — ОТКАЗ:", file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1
    print(f"[frozen] {len(shipped)} рецептов первой итерации: content_hash не сдвинулся")
    return 0

if __name__ == "__main__":
    sys.exit(main())
