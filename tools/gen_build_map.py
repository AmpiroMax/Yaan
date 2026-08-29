#!/usr/bin/env python3
# Module: tools
# File: tools/gen_build_map.py
#
# Responsibility:
# - НОВАЯ КАРТА «СТРОЙКА» (заказ 20.08: «сделать новую карту — копию демки из
#   папки домов, и там новые домики рисовать»). Берёт assets/scenes/demo.scene
#   КАК ЕСТЬ (три дома из деталей остаются эталоном для сравнения глазами),
#   дописывает вторую полку и регистрирует пять готовых построек кузницы
#   (tools/forge_houses.cpp) секциями [house]; пишет build.scene и build.map.
#
# Notes:
# - Правки вносить СЮДА и перегенерировать: build.scene — артефакт.
# - Повторы стоят СЕВЕРНЕЕ оригиналов на тех же X: разница читается взглядом
#   с одной точки. Г — восточнее ряда, П — на своей южной полке.

import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    src = os.path.join(ROOT, "assets/scenes/demo.scene")
    with open(src, encoding="utf-8") as f:
        scene = f.read()

    scene = scene.replace(
        "# Daggerfall N scene — ДЕМКА ЭТАЖНОСТИ И АРХИТЕКТУРЫ (зона домов, работа 6).",
        "# Daggerfall N scene — СТРОЙКА: демка домов + повторы НОВОЙ механикой (20.08).\n"
        "# СГЕНЕРИРОВАНО tools/gen_build_map.py — правки вносить в генератор.",
        1)
    scene = scene.replace("map = houses/demo", "map = houses/build", 1)

    pad2 = """
[pad]
center = 128 106
half_extents = 60 16
blend = 10
height = 25.5
note = южная полка стройки: П-образный дом кузницы стоит на ней
"""

    # Готовые постройки кузницы. Повторы — севернее оригиналов на тех же X
    # (оригиналы на z 136, зазор 10 м); Г — восточнее ряда; П — южная полка.
    houses = [
        ("assets/houses/log-replica.dfh", 118.0, 25.5, 122.0,
         "повтор сруба новой механикой; оригинал из деталей южнее"),
        ("assets/houses/frame-replica.dfh", 130.0, 25.5, 122.0,
         "повтор фахверка с маршем на антресоль"),
        ("assets/houses/stone-replica.dfh", 147.0, 25.5, 122.0,
         "повтор двухэтажного камень+фахверк"),
        ("assets/houses/l-house.dfh", 162.0, 25.5, 114.0,
         "Г-образный одноэтажный: две комнаты, межкомнатная дверь"),
        ("assets/houses/u-house.dfh", 90.0, 25.5, 100.0,
         "П-образный двухэтажный: два марша, двор, крыша из четырёх навесов"),
    ]
    rows = [pad2]
    for file, x, y, z, note in houses:
        rows.append(f"""
[house]
file = {file}
pos = {x:.3f} {y:.3f} {z:.3f}
yaw = 0
note = {note}
""")
    scene = scene.rstrip("\n") + "\n" + "".join(rows)

    with open(os.path.join(ROOT, "assets/scenes/build.scene"), "w",
              encoding="utf-8") as f:
        f.write(scene)

    map_text = """name = Стройка: демка домов + повторы новой механикой
zone = houses
size_chunks = 1
# КОПИЯ ДЕМКИ ДОМОВ (заказ 20.08) плюс пять готовых построек кузницы
# (tools/forge_houses.cpp, секции [house] сцены): повторы трёх домов демки
# СЕВЕРНЕЕ оригиналов — сравниваются взглядом с одной точки, — Г-образный
# восточнее ряда и П-образный двухэтажный на своей южной полке.
# Сцена СГЕНЕРИРОВАНА tools/gen_build_map.py.
source = stand:Gallery
scene = assets/scenes/build.scene
objects = assets/objects/parts;assets/objects/signs
description = Три дома демки из деталей + их повторы графовой механикой, Г-образный и П-образный двухэтажный с двумя лестницами, двором и рабочими дверями.
built_commit =
"""
    with open(os.path.join(ROOT, "assets/maps/houses/build.map"), "w",
              encoding="utf-8") as f:
        f.write(map_text)
    print("build.scene + build.map written")

if __name__ == "__main__":
    main()
