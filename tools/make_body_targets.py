#!/usr/bin/env python3
#
# File: tools/make_body_targets.py
#
# Responsibility:
# - ПЕЧЁТ ПОЛЗУНКИ ТЕЛА ПРЯМЫМИ ЦЕЛЯМИ MPFB2: базовая модель игрока — само тело
#   MakeHuman (tools/make_human_body.py, решение владельца 02.09), поэтому
#   именованная цель MPFB2 (файл «индекс dx dy dz» на 19 158 вершин basemesh)
#   и макро-решётка MakeHuman ложатся на НАШИ вершины без обёртки, проекции и
#   масок: та же сетка, тот же индекс. Результат — переносной файл целей
#   (.morf), который dfn_morph attach вписывает в .dfo секцией MORF.
#
# Usage (headless, Blender 5.2 + MPFB 2.0.17 в системе):
#     /Applications/Blender.app/Contents/MacOS/Blender --background \
#         --python tools/make_body_targets.py -- \
#         --rest <rest.bin> --out assets/objects/characters/HumanBase.morf
#   где rest.bin выдаёт `dfn_morph rest assets/objects/characters/HumanBase.dfo`
#   (рест-поза скина и матрица возврата на вершину). Ключи: --only имя,имя
#   (печь не весь набор), --epsilon МЕТРЫ (порог разреженности), --dump-diag.
#
# ЧТО ЗДЕСЬ ПРОИСХОДИТ, В ПЯТИ СТРОКАХ.
#   1. Тело выращивается РОВНО так, как его печёт tools/make_human_body.py
#      (тот же модуль, те же умолчания, тот же порядок операций) — до байта та
#      же геометрия, что в HumanBase.glb, и это проверяется в конце сверкой
#      с самим .glb (14 517 вершин, невязка печатается).
#   2. Пока меш ещё basemesh целиком (19 158 вершин, до срезки helper-геометрии),
#      каждая именованная цель грузится штатной TargetService в shape key и
#      читается как дельта; макро-ручка (вес, мышцы, возраст) — как разность
#      двух выпечек MakeHuman на концах её хода: та же топология, те же индексы.
#   3. Индексы basemesh протаскиваются через срезку helper'ов и зубов
#      атрибутом на вершине, а не пересчётом.
#   4. Дельта проводится через ту же запечку покоя (align_rest_to_donor):
#      цели сняты в A-позе MakeHuman, бинд — T-поза донора клипов; линейный
#      скиннинг переносит смещение матрицей 3×3 своей кости (Σ w·R), и
#      правильность этой матрицы проверяется тем, что она же воспроизводит
#      экспортированное тело (невязка в отчёте).
#   5. Экспорт glTF раздваивает вершины по швам UV (13 380 → 14 517): каждая
#      копия получает дельту своей уникальной вершины — по совпадению
#      координат в .glb, точно, без кванта сварки. Затем бинд → рест
#      матрицей возврата из rest.bin, порог разреженности, запись.
#
# ПОЧЕМУ МАСКИ РЕГИОНОВ СНЯТЫ. Они защищали от УТЕЧКИ ПРОЕКЦИИ (46 вершин
# стоп ехали от живота, когда наша топология ложилась на чужое место
# MakeHuman). Прямая цель течь не может: она перечисляет свои вершины сама.
# Макро-ручки трогают всё тело по построению MakeHuman, и это их смысл.
#
# Dependencies:
# - Uses: bpy (Blender 5.2 LTS), расширение MPFB 2.0.17 (bl_ext.blender_org.mpfb
#   или bl_ext.user_default.mpfb), аддон rigify, tools/make_human_body.py как
#   модуль, assets/objects/characters/{UAL_Clips.glb,HumanBase.glb}; вход —
#   rest.bin от `dfn_morph rest`.
# - Used by: рука; результат потребляет `dfn_morph attach` (CMake на сборке);
#   полосы меряет tools/check_morph_bands.py.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ДЕТЕРМИНИРОВАН: те же ключи — тот же .morf до байта. Ни одного случайного
#   числа, ни одного обхода по множеству (везде отсортированные списки).
# - ЦЕЛЬ, НЕ СДВИНУВШАЯ НИ ОДНОЙ ВЕРШИНЫ, — ОТКАЗ, а не пустая запись. Ровно на
#   этом записка поймала чужой генератор ARKit: 52 цели, 0 сдвигов, «успех».
# - Тело здесь НЕ перепекается и не пишется: HumanBase.glb — одобренный ассет,
#   скрипт только читает его для сверки.

import importlib
import importlib.util
import json
import os
import struct
import sys

import bpy  # type: ignore
import numpy as np  # type: ignore
from mathutils import Matrix  # type: ignore

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ------------------------------------------------------------------ ключи ---

DEFAULTS = {
    "rest": "",
    "out": "assets/objects/characters/HumanBase.morf",
    "only": "",
    # ПОРОГ РАЗРЕЖЕННОСТИ, метры. Ниже 0.1 мм сдвиг не виден ни в кадре, ни
    # судье, а в файле стоит 16 байт.
    "epsilon": "0.0001",
    "dump-diag": "",
    "glb": "assets/objects/characters/HumanBase.glb",
    "dfo": "assets/objects/characters/HumanBase.dfo",
}


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
    if not opt["rest"]:
        raise SystemExit("--rest <rest.bin> обязателен (dfn_morph rest ...)")
    return opt


def log(*a):
    print("[targets]", *a)
    sys.stdout.flush()


# ------------------------------------------------- 1. НАБОР ПОЛЗУНКОВ --------
#
# Одиннадцать ручек — те же, что были на манекене, потому что их знают экран
# создания (chargen_describe), народы (.people) и пресеты. Каждая — либо
# именованная цель MakeHuman, либо его макро-параметр; ни одна не крутится «на
# глаз», и ход каждой виден судье dfn_human_scale.
#
# ПОЛЯ:
#   kind    "macro" (решётка готовых тел: дельта — разность двух выпечек) либо
#           "target" (именованные файлы .target MakeHuman).
#   two     двусторонняя ли ручка. У двусторонней дельта — ПОЛУРАЗНОСТЬ концов
#           ((hi − lo)/2), полоса [-1, +1], нейтраль ровно посередине: −1 даёт
#           «тощего», а не «минус толстого».
#   amount  доля хода цели, которую берёт единица веса (у длин — «чуть-чуть»,
#           длина звена — то, что судья мерит первым).
#   range   полоса значений ползунка. ИЗМЕРЕНА, а не придумана: двоичный
#           поиск судьёй dfn_human_scale от baseline нейтрали (5 % суставы,
#           15 % силуэт), tools/check_morph_bands.py проверяет её на приёмке.
#           Где полосы нет, ручка ходит весь ход.
#
# НЕЙТРАЛЬ — ОДОБРЕННОЕ ТЕЛО (make_human_body.DEFAULTS): gender 1.0, age 0.6,
# muscle 0.7, weight 0.55, proportions 0.7, stature 0.62. Концы макро-ручек
# берутся симметрично вокруг неё, где ход MakeHuman позволяет.

SLIDERS = [
    dict(name="weight", kind="macro", two=True, key="weight", lo=0.30, hi=0.80,
         note="полнота: от сухого к плотному"),
    dict(name="muscle", kind="macro", two=True, key="muscle", lo=0.45, hi=0.95,
         note="мускулатура"),
    dict(name="age", kind="macro", two=False, key="age", lo=0.6, hi=0.95,
         note="возраст: сутулость и оплывший силуэт"),
    dict(name="belly", kind="target", two=False,
         hi_targets=("stomach-pregnant-incr",),
         note="вынос живота"),
    dict(name="shoulders", kind="target", two=True,
         hi_targets=("measure-shoulder-dist-incr",),
         lo_targets=("measure-shoulder-dist-decr",),
         note="ширина плеч"),
    dict(name="deltoid", kind="target", two=True,
         hi_targets=("l-upperarm-shoulder-muscle-incr",
                     "r-upperarm-shoulder-muscle-incr"),
         lo_targets=("l-upperarm-shoulder-muscle-decr",
                     "r-upperarm-shoulder-muscle-decr"),
         note="масса плечевого пояса"),
    dict(name="hips", kind="target", two=True,
         hi_targets=("hip-scale-horiz-incr",), lo_targets=("hip-scale-horiz-decr",),
         note="ширина таза"),
    dict(name="buttocks", kind="target", two=True,
         hi_targets=("buttocks-volume-incr",), lo_targets=("buttocks-volume-decr",),
         note="объём ягодиц"),
    dict(name="torso-depth", kind="target", two=True,
         hi_targets=("torso-scale-depth-incr",), lo_targets=("torso-scale-depth-decr",),
         note="глубина туловища"),
    dict(name="arm-length", kind="target", two=True, amount=0.35,
         hi_targets=("measure-upperarm-length-incr", "measure-lowerarm-length-incr"),
         lo_targets=("measure-upperarm-length-decr", "measure-lowerarm-length-decr"),
         note="длина руки (морф двигает меш, не суставы — потому 0.35 хода)"),
    # ДЛИНА НОГ — НЕ ПЕЧЁТСЯ, И ЭТО ЗАМЕР, А НЕ ЛЕНЬ (то же, что было сказано
    # о росте). Цели measure-upperleg/lowerleg-height готовы и на теле MPFB
    # двигают 51 мм на единице; но морф двигает МЕШ, а не СУСТАВЫ: подошва
    # уезжает, лодыжка стоит, и судья держит ручку в полосе [-0.078, +0.078]
    # хода (упор — «ankle height», двоичный поиск 02.09) — четыре миллиметра
    # подошвы на весь ход ползунка, то есть ползунок, который ничего не делает.
    # Длина ног, как и рост, — ручка СКЕЛЕТА (суставы обязаны ехать вместе с
    # мешем); это другой механизм, а не другая цель. Ход к шагу 2; вернуть
    # строку, когда скелет поедет вместе с мешем.
    # dict(name="leg-length", kind="target", two=True, amount=0.35,
    #      hi_targets=("measure-upperleg-height-incr", "measure-lowerleg-height-incr"),
    #      lo_targets=("measure-upperleg-height-decr", "measure-lowerleg-height-decr"),
    #      note="длина ноги"),
]

# ПОЛОСЫ, ИЗМЕРЕННЫЕ НА ТЕЛЕ MPFB (86500748). Двоичный поиск по ходу цели
# судьёй dfn_human_scale от baseline нейтрали (5 % суставы, 15 % силуэт);
# где строки нет — весь ход. В скобках — во что ручка упирается: самое полезное
# число здесь. Пересчитываются, когда тело или судья меняются; тест
# morph_bands ловит день, когда они перестанут быть верными.
RANGES = {
    "age": (0.0, 0.50),          # замер 0.523: головы на фигуру (сутулость опускает макушку)
    "belly": (0.0, 0.35),        # замер 0.359: глубина на пупке — свой же ориентир
    "hips": (-0.75, 0.85),       # замер −0.797 / +0.906: ширина таза по мешу
    "torso-depth": (-0.85, 0.85),  # замер −0.898 / +0.883: глубина груди
}


# ---------------------------------------------------- 2. ТЕЛО КАК В ИГРЕ -----

def load_body_module():
    """tools/make_human_body.py как модуль: те же функции, что печёт .glb."""
    path = os.path.join(ROOT, "tools", "make_human_body.py")
    spec = importlib.util.spec_from_file_location("make_human_body", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def mesh_coords(mesh):
    n = len(mesh.data.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    mesh.data.vertices.foreach_get("co", co)
    return co.reshape(n, 3)


def key_coords(key):
    n = len(key.data)
    co = np.empty(n * 3, dtype=np.float64)
    key.data.foreach_get("co", co)
    return co.reshape(n, 3)


def bake_mix(mesh):
    """MakeHuman лепит каждый параметр отдельным shape key; выпечь смесь в
    вершины — та же операция, что в make_human_body.make_body."""
    if mesh.data.shape_keys is None:
        return
    mixed = mesh.shape_key_add(name="mixed", from_mix=True)
    co = key_coords(mixed)
    mesh.shape_key_clear()
    mesh.data.vertices.foreach_set("co", co.reshape(-1))
    mesh.data.update()


def macro_of(mhb, opt):
    return {
        "gender": float(opt["gender"]), "age": float(opt["age"]),
        "muscle": float(opt["muscle"]), "weight": float(opt["weight"]),
        "proportions": float(opt["proportions"]), "height": float(opt["stature"]),
        "cupsize": 0.5, "firmness": 0.5,
        "race": {"asian": 0.0, "caucasian": 1.0, "african": 0.0},
    }


def grow_basemesh(mhb, macro):
    """Basemesh MakeHuman с макро-ручками, смесь выпечена. Те же флаги, что у
    make_human_body.make_body (detailed_helpers=True — иначе риг сядет по
    нейтральному телу)."""
    human = importlib.import_module(mhb.MPFB_PKG + ".services.humanservice").HumanService
    mesh = human.create_human(mask_helpers=True, detailed_helpers=True,
                              extra_vertex_groups=False, feet_on_ground=True,
                              scale=0.1, macro_detail_dict=macro)
    return mesh, human


def capture_targets(mhb, mesh, macro, sliders):
    """Дельты всех ручек на ПОЛНОМ basemesh (19 158 вершин), в локальных
    координатах меша (A-поза MakeHuman, метры)."""
    targets = importlib.import_module(mhb.MPFB_PKG + ".services.targetservice").TargetService
    neutral = mesh_coords(mesh)
    n = len(neutral)
    out = {}

    def named(names):
        total = np.zeros((n, 3), dtype=np.float64)
        for name in names:
            path = targets.target_full_path(name)
            if not path:
                raise SystemExit("цель MPFB не найдена: " + name)
            key = targets.load_target(mesh, path, weight=1.0, name="dfn_" + name)
            basis = mesh.data.shape_keys.reference_key
            d = key_coords(key) - key_coords(basis)
            moved = int((np.abs(d).max(axis=1) > 1e-7).sum())
            if moved == 0:
                raise SystemExit("цель \"%s\" не сдвинула НИ ОДНОЙ вершины — ОТКАЗ" % name)
            log("  %-40s %5d вершин, макс %.1f мм" % (name, moved, np.abs(d).max() * 1000.0))
            total += d
            mesh.shape_key_clear()
            mesh.data.vertices.foreach_set("co", neutral.reshape(-1))
            mesh.data.update()
        return total

    macro_cache = {}

    def macro_body(key, value):
        tag = (key, round(value, 6))
        if tag in macro_cache:
            return macro_cache[tag]
        m2 = json.loads(json.dumps(macro))
        m2[key] = value
        other, _ = grow_basemesh(mhb, m2)
        bake_mix(other)
        co = mesh_coords(other)
        if len(co) != n:
            raise SystemExit("макро-выпечка %s=%g: %d вершин против %d" % (key, value, len(co), n))
        # feet_on_ground у MPFB — положение ОБЪЕКТА, вершины в локальных
        # координатах; печатается на случай, если это когда-нибудь изменится.
        log("  macro %s=%.3f: объект на z=%.4f, подошва на z=%.4f"
            % (key, value, other.matrix_world.translation.z, co[:, 2].min()))
        bpy.data.objects.remove(other, do_unlink=True)
        macro_cache[tag] = co
        return co

    for spec in sliders:
        log("ручка %s (%s)" % (spec["name"], spec["note"]))
        if spec["kind"] == "macro":
            hi = macro_body(spec["key"], spec["hi"])
            if spec["two"]:
                lo = macro_body(spec["key"], spec["lo"])
                d = 0.5 * (hi - lo)
            else:
                d = hi - neutral
        else:
            hi = named(spec["hi_targets"])
            if spec["two"]:
                lo = named(spec["lo_targets"])
                d = 0.5 * (hi - lo)
            else:
                d = hi
        d = d * float(spec.get("amount", 1.0))
        out[spec["name"]] = d
        moved = int((np.abs(d).max(axis=1) > 1e-7).sum())
        if moved == 0:
            raise SystemExit("ручка \"%s\" не сдвинула НИ ОДНОЙ вершины — ОТКАЗ" % spec["name"])
        log("  = %d вершин, макс %.1f мм" % (moved, np.abs(d).max() * 1000.0))
    return out


ORIG_ATTR = "dfn_orig"


def stamp_indices(mesh):
    attr = mesh.data.attributes.new(ORIG_ATTR, "INT", "POINT")
    attr.data.foreach_set("value", list(range(len(mesh.data.vertices))))


def read_indices(mesh):
    attr = mesh.data.attributes[ORIG_ATTR]
    idx = np.empty(len(mesh.data.vertices), dtype=np.int64)
    attr.data.foreach_get("value", idx)
    return idx


def strip_helpers(mhb, mesh):
    """Срезка helper-геометрии и зубов — те же операции и в том же порядке,
    что в make_human_body.make_body."""
    mhb.activate(mesh)
    for mod in list(mesh.modifiers):
        if mod.type == "MASK":
            bpy.ops.object.modifier_apply(modifier=mod.name)
        elif mod.type == "ARMATURE":
            mesh.modifiers.remove(mod)
    mhb.drop_hidden(mesh)


def bone_weights(mesh, bone_names):
    """Веса скина на вершину по костям рига (после remap_weights)."""
    index = {g.index: g.name for g in mesh.vertex_groups}
    n = len(mesh.data.vertices)
    w = np.zeros((n, len(bone_names)), dtype=np.float64)
    col = {name: k for k, name in enumerate(bone_names)}
    for v in mesh.data.vertices:
        for g in v.groups:
            name = index.get(g.group)
            if name in col and g.weight > 0.0:
                w[v.index, col[name]] += g.weight
    return w


def lbs(points_or_deltas, weights, mats, translate):
    """Линейный скиннинг Blender: Σ w·M; при Σw < 1 остаток — без движения,
    при Σw > 1 — нормировка. `translate` — точки (с переносом) или дельты (без)."""
    n = len(points_or_deltas)
    total = weights.sum(axis=1)
    out = np.zeros((n, 3), dtype=np.float64)
    for k in range(weights.shape[1]):
        wk = weights[:, k]
        if not np.any(wk):
            continue
        R = mats[k][:3, :3]
        t = mats[k][:3, 3] if translate else np.zeros(3)
        out += wk[:, None] * (points_or_deltas @ R.T + t)
    over = total > 1.0
    out[over] /= total[over, None]
    under = total < 1.0
    out[under] += (1.0 - total[under])[:, None] * points_or_deltas[under]
    return out


# ------------------------------------------------ 3. ЧТЕНИЕ rest.bin ---------

def read_rest(path):
    """Рест-поза нашего скина: позиции, главный сустав каждой вершины,
    обратные линейные части блендов, индексы и суставы. Пишет `dfn_morph rest`."""
    with open(path, "rb") as f:
        buf = f.read()
    if buf[:4] != b"DFRS":
        raise SystemExit("%s: не rest.bin (магия)" % path)
    ver, vcount, icount = struct.unpack_from("<III", buf, 4)
    if ver != 1:
        raise SystemExit("%s: версия %d, знаю 1" % (path, ver))
    off = 16
    stride = 13
    words = np.frombuffer(buf, dtype="<u4", count=vcount * stride, offset=off)
    words = words.reshape(vcount, stride)
    floats = words.view("<f4")
    pos = floats[:, 0:3].astype(np.float64)
    inv = floats[:, 4:13].astype(np.float64).reshape(vcount, 3, 3).transpose(0, 2, 1)
    return pos.copy(), inv


# ------------------------------------------------ 4. ЧТЕНИЕ .glb -------------

def read_glb_positions(path):
    with open(path, "rb") as f:
        d = f.read()
    ln = struct.unpack_from("<I", d, 12)[0]
    js = json.loads(d[20:20 + ln])
    bo = 20 + ln + 8
    acc = js["accessors"]
    bv = js["bufferViews"]
    pos = []
    for mesh in js["meshes"]:
        for prim in mesh["primitives"]:
            a = acc[prim["attributes"]["POSITION"]]
            v = bv[a["bufferView"]]
            off = bo + v.get("byteOffset", 0) + a.get("byteOffset", 0)
            arr = np.frombuffer(d, dtype="<f4", count=a["count"] * 3, offset=off)
            pos.append(arr.reshape(a["count"], 3).astype(np.float64))
    return np.concatenate(pos)


def read_dfo_bind_positions(path):
    """Вершины SKIN выпечки .dfo в ПРОСТРАНСТВЕ ПРИВЯЗКИ, в порядке файла —
    то место, где лежат дельты, и то, по чему проверяется подобие .glb → .dfo."""
    with open(path, "rb") as f:
        d = f.read()
    off = 8
    while off < len(d):
        tag = d[off:off + 4]
        ln = struct.unpack_from("<Q", d, off + 6)[0]
        if tag == b"SKIN":
            body = d[off + 14:off + 14 + ln]
            # ObjectRegistry::write_skin_body: u32 вершин, вершины по 56 байт
            # (pos, normal, uv, color, 4×u8 joints, 4×f32 weights), u32
            # индексов, индексы.
            (vcount,) = struct.unpack_from("<I", body, 0)
            stride = 56
            (icount,) = struct.unpack_from("<I", body, 4 + vcount * stride)
            if 8 + vcount * stride + icount * 4 != len(body):
                raise SystemExit("%s: секция SKIN другой раскладки (%d вершин, %d индексов, %d байт)"
                                 % (path, vcount, icount, len(body)))
            raw = np.frombuffer(body, dtype="<f4", count=vcount * (stride // 4), offset=4)
            return raw.reshape(vcount, stride // 4)[:, 0:3].astype(np.float64)
        off += 14 + ln
    raise SystemExit("%s: секции SKIN нет" % path)


# ------------------------------------------------------------------ main ---

def main():
    opt = parse_args()
    mhb = load_body_module()
    body_opt = dict(mhb.DEFAULTS)
    only = [x for x in opt["only"].split(",") if x]
    sliders = [s for s in SLIDERS if not only or s["name"] in only]
    if not sliders:
        raise SystemExit("--only не оставил ни одной ручки")

    rest, inv_blend = read_rest(opt["rest"])
    glb_path = opt["glb"] if os.path.isabs(opt["glb"]) else os.path.join(ROOT, opt["glb"])
    dfo_path = opt["dfo"] if os.path.isabs(opt["dfo"]) else os.path.join(ROOT, opt["dfo"])
    glb = read_glb_positions(glb_path)
    bind = read_dfo_bind_positions(dfo_path)
    if len(glb) != len(rest) or len(bind) != len(rest):
        raise SystemExit("%s: %d вершин, .dfo: %d, rest.bin — %d: это не одно тело"
                         % (glb_path, len(glb), len(bind), len(rest)))
    log("rest.bin: %d вершин; .glb: %d вершин; .dfo: %d вершин" % (len(rest), len(glb), len(bind)))

    mhb.enable_addons()
    mhb.wipe()
    bpy.context.scene.render.fps = 30

    # 1. basemesh целиком, смесь макро выпечена — как в make_body
    macro = macro_of(mhb, body_opt)
    mesh, human = grow_basemesh(mhb, macro)
    targets = importlib.import_module(mhb.MPFB_PKG + ".services.targetservice").TargetService
    stack = [{"target": name, "value": float(body_opt[key])}
             for key, names in sorted(mhb.DETAIL_TARGETS.items())
             for name in names if float(body_opt[key]) != 0.0]
    if stack:
        targets.bulk_load_targets(mesh, stack)
    meta = human.add_builtin_rig(mesh, "rigify.human", import_weights=True)
    bake_mix(mesh)
    full = mesh_coords(mesh)
    log("basemesh %d вершин" % len(full))

    # 2. дельты ручек на полном basemesh
    deltas_full = capture_targets(mhb, mesh, macro, sliders)

    # 3. срезка helper'ов с протаскиванием индексов
    stamp_indices(mesh)
    strip_helpers(mhb, mesh)
    orig = read_indices(mesh)
    if len(set(orig.tolist())) != len(orig) or orig.max() >= len(full):
        raise SystemExit("индексы basemesh не протащились через срезку")
    log("тело %d вершин после срезки" % len(orig))
    body_rest = mesh_coords(mesh)
    if np.abs(body_rest - full[orig]).max() > 1e-6:
        raise SystemExit("срезка сдвинула вершины — карта индексов неверна")

    # 4. скелет и веса — как в main() make_human_body
    height = max((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
    spec = mhb.build_spec()
    rig = mhb.build_rig(meta, spec, height)
    mhb.remap_weights(mesh, meta, rig, spec)
    mhb.bind(mesh, rig)
    bpy.data.objects.remove(meta, do_unlink=True)
    mhb.decimate(mesh, int(body_opt["tris"]))
    bone_names = [b.name for b in rig.data.bones]
    weights = bone_weights(mesh, bone_names)

    # 5. покой по донору — та же запечка, что у тела в игре
    clips = os.path.join(ROOT, body_opt["clips"])
    before = {o.name for o in bpy.data.objects}
    bpy.ops.import_scene.gltf(filepath=clips)
    src_rig = None
    for o in bpy.data.objects:
        if o.name not in before and o.type == "ARMATURE":
            src_rig = o
    if src_rig is None:
        raise SystemExit("no armature in " + clips)
    old_rest = {n: np.array(rig.data.bones[n].matrix_local, dtype=np.float64) for n in bone_names}
    mhb.align_rest_to_donor(mesh, rig, src_rig)
    new_rest = {n: np.array(rig.data.bones[n].matrix_local, dtype=np.float64) for n in bone_names}
    for o in list(bpy.data.objects):
        if o.name not in before:
            bpy.data.objects.remove(o, do_unlink=True)
    mats = [new_rest[n] @ np.linalg.inv(old_rest[n]) for n in bone_names]

    # КОНТРОЛЬ ЗАПЕЧКИ: та же матрица обязана воспроизвести тело, которое
    # Blender запёк сам, — иначе дельты поедут не туда, куда поехало мясо.
    posed = mesh_coords(mesh)
    mine = lbs(body_rest, weights, mats, translate=True)
    err = np.linalg.norm(mine - posed, axis=1)
    log("запечка покоя: воспроизведена с невязкой макс %.3f мм, средняя %.4f мм"
        % (err.max() * 1000.0, err.mean() * 1000.0))
    if err.max() > 1e-3:
        raise SystemExit("матрицы запечки не совпали с Blender (%.3f мм)" % (err.max() * 1000.0))

    # 6. соответствие «наша вершина → копии в .glb» по координатам. Экспорт
    # y-up: (x, y, z) Blender → (x, z, −y); импортёр .dfo хранит свой порядок
    # вершин таким же, каким прочитал .glb (14 517, без сварки).
    yup = np.stack([posed[:, 0], posed[:, 2], -posed[:, 1]], axis=1)
    key_of = {}
    for i, p in enumerate(np.round(yup, 5)):
        key_of.setdefault((p[0], p[1], p[2]), []).append(i)
    glb_to_body = np.full(len(glb), -1, dtype=np.int64)
    for j, p in enumerate(np.round(glb, 5)):
        hits = key_of.get((p[0], p[1], p[2]))
        if hits is None or len(hits) != 1:
            # округление на границе: ищем ближайшую
            d = np.linalg.norm(yup - glb[j], axis=1)
            k = int(d.argmin())
            if d[k] > 2e-5:
                raise SystemExit("вершина .glb %d не нашла себя в теле (%.2f мм): "
                                 "это не то тело, что печёт make_human_body.py"
                                 % (j, d[k] * 1000.0))
            glb_to_body[j] = k
        else:
            glb_to_body[j] = hits[0]
    covered = len(set(glb_to_body.tolist()))
    worst = np.linalg.norm(yup[glb_to_body] - glb, axis=1).max()
    log("сверка с .glb: %d копий → %d уникальных вершин, невязка макс %.3f мм"
        % (len(glb), covered, worst * 1000.0))
    if covered != len(posed):
        raise SystemExit("в .glb нашлись не все вершины тела (%d из %d)" % (covered, len(posed)))

    # 7. ПОДОБИЕ .glb → БИНД .dfo, ИЗМЕРЕННОЕ, А НЕ ПЕРЕСКАЗАННОЕ. Импортёр
    # разворачивает модель на --yaw 180 (x и z меняют знак) и равномерно
    # масштабирует к росту; здесь масштаб и перенос подбираются МНК по всем
    # 14 517 парам «вершина .glb → та же вершина .dfo», а невязка печатается:
    # она же доказывает, что порядок вершин в .dfo — порядок .glb.
    R_yaw = np.diag([-1.0, 1.0, -1.0])
    g = glb @ R_yaw.T
    gc = g - g.mean(axis=0)
    bc = bind - bind.mean(axis=0)
    scale = float((gc * bc).sum() / (gc * gc).sum())
    shift = bind.mean(axis=0) - scale * g.mean(axis=0)
    fit_err = np.linalg.norm(scale * g + shift - bind, axis=1)
    log("подобие .glb → .dfo: yaw 180°, масштаб %.6f, перенос (%.4f, %.4f, %.4f), "
        "невязка макс %.3f мм" % (scale, shift[0], shift[1], shift[2], fit_err.max() * 1000.0))
    if fit_err.max() > 1e-3:
        raise SystemExit("подобие .glb → .dfo не сходится (%.3f мм): порядок вершин или "
                         "импорт другой" % (fit_err.max() * 1000.0))
    rest_h = rest[:, 1].max() - rest[:, 1].min()
    blend = np.linalg.inv(inv_blend)

    out_targets = []
    diag = []
    eps = float(opt["epsilon"])
    for spec in sliders:
        d_full = deltas_full[spec["name"]]
        d_body = d_full[orig]
        d_posed = lbs(d_body, weights, mats, translate=False)
        d_yup = np.stack([d_posed[:, 0], d_posed[:, 2], -d_posed[:, 1]], axis=1)
        d_bind = (d_yup[glb_to_body] @ R_yaw.T) * scale
        # бинд → рест: линейная часть бленда вершины (dfn_morph attach вернёт
        # обратно той же матрицей, и вершина ляжет туда, где лежит)
        d_rest = np.einsum("nij,nj->ni", blend, d_bind)
        length = np.linalg.norm(d_rest, axis=1)
        keep = np.nonzero(length > eps)[0]
        if len(keep) == 0:
            raise SystemExit("цель \"%s\" не сдвинула НИ ОДНОЙ вершины — ОТКАЗ" % spec["name"])
        floor = float(rest[:, 1].min())
        ys = (rest[keep, 1] - floor) / rest_h
        log("%-12s вершин %5d  макс %6.1f мм  полоса высот %.3f..%.3f"
            % (spec["name"], len(keep), length[keep].max() * 1000.0, ys.min(), ys.max()))
        diag.append((spec["name"], len(keep), float(length[keep].max()),
                     float(ys.min()), float(ys.max()), spec["note"]))
        default = (-1.0, 1.0) if spec["two"] else (0.0, 1.0)
        lo_v, hi_v = RANGES.get(spec["name"], default)
        out_targets.append((spec["name"], lo_v, hi_v,
                            [(int(i), d_rest[i]) for i in keep]))

    out_targets.sort(key=lambda t: t[0])
    path = opt["out"] if os.path.isabs(opt["out"]) else os.path.join(ROOT, opt["out"])
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"DFMF")
        f.write(struct.pack("<III", 1, len(rest), len(out_targets)))
        for name, lo_v, hi_v, deltas in out_targets:
            raw = name.encode("utf-8")
            f.write(struct.pack("<I", len(raw)))
            f.write(raw)
            f.write(struct.pack("<ffI", lo_v, hi_v, len(deltas)))
            for i, d in deltas:
                f.write(struct.pack("<Ifff", i, d[0], d[1], d[2]))
    total = sum(len(d) for _, _, _, d in out_targets)
    log("написано %s: %d целей, %d дельт, %.1f КБ"
        % (path, len(out_targets), total, os.path.getsize(path) / 1024.0))
    if opt["dump-diag"]:
        with open(opt["dump-diag"], "w", encoding="utf-8") as f:
            f.write("name\tverts\tmax_mm\ty_lo\ty_hi\tnote\n")
            for row in diag:
                f.write("%s\t%d\t%.2f\t%.4f\t%.4f\t%s\n"
                        % (row[0], row[1], row[2] * 1000.0, row[3], row[4], row[5]))


if __name__ == "__main__":
    main()
