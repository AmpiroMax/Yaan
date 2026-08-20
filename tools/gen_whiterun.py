#!/usr/bin/env python3
# Created: 21:08:2026 - 04:10:00
# Last updated: 21:08:2026 - 01:35:00
# Module: tools
# File: tools/gen_whiterun.py
#
# Responsibility:
# - ГОРОД ВАЙТРАН (заказ 21.08: «максимально приближенная копия») на рельефе
#   пользователя town-land (три террасы 25/31/43, река двумя рукавами, ров).
#   План — docs/WHITERUN_RESEARCH.md + карта GameMapScout: ворота с юга,
#   рынок и кузница на нижней террасе, Гилдергрин/храм/Йоррваскр на средней,
#   замок на верхней; стены с башнями по кромкам, снаружи фермы.
# - Рельеф НЕ трогается: pads/rivers скопированы из town-land.scene, ручная
#   лепка town-land.relief копируется файлом whiterun.relief.
#
# UPD:
# - 21:08:2026 - 04:10:00: Создан.
# - 21:08:2026 - 00:10:00: ЛЕСТНИЦЫ ЛОЖАТСЯ НА СКЛОНЫ, А НЕ НА ПЛАТО. Бот трижды
#   застревал в (118, 30.5, 124) — «невидимый блокер» оказался склоном рельефа:
#   подъёмы террас живут в town-land.relief пологими валами (T2: z120..130,
#   35 гр. в середине), а марши стояли целиком на плато. Профили промерены
#   честной пробой terrain_height (build_world_context на pads+relief сцены):
#   T2 -> (116,25,120); перешеек -> (164,32,64); замок -> (174,37,50).
#   smooth_keep_hump: ручной горб town-land (+3..5 м) на кромке замкового плато
#   гасится с каймой 5 м — он перекрывал единственный заход отвесом 45-70 гр.
#   Кольцо стен: восточная грань шла меридианом x166-168 ПО перешейку и резала
#   его лестницу; теперь обходит по кромке оврага x182-184, разрыв
#   (180,52)->(170,52) — проход под марш замка с башнями по флангам.
# - 21:08:2026 - 01:35:00: Просека лестницы T2 ([pad] 118/128 h25): марш стоит
#   над плоским дном — рисуемая земля не протыкает плиты (глаза, rec_00528).
#   Сам стоп в (118,·,123.93) оказался ЗАЖИМОМ КАПСУЛЫ НА КРОМКАХ ступеней —
#   починен рампой в HouseStairs.cpp; здесь только геометрия про склоны.

import math
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = []   # [house] rows
P = []   # [place] rows

def house(file, x, y, z, yaw_deg, note):
    H.append((f"assets/houses/{file}", x, y, z, math.radians(yaw_deg), note))

def place(obj, x, y, z, yaw_deg=0.0, note=None):
    P.append((obj, x, y, z, math.radians(yaw_deg), note))

# --- террасы (высоты пола) --------------------------------------------------
T1, T2, T3, NECK = 25.0, 31.0, 43.0, 37.0

def wall_ring(points, y_of):
    """ЗАМКНУТОЕ кольцо (приёмка №1: прогоны обрывались торцами в поле):
    сегменты 12 м от вершины к вершине, хвост каждой грани ПЕРЕКРЫВАЕТ угол,
    в каждой вершине — башня. y_of(x, z) -> высота полки под точкой."""
    import math as _m
    n = len(points)
    for k in range(n):
        ax, az = points[k]
        bx, bz = points[(k + 1) % n]
        dx, dz = bx - ax, bz - az
        L = _m.hypot(dx, dz)
        ux, uz = dx / L, dz / L
        yaw = _m.degrees(_m.atan2(-uz, ux)) % 360.0  # local +X -> (cos,-sin)
        t = 0.0
        while t < L - 0.5:
            seg = min(12.0, L - t)
            x, z = ax + ux * t, az + uz * t
            house("city-wall12.dfh", x, y_of(x, z), z, yaw,
                  "кольцо стены" if seg > 11.0 else "кольцо стены (замык. нахлёст)")
            t += min(seg, 11.5)  # лёгкий нахлёст: щелей на стыках нет
    for (x, z) in points:
        house("city-tower.dfh", x - 1.8, y_of(x, z), z - 1.8, 0, "угловая башня")

def walls():
    """ЕДИНОЕ кольцо по кромкам T1+T2 с воротами на юге; подпорные стены
    кромок террас (приёмка: «кромка — нашлёпка на склоне»)."""
    def y_of(x, z):
        # Восточный сектор: гребень перешейка (з<=66) на 37, ниже — равнина.
        if x > 168.0:
            return NECK if z <= 66.0 else T1
        if z < 120.0 and 68.0 <= x <= 168.0:
            return T2
        return T1
    # Кольцо по часовой (изнутри лицо наружу решает сам рецепт): юг с разрывом
    # под ворота делаем двумя вершинами по бокам проёма.
    # Восточная грань шла меридианом x166-168 и РЕЗАЛА перешеек вместе с его
    # лестницей (марш x164..168 z64..76 втыкался в сегменты кольца). Кольцо
    # обходит перешеек по кромке оврага x182-184; разрыв (180,52)->(170,52) —
    # проход под марш замка (x174..178), его фланкируют башни двух вершин.
    ring = [(70, 192), (112, 192),  # южная грань до ворот
            (136, 192), (176, 192),  # после ворот
            (176, 128), (184, 120), (182, 64), (180, 52),
            (170, 52), (72, 52), (68, 118), (56, 128), (56, 192)]
    # Сегменты между вершинами; ПРОПУСКАЕМ грань ворот (112,192)->(136,192)
    # и проход замковой лестницы (180,52)->(170,52).
    import math as _m
    n = len(ring)
    for k in range(n):
        ax, az = ring[k]
        bx, bz = ring[(k + 1) % n]
        if (ax, az) == (112, 192) and (bx, bz) == (136, 192):
            continue
        if (ax, az) == (180, 52) and (bx, bz) == (170, 52):
            continue
        dx, dz = bx - ax, bz - az
        L = _m.hypot(dx, dz)
        if L < 1.0:
            continue
        ux, uz = dx / L, dz / L
        yaw = _m.degrees(_m.atan2(-uz, ux)) % 360.0
        t = 0.0
        while t < L - 0.5:
            x, z = ax + ux * t, az + uz * t
            house("city-wall12.dfh", x, y_of(x, z), z, yaw, "кольцо стены")
            t += 11.5
    for (x, z) in ring:
        house("city-tower.dfh", x - 1.8, y_of(x, z), z - 1.8, 0, "башня кольца")
    # ВОРОТА с фланговыми башнями вплотную к косякам.
    house("city-gate.dfh", 112, T1, 192, 0, "главные ворота (проём 4 м в центре)")
    house("city-tower.dfh", 108.5, T1, 190.5, 0, "надвратная башня, запад")
    house("city-tower.dfh", 131.5, T1, 190.5, 0, "надвратная башня, восток")
    # ПОДПОРНАЯ КРОМКА T1->T2 (z~119): стена во всю грань, проход под лестницу.
    # Сегмент 12 м занимает [x, x+12]: последний старт 96, иначе конец
    # сегмента x=107..119 перекрывал проход лестницы (бот вис на зубцах).
    for x in range(74, 97, 11):
        house("city-wall12.dfh", x, T1, 119, 0, "подпор террасы, запад от лестницы")
    for x in range(126, 163, 11):
        house("city-wall12.dfh", x, T1, 119, 0, "подпор террасы, восток от лестницы")
    # ПОДПОР ПЛАТО ЗАМКА: южная кромка T3. Начало с x=178 — сегмент
    # x172..184 перекрывал створ лестницы замка (x174..178), как это уже
    # было с подпором T2; западный фланг прохода держит башня в cloud().
    for x in range(178, 218, 11):
        house("city-wall12.dfh", x, NECK, 49, 0, "подпор замкового плато")

def plains():
    """Нижняя терраса: ворота -> мост -> рынок; кузница и жильё."""
    # Главная улица: непрерывное мощение хребта + ДОРОГА наружу от ворот.
    for z in (208, 196):
        house("city-plaza12.dfh", 118, T1, z, 0, "дорога перед воротами")
    house("city-plaza12.dfh", 118, T1, 180, 0, "улица от ворот")
    house("city-plaza12.dfh", 112, T1, 126, 0, "улица к лестнице террасы")
    # Мост через городской рукав: ПОДНЯТ над зеркалом (вода ~23.5).
    house("city-bridge.dfh", 121, T1 + 0.1, 172, 120, "мост главной улицы (поперёк русла)")
    # Рынок севернее реки.
    house("city-plaza20.dfh", 104, T1, 138, 0, "рыночная площадь")
    house("city-well.dfh", 113, T1, 147, 0, "колодец рынка")
    house("city-stall.dfh", 108, T1, 142, 0, "прилавок, ряд севера")
    house("city-stall.dfh", 114, T1, 142, 0, "прилавок, ряд севера")
    house("city-stall.dfh", 108, T1, 152, 180, "прилавок, ряд юга")
    house("city-stall.dfh", 114, T1, 152, 180, "прилавок, ряд юга")
    # Лавки у рынка (двери к площади).
    house("city-shop.dfh", 92, T1, 128, 180, "лавка (Белетор)")
    house("frame-replica.dfh", 128, T1, 132, 90, "алхимик")
    # Таверна — П-образный двор к улице.
    house("u-house.dfh", 132, T1, 148, 0, "таверна «Гарцующая кобыла»")
    # Кузница у ворот (навес-лавка) и дом кузнеца.
    house("city-shop.dfh", 136, T1, 182, 90, "кузница у ворот")
    house("city-house-s.dfh", 148, T1, 178, 0, "дом кузнеца")
    # Жилые дома T1.
    house("city-house-s.dfh", 76, T1, 172, 90, "жилой дом")
    house("city-house-s.dfh", 84, T1, 184, 0, "жилой дом")
    house("log-replica.dfh", 66, T1, 150, 90, "жилой сруб")
    house("l-house.dfh", 150, T1, 160, 180, "Г-образный дом")
    # ФРОНТ главной улицы: дома плечом к плечу с двух сторон.
    house("city-house-s.dfh", 108, T1, 184, 90, "фронт улицы, запад")
    house("city-house-s.dfh", 108, T1, 177, 90, "фронт улицы, запад")
    house("city-house-s.dfh", 130, T1, 184, 270, "фронт улицы, восток")
    house("city-house-s.dfh", 130, T1, 177, 270, "фронт улицы, восток")
    house("city-house-s.dfh", 96, T1, 166, 90, "жилой дом у улицы")
    house("city-house-s.dfh", 134, T1, 160, 270, "жилой дом")
    house("city-house-s.dfh", 68, T1, 132, 180, "дом у западной стены")
    house("city-shop.dfh", 90, T1, 150, 90, "лавка у рынка")

def stairs_t1_t2():
    """ОДИН марш на полный подъём 25 -> 31 (стыки пар ловили бота в щель).

    Марш лежит НА склоне рельефа: подъём террасы живёт в town-land.relief
    полосой z120..130 (35 градусов в середине — капсула виснет). Прежний
    origin z110 клал марш целиком на плато, а бот карабкался по голому
    склону и застревал на (118, 30.5, 124). Профиль мерён terrain_height:
    z133=25.0, z121=30.78, z120=31.0 — марш z121..133 идёт НАД землёй всей
    длиной (+0.2..+1.6) и касается её точно обоими концами; origin z120
    топил последние три метра марша в склоне 34 гр., и бот застревал на
    (118, 30.9, 124) уже стоя на ступенях."""
    house("city-stairs6.dfh", 116, T1, 121, 0, "лестница средней террасы")

def wind():
    """Средняя терраса: Гилдергрин, храм, Йоррваскр, жильё."""
    house("city-plaza12.dfh", 110, T2, 82, 0, "площадь Гилдергрина")
    place("great-forge-oak", 116, T2, 88, 0, "Гилдергрин (розовая листва — заказ зоне флоры)")
    # Приподнятый восьмигранный бортик вокруг ствола, ЦЕНТРОМ на дереве.
    house("city-treering.dfh", 113.4, T2, 85.4, 0, "кольцо Гилдергрина")
    # Стилобат-плаза под храмом: колонны больше не в траве.
    house("city-plaza12.dfh", 82, T2, 62, 0, "двор храма")
    house("city-temple.dfh", 84, T2, 62, 0, "храм Кинарет")
    house("city-longhall.dfh", 128, T2, 60, 0, "Йоррваскр")
    house("city-plaza12.dfh", 112, T2, 100, 0, "улица средней террасы")
    house("stone-replica.dfh", 82, T2, 96, 0, "дом Серых Грив")
    # ФРОНТ УЛИЦЫ (приёмка №2: «плечом к плечу, щипцом на улицу»).
    house("city-house-s.dfh", 100, T2, 104, 90, "фронт улицы, запад")
    house("city-house-s.dfh", 100, T2, 97, 90, "фронт улицы, запад")
    house("city-house-s.dfh", 128, T2, 104, 270, "фронт улицы, восток")
    house("city-house-s.dfh", 128, T2, 97, 270, "фронт улицы, восток")
    house("city-house-s.dfh", 144, T2, 96, 270, "жилой дом")
    house("city-shop.dfh", 148, T2, 78, 270, "лавка")
    house("log-replica.dfh", 96, T2, 58, 0, "жилой сруб")
    house("city-house-s.dfh", 110, T2, 62, 180, "дом у северной стены")
    house("city-house-s.dfh", 118, T2, 62, 180, "дом у северной стены")

def smooth_keep_hump(path):
    """Гасит relief-дельты на кромке замкового плато (ядро x169..185 z40..64,
    кайма 5 м). Ручной горб town-land (+3..5 м) стоял ровно на единственном
    пологом заходе на плато и превращал кромку в отвес 45-70 градусов —
    промерено terrain_height. Тропы (path/pp) и mat не трогаются."""
    lines = open(path, encoding="utf-8").read().split("\n")
    assert any(l.strip() == "step 1" for l in lines), "relief step != 1"
    x0, x1, z0, z1, fade = 169.0, 185.0, 40.0, 64.0, 5.0
    out = []
    for line in lines:
        t = line.split()
        if len(t) == 4 and t[0] == "dh":
            wx, wz = float(t[1]), float(t[2])  # step 1: индекс == метр
            o = max(x0 - wx, wx - x1, z0 - wz, wz - z1, 0.0)
            if o < fade:
                d = float(t[3]) * (o / fade)
                if d == 0.0:
                    continue  # нулевую дельту стирает и set_delta
                line = "dh %s %s %.9g" % (t[1], t[2], d)
        out.append(line)
    open(path, "w", encoding="utf-8").write("\n".join(out))


def cloud():
    """Перешеек и замок."""
    # Обе лестницы промерены terrain_height по своим осям (x166 и x176):
    # перешеек — земля 31.6 у z76 и 38.0 у z64 (склон 45 гр. в z65..70),
    # марш z64..76 с базой 32 входит ступенькой 0.43 и сходит вровень.
    # Замок — горб relief на кромке гасится smooth_keep_hump; после
    # чистки кромка это пад: перешеек 37 -> плато 43, марш z50..62.
    house("city-stairs6.dfh", 164, 32.0, 64, 0, "лестница перешейка (32->38)")
    house("city-stairs6.dfh", 174, NECK, 50, 0, "лестница замка (37->43)")
    house("city-keep.dfh", 184, T3, 16, 0, "Драконий Предел")
    house("city-donjon.dfh", 176, T3, 17, 0, "донжон, запад")
    house("city-donjon.dfh", 204, T3, 17, 0, "донжон, восток")

def outskirts():
    """Фермы снаружи и зелень."""
    house("log-replica.dfh", 56, T1, 214, 0, "ферма у дороги")
    house("city-house-s.dfh", 88, T1, 216, 90, "домик у дороги")
    house("city-house-s.dfh", 216, 24.8, 214, 180, "хутор за рвом (восточнее русла)")
    # Деревья: НЕ на осях улиц, НЕ в постройках, ели — подальше от стен
    # (приёмка: берёза в портике, дуб за воротами, ели резали стену).
    trees = [
        ("birch-forge-a", 94, T1, 158), ("birch-forge-b", 142, T1, 170),
        ("aspen-forge-a", 156, T1, 146),
        ("birch-forge-a", 92, T2, 100), ("aspen-forge-a", 140, T2, 108),
        ("juniper-forge-a", 124, T2, 74), ("juniper-forge-a", 134, T2, 90),
        ("pine-forge-a", 20, T1, 222), ("pine-forge-b", 44, T1, 244),
        ("spruce-forge-a", 200, 23.6, 238), ("pine-forge-a", 222, 24.8, 200),
        ("oak-forge-a", 32, 25.0, 156), ("spruce-forge-b", 236, 25.0, 116),
    ]
    for obj, x, y, z in trees:
        place(obj, x, y, z, (hash(obj + str(x)) % 360))

def main():
    walls()
    plains()
    stairs_t1_t2()
    wind()
    cloud()
    outskirts()

    # Рельеф: копия ручной лепки пользователя (его файл не трогаем).
    src_rel = os.path.join(ROOT, "assets/scenes/town-land.relief")
    dst_rel = os.path.join(ROOT, "assets/scenes/whiterun.relief")
    if os.path.exists(src_rel):
        shutil.copyfile(src_rel, dst_rel)
        smooth_keep_hump(dst_rel)
        relief_line = "relief = whiterun.relief\n"
    else:
        relief_line = ""

    # Полки и реки — снятые с town-land.scene пользователя блоки.
    src_scene = open(os.path.join(ROOT, "assets/scenes/town-land.scene"),
                     encoding="utf-8").read()
    keep = []
    take = False
    for line in src_scene.split("\n"):
        if line.startswith("[river]") or line.startswith("[pad]"):
            take = True
        elif line.startswith("["):
            take = False
        if take:
            keep.append(line)
    terrain = "\n".join(keep).strip("\n")

    terrain += ("\n\n[pad]\ncenter = 118 116\nhalf_extents = 47 4\nblend = 2\n"
                "height = 31\n"
                "note = резкая кромка T2: подпорные стены стоят у настоящей ступени\n"
                "\n[pad]\ncenter = 118 128\nhalf_extents = 3 6\nblend = 1\n"
                "height = 25\n"
                "note = просека лестницы T2: марш стоит над плоским дном, а не в склоне\n"
                "\n[pad]\ncenter = 195 45\nhalf_extents = 24 5\nblend = 2\n"
                "height = 43\n"
                "note = резкая кромка замкового плато\n")
    out = ["# Daggerfall N scene — ВАЙТРАН (заказ 21.08: копия города на холме).",
           "# СГЕНЕРИРОВАНО tools/gen_whiterun.py — правки вносить в генератор.",
           "# Рельеф — ручная лепка пользователя (town-land), скопирован файлом.",
           "map = houses/whiterun",
           "world_span_m = 256",
           relief_line.strip(),
           "spawn = 124 0 214",
           "spawn_yaw = 0",
           "", terrain, ""]
    for obj, x, y, z, yaw, note in P:
        out += ["[place]", f"object = {obj}",
                f"pos = {x:.3f} {y:.3f} {z:.3f}", f"yaw = {yaw:.6f}", "scale = 1"]
        if note:
            out += [f"note = {note}"]
        out += [""]
    for file, x, y, z, yaw, note in H:
        out += ["[house]", f"file = {file}",
                f"pos = {x:.3f} {y:.3f} {z:.3f}", f"yaw = {yaw:.6f}",
                f"note = {note}", ""]
    open(os.path.join(ROOT, "assets/scenes/whiterun.scene"), "w",
         encoding="utf-8").write("\n".join(out))

    open(os.path.join(ROOT, "assets/maps/houses/whiterun.map"), "w",
         encoding="utf-8").write(
        "name = Вайтран: город на холме\n"
        "zone = houses\n"
        "size_chunks = 1\n"
        "# КОПИЯ ВАЙТРАНА (заказ 21.08) на рельефе town-land пользователя:\n"
        "# три террасы, ворота с юга, рынок у реки, Гилдергрин, храм,\n"
        "# Йоррваскр, замок на скале; стены с башнями, фермы снаружи.\n"
        "# СГЕНЕРИРОВАНО tools/gen_whiterun.py.\n"
        "source = stand:Gallery\n"
        "scene = assets/scenes/whiterun.scene\n"
        "objects = assets/objects/parts;assets/objects/signs;assets/objects/trees\n"
        "description = Город на трёх террасах: ворота, рынок, Гилдергрин, замок; "
        "стены со смотровыми башнями, мост через реку, фермы за стеной.\n"
        "built_commit =\n")
    print(f"whiterun: {len(H)} построек, {len(P)} расстановок")

if __name__ == "__main__":
    main()
