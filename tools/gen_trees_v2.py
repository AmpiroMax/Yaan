#!/usr/bin/env python3
#
# Created: 28:08:2026 - 18:05:00
# Last updated: 29:08:2026 - 00:50:00
# File: tools/gen_trees_v2.py
#
# Responsibility:
# - Генератор СМОТРОВОЙ ПЛОЩАДКИ ВТОРОЙ ИТЕРАЦИИ ДЕРЕВЬЕВ: карта
#   assets/maps/trees/forest-v2.map, композиция assets/scenes/trees-v2.scene и
#   правка земли assets/scenes/trees-v2.relief (СКЛОН — деревья второй итерации
#   обязаны показываться на уклоне, разница №3 записки о наклоне ствола).
#
# Usage:
#     python3 tools/gen_trees_v2.py [<путь к dfn_scene_check>]
#
# ЧТО ЗДЕСЬ ЗАЧЕМ (композиция — это довод, а не расстановка по вкусу):
#   * ПАРЫ ДО/ПОСЛЕ на ровном лугу. Шесть пар «дерево первой итерации рядом с
#     деревом второй», по одной паре на вид. Одна пара — одно плечо и один
#     контроль в ОДНОМ кадре (правило 30): владелец видит разницу, не листая
#     два скриншота и не веря на слово подписи.
#   * ТРИ РОЩИЦЫ НА СКЛОНЕ — по одной на структурный закон (лесной дуб, бук,
#     многоствольная ольха). Роща нужна, потому что разница №2 («лесной рецепт
#     против одиночного») существует только во множественном числе: одно дерево
#     не смыкается в потолок.
#   * ВЕРХНЯЯ ТЕРРАСА — многоствольные одиночки против неба: крона шире высоты
#     читается только на фоне, а не на фоне соседей.
#
# ОТКУДА БЕРЁТСЯ y. Земля карты — это ГЕНЕРАТОР ПЛЮС ПРАВКА .relief, и обе
# половины знает судья dfn_scene_check с ключом --relief (ключ заведён этой же
# волной 28.08 — до неё судья не читал правку вовсе и на склоне докладывал
# 42 находки из 42, все ровно на глубину склона). Поэтому скрипт пишет склон в
# файл, а высоты спрашивает у судьи: один источник истины, а не два.
#
# Dependencies:
# - Uses: Python stdlib; бинарник dfn_scene_check; полка assets/objects/trees.
# - Used by: рука; карта trees/forest-v2.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ДЕТЕРМИНИРОВАН: один и тот же посев — тот же файл до байта.
#
# UPD:
# - 28:08:2026 - 18:05:00: Создан — смотровая площадка второй итерации.
# - 29:08:2026 - 00:50:00: ЯРУСЫ (волна по записке №2 ресёрчера, пункты 1/2/3/5).
#   Стенд перестал быть только галереей деревьев: поверх полога сеются куртины
#   подлеска, сплошной ковёр мха и травы с переключением по видимости неба,
#   редкие акценты группами и ярус подроста 1-3 м. Сам ЗАКОН посева живёт в
#   tools/flora_sow.py — здесь только палитры стенда, масштабы посадки и порядок
#   заходов; иначе у каждой карты завелась бы своя плотность куртины, и кадр
#   перестал бы быть доводом о законе.
#   Два КОНТРОЛЬНЫХ ПЛЕЧА ключами (правило 30): --no-tiers (сцена «до») и
#   --uniform (тот же счёт подлеска РОВНЫМ рассевом). Второе — единственный
#   способ доказать, что куртина есть закон, а не украшение.
#   БАЗА НАХОДОК СУДЬИ теперь считается ЯВНО: полог рощиц даёт 5 находок
#   [no-overlap] (сомкнутые кроны — правило «два ствола в одной яме», прочитанное
#   на роще), и ярусы обязаны не добавить НИ ОДНОЙ. Прежний код возвращал успех
#   по «0 находок», которого у этой карты не бывает.

import math
import os
import random
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dfo_read           # noqa: E402
import flora_sow          # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCENE = os.path.join(ROOT, "assets", "scenes", "trees-v2.scene")
RELIEF = os.path.join(ROOT, "assets", "scenes", "trees-v2.relief")
MAP = os.path.join(ROOT, "assets", "maps", "trees", "forest-v2.map")
SHELVES = [os.path.join(ROOT, "assets", "objects", "trees"),
           os.path.join(ROOT, "assets", "objects", "glade"),
           os.path.join(ROOT, "assets", "objects", "tiers")]
SPAN = 256          # м, одна плитка мира
STEP = 1            # шаг решётки .relief; обязан совпасть с RELIEF_STEP_M


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def delta(x, z):
    """СКЛОН. Плато-луг до x=40, подъём 19 м до x=200, верхняя терраса дальше.
    Максимальная крутизна 19*1.5/160 = 0.178, то есть 10.1 градуса — уклон, на
    котором наклон ствола обязан быть виден, и по которому ещё можно ходить."""
    s = smoothstep((x - 40.0) / 160.0)
    base = 19.0 * s
    # Волны, чтобы склон не был пандусом: две по склону, одна по лугу.
    wav = 1.6 * math.sin(z * 0.048) * s \
        + 0.9 * math.sin(x * 0.062) * s \
        + 0.7 * math.sin(z * 0.021 + 1.3)
    return base + wav


# --- КОМПОЗИЦИЯ ------------------------------------------------------------
# (объект, x, z, заметка). yaw и малый разброс — из посева.
PAIRS = [
    # v2 (ближе к зрителю)      v1 (контроль, за ним)
    ("oak-v2-luga-a", "oak-forge-b", "дуб: луговой рецепт v2 против v1"),
    ("oak-v2-luga-b", "oak-forge-c", "дуб: второй посев v2 против v1"),
    ("beech-v2-luga-a", "aspen-forge-a", "бук v2 (одиночный) против осины v1"),
    ("beech-v2-luga-b", "birch-forge-a", "бук v2 против берёзы v1"),
    ("acacia-v2-luga-a", "birch-forge-b", "акация v2 (3 ствола) против v1"),
    ("acacia-v2-luga-b", "oak-forge-a", "акация v2 (4 ствола) против v1"),
]

GROVES = [
    (["oak-v2-forest-a", "oak-v2-forest-b"], 118, 178, 22, 84,
     "рощица: лесной рецепт дуба"),
    (["beech-v2-forest-a", "beech-v2-forest-b"], 118, 178, 100, 162,
     "рощица: лесной ярусный бук"),
    (["alder-v2-forest-a", "alder-v2-forest-b"], 118, 178, 178, 240,
     "рощица: многоствольная ольха в лесу"),
]

TERRACE = ["acacia-v2-luga-b", "acacia-v2-luga-a", "beech-v2-luga-a"]


def build_places(rng):
    places = []
    for i, (v2, v1, note) in enumerate(PAIRS):
        z = 34.0 + i * 37.0
        places.append((v2, 52.0 + rng.uniform(-1.5, 1.5), z + rng.uniform(-2, 2),
                       rng.uniform(0, 6.283), "v2 — " + note))
        places.append((v1, 84.0 + rng.uniform(-1.5, 1.5), z + rng.uniform(-2, 2),
                       rng.uniform(0, 6.283), "v1 (контроль) — " + note))
    for names, x0, x1, z0, z1, note in GROVES:
        # Ряды в шахматном порядке: роща, а не сад.
        k = 0
        for row in range(3):
            for col in range(3):
                x = x0 + (x1 - x0) * (col + 0.5) / 3.0 + (row % 2) * 8.0
                z = z0 + (z1 - z0) * (row + 0.5) / 3.0
                x += rng.uniform(-5.0, 5.0)
                z += rng.uniform(-5.0, 5.0)
                places.append((names[k % len(names)], x, z,
                               rng.uniform(0, 6.283), note))
                k += 1
    for i, name in enumerate(TERRACE):
        places.append((name, 216.0 + rng.uniform(-4, 4), 60.0 + i * 70.0,
                       rng.uniform(0, 6.283), "верхняя терраса: крона против неба"))
    return places


# --- ЯРУСЫ (записка №2 ресёрчера, пункты 1.2/1.3/1.4/3.1) ------------------
# Ни одного нового имени в подлеске и акцентах: кузницы полянки (assets/objects/
# glade) уже испекли и папоротник, и кусты, и высокие злаки, и грибы. Новое —
# только то, чего не было ВОВСЕ: подрост и платы ковра (assets/objects/tiers).
UNDER_SHADE = ["glade-fern", "glade-fern", "glade-juniper-creep"]   # тень полога
UNDER_LIGHT = ["glade-fern", "glade-bush-a", "glade-berry-pink",
               "glade-berry-red", "glade-bush-b"]
UNDER_ANY = ["glade-fern", "glade-berry-pink", "glade-berry-dark"]
CARPET_MOSS = ["tier-moss-a", "tier-moss-b", "tier-moss-c", "tier-moss-d"]
CARPET_SWARD = ["tier-sward-a", "tier-sward-b", "tier-sward-c", "tier-sward-d"]
ACCENT_LIGHT = ["glade-grass-b", "glade-grass-c", "glade-flowers-yellow",
                "glade-flowers-white", "glade-flowers-pink"]
ACCENT_SHADE = ["glade-mushrooms-brown", "glade-mushrooms-red", "glade-grass-a"]
SAPLINGS = ["tier-sapling-oak-a", "tier-sapling-oak-b", "tier-sapling-beech-a",
            "tier-sapling-beech-b", "tier-sapling-alder-a", "tier-sapling-alder-b"]
# МАСШТАБ ПРИ ПОСАДКЕ. Кузница полянки пекла папоротник ростом 0.36 м — это
# не подлесок, а приземный ярус: на десяти шагах его нет вовсе. Ярус кустов
# начинается там, где растение ЗАКРЫВАЕТ ЗЕМЛЮ, то есть с полуметра. Масштаб —
# а не новая кузница — потому что форма папоротника верна, неверен только рост;
# и потому что судья множит габарит на scale, а сеятель — вместе с ним.
SCALE = {
    "glade-fern": 1.9,             # 0.36 -> 0.68 м
    "glade-mushrooms-brown": 1.7,  # акцент обязан читаться с тропы
    "glade-mushrooms-red": 1.7,
    "glade-juniper-creep": 0.8,    # стелющийся можжевельник был шире куртины
}

# Ковёр кладётся ТАМ, ГДЕ ЕСТЬ ЛЕС: приземный ярус — это лесная подстилка, а не
# газон на всю плитку мира. 20 м — половина пролёта между рощицами стенда.
CARPET_REACH_M = 20.0
TIER_SEED = 20260828


def path_field(checker):
    """Поле троп у СУДЬИ (--path-field): та же величина, которой он ловит
    [off-path]. Свой расчёт троп в генераторе был бы вторым источником."""
    r = subprocess.run([checker, os.path.relpath(SCENE, ROOT), "--path-field", "1.0"],
                       cwd=ROOT, capture_output=True, text=True)
    return flora_sow.PathField.parse(r.stdout)


def sow_tiers(places, checker, uniform=False):
    """Сеет ярусы поверх готовой расстановки деревьев. places — список
    (name, x, z, yaw, note, y) яруса полога, уже посаженный на землю."""
    objects = dfo_read.shelf(SHELVES)
    shelf = flora_sow.Shelf(objects, SCALE)
    trees = [(x, z, objects[n].radius * flora_sow.CANOPY_R_FRAC, objects[n].height)
             for (n, x, z, yaw, note, y) in places if n in objects]
    canopy = flora_sow.Canopy(trees)
    paths = path_field(checker)

    def in_forest(x, z):
        for (tx, tz, tr, th) in trees:
            if (x - tx) ** 2 + (z - tz) ** 2 < CARPET_REACH_M ** 2:
                return True
        return False

    saplings = flora_sow.sow_saplings(SPAN, TIER_SEED, canopy, paths, shelf,
                                      SAPLINGS, trees)
    # ПОДЛЕСОК ЖИВЁТ ПОД ПОЛОГОМ И НА ОПУШКЕ, а не по всей плитке мира:
    # заказ владельца — «подлесок под её рощицы», и это же есть §3.1.
    under = flora_sow.sow_undergrowth(SPAN, TIER_SEED, canopy, paths, shelf,
                                      {"shade": UNDER_SHADE, "light": UNDER_LIGHT,
                                       "any": UNDER_ANY}, uniform=uniform,
                                      region=in_forest)
    carpet = flora_sow.sow_carpet(SPAN, TIER_SEED, canopy, paths, shelf,
                                  CARPET_MOSS, CARPET_SWARD, region=in_forest)
    accents = flora_sow.sow_accents(SPAN, TIER_SEED, canopy, paths, shelf,
                                    ACCENT_LIGHT, ACCENT_SHADE)

    grown = [(n, x, z) for (n, x, z, yaw, note, y) in places]
    saplings, d1 = flora_sow.thin_solids(saplings, shelf, existing=grown)
    grown += [(n, x, z) for (n, x, z, yaw, note) in saplings]
    under, d2 = flora_sow.thin_solids(under, shelf, existing=grown)
    grown += [(n, x, z) for (n, x, z, yaw, note) in under]
    accents, d3 = flora_sow.thin_solids(accents, shelf, existing=grown)

    stats = {
        "подрост": len(saplings), "подлесок": len(under),
        "ковёр": len(carpet), "акценты": len(accents),
        "снято прореживанием": d1 + d2 + d3,
    }
    out = []
    for group in (saplings, under, carpet, accents):
        for (n, x, z, yaw, note) in group:
            out.append((n, x, z, yaw, note, 0.0))
    return out, stats, canopy, paths, objects


def write_relief():
    lines = [
        "# Daggerfall N relief — СКЛОН смотровой площадки второй итерации.",
        "# Сгенерирован tools/gen_trees_v2.py; правится в функции delta() там же.",
        "# Координаты — ИНДЕКСЫ мировой решётки: мир = индекс * step.",
        "step %d" % STEP,
    ]
    for z in range(0, SPAN + 1):
        for x in range(0, SPAN + 1):
            d = delta(x * STEP, z * STEP)
            if abs(d) < 0.005:
                continue
            lines.append("dh %d %d %.4f" % (x, z, d))
    with open(RELIEF, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return len(lines) - 4


def write_scene(places):
    out = [
        "# Daggerfall N scene — СМОТРОВАЯ ПЛОЩАДКА ВТОРОЙ ИТЕРАЦИИ ДЕРЕВЬЕВ.",
        "# СГЕНЕРИРОВАНО tools/gen_trees_v2.py — править композицию там, не здесь.",
        "# Пары «v1 рядом с v2» на лугу, три рощицы на склоне, многоствольные",
        "# одиночки на верхней террасе. Земля: генератор + trees-v2.relief.",
        "map = trees/forest-v2",
        "world_span_m = %d" % SPAN,
        "relief = trees-v2.relief",
        "spawn = 96 0 128",
        "spawn_yaw = 0",
        "",
    ]
    for name, x, z, yaw, note, y in places:
        out += [
            "[place]",
            "object = %s" % name,
            "pos = %.2f %.4f %.2f" % (x, y, z),
            "yaw = %.3f" % yaw,
            "scale = %.3f" % SCALE.get(name, 1.0),
            "note = %s" % note,
            "",
        ]
    with open(SCENE, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def read_scene_y():
    """Читает pos из .scene обратно — после --fix там генераторная земля."""
    out = []
    cur = {}
    with open(SCENE, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("[place]"):
                cur = {}
            elif line.startswith("object ="):
                cur["object"] = line.split("=", 1)[1].strip()
            elif line.startswith("pos ="):
                p = line.split("=", 1)[1].split()
                cur["pos"] = (float(p[0]), float(p[1]), float(p[2]))
            elif line.startswith("yaw ="):
                cur["yaw"] = float(line.split("=", 1)[1])
            elif line.startswith("note ="):
                cur["note"] = line.split("=", 1)[1].strip()
                out.append((cur["object"], cur["pos"][0], cur["pos"][2],
                            cur["yaw"], cur["note"], cur["pos"][1]))
    return out


def write_map():
    text = """name = Лес второй итерации
zone = flora
size_chunks = 1
# ИСТОЧНИК: смотровая площадка ВТОРОЙ ИТЕРАЦИИ ДЕРЕВЬЕВ (заказ владельца
# 28.08.2026 по записке docs/reports/trees-g3/index.html). Земля — генератор
# галереи ПЛЮС склон 10 градусов из assets/scenes/trees-v2.relief: наклон и
# изгиб ствола второй итерации существуют только на уклоне. Состав — сцена
# assets/scenes/trees-v2.scene: шесть пар «первая итерация рядом со второй»
# на ровном лугу, три рощицы на склоне (лесной дуб, бук, многоствольная
# ольха) и многоствольные одиночки на верхней террасе.
#
# ЯРУСЫ (волна 28.08 по записке docs/reports/trees-tiers/index.html): под
# рощицы посеяны КУРТИНЫ подлеска, сплошной ковёр мха и травы с переключением
# по видимости неба, редкие акценты группами и ярус ПОДРОСТА 1-3 м. Закон
# посева — tools/flora_sow.py; платы ковра и подрост — assets/objects/tiers.
# ТРИ ПОЛКИ, потому что композиция стоит на деревьях v2, кузницах полянки и
# новой полке ярусов сразу.
source = stand:Gallery
scene = assets/scenes/trees-v2.scene
objects = assets/objects/trees;assets/objects/glade;assets/objects/tiers
description = Деревья v2 и ЯРУСЫ: куртины подлеска, ковёр по свету, подрост.
built_commit =
"""
    os.makedirs(os.path.dirname(MAP), exist_ok=True)
    with open(MAP, "w", encoding="utf-8") as f:
        f.write(text)


def run_checker(checker, fix):
    # --relief: судья читает НАШ ЖЕ файл склона (ключ заведён 28.08 этой же
    # волной). Пока его не было, y приходилось считать в два шага — генератор
    # от судьи плюс дельта от скрипта, — то есть держать второй источник
    # истины о высоте земли. Теперь источник один: файл.
    args = [checker, os.path.relpath(SCENE, ROOT), "--relief",
            "--objects", ";".join(os.path.relpath(d, ROOT) for d in SHELVES)]
    if fix:
        args.append("--fix")
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True)


def findings(stdout):
    """Сколько находок насчитал судья в последней строке отчёта."""
    for line in reversed(stdout.strip().splitlines()):
        if "finding(s)" in line:
            for tok in line.split():
                if tok.isdigit():
                    n = int(tok)
            try:
                return int(line.split("placement(s),")[1].split()[0])
            except (IndexError, ValueError):
                return -1
    return -1


def offenders(stdout):
    """Индексы размещений, на которые судья ругается «стоит НА тропе».

    Тропы у стенда рисует генератор, а не мы: их шесть, и они проходят там, где
    им угодно. Дерево на тропе — законная находка судьи (тропа обязана быть
    проходимой), поэтому композиция не спорит с ним, а отступает."""
    out = []
    for line in stdout.splitlines():
        if line.startswith("[off-path] #"):
            try:
                out.append(int(line.split("#", 1)[1].split()[0]))
            except ValueError:
                pass
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    # ДВА КОНТРОЛЬНЫХ ПЛЕЧА (правило 30), и оба — ключами, а не правкой файла:
    #   --no-tiers      сцена ДО ярусов (полог как был);
    #   --uniform       тот же СЧЁТ подлеска, но РОВНЫМ рассевом. Это плечо и
    #                   есть довод, что куртина — закон, а не украшение.
    no_tiers = "--no-tiers" in flags
    uniform = "--uniform" in flags
    checker = args[0] if args else os.path.join(
        ROOT, "build_trees", "dfn_scene_check")
    rng = random.Random(20260828)
    places = [(n, x, z, y, note, 0.0) for (n, x, z, y, note) in build_places(rng)]
    samples = write_relief()
    write_map()

    # СХОДИМОСТЬ, А НЕ ОДИН ЗАХОД: судья сажает деревья на генераторную землю,
    # а потом называет тех, кто встал на тропу; такие отступают на шаг в
    # сторону, и заход повторяется. Восемь заходов — потолок, и если он не
    # сошёлся, скрипт говорит об этом вслух, а не молчит с находками.
    last = ""
    for attempt in range(8):
        write_scene(places)
        r = run_checker(checker, fix=True)
        if r.returncode not in (0, 1):
            sys.stderr.write(r.stderr)
            return 1
        last = r.stdout
        bad = offenders(r.stdout)
        if not bad:
            break
        for idx in bad:
            if 0 <= idx < len(places):
                n, x, z, yaw, note, y = places[idx]
                # Шаг в сторону детерминированный: 7 м по чередующейся оси,
                # чтобы два соседних отступа не сошлись в одну точку.
                dx = 7.0 if (idx + attempt) % 2 == 0 else -3.0
                dz = 7.0 if (idx + attempt) % 2 else -7.0
                places[idx] = (n, x + dx, z + dz, yaw, note, y)
    left = offenders(last)
    if left:
        sys.stdout.write(last[-1500:])
        print("[trees-v2] полог не сошёлся: %d деревьев на тропах" % len(left))
        return 1
    canopy_places = read_scene_y()
    # БАЗА НАХОДОК — ЭТО ПОЛОГ, А НЕ НОЛЬ. Рощица второй итерации смыкает
    # кроны, и судья считает сомкнутые кроны находкой [no-overlap]: это его
    # правило про «два ствола в одной яме», прочитанное на роще. Волна ярусов
    # обязана не ДОБАВИТЬ ни одной — вот с чем и сравнивается результат.
    base = findings(last)
    print("[trees-v2] полог: %d деревьев, %d отсчётов склона, находок судьи %d "
          "(сомкнутые кроны рощиц — база волны)"
          % (len(canopy_places), samples, base))

    if no_tiers:
        print("[trees-v2] ЯРУСЫ ОТКЛЮЧЕНЫ (--no-tiers): контрольное плечо «до»")
        return 0

    # --- ЯРУСЫ ПОВЕРХ ПОЛОГА ------------------------------------------------
    tiers, stats, canopy, paths, objects = sow_tiers(canopy_places, checker,
                                                     uniform=uniform)
    places = canopy_places + tiers
    write_scene(places)
    r = run_checker(checker, fix=True)
    if r.returncode not in (0, 1):
        sys.stderr.write(r.stderr)
        return 1
    tail = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
    grown = findings(r.stdout)
    print("[trees-v2] ярусы: " + ", ".join("%s %d" % (k, v)
                                           for k, v in stats.items()))
    print("[trees-v2] " + tail)
    if grown > base:
        print("[trees-v2] ЯРУСЫ ДОБАВИЛИ %d находок сверх базы %d — ОТКАЗ"
              % (grown - base, base))
        return 1
    print("[trees-v2] ярусы не добавили НИ ОДНОЙ находки сверх базы %d" % base)
    return 0


if __name__ == "__main__":
    sys.exit(main())
