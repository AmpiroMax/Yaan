#!/usr/bin/env python3
#
# File: tools/make_human_body.py
#
# Responsibility:
# - ВЫРАЩИВАЕТ ВИДИМОГО ЧЕЛОВЕКА В BLENDER И ОТДАЁТ ЕГО ОДНИМ .glb: тело
#   MPFB2/MakeHuman (анатомическое, не слепленное руками), скелет с ТОЙ ЖЕ
#   иерархией и теми же именами, что у нашего прежнего ассета, и все 46 клипов
#   UAL, перенесённых на этот скелет.
#
# Usage (headless, один вызов = то же тело до байта; умолчания = базовое тело
# игрока, одобренное владельцем 02.09, — запуск БЕЗ ключей перепекает его):
#     /Applications/Blender.app/Contents/MacOS/Blender --background \
#         --python tools/make_human_body.py -- \
#         [--out   assets/objects/characters/HumanBase.glb] \
#         [--clips assets/objects/characters/UAL_Clips.glb] \
#         [--tris  0]
#   Ключи телосложения: --gender --age --muscle --weight --proportions
#   --stature и семь именованных: --shoulders --deltoid --neck --buttocks
#   --belly --flat --tuck --trunk. Без --clips выходит голое тело (быстрый
#   круг для подбора по судье dfn_human_scale); с --only <клипы через запятую>
#   переносится только названное (40 секунд вместо четверти часа — этим
#   кругом и мерилось проскальзывание опорной стопы).
#
# ПОЧЕМУ СКЕЛЕТ — КОПИЯ ЧУЖОГО, А НЕ СВОЙ. Клип в .dfo адресует сустав
# ИНДЕКСОМ (skel::AnimChannel::joint), а роль клипа и точки контакта стопы
# движок ищет по ИМЕНИ ("DEF-toe.L" — дословно в tests/character). Скелет,
# совпадающий с прежним по именам, порядку и родителям, — это единственная
# форма, при которой смена тела не является одновременно сменой ретаргета:
# всё, что мерили другие волны (цикл шага, фаза постановки стопы, хитбоксы,
# слой стойки), продолжает мерить ТО ЖЕ САМОЕ. Меняется ровно мясо, а претензия
# владельца 01.09 была ровно про мясо.
#
# ПОЧЕМУ ТЕЛО ВСЁ-ТАКИ НЕ САДИТСЯ НА ЧУЖОЙ СКЕЛЕТ КАК ЕСТЬ. Прежний ассет
# связан в T-позе (руки строго по ±X), тело MakeHuman — в A-позе (руки под 48°
# вниз). Посадить A-позное мясо на T-позный бинд можно только развернув его
# линейным блендингом на 48° в плече — это ровно тот «комок в плече», за
# который отклонён --reshape. Поэтому бинд остаётся СВОЙ (A-поза, родная для
# этого меша), а совпадение с прежним достигается на другом конце: клипы
# пересчитаны на этот бинд.
#
# ЧТО ДЕЛАЕТ ПЕРЕНОС КЛИПОВ, В ОДНОЙ СТРОКЕ: для каждого кадра каждая кость
# нового скелета получает МИРОВОЙ поворот одноимённой кости старого, а её
# голова считается прямой кинематикой по СОБСТВЕННЫМ длинам сегментов. Таз
# (DEF-hips) и корень получают ещё и смещение — как разницу от своей же
# рест-позы, а не как чужую абсолютную высоту. Формула базиса Blender
# (matrix_basis) проверяется в конце прогона сравнением с тем, что посчитал
# сам Blender: расхождение печатается, и это контроль, а не обещание.
#
# И ОДНОГО ПОВОРОТА НЕ ХВАТАЛО — ЭТО БЫЛО ЗАМЕРЕНО, И ЭТО БОЛЬШЕ НЕ ЧИНИТСЯ
# ЗДЕСЬ. Опорная стопа стоит на земле, пока повороты бедра, голени и стопы
# ТОЧНО гасят друг друга, а гашение зависит от ДЛИН звеньев; прежняя версия
# подгоняла бедро и голень к длинам донора (три опыта, сошёлся третий) ценой
# колена, гнущегося не там, где оно в мясе, и лодыжки в стопе. Снято 02.09:
# схождение шага с перемещением — задача движка (перемещение ведёт опорная
# стопа, docs/design/LOCOMOTION_GROUNDED.md), а скелет стоит там, где мясо.
#
# Dependencies:
# - Uses: bpy (Blender 5.2 LTS; 4.2 тоже читается — имя пакета MPFB ищется),
#   аддон rigify, расширение MPFB 2.0.17 (bl_ext.blender_org.mpfb или
#   bl_ext.user_default.mpfb), assets/objects/characters/UAL_Clips.glb.
# - Used by: рука; выпечка .dfo делает dfn_import_gltf (CMakeLists.txt).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ДЕТЕРМИНИРОВАН: те же ключи — тот же .glb. Ни одного случайного числа.

import math
import os
import sys

import bpy  # type: ignore
import bmesh  # type: ignore
from mathutils import Matrix, Vector  # type: ignore

# ---------------------------------------------------------------- аргументы --

DEFAULTS = {
    # ТЕЛО, ОДОБРЕННОЕ ВЛАДЕЛЬЦЕМ 02.09 («он прям супер выглядит как надо»):
    # чистые макро-ручки MPFB, правочные цели в нуле — те цели были ответом на
    # претензии к ДРУГОМУ телу (рубеж 2b) и меняют одобренный облик.
    "out": "assets/objects/characters/HumanBase.glb",
    "clips": "assets/objects/characters/UAL_Clips.glb",
    # Отбор клипов ДЛЯ БЫСТРОГО КРУГА: перенос всех сорока шести и экспорт
    # занимают четверть часа, а мерить проскальзывание опорной стопы надо на
    # трёх. Пустая строка — все.
    "only": "",
    # ВТОРОЙ ДОНОР АНИМАЦИЙ (владелец 02.09-2: «анимации найти в интернете и
    # переиспользовать, не изобретать»): glb с ЧУЖИМ ригом, кости которого
    # сопоставлены нашим картой имён (--donor2-map: kaykit). Его клипы
    # переносятся мировой ориентацией с дельтой покоя и выходят под
    # приставкой --donor2-prefix, роли им раздаёт дверь DFN_CLIP_ROLES.
    "donor2": "",
    "donor2-map": "kaykit",
    "donor2-only": "",
    "donor2-prefix": "KK_",
    # КОЖА (владелец 02.09-2: «нормальные текстуры, кожа»; решение лида: в
    # .dfo — ссылки, PNG внешними файлами): имя набора кожи MPFB
    # (data/skins/<name>/<name>.mhmat, CC0). Альбедо копируется в
    # textures/<stem>/albedo.png рядом с выходом, glb ссылается на него
    # ОТНОСИТЕЛЬНЫМ путём (не встраивает). Пусто — материал без текстуры.
    "skin": "",
    # ЧАСТИ ТЕЛА ОТДЕЛЬНЫМИ МЕШАМИ НА ТОМ ЖЕ СКЕЛЕТЕ (заказ 02.09-2: глаза,
    # брови, ресницы, зубы, язык, волосы): "тип=имя,..." из
    # data/<тип>/<имя>/<имя>.mhclo MPFB. Сажаются на тело ДО снятия хелперов
    # (mhclo адресует вершины базового меша hm08), веса — переносом с тела,
    # выход — <stem>.parts.glb (риг + части, без клипов) и textures/<stem>/
    # <часть>_albedo.png (+ _normal.png), внешними файлами.
    "parts": "",
    # ОДЕЖДА (CLOTHING_AND_CLOTH.md, первая волна): "имя,имя" из data/clothes/<имя>/
    # <имя>.mhclo MPFB, той же схемой, что части, но в <stem>.clothes.glb; у
    # каждой вещи в extras меша hide_body_vertices — индексы вершин тела ПО
    # ПОРЯДКУ HumanBase.glb, которые под ней не рисуются (delete_verts mhclo,
    # переведённые через служебный атрибут _HM08_INDEX).
    "clothes": "",
    "tris": "0",          # без децимации: 26 756 треугольников, пальцы целы
    # МУЖЧИНА ~30 ЛЕТ, ОБЫЧНОГО СЛОЖЕНИЯ. Шкала возраста MakeHuman линейна по
    # половинам и ставит 25 лет ровно в 0.5, 90 — в 1.0; 30 лет = 0.5385.
    "gender": "1.0",
    "age": "0.6",
    "muscle": "0.7",
    "weight": "0.55",
    "proportions": "0.7",
    "stature": "0.62",
    # ФИДБЕК ВЛАДЕЛЬЦА 01.09 ПО РУБЕЖУ 2 («уф, он очень страшный») — ПЯТЬ
    # ИМЕНОВАННЫХ РУЧЕК, ПО ОДНОЙ НА ПУНКТ. Ни одна из них не крутится «на
    # глаз»: каждая — ИМЕНОВАННАЯ цель MakeHuman, то есть та же величина,
    # которую называет анатомический обмер, и её ход виден судье пропорций.
    #
    # ПОЧЕМУ НЕ ХВАТАЕТ МАКРО-ВЕСА И МАКРО-МЫШЦЫ. Они раздают форму по всей
    # фигуре разом: замерено, `weight` с 0.58 до 0.70 двигает глубину
    # туловища на уровне пупка с 0.118H всего до 0.121H, зато заодно
    # раскармливает бёдра и плечи. Владелец назвал ПЯТЬ РАЗНЫХ МЕСТ, и
    # каждому нужна своя ручка, иначе правка одного портит другое.
    "shoulders": "0.0",  # 1. «плечи ОГРОМНЫЕ» — межакромиальная ширина
    "deltoid": "0.0",    # 1. ...и масса самого плечевого пояса
    "neck": "0.0",       # 2. «голова сидит НИЗКО» — высота шеи
    "buttocks": "0.0",   # 3. «зад ВЫПИРАЕТ» — объём ягодиц
    "belly": "0.0",       # 4. «живот УЖАСНЫЙ, сделать ПЛОСКИЙ» — вынос живота
    "flat": "0.0",       # 4. ...подтянутость брюшной стенки
    "tuck": "0.0",       # 4. ...и втянутость самого живота
    # Глубина туловища. `BODY_TORSO_DEPTH_FRAC` 0.14 — это, по собственному
    # примечанию реестра, живая глубина груди 0.13H ПЛЮС 8 % «под коренастый
    # даггерфолловский вид»: стилизация, которую анатомическое тело само по
    # себе не даёт и обязано получить ручкой.
    "trunk": "0.0",
}

# Какие именованные цели MakeHuman ставит каждая ручка. Ключ может вести
# больше чем к одной цели: «плечевой пояс» у MakeHuman левый и правый по
# отдельности, а фигура обязана остаться симметричной.
DETAIL_TARGETS = {
    "shoulders": ("measure-shoulder-dist-decr",),
    "deltoid": ("l-upperarm-shoulder-muscle-decr", "r-upperarm-shoulder-muscle-decr"),
    "neck": ("measure-neck-height-incr",),
    "buttocks": ("buttocks-volume-decr",),
    "belly": ("stomach-pregnant-incr",),
    "flat": ("stomach-tone-incr",),
    "tuck": ("stomach-pregnant-decr",),
    "trunk": ("torso-scale-depth-incr",),
}


def _find_mpfb():
    """Имя пакета MPFB зависит от того, откуда он поставлен: extensions.blender.org
    (Blender 5.2, `blender_org`) или из файла (`user_default`)."""
    import importlib.util
    for name in ("bl_ext.blender_org.mpfb", "bl_ext.user_default.mpfb"):
        try:
            if importlib.util.find_spec(name) is not None:
                return name
        except (ImportError, ValueError):
            continue
    raise SystemExit("MPFB extension not found (bl_ext.blender_org.mpfb / "
                     "bl_ext.user_default.mpfb)")


MPFB_PKG = _find_mpfb()


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opt = dict(DEFAULTS)
    i = 0
    while i < len(argv):
        key = argv[i].lstrip("-")
        if key not in opt:
            raise SystemExit("unknown option: " + argv[i])
        opt[key] = argv[i + 1]
        i += 2
    return opt


# ------------------------------------------------- скелет: спецификация ------
#
# Ровно 53 сустава прежнего ассета (палитра держит 64). Третий столбец — кость
# метарига MPFB, ОТКУДА берётся геометрия; четвёртый, где отличается, — чья
# нужна ХВОСТОВАЯ точка (у метарига грудной отдел на одну кость длиннее, и
# DEF-spine.003 накрывает обе).

SPINE = [
    ("root", None, None, None),
    ("DEF-hips", "root", "spine", "spine"),
    ("DEF-spine.001", "DEF-hips", "spine.001", "spine.001"),
    ("DEF-spine.002", "DEF-spine.001", "spine.002", "spine.002"),
    ("DEF-spine.003", "DEF-spine.002", "spine.003", "spine.004"),
    ("DEF-neck", "DEF-spine.003", "spine.005", "spine.005"),
    ("DEF-head", "DEF-neck", "spine.006", "spine.006"),
]

LIMB = [
    ("DEF-shoulder.%s", "DEF-spine.003", "shoulder.%s"),
    ("DEF-upper_arm.%s", "DEF-shoulder.%s", "upper_arm.%s"),
    ("DEF-forearm.%s", "DEF-upper_arm.%s", "forearm.%s"),
    ("DEF-hand.%s", "DEF-forearm.%s", "hand.%s"),
    ("DEF-thigh.%s", "DEF-hips", "thigh.%s"),
    ("DEF-shin.%s", "DEF-thigh.%s", "shin.%s"),
    ("DEF-foot.%s", "DEF-shin.%s", "foot.%s"),
    ("DEF-toe.%s", "DEF-foot.%s", "toe.%s"),
]

FINGERS = ["f_index", "f_middle", "f_pinky", "f_ring", "thumb"]


def build_spec():
    """(имя, родитель, кость-голова, кость-хвост) в порядке родитель-раньше."""
    spec = list(SPINE)
    for side in ("L", "R"):
        for name, parent, src in LIMB:
            spec.append((name % side, parent % side if "%s" in parent else parent,
                         src % side, src % side))
        for f in FINGERS:
            for n in (1, 2, 3):
                parent = ("DEF-hand.%s" % side) if n == 1 \
                    else ("DEF-%s.0%d.%s" % (f, n - 1, side))
                spec.append(("DEF-%s.0%d.%s" % (f, n, side), parent,
                             "%s.0%d.%s" % (f, n, side), "%s.0%d.%s" % (f, n, side)))
    return spec


# Куда лить веса групп, которых в 53 суставах нет. Явная строка стоит там, где
# геометрия соврала бы: грудь метарига ближе к плечу, чем к грудному отделу, а
# лицевые кости — к макушке, а не к шее.
WEIGHT_MAP = {
    "spine": "DEF-hips", "spine.001": "DEF-spine.001", "spine.002": "DEF-spine.002",
    "spine.003": "DEF-spine.003", "spine.004": "DEF-spine.003",
    "spine.005": "DEF-neck", "spine.006": "DEF-head",
    "pelvis.L": "DEF-hips", "pelvis.R": "DEF-hips",
    "breast.L": "DEF-spine.003", "breast.R": "DEF-spine.003",
    "heel.02.L": "DEF-foot.L", "heel.02.R": "DEF-foot.R",
}
for _s in ("L", "R"):
    for _n in (1, 2, 3, 4):
        WEIGHT_MAP["palm.0%d.%s" % (_n, _s)] = "DEF-hand.%s" % _s

# ДВА ИМЕНИ, КОТОРЫХ НЕТ НИ СРЕДИ КОСТЕЙ МЕТАРИГА, НИ СРЕДИ НАШИХ 53. Файл
# весов MakeHuman ссылается на скручивающие звенья Rigify
# ("DEF-forearm.L.001") и на глазной управляющий ("DEF-eye_master.L") —
# кости, которых в собранном метариге нет вовсе. Геометрический поиск их не
# спасает: у группы без кости нет и отрезка, — и 450 вершин, оба глазных
# яблока и манжеты предплечий, уходили в файл БЕЗ ЕДИНОГО ВЕСА, то есть в
# начало координат на первом же кадре.
WEIGHT_MAP.update({"DEF-eye_master.L": "DEF-head", "DEF-eye_master.R": "DEF-head"})
for _s in ("L", "R"):
    for _b in ("upper_arm", "forearm", "thigh", "shin"):
        WEIGHT_MAP["DEF-%s.%s.001" % (_b, _s)] = "DEF-%s.%s" % (_b, _s)

# Группы вершин, которые НЕ кость: маски и служебная геометрия MakeHuman.
NOT_A_BONE = {"body", "HelperGeometry", "JointCubes"}

# Внутренняя геометрия рта. Её не видит ни один кадр, а в бюджете силуэта она
# стоит ровно столько же, сколько силуэт.
HIDDEN_INSIDE = ("teeth.", "tongue")


# ----------------------------------------------------------------- утилиты --

def log(*a):
    print("[body]", *a)
    sys.stdout.flush()


def enable_addons():
    for mod in ("rigify", MPFB_PKG):
        try:
            bpy.ops.preferences.addon_enable(module=mod)
        except Exception as exc:  # noqa: BLE001
            raise SystemExit("cannot enable %s: %s" % (mod, exc))


def wipe():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def activate(ob):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def tri_count(ob):
    dep = bpy.context.evaluated_depsgraph_get()
    mesh = ob.evaluated_get(dep).to_mesh()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    n = len(bm.faces)
    bm.free()
    return n


def seg_dist(p, a, b):
    ab = b - a
    l2 = ab.dot(ab)
    t = 0.0 if l2 < 1e-9 else max(0.0, min(1.0, (p - a).dot(ab) / l2))
    return (p - (a + ab * t)).length


# --------------------------------------------------------------- 1. тело ----

def make_body(opt):
    import importlib
    pkg = MPFB_PKG
    human = importlib.import_module(pkg + ".services.humanservice").HumanService
    macro = {
        "gender": float(opt["gender"]), "age": float(opt["age"]),
        "muscle": float(opt["muscle"]), "weight": float(opt["weight"]),
        "proportions": float(opt["proportions"]), "height": float(opt["stature"]),
        "cupsize": 0.5, "firmness": 0.5,
        "race": {"asian": 0.0, "caucasian": 1.0, "african": 0.0},
    }
    # detailed_helpers=True ОБЯЗАТЕЛЬНО (находка 02.09): без групп joint-* MPFB
    # сажает риг по НЕЙТРАЛЬНОМУ телу 1,67 м, а меш после макро-ручек — 1,91 м;
    # скелет неутрального роста внутри рослого мяса давал «сбитые пропорции»
    # (плечи −32 %, суставы ниже на 14 %), кисти в 10 см от своих вершин и
    # пальцы-иглы на любом клипе. Helper-геометрия всё равно срезается маской
    # ниже, в игру она не едет.
    mesh = human.create_human(mask_helpers=True, detailed_helpers=True,
                              extra_vertex_groups=False, feet_on_ground=True,
                              scale=0.1, macro_detail_dict=macro)
    targets = importlib.import_module(pkg + ".services.targetservice").TargetService
    stack = [{"target": name, "value": float(opt[key])}
             for key, names in sorted(DETAIL_TARGETS.items())
             for name in names if float(opt[key]) != 0.0]
    if stack:
        targets.bulk_load_targets(mesh, stack)
        log("targets", {s["target"]: s["value"] for s in stack})
    meta = human.add_builtin_rig(mesh, "rigify.human", import_weights=True)
    log("macro", {k: macro[k] for k in ("gender", "age", "muscle", "weight",
                                        "proportions", "height")})

    # ФОРМА ТЕЛА ЖИВЁТ В КЛЮЧАХ ФОРМЫ. MakeHuman лепит каждый параметр
    # (пол, возраст, вес, мышцы) отдельным shape key поверх нейтральной сетки,
    # поэтому до их запекания «меш» — это ещё нейтральный манекен, а любой
    # модификатор поверх ключей Blender применить отказывается.
    if mesh.data.shape_keys is not None:
        mixed = mesh.shape_key_add(name="mixed", from_mix=True)
        co = [v.co.copy() for v in mixed.data]
        mesh.shape_key_clear()
        for i, v in enumerate(mesh.data.vertices):
            v.co = co[i]
        log("shape keys baked into the mesh")

    # ВСПОМОГАТЕЛЬНАЯ ГЕОМЕТРИЯ УХОДИТ СРАЗУ. MakeHuman держит её в том же меше
    # и прячет маской; пока маска — модификатор, всякий замер меша (габарит,
    # число треугольников, дециматор) считает по НЕЙ, а не по телу.
    # Маски ДО посадки одежды — это маска хелперов (её применяем); маски, что
    # MPFB повесит на тело за вещи с delete_verts, — снимаем, не применяя: тело
    # в glb остаётся целым, скрытие под одеждой едет данными (hide_body_vertices).
    helper_masks = {m.name for m in mesh.modifiers if m.type == "MASK"}
    parts = load_parts(opt["parts"], mesh, human)
    parts += load_parts(",".join("clothes=" + c.strip() for c in opt["clothes"].split(",") if c.strip()),
                        mesh, human)
    for mod in list(mesh.modifiers):
        if mod.type == "MASK" and mod.name not in helper_masks:
            mesh.modifiers.remove(mod)

    # ИНДЕКС hm08 КАЖДОЙ ВЕРШИНЫ — атрибутом: маска снимает хелперы и сдвигает
    # индексы, экспортёр режет вершины по швам UV; атрибут доезжает до glb
    # (_HM08_INDEX) и по нему delete_verts одежды переводятся в порядок файла.
    attr = mesh.data.attributes.new("_HM08_INDEX", "INT", "POINT")
    for i in range(len(mesh.data.vertices)):
        attr.data[i].value = i

    activate(mesh)
    for mod in list(mesh.modifiers):
        if mod.type == "MASK":
            bpy.ops.object.modifier_apply(modifier=mod.name)
        elif mod.type == "ARMATURE":
            mesh.modifiers.remove(mod)
    drop_hidden(mesh)
    log("body mesh", len(mesh.data.vertices), "verts,", tri_count(mesh), "tris")
    return mesh, meta, parts


PART_TYPES = ("eyes", "eyebrows", "eyelashes", "teeth", "tongue", "hair", "clothes")


def load_parts(spec, mesh, human):
    """«тип=имя,…» → [(тип, имя, объект, путь mhclo)], посажены на тело MPFB."""
    if not spec:
        return []
    from bl_ext.blender_org.mpfb.services.locationservice import LocationService  # type: ignore
    out = []
    for item in spec.split(","):
        item = item.strip()
        if not item:
            continue
        kind, _, name = item.partition("=")
        kind = kind.strip()
        name = name.strip()
        if kind not in PART_TYPES or not name:
            raise SystemExit("bad part: " + item + " (тип=имя, типы: " + ", ".join(PART_TYPES) + ")")
        mhclo = None
        for base in (LocationService.get_user_data(kind), LocationService.get_mpfb_data(kind)):
            cand = os.path.join(base, name, name + ".mhclo")
            if os.path.isfile(cand):
                mhclo = cand
                break
        if mhclo is None:
            raise SystemExit("part not found: %s/%s" % (kind, name))
        before = {o.name for o in bpy.data.objects}
        human.add_mhclo_asset(mhclo, mesh, asset_type=kind, subdiv_levels=0,
                              material_type="MAKESKIN", set_up_rigging=False,
                              interpolate_weights=False, import_subrig=False,
                              import_weights=False)
        added = [o for o in bpy.data.objects if o.name not in before and o.type == "MESH"]
        if len(added) != 1:
            raise SystemExit("part %s: expected one mesh, got %d" % (item, len(added)))
        ob = added[0]
        ob.name = kind if kind != "clothes" else name
        ob.data.name = ob.name
        out.append((kind, name, ob, mhclo))
        log("part %s = %s: %d verts, %d tris" % (kind, name, len(ob.data.vertices), tri_count(ob)))
    return out


def rig_parts(parts, mesh, rig):
    """Веса частей — переносом с тела (ближайшая грань), потом привязка к ригу."""
    for kind, _name, ob, _mhclo in parts:
        for grp in list(ob.vertex_groups):
            ob.vertex_groups.remove(grp)
        for grp in mesh.vertex_groups:
            ob.vertex_groups.new(name=grp.name)
        ob.parent = None
        ob.matrix_world = Matrix.Identity(4)
        activate(ob)
        mod = ob.modifiers.new("dt", "DATA_TRANSFER")
        mod.object = mesh
        mod.use_vert_data = True
        mod.data_types_verts = {"VGROUP_WEIGHTS"}
        mod.vert_mapping = "POLYINTERP_NEAREST"
        mod.layers_vgroup_select_src = "ALL"
        mod.layers_vgroup_select_dst = "NAME"
        bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
        bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        loose = sum(1 for v in ob.data.vertices if not v.groups)
        if loose:
            head = ob.vertex_groups.get("DEF-head")
            if head is None:
                raise SystemExit("part %s: %d unweighted verts and no DEF-head" % (kind, loose))
            head.add([v.index for v in ob.data.vertices if not v.groups], 1.0, "REPLACE")
        bind(ob, rig)
        log("part %s: weights transferred (%d verts without a face nearby -> head)" % (kind, loose))


def part_material(kind, name, ob, mhclo, tex_dir, rel_dir, sums, licence):
    """Материал части: альбедо (+нормаль) внешними файлами <часть>_albedo.png."""
    import hashlib
    import shutil
    mh = parse_mhmat_of(mhclo)
    if mh is None or "diffuseTexture" not in mh[1]:
        raise SystemExit("part %s has no material with diffuseTexture" % kind)
    mat_path, tex = mh
    src = os.path.join(os.path.dirname(mat_path), tex["diffuseTexture"])
    files = [("albedo", src)]
    normal = os.path.splitext(src)[0].replace("_diffuse", "") + "_normal.png"
    if "normalmapTexture" in tex and os.path.isfile(os.path.join(os.path.dirname(mat_path), tex["normalmapTexture"])):
        files.append(("normal", os.path.join(os.path.dirname(mat_path), tex["normalmapTexture"])))
    elif os.path.isfile(normal):
        files.append(("normal", normal))
    if "aomapTexture" in tex and os.path.isfile(os.path.join(os.path.dirname(mat_path), tex["aomapTexture"])):
        files.append(("ao", os.path.join(os.path.dirname(mat_path), tex["aomapTexture"])))
    mat = bpy.data.materials.new("M_" + kind)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    transparent = str(tex.get("transparent", "False")).lower() == "true"
    two_sided = str(tex.get("backfaceCull", "True")).lower() != "true"
    label = kind if kind != "clothes" else name
    for role, path in files:
        fname = "%s_%s.png" % (label, role)
        dst = os.path.join(tex_dir, fname)
        shutil.copyfile(path, dst)
        sha = hashlib.sha256(open(dst, "rb").read()).hexdigest()
        sums.append("%s  %s" % (sha, fname))
        licence.append("%s — %s «%s» (%s), системные ассеты MPFB / MakeHuman, CC0 1.0; источник %s"
                       % (fname, kind, name, os.path.basename(path), mat_path))
        img = bpy.data.images.get(fname)
        if img is None or os.path.abspath(bpy.path.abspath(img.filepath)) != os.path.abspath(dst):
            img = bpy.data.images.load(dst, check_existing=True)
            img.name = fname
        texn = nodes.new("ShaderNodeTexImage")
        texn.image = img
        if role == "albedo":
            img.colorspace_settings.name = "sRGB"
            links.new(texn.outputs["Color"], bsdf.inputs["Base Color"])
            if transparent:
                # ВЫРЕЗ, НЕ СМЕШИВАНИЕ: карточки волос/бровей/ресниц — alphaMode
                # MASK с порогом 0,5 (экспортёр узнаёт узел «больше чем» перед
                # альфой); непрозрачным частям альфа не подключается — OPAQUE.
                cut = nodes.new("ShaderNodeMath")
                cut.operation = "GREATER_THAN"
                cut.inputs[1].default_value = 0.5
                links.new(texn.outputs["Alpha"], cut.inputs[0])
                links.new(cut.outputs["Value"], bsdf.inputs["Alpha"])
        elif role == "normal":
            img.colorspace_settings.name = "Non-Color"
            nm = nodes.new("ShaderNodeNormalMap")
            links.new(texn.outputs["Color"], nm.inputs["Color"])
            links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
        else:
            # AO: в glTF идёт occlusionTexture (экспортёр узнаёт узел glTF Material
            # Output); здесь — просто внешним файлом с ролью ao, узел не строим.
            img.colorspace_settings.name = "Non-Color"
            nodes.remove(texn)
    if hasattr(mat, "blend_method"):
        mat.blend_method = "CLIP" if transparent else "OPAQUE"
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "DITHERED"
    mat.use_backface_culling = not two_sided
    bsdf.inputs["Roughness"].default_value = 0.5
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return [("%s_%s.png" % (label, role), rel_dir + "/" + "%s_%s.png" % (label, role)) for role, _ in files]


def parse_delete_verts(mhclo_path):
    """delete_verts из mhclo: индексы вершин hm08, скрытых под вещью."""
    out = set()
    active = False
    for line in open(mhclo_path, encoding="utf-8"):
        s = line.strip()
        if not s:
            continue
        if s.startswith("delete_verts"):
            active = True
            continue
        if active:
            if s[0].isalpha():
                active = False
                continue
            toks = s.replace("-", " - ").split()
            i = 0
            while i < len(toks):
                if i + 2 < len(toks) and toks[i + 1] == "-":
                    out.update(range(int(toks[i]), int(toks[i + 2]) + 1))
                    i += 3
                else:
                    out.add(int(toks[i]))
                    i += 1
    return out


def body_index_map(glb_path):
    """hm08-индекс → список индексов вершин тела в glb (по атрибуту _HM08_INDEX)."""
    import json
    import struct
    b = open(glb_path, "rb").read()
    ln = struct.unpack_from("<I", b, 12)[0]
    doc = json.loads(b[20:20 + ln])
    off = 20 + ln
    bl = struct.unpack_from("<I", b, off)[0]
    binc = b[off + 8:off + 8 + bl]
    prim = doc["meshes"][0]["primitives"][0]
    if "_HM08_INDEX" not in prim["attributes"]:
        raise SystemExit("body glb lacks _HM08_INDEX — bake the body with this tool first")
    acc = doc["accessors"][prim["attributes"]["_HM08_INDEX"]]
    bv = doc["bufferViews"][acc["bufferView"]]
    fmt = {5121: "B", 5123: "H", 5125: "I", 5120: "b", 5122: "h", 5126: "f"}[acc["componentType"]]
    size = struct.calcsize(fmt)
    stride = bv.get("byteStride", size)
    base = bv["byteOffset"] + acc.get("byteOffset", 0)
    out = {}
    for i in range(acc["count"]):
        v = int(struct.unpack_from("<" + fmt, binc, base + i * stride)[0])
        out.setdefault(v, []).append(i)
    return out


def parse_mhmat_of(mhclo_path):
    """Путь материала из mhclo (относительно него) и его текстурные ключи."""
    for line in open(mhclo_path, encoding="utf-8"):
        parts = line.strip().split(None, 1)
        if len(parts) == 2 and parts[0] == "material":
            mat = os.path.normpath(os.path.join(os.path.dirname(mhclo_path), parts[1].strip()))
            if os.path.isfile(mat):
                return mat, parse_mhmat(mat)
    return None


def drop_hidden(mesh):
    """Зубы и язык — внутрь рта, и оттуда их не видит ни один кадр."""
    index = {i: g.name for i, g in enumerate(mesh.vertex_groups)}
    doomed = set()
    for v in mesh.data.vertices:
        names = [index.get(g.group, "") for g in v.groups if g.weight > 0.0]
        if names and all(n.startswith(HIDDEN_INSIDE) for n in names):
            doomed.add(v.index)
    if not doomed:
        return
    bm = bmesh.new()
    bm.from_mesh(mesh.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in sorted(doomed)], context="VERTS")
    bm.to_mesh(mesh.data)
    bm.free()
    mesh.data.update()
    log("dropped", len(doomed), "hidden interior vertices (teeth, tongue)")


# ------------------------------------------------------- 2. скелет доставки --

def build_rig(meta, spec, height):
    activate(meta)
    bpy.ops.object.mode_set(mode="EDIT")
    geo = {b.name: (b.head.copy(), b.tail.copy(), b.roll)
           for b in meta.data.edit_bones}
    bpy.ops.object.mode_set(mode="OBJECT")

    data = bpy.data.armatures.new("Rig")
    rig = bpy.data.objects.new("Rig", data)
    bpy.context.collection.objects.link(rig)
    activate(rig)
    bpy.ops.object.mode_set(mode="EDIT")
    # ТАЗ НАЧИНАЕТСЯ В ЦЕНТРЕ ТАЗОБЕДРЕННЫХ СУСТАВОВ, А НЕ НА КРЕСТЦЕ.
    # docs/RIG.md ставит проксимальный сустав кости Pelvis в ЦЕНТР БЁДЕР, и
    # судья мерит по нему `hip height` против канонических 0.530H. Кость
    # `spine` метарига начинается на крестце — на 8 см ниже, — и та же
    # фигура читалась как 0.482H (−9 %), то есть отвергалась за посадку
    # кости, а не за форму тела.
    hips_head = (geo["thigh.L"][0] + geo["thigh.R"][0]) * 0.5
    for name, parent, src_head, src_tail in spec:
        eb = data.edit_bones.new(name)
        if src_head is None:  # корень: ось фигуры, как у прежнего ассета
            eb.head = Vector((0.0, 0.0, 0.0))
            eb.tail = Vector((0.0, 0.0, height * 0.5))
            eb.roll = 0.0
        else:
            eb.head = hips_head if name == "DEF-hips" else geo[src_head][0]
            eb.tail = geo[src_tail][1]
            eb.roll = geo[src_head][2]
        eb.use_connect = False
        if parent is not None:
            eb.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    log("rig", len(data.bones), "bones")
    return rig


# ------------------------------------------------------------- 3. веса ------

def remap_weights(mesh, meta, rig, spec):
    kept = [n for n, _, _, _ in spec if n != "root"]
    seg = []
    for n in kept:
        b = rig.data.bones[n]
        seg.append((n, b.head_local.copy(), b.tail_local.copy()))

    meta_bones = {b.name: b for b in meta.data.bones}
    target = {}
    for grp in mesh.vertex_groups:
        n = grp.name
        if n in NOT_A_BONE or n.startswith("mhmask-"):
            continue
        if n in kept:
            target[n] = n
            continue
        if n in WEIGHT_MAP:
            target[n] = WEIGHT_MAP[n]
            continue
        if "DEF-" + n in kept:
            target[n] = "DEF-" + n
            continue
        b = meta_bones.get(n)
        if b is None:
            continue
        # ЛИЦО — ГОЛОВЕ, ЦЕЛИКОМ (02.09-2). Кости лица метарига (nose, lip, jaw,
        # cheek, lid, brow, ear, teeth, tongue — все под «face» и spine.006)
        # лежат НИЖЕ сустава головы, и ближайший отрезок для носа и губ — шея:
        # 496 вершин лица уходили в DEF-neck с весом до 0,999, а покой донора
        # поворачивает шею и голову по-разному — нос сминался в лоскут, губы
        # шли складкой (рендеры 49_stage_*). У лица одна кость — голова.
        anc = b
        while anc is not None:
            if anc.name in ("face", "spine.006"):
                target[n] = "DEF-head"
                break
            anc = anc.parent
        if n in target:
            continue
        # БЛИЖАЙШИЙ ОТРЕЗОК, А НЕ БЛИЖАЙШИЙ ПРЕДОК. Прогулка вверх по родителям
        # уносила вес локтевого помощника в позвоночник и делала туловище
        # полуметровым: мясо принадлежит ближайшему звену, а не старшему.
        mid = (b.head_local + b.tail_local) * 0.5
        target[n] = min(seg, key=lambda s: seg_dist(mid, s[1], s[2]))[0]

    for n in kept:
        if n not in mesh.vertex_groups:
            mesh.vertex_groups.new(name=n)
    index = {i: g.name for i, g in enumerate(mesh.vertex_groups)}
    adds = {}
    for v in mesh.data.vertices:
        for g in v.groups:
            src = index.get(g.group)
            dst = target.get(src)
            if dst is not None and dst != src and g.weight > 0.0:
                adds.setdefault(dst, []).append((v.index, g.weight))
    for dst, lst in adds.items():
        grp = mesh.vertex_groups[dst]
        for vi, w in lst:
            grp.add([vi], w, "ADD")
    moved = sum(1 for s, d in target.items() if s != d)
    log("weights: %d groups merged into %d, %d groups kept"
        % (moved, len(adds), len(target) - moved))

    for grp in list(mesh.vertex_groups):
        if grp.name not in kept:
            mesh.vertex_groups.remove(grp)

    # Каждая вершина обязана иметь вес: непривязанная поедет с началом координат.
    loose = 0
    for v in mesh.data.vertices:
        if not any(g.weight > 0.0 for g in v.groups):
            loose += 1
    log("unweighted vertices", loose)
    return loose


def bind(mesh, rig):
    mesh.parent = rig
    mesh.matrix_parent_inverse = rig.matrix_world.inverted()
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = rig
    mod.use_vertex_groups = True


def decimate(mesh, budget):
    before = tri_count(mesh)
    if budget <= 0 or before <= budget:
        log("tris", before, "(no decimate)")
        return before
    mod = mesh.modifiers.new("LOD0", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.use_collapse_triangulate = True
    mod.use_symmetry = True           # силуэт обязан остаться симметричным
    mod.symmetry_axis = "X"
    mod.ratio = float(budget) / float(before)
    after = tri_count(mesh)
    log("tris", before, "->", after, "(ratio %.4f)" % mod.ratio)
    return after


# КОСТИ, ЧЕЙ ПОКОЙ ОСТАЁТСЯ СВОИМ (правка 02.09-2, «плечи сильно назад»): ключица
# манекена Quaternius в покое смотрит на 30,5° НАЗАД (замер по UAL_Clips.glb), у
# MPFB — на 3,5°. Выравнивание по донору поворачивало ключицу на 27° назад и
# запекало это в мясо — плечи уезжали за спину. Ключицы держат свой покой, а
# клип переносится на них ДЕЛЬТОЙ (поворот донора относительно его покоя,
# приложенный к нашему покою), см. retarget().
OWN_REST_BONES = frozenset({"DEF-shoulder.L", "DEF-shoulder.R"})


def align_rest_to_donor(mesh, rig, src_rig, extra=()):
    """ПОКОЙ НАШЕГО РИГА ПРИВОДИТСЯ К ПОКОЮ ДОНОРА КЛИПОВ (правка 02.09).

    Находка бисекта: ретаргет копирует МИРОВУЮ ориентацию кости (совпадение с
    донором проверено до 0°), а покой MPFB отличается от T-позы донора на
    48° в плече, 65° в кисти и 83–98° в фалангах (A-поза, другой ролл ладони,
    расслабленно согнутые пальцы). Чтобы попасть в кулак Idle, фаланге
    приходилось крутиться на 96–155° от СВОЕГО бинда, и линейный скиннинг тянул
    кисть в иглы. Здесь наш риг один раз ставится в покой донора, эта поза
    запекается в меш нашими же весами, объявляется покоем рига, и меш
    привязывается заново. После этого разница покоев — ноль, и любой клип
    крутит кость ровно на столько, на сколько её крутил автор.
    """
    names = [b.name for b in rig.data.bones]
    order, seen = [], set()

    def walk(b):
        if b.name in seen:
            return
        if b.parent is not None:
            walk(b.parent)
        seen.add(b.name)
        order.append(b.name)
    for b in rig.data.bones:
        walk(b)
    rest = {n: rig.data.bones[n].matrix_local.copy() for n in names}
    rest_head = {n: rig.data.bones[n].head_local.copy() for n in names}
    parent = {n: (rig.data.bones[n].parent.name if rig.data.bones[n].parent else None)
              for n in names}
    to_src = src_rig.matrix_world
    to_rig = rig.matrix_world.inverted()
    want = {n: ((to_rig @ to_src @ src_rig.data.bones[n].matrix_local)
                if (n in src_rig.data.bones and n not in OWN_REST_BONES) else rest[n])
            for n in names}
    pose = {}
    for n in order:
        rot = rot_of(want[n])
        p = parent[n]
        if p is None:
            head = rest_head[n]
        else:
            delta = rot_of(want[p]) @ rest[p].to_3x3().normalized().inverted()
            head = pose[p].to_translation() + delta @ (rest_head[n] - rest_head[p])
        pose[n] = Matrix.Translation(head) @ rot.to_4x4()
    for n in order:
        p = parent[n]
        if p is None:
            basis = rest[n].inverted() @ pose[n]
        else:
            basis = rest[n].inverted() @ rest[p] @ pose[p].inverted() @ pose[n]
        pb = rig.pose.bones[n]
        pb.rotation_mode = "QUATERNION"
        pb.matrix_basis = basis
    bpy.context.view_layer.update()
    turned = {n: math.degrees(rot_of(rest[n]).to_quaternion().rotation_difference(
        rot_of(pose[n]).to_quaternion()).angle) for n in names}
    # запечь позу в меш ТЕМИ ЖЕ весами, что пойдут в игру, и объявить её покоем
    activate(mesh)
    bpy.ops.object.modifier_apply(modifier="Armature")
    for ob in extra:
        activate(ob)
        bpy.ops.object.modifier_apply(modifier="Armature")
    activate(rig)
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    bind(mesh, rig)
    for ob in extra:
        bind(ob, rig)
    bpy.context.view_layer.update()
    left = 0.0
    for n in names:
        if n in src_rig.data.bones and n not in OWN_REST_BONES:
            a = rot_of(to_rig @ to_src @ src_rig.data.bones[n].matrix_local).to_quaternion()
            b = rot_of(rig.data.bones[n].matrix_local).to_quaternion()
            ang = math.degrees(a.rotation_difference(b).angle)
            left = max(left, min(ang, 360.0 - ang))
    log("rest aligned to the clip source: turned upper_arm.L %.1f deg, hand.L %.1f, "
        "f_index.02.L %.1f, thigh.L %.1f; residual rest difference %.3f deg"
        % (turned.get("DEF-upper_arm.L", 0.0), turned.get("DEF-hand.L", 0.0),
           turned.get("DEF-f_index.02.L", 0.0), turned.get("DEF-thigh.L", 0.0), left))


def retarget(rig, src_rig, moving, only=None):
    """Мировой поворот одноимённой кости + прямая кинематика по своим длинам."""
    names = [b.name for b in rig.data.bones]
    order = []                      # родитель раньше ребёнка
    seen = set()

    def walk(b):
        if b.name in seen:
            return
        if b.parent is not None:
            walk(b.parent)
        seen.add(b.name)
        order.append(b.name)
    for b in rig.data.bones:
        walk(b)

    rest = {n: rig.data.bones[n].matrix_local.copy() for n in names}
    rest_head = {n: rig.data.bones[n].head_local.copy() for n in names}
    parent = {n: (rig.data.bones[n].parent.name if rig.data.bones[n].parent else None)
              for n in names}
    to_src = src_rig.matrix_world
    to_rig = rig.matrix_world.inverted()
    src_rest = {n: (to_rig @ to_src @ src_rig.data.bones[n].head_local)
                for n in names if n in src_rig.data.bones}
    # покой донора и наш покой (повороты) — для костей, что переносятся ДЕЛЬТОЙ
    src_rest_rot = {n: rot_of(to_rig @ to_src @ src_rig.data.bones[n].matrix_local)
                    for n in names if n in src_rig.data.bones}
    own_rest_rot = {n: rot_of(rest[n]) for n in names}

    if rig.animation_data is None:
        rig.animation_data_create()
    worst = 0.0
    # ИМЯ КЛИПА — ЭТО ЕГО РОЛЬ, и оно теряется в ТРЁХ местах подряд.
    # `ClipPlayer::build_clip_library` ищет роль по имени клипа ("Idle_Loop",
    # "Walk_Loop", "Jog_Fwd_Loop"), а имя по дороге успевает обрасти дважды и
    # столкнуться один раз:
    #   1. НА ВХОДЕ импортёр glTF Blender подписывает действие именем
    #      АРМАТУРЫ, в которую его положил, — "Walk_Loop_Rig.001", где ".001"
    #      взялось из того, что наша арматура уже заняла имя "Rig". Снимается
    #      здесь, `decor`.
    #   2. ПРИ ПЕРЕИМЕНОВАНИИ: пока исходное действие живо, Blender молча
    #      даёт новому имя "Walk_Loop.001". Поэтому имена раздаются ПОСЛЕ
    #      удаления исходников, ниже.
    #   3. НА ВЫХОДЕ экспортёр приписывает имя объекта — снимается в
    #      `fix_clip_names`, и там же сверяется весь набор имён.
    # Каждое из трёх по отдельности даёт файл с сорока шестью живыми клипами,
    # из которых движок не узнаёт НИ ОДНОГО, и рисует T-позу. Снимать приписку
    # только на выходе мало: проверка сошлась бы сама с собой.
    decor = "_" + src_rig.name
    all_sources = sorted(bpy.data.actions, key=lambda a: a.name)
    sources = all_sources
    if only:
        sources = [a for a in sources
                   if (a.name[:-len(decor)] if a.name.endswith(decor) else a.name) in only]
    made = []
    for action in sources:
        src_rig.animation_data.action = action
        f0 = int(math.floor(action.frame_range[0]))
        f1 = int(math.ceil(action.frame_range[1]))
        clip = action.name[:-len(decor)] if action.name.endswith(decor) \
            else action.name
        out = bpy.data.actions.new("retargeted@" + clip)
        made.append((out, clip))
        rig.animation_data.action = out
        for frame in range(f0, f1 + 1):
            bpy.context.scene.frame_set(frame)
            src_world = {}
            for n in names:
                pb = src_rig.pose.bones.get(n)
                src_world[n] = to_rig @ to_src @ pb.matrix if pb else Matrix()
            pose = {}
            for n in order:
                rot = src_world[n].to_3x3().normalized()
                if n in OWN_REST_BONES and n in src_rest_rot:
                    # дельта донора (кадр × покой⁻¹) на НАШ покой: ключица
                    # двигается, как у автора, но из своего положения
                    rot = rot @ src_rest_rot[n].inverted() @ own_rest_rot[n]
                p = parent[n]
                if p is None or n in moving:
                    head = rest_head[n] + (src_world[n].to_translation() - src_rest[n])
                else:
                    delta = rot_of(src_world[p]) @ rest[p].to_3x3().normalized().inverted()
                    head = pose[p].to_translation() + delta @ (rest_head[n] - rest_head[p])
                pose[n] = Matrix.Translation(head) @ rot.to_4x4()
            for n in order:
                p = parent[n]
                if p is None:
                    basis = rest[n].inverted() @ pose[n]
                else:
                    basis = rest[n].inverted() @ rest[p] @ pose[p].inverted() @ pose[n]
                pb = rig.pose.bones[n]
                pb.rotation_mode = "QUATERNION"
                pb.matrix_basis = basis
                pb.keyframe_insert("rotation_quaternion", frame=frame, group=n)
                if p is None or n in moving:
                    pb.keyframe_insert("location", frame=frame, group=n)
        # КОНТРОЛЬ, А НЕ ОБЕЩАНИЕ: то, что посчитала формула, против того, что
        # из этих же базисов собрал сам Blender.
        bpy.context.view_layer.update()
        for n in names:
            err = (rig.pose.bones[n].matrix.to_translation()
                   - pose[n].to_translation()).length
            worst = max(worst, err)
    src_rig.animation_data.action = None
    for action in all_sources:
        bpy.data.actions.remove(action)
    for out, name in made:
        out.name = name
    rig.animation_data.action = made[0][0] if made else None
    kept = sorted(a.name for a in bpy.data.actions)
    if len(kept) != len(made):
        raise SystemExit("clip bookkeeping: %d actions left for %d clips"
                         % (len(kept), len(made)))
    # РОЛИ РЕЗОЛВЯТСЯ ПО ИМЕНИ, поэтому четыре имени локомоции проверяются
    # здесь, а не в отчёте: файл без них выйдет в кадр T-позой при сорока
    # шести живых клипах внутри, и ни один прогон об этом не скажет.
    names = {name for _, name in made}
    wanted = only or ("Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop")
    missing = [n for n in wanted if n not in names]
    if missing:
        raise SystemExit("clip names lost the locomotion roles: " + ", ".join(missing)
                         + " (have: " + ", ".join(sorted(names)[:6]) + " ...)")
    log("retarget: %d clips, worst head error %.6f m" % (len(made), worst))
    return [name for _, name in made]


# КАРТЫ ИМЁН ЧУЖИХ РИГОВ: наша кость → кость донора. Что не названо, держит
# покой относительно родителя (у KayKit нет ключиц, шеи и пальцев).
DONOR_MAPS = {
    "kaykit": {
        "DEF-hips": "hips", "DEF-spine.001": "spine", "DEF-spine.003": "chest",
        "DEF-head": "head",
        "DEF-upper_arm.L": "upperarm.l", "DEF-forearm.L": "lowerarm.l",
        "DEF-hand.L": "hand.l",
        "DEF-upper_arm.R": "upperarm.r", "DEF-forearm.R": "lowerarm.r",
        "DEF-hand.R": "hand.r",
        "DEF-thigh.L": "upperleg.l", "DEF-shin.L": "lowerleg.l",
        "DEF-foot.L": "foot.l", "DEF-toe.L": "toes.l",
        "DEF-thigh.R": "upperleg.r", "DEF-shin.R": "lowerleg.r",
        "DEF-foot.R": "foot.r", "DEF-toe.R": "toes.r",
    },
}


def retarget_mapped(rig, src_rig, bone_map, sources, prefix, only=None):
    """Чужой риг: мировая ориентация сопоставленной кости × покой донора⁻¹ ×
    наш покой. Оба покоя обязаны быть одной позой тела (Т-поза) — это
    проверяется здесь по плечу и бедру, а не предполагается."""
    names = [b.name for b in rig.data.bones]
    order, seen = [], set()

    def walk(b):
        if b.name in seen:
            return
        if b.parent is not None:
            walk(b.parent)
        seen.add(b.name)
        order.append(b.name)
    for b in rig.data.bones:
        walk(b)

    rest = {n: rig.data.bones[n].matrix_local.copy() for n in names}
    rest_head = {n: rig.data.bones[n].head_local.copy() for n in names}
    parent = {n: (rig.data.bones[n].parent.name if rig.data.bones[n].parent else None)
              for n in names}
    to_src = src_rig.matrix_world
    to_rig = rig.matrix_world.inverted()
    missing = [s for s in bone_map.values() if s not in src_rig.data.bones]
    if missing:
        raise SystemExit("donor2 lacks bones: " + ", ".join(missing))
    src_rest_rot = {n: rot_of(to_rig @ to_src @ src_rig.data.bones[s].matrix_local)
                    for n, s in bone_map.items()}
    src_rest_head = {n: (to_rig @ to_src @ src_rig.data.bones[s].head_local)
                     for n, s in bone_map.items()}
    own_rest_rot = {n: rot_of(rest[n]) for n in names}

    # ОДНА ПОЗА ПОКОЯ: направление плечо→кисть и бедро→стопа у обоих.
    def axis(head, a, b):
        v = head[b] - head[a]
        return v.normalized() if v.length > 1e-6 else v
    for a, b in (("DEF-upper_arm.L", "DEF-hand.L"), ("DEF-thigh.L", "DEF-foot.L")):
        own = axis(rest_head, a, b)
        src = axis(src_rest_head, a, b)
        if own.dot(src) < math.cos(math.radians(8.0)):
            raise SystemExit("donor2 rest pose differs from ours at %s→%s: %.1f°"
                             % (a, b, math.degrees(math.acos(max(-1.0, min(1.0, own.dot(src)))))))
    # рост таза над стопой — масштаб для перемещения таза (боб, присед)
    own_h = rest_head["DEF-hips"].z - rest_head["DEF-toe.L"].z
    src_h = src_rest_head["DEF-hips"].z - src_rest_head["DEF-toe.L"].z
    hips_scale = own_h / src_h if abs(src_h) > 1e-6 else 1.0
    log("donor2: hips scale %.3f (own %.3f m, donor %.3f m)" % (hips_scale, own_h, src_h))

    decor = "_" + src_rig.name
    if rig.animation_data is None:
        rig.animation_data_create()
    if src_rig.animation_data is None:
        src_rig.animation_data_create()
    made = []
    worst = 0.0
    for action in sorted(sources, key=lambda a: a.name):
        clip = action.name[:-len(decor)] if action.name.endswith(decor) else action.name
        if only and clip not in only:
            continue
        src_rig.animation_data.action = action
        f0 = int(math.floor(action.frame_range[0]))
        f1 = int(math.ceil(action.frame_range[1]))
        out = bpy.data.actions.new("retargeted2@" + clip)
        made.append((out, prefix + clip))
        rig.animation_data.action = out
        for frame in range(f0, f1 + 1):
            bpy.context.scene.frame_set(frame)
            src_world = {n: to_rig @ to_src @ src_rig.pose.bones[s].matrix
                         for n, s in bone_map.items()}
            pose = {}
            for n in order:
                p = parent[n]
                if n in src_world:
                    rot = rot_of(src_world[n]) @ src_rest_rot[n].inverted() @ own_rest_rot[n]
                elif p is None:
                    rot = own_rest_rot[n]
                else:
                    rot = (rot_of(pose[p]) @ rest[p].to_3x3().normalized().inverted()
                           @ own_rest_rot[n])
                if p is None:
                    head = rest_head[n].copy()
                elif n == "DEF-hips" and n in src_world:
                    head = rest_head[n] + (src_world[n].to_translation()
                                           - src_rest_head[n]) * hips_scale
                else:
                    delta = rot_of(pose[p]) @ rest[p].to_3x3().normalized().inverted()
                    head = pose[p].to_translation() + delta @ (rest_head[n] - rest_head[p])
                pose[n] = Matrix.Translation(head) @ rot.to_4x4()
            for n in order:
                p = parent[n]
                if p is None:
                    basis = rest[n].inverted() @ pose[n]
                else:
                    basis = rest[n].inverted() @ rest[p] @ pose[p].inverted() @ pose[n]
                pb = rig.pose.bones[n]
                pb.rotation_mode = "QUATERNION"
                pb.matrix_basis = basis
                pb.keyframe_insert("rotation_quaternion", frame=frame, group=n)
                if p is None or n == "DEF-hips":
                    pb.keyframe_insert("location", frame=frame, group=n)
        bpy.context.view_layer.update()
        for n in names:
            err = (rig.pose.bones[n].matrix.to_translation()
                   - pose[n].to_translation()).length
            worst = max(worst, err)
    src_rig.animation_data.action = None
    for action in sources:
        bpy.data.actions.remove(action)
    for out, name in made:
        out.name = name
    log("donor2: %d clips, worst head error %.6f m" % (len(made), worst))
    return [name for _, name in made]


def rot_of(m):
    return m.to_3x3().normalized()


# ------------------------------------------------------------------- main ---

def main():
    opt = parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = opt["out"] if os.path.isabs(opt["out"]) else os.path.join(root, opt["out"])
    clips = opt["clips"]
    if clips and not os.path.isabs(clips):
        clips = os.path.join(root, clips)

    enable_addons()
    wipe()
    # 30 к/с — частота, на которой авторы клипов их и ставили; при 24 ключи
    # легли бы между кадрами и выборка по целым кадрам потеряла бы точность.
    bpy.context.scene.render.fps = 30

    mesh, meta, parts = make_body(opt)
    height = max((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
    spec = build_spec()
    rig = build_rig(meta, spec, height)
    remap_weights(mesh, meta, rig, spec)
    bind(mesh, rig)
    rig_parts(parts, mesh, rig)
    bpy.data.objects.remove(meta, do_unlink=True)
    decimate(mesh, int(opt["tris"]))

    if clips:
        before = {o.name for o in bpy.data.objects}
        bpy.ops.import_scene.gltf(filepath=clips)
        src_rig = None
        for o in bpy.data.objects:
            if o.name not in before and o.type == "ARMATURE":
                src_rig = o
        if src_rig is None:
            raise SystemExit("no armature in " + clips)
        missing = [n for n, _, _, _ in spec if n not in src_rig.data.bones]
        if missing:
            raise SystemExit("clip source lacks bones: " + ", ".join(missing))
        # ТАЗ И КОРЕНЬ — ЕДИНСТВЕННЫЕ, КТО ЕЗДИТ. Замерено по файлу: у всех
        # прочих суставов канал translation постоянен, то есть равен привязке.
        align_rest_to_donor(mesh, rig, src_rig, extra=[ob for _, _, ob, _ in parts])
        only = [x for x in opt["only"].split(",") if x]
        made = retarget(rig, src_rig, moving={"DEF-hips"}, only=only)
        for o in list(bpy.data.objects):
            if o.name not in before and o is not src_rig:
                bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.objects.remove(src_rig, do_unlink=True)

    donor2 = opt["donor2"]
    if clips and donor2:
        if not os.path.isabs(donor2):
            donor2 = os.path.join(root, donor2)
        before = {o.name for o in bpy.data.objects}
        acts_before = {a.name for a in bpy.data.actions}
        bpy.ops.import_scene.gltf(filepath=donor2)
        src2 = None
        for o in bpy.data.objects:
            if o.name not in before and o.type == "ARMATURE":
                src2 = o
        if src2 is None:
            raise SystemExit("no armature in " + donor2)
        sources = [a for a in bpy.data.actions if a.name not in acts_before]
        only2 = [x for x in opt["donor2-only"].split(",") if x]
        made += retarget_mapped(rig, src2, DONOR_MAPS[opt["donor2-map"]], sources,
                                opt["donor2-prefix"], only=only2)
        for o in list(bpy.data.objects):
            if o.name not in before and o is not src2:
                bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.objects.remove(src2, do_unlink=True)
        rig.animation_data.action = None

    # Один материал: собственные материалы MakeHuman телу не нужны — цвет по
    # частям тела кладёт импортёр (--skin-palette), а безымянный примитив
    # читатель .glb принимает за отсутствие материала.
    skin_uri = None
    if opt["skin"]:
        skin_uri, _ = apply_skin(mesh, opt["skin"], out)
    if not mesh.data.materials:
        mat = bpy.data.materials.new("M_Skin")
        mat.diffuse_color = (0.76, 0.60, 0.50, 1.0)
        mesh.data.materials.append(mat)

    # НИЧЕГО, КРОМЕ ТЕЛА И СКЕЛЕТА. MPFB кладёт рядом с фигурой служебную
    # икосферу (42 вершины), и `use_selection` её не отсекает: экспортёр glTF
    # тянет ДЕТЕЙ выделенного. В .dfo она не попадала (импортёр берёт только
    # скиннованные части), но в .glb ехала и портила всякий замер габарита по
    # файлу — на ней габарит фигуры читался 2.73 м вместо 1.73.
    part_objects = {ob.name for _, _, ob, _ in parts}
    for ob in list(bpy.data.objects):
        if ob is not mesh and ob is not rig and ob.name not in part_objects:
            bpy.data.objects.remove(ob, do_unlink=True)

    # ИМЕНА ОБЪЕКТОВ ЗАДАЮТСЯ ЯВНО, потому что от них зависит имя КЛИПА в
    # файле (см. fix_clip_names): импорт исходника клипов заводит вторую
    # арматуру с тем же именем, наша молча становится "Rig.001", и суффикс
    # уезжает в имя анимации.
    rig.name = "Rig"
    rig.data.name = "Rig"
    mesh.name = "Human"
    mesh.data.name = "base"

    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    os.makedirs(os.path.dirname(out), exist_ok=True)
    common = dict(
        use_selection=True,
        export_skins=True, export_yup=True, export_apply=True,
        export_animations=bool(clips), export_animation_mode="ACTIONS",
        export_bake_animation=False,
        # ВЫБРАСЫВАТЬ ПОВТОРЯЮЩИЕСЯ КЛЮЧИ, НО НЕ КАНАЛЫ. Двадцать пальцевых
        # суставов стоят неподвижно в сорока клипах из сорока шести, и без
        # этого ключа файл несёт их покадрово; с ним — но БЕЗ второго ключа —
        # экспортёр выкидывает канал целиком, а канала, которого нет, у
        # скелета нет и в бинде.
        export_optimize_animation_size=True,
        export_optimize_animation_keep_anim_armature=True,
        export_rest_position_armature=True)
    common["export_attributes"] = True  # _HM08_INDEX — карта вершин для скрытий одежды
    if skin_uri is None:
        bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", **common)
    else:
        # ВНЕШНИЕ ТЕКСТУРЫ: экспорт врозь (.gltf + .bin + png), затем
        # упаковка в glb со ссылкой на albedo.png рядом — GLB не пухнет, кожа
        # переиспользуется между телами, sha ловит подмену (решение лида).
        import tempfile
        tmp = tempfile.mkdtemp(prefix="dfn_glb_")
        tmp_gltf = os.path.join(tmp, "body.gltf")
        bpy.ops.export_scene.gltf(filepath=tmp_gltf, export_format="GLTF_SEPARATE",
                                  export_keep_originals=True, **common)
        pack_glb_with_external_images(tmp_gltf, out, {"albedo.png": skin_uri})
    body_parts = [p for p in parts if p[0] != "clothes"]
    clothes = [p for p in parts if p[0] == "clothes"]
    if body_parts:
        export_parts(body_parts, mesh, rig, out, "parts")
    if clothes:
        export_parts(clothes, mesh, rig, out, "clothes", body_map=body_index_map(out))
    if clips:
        fix_clip_names(out, made, "_" + rig.name)
    log("wrote", out, os.path.getsize(out), "bytes")


def parse_mhmat(path):
    """Плоский текст MakeHuman: ключ значение… — нужны только текстуры."""
    out = {}
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2 and (parts[0].endswith("Texture")
                                or parts[0] in ("transparent", "backfaceCull")):
            out[parts[0]] = parts[1].strip()
    return out


def apply_skin(mesh, skin_name, out_path):
    """Кожа MPFB как материал с альбедо; PNG — внешним файлом рядом с выходом.

    Возвращает (относительный путь альбедо от glb, sha256 файла)."""
    import hashlib
    import shutil
    from bl_ext.blender_org.mpfb.services.locationservice import LocationService  # type: ignore
    skin_dir = None
    for base in (LocationService.get_user_data("skins"), LocationService.get_mpfb_data("skins")):
        cand = os.path.join(base, skin_name)
        if os.path.isdir(cand):
            skin_dir = cand
            break
    if skin_dir is None:
        raise SystemExit("skin not found: " + skin_name)
    mhmat = os.path.join(skin_dir, skin_name + ".mhmat")
    if not os.path.isfile(mhmat):
        cands = [f for f in os.listdir(skin_dir) if f.endswith(".mhmat")]
        if not cands:
            raise SystemExit("no .mhmat in " + skin_dir)
        mhmat = os.path.join(skin_dir, cands[0])
    tex = parse_mhmat(mhmat)
    if "diffuseTexture" not in tex:
        raise SystemExit("skin has no diffuseTexture: " + mhmat)
    src = os.path.join(skin_dir, tex["diffuseTexture"])
    stem = os.path.splitext(os.path.basename(out_path))[0]
    rel_dir = os.path.join("textures", stem)
    tex_dir = os.path.join(os.path.dirname(out_path), rel_dir)
    os.makedirs(tex_dir, exist_ok=True)
    albedo = os.path.join(tex_dir, "albedo.png")
    shutil.copyfile(src, albedo)
    sha = hashlib.sha256(open(albedo, "rb").read()).hexdigest()
    with open(os.path.join(tex_dir, "SHA256SUMS"), "w", encoding="utf-8") as f:
        f.write("%s  albedo.png\n" % sha)
    with open(os.path.join(tex_dir, "LICENSE.txt"), "w", encoding="utf-8") as f:
        f.write("albedo.png — кожа MPFB «%s» (%s), системные ассеты MPFB / MakeHuman,\n"
                "CC0 1.0 (см. assets/objects/characters/MPFB_LICENSE.txt). Источник:\n"
                "%s\nПереименовано в albedo.png без изменения пикселей; sha256 в SHA256SUMS.\n"
                % (skin_name, os.path.basename(src), mhmat))
    # материал: Principled BSDF + альбедо (sRGB), больше ничего — остальное
    # (нормали, шероховатость) решает рендер-волна лида
    img = bpy.data.images.load(albedo)
    img.colorspace_settings.name = "sRGB"
    mat = bpy.data.materials.new("M_Skin")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    texn = nodes.new("ShaderNodeTexImage")
    texn.image = img
    links.new(texn.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.55
    mesh.data.materials.clear()
    mesh.data.materials.append(mat)
    log("skin: %s -> %s (sha256 %s…)" % (skin_name, os.path.join(rel_dir, "albedo.png"), sha[:12]))
    return os.path.join(rel_dir, "albedo.png").replace(os.sep, "/"), sha


def export_parts(parts, mesh, rig, out, suffix="parts", body_map=None):
    """<stem>.<suffix>.glb: риг + части без клипов; текстуры внешними файлами."""
    stem = os.path.splitext(os.path.basename(out))[0]
    rel_dir = "textures/" + stem
    tex_dir = os.path.join(os.path.dirname(out), "textures", stem)
    os.makedirs(tex_dir, exist_ok=True)
    sums, licence, uris = [], [], {}
    for kind, name, ob, mhclo in parts:
        for img_name, uri in part_material(kind, name, ob, mhclo, tex_dir, rel_dir, sums, licence):
            uris[img_name] = uri
        if body_map is not None:
            hidden = parse_delete_verts(mhclo)
            hide = sorted(j for i in hidden for j in body_map.get(i, ()))
            ob.data["hide_body_vertices"] = hide
            log("clothes %s: hides %d hm08 verts -> %d verts of the body glb" % (name, len(hidden), len(hide)))
    with open(os.path.join(tex_dir, "SHA256SUMS"), "a", encoding="utf-8") as f:
        f.write("\n".join(sums) + "\n")
    with open(os.path.join(tex_dir, "LICENSE.txt"), "a", encoding="utf-8") as f:
        f.write("\n".join(licence) + "\n")
    bpy.ops.object.select_all(action="DESELECT")
    for _, _, ob, _ in parts:
        ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    import tempfile
    tmp = tempfile.mkdtemp(prefix="dfn_parts_")
    tmp_gltf = os.path.join(tmp, "parts.gltf")
    bpy.ops.export_scene.gltf(
        filepath=tmp_gltf, export_format="GLTF_SEPARATE", export_keep_originals=True,
        use_selection=True, export_skins=True, export_yup=True, export_apply=True,
        export_animations=False, export_rest_position_armature=True,
        export_extras=True)
    parts_out = os.path.join(os.path.dirname(out), stem + "." + suffix + ".glb")
    pack_glb_with_external_images(tmp_gltf, parts_out, uris)
    log("%s: %d meshes -> %s (%d bytes); textures %s" % (
        suffix, len(parts), parts_out, os.path.getsize(parts_out), ", ".join(sorted(uris.values()))))


def pack_glb_with_external_images(gltf_path, glb_path, image_uri):
    """gltf + bin → glb; картинки остаются ВНЕШНИМИ (uri относительно glb)."""
    import json
    import struct
    doc = json.load(open(gltf_path, encoding="utf-8"))
    base = os.path.dirname(gltf_path)
    if len(doc.get("buffers", [])) != 1 or "uri" not in doc["buffers"][0]:
        raise SystemExit("expected one external .bin buffer in " + gltf_path)
    blob = open(os.path.join(base, doc["buffers"][0]["uri"]), "rb").read()
    doc["buffers"][0] = {"byteLength": len(blob)}
    for img in doc.get("images", []):
        img.pop("bufferView", None)
        import re
        name = re.sub(r"\.\d{3}$", "", img.get("name", ""))
        key = name if name.endswith(".png") else name + ".png"
        if key not in image_uri:
            raise SystemExit("image %r has no external uri (known: %s)" % (name, ", ".join(image_uri)))
        img["uri"] = image_uri[key]
        img["mimeType"] = "image/png"
    text = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    text += b" " * ((4 - len(text) % 4) % 4)
    blob += b"\0" * ((4 - len(blob) % 4) % 4)
    out = bytearray()
    out += struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(text) + 8 + len(blob))
    out += struct.pack("<II", len(text), 0x4E4F534A) + text
    out += struct.pack("<II", len(blob), 0x004E4942) + blob
    open(glb_path, "wb").write(bytes(out))


def fix_clip_names(path, wanted, suffix):
    """Снять с имён анимаций приписку экспортёра — ИМЯ КЛИПА ЭТО ЕГО РОЛЬ.

    Экспортёр glTF Blender подписывает анимацию именем ОБЪЕКТА
    ("Walk_Loop_Rig"), а `ClipPlayer::build_clip_library` ищет роль по имени
    клипа — "Walk_Loop", "Jog_Fwd_Loop", "Sword_Idle". С припиской не
    резолвится ни одна из одиннадцати ролей, и модель едет в кадр T-позой при
    сорока шести живых клипах в файле. Проверка тут же: набор имён после
    правки обязан СОВПАСТЬ с набором испечённых записей, иначе выход с ошибкой
    — молча переименовать не то хуже, чем не переименовать вовсе.
    """
    import json
    import struct
    blob = bytearray(open(path, "rb").read())
    off = 12
    js_off = js_len = 0
    while off < len(blob):
        length, kind = struct.unpack_from("<II", blob, off)
        off += 8
        if kind == 0x4E4F534A:
            js_off, js_len = off, length
        off += length
    doc = json.loads(bytes(blob[js_off:js_off + js_len]))
    for anim in doc.get("animations", []):
        if anim.get("name", "").endswith(suffix):
            anim["name"] = anim["name"][:-len(suffix)]
    got = sorted(a.get("name", "") for a in doc.get("animations", []))
    if got != sorted(wanted):
        raise SystemExit("clip names after export do not match the bake: %s"
                         % ", ".join(sorted(set(got) ^ set(wanted))))
    text = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    text += b" " * ((4 - len(text) % 4) % 4)
    head = blob[:js_off - 8]
    tail = blob[js_off + js_len:]
    out = bytearray(head)
    out += struct.pack("<II", len(text), 0x4E4F534A)
    out += text
    out += tail
    struct.pack_into("<I", out, 8, len(out))
    open(path, "wb").write(bytes(out))
    log("clip names: stripped exporter suffix %r from %d animations"
        % (suffix, len(doc.get("animations", []))))


if __name__ == "__main__":
    main()
