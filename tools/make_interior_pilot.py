#!/usr/bin/env python3
"""
Created: 24:08:2026 - 00:20:00
Last updated: 27:08:2026 - 01:25:00
Module: tools
File: tools/make_interior_pilot.py

Responsibility:
- ПИЛОТ-ДОМ ЗОНЫ И15 (волна А), собранный ИЗ БОЕВОГО ВАЙТРАНА, но НЕ ТРОГАЯ
  его. Выпускает три файла:
    assets/scenes/int/whiterun/<slug>.scene  — локация (та же постройка +
        переехавшая в местные координаты раскладка FURN + очаг + [spawn] +
        обратный [portal]);
    assets/scenes/wr-int.scene               — КОПИЯ Вайтрана, у выбранного
        дома появились interior= и [portal];
    assets/maps/houses/wr-int.map            — паспорт карты пилота.
  Плюс assets/houses/city-house-s-portal.dfh — копия чертежа с portal=1 у
  створки (инверсия коллайдера двери).

Usage:
    python3 tools/make_interior_pilot.py            # из корня репозитория

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- БОЕВОЙ ВАЙТРАН НЕ ПРАВИТСЯ НИ ОДНИМ БАЙТОМ, и это ПРАВИЛО 47 в чистом
  виде: мир без порталов обязан остаться прежним, иначе всякое сравнение
  «до/после» этой волны меряет не её. Пилот живёт на КОПИИ, а копия
  выпускается скриптом, а не руками, — чтобы не протухнуть молча, когда
  конвейер перевыпустит город.
- ОБЩИЙ ЧЕРТЁЖ НЕ ПРАВИТСЯ ТОЖЕ. city-house-s.dfh носят больше сотни домов
  города; portal=1 в нём запечатал бы КАЖДЫЙ дверной проём Вайтрана. Поэтому
  у пилота свой файл, отличающийся ровно одним ключом.
- РАСКЛАДКА ПЕРЕЕЗЖАЕТ, А НЕ СОЧИНЯЕТСЯ ЗАНОВО. Мебель локации — та же
  мебель, что стоит в доме снаружи, пересчитанная в местные координаты
  обратным поворотом. Сочинённая заново раскладка была бы вторым ответом на
  вопрос «что стоит в этом доме» (правило 39).
"""
# UPD:
# - 24:08:2026 - 00:20:00: Создан (И15 волна А, шаг 5: пилот-дом).
# - 27:08:2026 - 01:25:00: ВЫПУСК ПИЛОТА СНЯТ ВОЛНОЙ Б, скрипт оставлен. Свод И15
#   называл переезд interior=/[portal] из копии в боевой Вайтран пунктом 6
#   волны Б, и он исполнен: у самого Вайтрана теперь 23 локации, а
#   assets/scenes/wr-int.scene и assets/scenes/int/whiterun/x100z84.scene из
#   дерева убраны (карта wr-int.map исчезла раньше, при переезде карт в
#   assets/maps/cities). Отрицательное плечо приёмки переехало на боевой
#   город. ЗАПУСКАТЬ ЭТОТ СКРИПТ БОЛЬШЕ НЕ НАДО: он выпустит копию, которая
#   разойдётся с городом при первом же его перевыпуске, и положит в
#   assets/scenes/int/whiterun файл, который стадия 14а генератора снесёт как
#   осиротевший. Оставлен как запись того, КАК был снят пилот (правило 24).

import math
import os
import re
import sys

CITY = "assets/scenes/whiterun.scene"
PILOT_SCENE = "assets/scenes/wr-int.scene"
PILOT_MAP = "assets/maps/houses/wr-int.map"
DFH_SRC = "assets/houses/city-house-s.dfh"
DFH_PORTAL = "assets/houses/city-house-s-portal.dfh"

# КАКОЙ ДОМ. Назван КООРДИНАТАМИ, а не номером записи: номер сдвигается при
# любой перегенерации города, координаты — нет, а если и сдвинутся, скрипт
# скажет вслух, что дома там нет.
TARGET_XZ = (99.983, 83.773)
TARGET_TOL_M = 0.5


def blocks(text):
    """Разбор .scene на (заголовок, [(ключ, значение), ...], сырой текст)."""
    out = []
    cur_name = ""
    cur_keys = []
    cur_lines = []
    for line in text.splitlines():
        t = line.strip()
        if t.startswith("[") and t.endswith("]"):
            out.append((cur_name, cur_keys, cur_lines))
            cur_name = t
            cur_keys = []
            cur_lines = [line]
            continue
        cur_lines.append(line)
        if "=" in t and not t.startswith("#"):
            k, v = t.split("=", 1)
            cur_keys.append((k.strip(), v.strip()))
    out.append((cur_name, cur_keys, cur_lines))
    return out


def kv(keys, name, default=None):
    for k, v in keys:
        if k == name:
            return v
    return default


def vec3(s):
    a = s.split()
    return (float(a[0]), float(a[1]), float(a[2]))


def to_local(world, origin, yaw):
    """Мировая точка -> местная. Обратный к w = pos + R(yaw)*l, где
    R: (x,y,z) -> (x*c + z*s, y, -x*s + z*c) — конвенция сцены."""
    c, s = math.cos(yaw), math.sin(yaw)
    d = (world[0] - origin[0], world[1] - origin[1], world[2] - origin[2])
    return (d[0] * c - d[2] * s, d[1], d[0] * s + d[2] * c)


def main():
    if not os.path.exists(CITY):
        sys.exit("нет %s — запускать из корня репозитория" % CITY)
    text = open(CITY, encoding="utf-8").read()
    parts = blocks(text)

    # 1. Найти дом-пилот и его убранство.
    target = None
    target_i = None
    for i, (name, keys, _) in enumerate(parts):
        if name != "[house]":
            continue
        p = kv(keys, "pos")
        if p is None:
            continue
        x, _, z = vec3(p)
        if (abs(x - TARGET_XZ[0]) < TARGET_TOL_M
                and abs(z - TARGET_XZ[1]) < TARGET_TOL_M
                and kv(keys, "file", "").endswith("city-house-s.dfh")):
            target = (name, keys)
            target_i = i
            break
    if target is None:
        sys.exit("дом-пилот на (%.1f, %.1f) НЕ НАЙДЕН — город перевыпущен? "
                 "поправь TARGET_XZ" % TARGET_XZ)

    origin = vec3(kv(target[1], "pos"))
    yaw = float(kv(target[1], "yaw", "0"))
    slug = "x%dz%d" % (round(origin[0]), round(origin[2]))

    # Убранство — записи «убранство: <заметка дома>» и «огонь очага: ...»,
    # идущие СРАЗУ за домом. Конвейер пишет их подряд, и это не догадка:
    # тем же порядком их читает приёмка мебели.
    furn = []
    for name, keys, _ in parts[target_i + 1:]:
        if name != "[house]":
            break
        note = kv(keys, "note", "")
        if not (note.startswith("убранство") or note.startswith("огонь очага")):
            break
        furn.append(keys)

    # Очаг-лампа: ближайший [light] с заметкой «очаг».
    lamp = None
    best = 1e9
    for name, keys, _ in parts:
        if name != "[light]":
            continue
        if "очаг" not in kv(keys, "note", ""):
            continue
        p = vec3(kv(keys, "pos"))
        d = (p[0] - origin[0]) ** 2 + (p[2] - origin[2]) ** 2
        if d < best:
            best = d
            lamp = keys
    if lamp is not None and best > 100.0:
        lamp = None

    # 2. Чертёж с portal=1 у створки.
    dfh = open(DFH_SRC, encoding="utf-8").read()
    out_lines = []
    sealed = 0
    for line in dfh.splitlines():
        if " door=1" in line and " portal=" not in line:
            line = line + " portal=1"
            sealed += 1
        out_lines.append(line)
    if sealed == 0:
        sys.exit("в %s нет створки (door=1) — запечатывать нечего" % DFH_SRC)
    open(DFH_PORTAL, "w", encoding="utf-8").write("\n".join(out_lines) + "\n")

    # 3. Сцена локации.
    int_dir = "assets/scenes/int/whiterun"
    os.makedirs(int_dir, exist_ok=True)
    int_path = "%s/%s.scene" % (int_dir, slug)
    # Дверь дома: створка e14 лежит на стене z=6 в полосе x 1.75..2.75 —
    # это МЕСТНЫЕ координаты чертежа, и именно они делают точку входа
    # свойством ДОМА, а не города.
    door_x, door_z = 2.25, 6.0
    L = []
    L.append("# Daggerfall N — ЛОКАЦИЯ (И15 волна А, пилот).")
    L.append("# СГЕНЕРИРОВАНО tools/make_interior_pilot.py из %s." % CITY)
    L.append("# Координаты СВОИ от нуля; пол y=0. Наружная оболочка задаёт")
    L.append("# внутренность: это ТОТ ЖЕ .dfh, поэтому интерьер не больше дома.")
    L.append("map = int/whiterun/%s" % slug)
    L.append("world_span_m = 64")
    L.append("")
    L.append("[spawn]")
    L.append("name = door")
    L.append("pos = %.3f 0.200 %.3f" % (door_x, door_z - 0.80))
    L.append("yaw = 0")
    L.append("note = сразу за порогом, лицом в комнату")
    L.append("")
    L.append("[house]")
    L.append("file = %s" % DFH_PORTAL)
    L.append("pos = 0 0 0")
    L.append("yaw = 0")
    L.append("note = оболочка: тот же чертёж, что снаружи")
    for keys in furn:
        p = to_local(vec3(kv(keys, "pos")), origin, yaw)
        fy = float(kv(keys, "yaw", "0")) - yaw
        L.append("")
        L.append("[house]")
        L.append("file = %s" % kv(keys, "file"))
        L.append("pos = %.3f %.3f %.3f" % p)
        L.append("yaw = %.6f" % fy)
        L.append("note = %s (местные координаты)" % kv(keys, "note", ""))
    if lamp is not None:
        p = to_local(vec3(kv(lamp, "pos")), origin, yaw)
        L.append("")
        L.append("[light]")
        L.append("pos = %.3f %.3f %.3f" % p)
        L.append("color = %s" % kv(lamp, "color", "1 0.85 0.55"))
        L.append("radius_m = %s" % kv(lamp, "radius_m", "7"))
        L.append("softness = %s" % kv(lamp, "softness", "0.25"))
        L.append("flicker = %s" % kv(lamp, "flicker", "0.5"))
        L.append("note = очаг локации")
    L.append("")
    L.append("[air]")
    L.append("fog_start = 40")
    L.append("fog_end = 120")
    # ОБЩИЙ СВЕТ КОМНАТЫ. Не ноль: комната без окон и без ambient — это
    # чёрный кадр, на котором нельзя отличить «свет не работает» от
    # «локация не загрузилась».
    L.append("ambient = 0.10")
    L.append("")
    L.append("[portal]")
    L.append("at = %.3f 1.000 %.3f" % (door_x, door_z - 0.30))
    L.append("radius_m = 1.1")
    L.append("to = ^back")
    L.append("note = обратная дверь")
    open(int_path, "w", encoding="utf-8").write("\n".join(L) + "\n")

    # 4. Копия города с interior= и порталом у той же двери.
    door_world = (
        origin[0] + door_x * math.cos(yaw) + door_z * math.sin(yaw),
        origin[1] + 1.0,
        origin[2] - door_x * math.sin(yaw) + door_z * math.cos(yaw),
    )
    city_lines = text.splitlines()
    # Найти строку `file = ...city-house-s.dfh` у выбранного дома по её pos.
    out = []
    i = 0
    patched = False
    while i < len(city_lines):
        line = city_lines[i]
        out.append(line)
        if (not patched and line.strip() == "[house]"
                and i + 3 < len(city_lines)
                and "city-house-s.dfh" in city_lines[i + 1]
                and re.match(r"\s*pos = ", city_lines[i + 2])):
            x, _, z = vec3(city_lines[i + 2].split("=", 1)[1].strip())
            if (abs(x - origin[0]) < 1e-3 and abs(z - origin[2]) < 1e-3):
                out.append(city_lines[i + 1])
                out.append(city_lines[i + 2])
                out.append(city_lines[i + 3])  # yaw
                out.append("interior = %s" % int_path)
                i += 4
                patched = True
                continue
        i += 1
    if not patched:
        sys.exit("не нашёл, куда вписать interior= — формат города изменился")
    # Дом-пилот носит СВОЙ чертёж (створка в коллайдере). Заменяется ровно
    # одна строка — та, что сразу после найденного [house].
    for j, line in enumerate(out):
        if line.strip() == "interior = %s" % int_path:
            for k in range(j - 4, j):
                if "city-house-s.dfh" in out[k]:
                    out[k] = "file = %s" % DFH_PORTAL
            break
    out.append("")
    out.append("[portal]")
    out.append("at = %.3f %.3f %.3f" % door_world)
    out.append("radius_m = 1.2")
    out.append("to = %s" % int_path)
    out.append("to_spawn = door")
    out.append("note = пилот И15: дверь дома у рынка")
    open(PILOT_SCENE, "w", encoding="utf-8").write("\n".join(out) + "\n")

    # 5. Паспорт карты.
    src_map = open("assets/maps/houses/whiterun.map", encoding="utf-8").read()
    m = src_map.replace("scene = assets/scenes/whiterun.scene",
                        "scene = %s" % PILOT_SCENE)
    m = m.replace("name = Вайтран: город на холме",
                  "name = Вайтран: пилот интерьера (И15)")
    m = m.replace("description = ", "description = ПИЛОТ И15 (волна А): копия "
                  "боевого Вайтрана, у дома у рынка есть внутренность. ")
    open(PILOT_MAP, "w", encoding="utf-8").write(m)

    print("дом-пилот:      (%.3f, %.3f) yaw %.6f, слаг %s"
          % (origin[0], origin[2], yaw, slug))
    print("створок запечатано в чертеже: %d -> %s" % (sealed, DFH_PORTAL))
    print("локация:        %s (%d предметов убранства, очаг %s)"
          % (int_path, len(furn), "есть" if lamp is not None else "НЕТ"))
    print("копия города:   %s" % PILOT_SCENE)
    print("паспорт карты:  %s" % PILOT_MAP)
    print("дверь в мире:   %.3f %.3f %.3f" % door_world)


if __name__ == "__main__":
    main()
