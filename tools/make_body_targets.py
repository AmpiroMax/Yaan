#!/usr/bin/env python3
#
# File: tools/make_body_targets.py
#
# Responsibility:
# - ПЕЧЁТ ПОЛЗУНКИ ТЕЛА И ЛИЦА ПРЯМЫМИ ЦЕЛЯМИ MPFB2: базовая модель игрока —
#   само тело MakeHuman (tools/make_human_body.py, решение владельца 02.09),
#   поэтому именованная цель MPFB2 (файл «индекс dx dy dz» на 19 158 вершин
#   basemesh) и макро-решётка MakeHuman ложатся на НАШИ вершины без обёртки,
#   проекции и масок: та же сетка, тот же индекс. Результат — переносной файл
#   целей (.morf), который dfn_morph attach вписывает в .dfo секцией MORF.
#   Ручки ТЕЛА названы здесь (SLIDERS), ручки ЛИЦА — манифестом
#   assets/characters/targets/face.targets (сессия 62): группа, цели MPFB,
#   полоса, подпись. Вместе с .morf пишутся МАСКИ ЛИЦА
#   (assets/characters/targets/face.masks): области судьи лица
#   (docs/research/FACE_CANON.md §1) в номерах вершин .dfo — маска области
#   есть объединение целей MPFB, которые её двигают.
#
# Usage (headless, Blender 5.2 + MPFB 2.0.17 в системе):
#     /Applications/Blender.app/Contents/MacOS/Blender --background \
#         --python tools/make_body_targets.py -- \
#         --rest <rest.bin> --out assets/objects/characters/HumanBase.morf
#   где rest.bin выдаёт `dfn_morph rest assets/objects/characters/HumanBase.dfo`
#   (рест-поза скина и матрица возврата на вершину). Ключи: --only имя,имя
#   (печь не весь набор), --epsilon МЕТРЫ (порог разреженности), --dump-diag,
#   --bands <face.bands> (калиброванные полосы лица, см. ниже).
#   БЕЗ BLENDER, чистым python3 — перезапись одних полос в готовом .morf:
#     python3 tools/make_body_targets.py --reband 1 --bands <face.bands> \
#         --out assets/objects/characters/HumanBase.morf
#   Дельты при этом не трогаются до байта; полосы лица приходят из файла
#   калибровки, который пишет tools/check_morph_bands.py --calibrate (двоичный
#   поиск судьями dfn_human_scale и dfn_face_scale). Круг «печь с полосами
#   манифеста → калибровать → перезаписать полосы» не требует второй выпечки.
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
#   модуль, assets/objects/characters/{UAL_Clips.glb,HumanBase.glb} (атрибут
#   _HM08_INDEX — номер вершины basemesh на каждой вершине .glb),
#   assets/characters/targets/face.targets; вход — rest.bin от `dfn_morph rest`.
# - Used by: рука; результат потребляет `dfn_morph attach` (CMake на сборке);
#   полосы меряет и калибрует tools/check_morph_bands.py; маски читает
#   dfn_face_scale (tools/check_face_scale.cpp); имена ручек лица теми же
#   правилами выводит engine/app/sources/FaceManifest.cpp.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ДЕТЕРМИНИРОВАН: те же ключи — тот же .morf до байта. Ни одного случайного
#   числа, ни одного обхода по множеству (везде отсортированные списки).
# - ЦЕЛЬ, НЕ СДВИНУВШАЯ НИ ОДНОЙ ВЕРШИНЫ, — ОТКАЗ, а не пустая запись. Ровно на
#   этом записка поймала чужой генератор ARKit: 52 цели, 0 сдвигов, «успех».
# - Тело здесь НЕ перепекается и не пишется: HumanBase.glb — одобренный ассет,
#   скрипт только читает его для сверки.
# - ИМЯ РУЧКИ ЛИЦА ВЫВОДИТСЯ ИЗ СТЕМА ЦЕЛИ (handle_name) и ровно тем же правилом
#   в FaceManifest.cpp: два правила — две копии; тест app_chargen сверяет, что
#   каждая строка манифеста находит свою цель MORF по имени.

import importlib
import importlib.util
import gzip
import json
import os
import re
import struct
import sys

import numpy as np  # type: ignore

try:
    import bpy  # type: ignore
except ImportError:  # --reband идёт чистым python3, без Blender
    bpy = None

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
    "face": "assets/characters/targets/face.targets",
    "masks": "assets/characters/targets/face.masks",
    # КАЛИБРОВАННЫЕ ПОЛОСЫ ЛИЦА (пишет check_morph_bands.py --calibrate). Пусто
    # — полосы манифеста. Файла нет — тоже полосы манифеста, и это сказано
    # вслух: полоса без калибровки — предложение, а не замер.
    "bands": "assets/characters/targets/face.bands",
    "reband": "",
}


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    opt = dict(DEFAULTS)
    i = 0
    while i < len(argv):
        key = argv[i].lstrip("-")
        if key not in opt:
            raise SystemExit("unknown option: " + argv[i])
        opt[key] = argv[i + 1]
        i += 2
    if not opt["rest"] and not opt["reband"]:
        raise SystemExit("--rest <rest.bin> обязателен (dfn_morph rest ...)")
    return opt


def abs_path(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


# ------------------------------------------------ 0. МАНИФЕСТ ЛИЦА -----------
#
# Строка: группа | ручка | цели MPFB | полоса lo hi | подпись RU | словесная пара
# (синтаксис — в шапке самого файла). Здесь из неё берётся ТОЛЬКО то, что
# нужно выпечке: цели по сторонам ручки и полоса; слова — дело экрана и
# локализации.

def handle_name(spec):
    """ИМЯ РУЧКИ ИЗ СПЕЦИФИКАЦИИ ЦЕЛЕЙ. «{l,r}-eye-scale-decr/incr» → «eye-scale»,
    «nose-point-down/up» → «nose-point», «head-oval» → «head-oval». Правило
    одно на экспортёр и на FaceManifest.cpp."""
    stem = spec.replace("{l,r}-", "")
    if "/" in stem:
        left = stem.split("/")[0]
        return left.rsplit("-", 1)[0]
    return stem


def face_target_names(spec):
    """Пары файлов целей по сторонам: (lo_targets, hi_targets). У одиночной цели
    lo пуст."""
    sides = ["l-", "r-"] if spec.startswith("{l,r}-") else [""]
    stem = spec.replace("{l,r}-", "")
    if "/" in stem:
        left, hi_suffix = stem.split("/")
        base, lo_suffix = left.rsplit("-", 1)
        lo = tuple(s + base + "-" + lo_suffix for s in sides)
        hi = tuple(s + base + "-" + hi_suffix for s in sides)
        return lo, hi
    return (), tuple(s + stem for s in sides)


def read_face_manifest(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            cols = [c.strip() for c in line.split("|")]
            if len(cols) != 6:
                raise SystemExit("%s: строка не из шести колонок: %s" % (path, line))
            lo_hi = cols[3].split()
            if len(lo_hi) != 2:
                raise SystemExit("%s: полоса «%s» не два числа" % (path, cols[3]))
            lo_t, hi_t = face_target_names(cols[2])
            name = handle_name(cols[2])
            lo_v, hi_v = float(lo_hi[0]), float(lo_hi[1])
            # ПАРА С ПОЛОСОЙ 0..hi (мешки, ямочка) — ОДНОСТОРОННЯЯ РУЧКА: нейтраль
            # на нуле, ход только в incr, decr-половина не печётся. Пара с
            # полосой lo<0<hi — двусторонняя, дельта — полуразность концов, как у
            # ручек тела (см. SLIDERS).
            two = bool(lo_t) and lo_v < 0.0
            spec = dict(name=name, kind="target", two=two,
                        hi_targets=hi_t, lo_targets=lo_t if two else (), group=cols[0],
                        note="лицо · %s · %s" % (cols[0], cols[1]),
                        lo=lo_v, hi=hi_v)
            if two and not (lo_v < 0.0 < hi_v):
                raise SystemExit("%s: у парной ручки %s полоса %s не содержит нуля"
                                 % (path, name, cols[3]))
            if not two and (lo_v != 0.0 or not hi_v > 0.0):
                raise SystemExit("%s: у односторонней ручки %s полоса %s не 0..hi"
                                 % (path, name, cols[3]))
            rows.append(spec)
    names = [r["name"] for r in rows]
    if len(set(names)) != len(names):
        dup = sorted({n for n in names if names.count(n) > 1})
        raise SystemExit("%s: имена ручек повторяются: %s" % (path, ", ".join(dup)))
    return rows


def read_face_bands(path):
    """Калиброванные полосы: строки «имя lo hi ...», остальное — комментарий."""
    out = {}
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#")[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 3:
                raise SystemExit("%s: строка «%s» — не «имя lo hi»" % (path, line))
            out[parts[0]] = (float(parts[1]), float(parts[2]))
    return out


# ------------------------------------------------ 0b. МАСКИ ЛИЦА -------------
#
# ОБЛАСТИ СУДЬИ ЛИЦА (docs/research/FACE_CANON.md §1): у лица нет суставов, и
# размечать точки руками не нужно — файл цели MPFB2 сам является маской
# области: он перечисляет ровно те вершины, которые двигает. Объединение всех
# l-eye-* есть левый глаз, corner1/corner2 — внутренний и внешний углы,
# mouth-angles — углы рта. Список ниже — это список ПРЕФИКСОВ файлов целей.

FACE_REGIONS = [
    ("eye-l", ("l-eye-",)),
    ("eye-r", ("r-eye-",)),
    ("eye-corner-inner-l", ("l-eye-corner1-",)),
    ("eye-corner-outer-l", ("l-eye-corner2-",)),
    ("eye-corner-inner-r", ("r-eye-corner1-",)),
    ("eye-corner-outer-r", ("r-eye-corner2-",)),
    ("eyelid-l", ("l-eye-height1-", "l-eye-height2-", "l-eye-height3-")),
    ("eyelid-r", ("r-eye-height1-", "r-eye-height2-", "r-eye-height3-")),
    ("mouth-angles", ("mouth-angles-",)),
    ("lip-upper", ("mouth-upperlip-",)),
    ("lip-lower", ("mouth-lowerlip-",)),
    ("nostrils", ("nose-nostrils-width-",)),
    ("nose-point", ("nose-point-down", "nose-point-up")),
    ("nose-base", ("nose-base-",)),
    ("ears", ("l-ear-", "r-ear-")),
]

# ТЕЛО BASEMESH БЕЗ ПОМОЩНИКОВ: вершины с этого номера — helper-геометрия
# MakeHuman (одежда, волосы, глаза-заглушки), их нет в .glb.
BASEMESH_BODY_VERTS = 13380

# ПОЛ ЛИЦЕВОЙ ЦЕЛИ, доля роста: ниже него дельта ручки лица ОТБРАСЫВАЕТСЯ.
# Причина, а не величина (правило 36): в файлах целей MPFB встречаются
# ЧУЖИЕ индексы — mouth-cupidsbow и chin-width двигают вершину 4746 на
# 0.505 роста (бедро) на 0.13 мм. Шею (0.82–0.86 роста) лицо трогать вправе:
# высота подбородка честно тянет горло; торс — нет. 0.80 — ниже основания
# шеи (плечи 0.818H по канону) и выше всего, что лицо двигает по праву.
# Что отброшено — печатается: это паспорт утечки, а не молчаливая чистка.
FACE_FLOOR_FRAC = 0.80


def target_index_of(mpfb_targets_dir):
    """Стем файла цели → путь, по всем категориям каталога MPFB."""
    index = {}
    for cat in sorted(os.listdir(mpfb_targets_dir)):
        d = os.path.join(mpfb_targets_dir, cat)
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.endswith(".target.gz"):
                index[f[:-len(".target.gz")]] = os.path.join(d, f)
    return index


def target_moved_vertices(path):
    """Номера вершин basemesh, которые цель сдвигает (нулевые строки не в
    счёт: нулевой сдвиг — не движение)."""
    out = []
    with gzip.open(path, "rt") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 4 or parts[0].startswith("#"):
                continue
            if float(parts[1]) == 0.0 and float(parts[2]) == 0.0 and float(parts[3]) == 0.0:
                continue
            out.append(int(parts[0]))
    return out


def read_glb_hm08(path):
    """Атрибут _HM08_INDEX на каждой вершине .glb: номер вершины basemesh."""
    with open(path, "rb") as f:
        d = f.read()
    ln = struct.unpack_from("<I", d, 12)[0]
    js = json.loads(d[20:20 + ln])
    bo = 20 + ln + 8
    out = []
    for mesh in js["meshes"]:
        for prim in mesh["primitives"]:
            if "_HM08_INDEX" not in prim["attributes"]:
                raise SystemExit("%s: у меша нет атрибута _HM08_INDEX" % path)
            a = js["accessors"][prim["attributes"]["_HM08_INDEX"]]
            v = js["bufferViews"][a["bufferView"]]
            off = bo + v.get("byteOffset", 0) + a.get("byteOffset", 0)
            dtype = {5121: "<u1", 5123: "<u2", 5125: "<u4", 5126: "<f4",
                     5122: "<i2", 5120: "<i1"}[a["componentType"]]
            arr = np.frombuffer(d, dtype=dtype, count=a["count"], offset=off)
            out.append(arr.astype(np.int64))
    return np.concatenate(out)


def build_face_masks(mpfb_targets_dir, hm08):
    """Область → отсортированные номера вершин .dfo (все копии по швам)."""
    index = target_index_of(mpfb_targets_dir)
    copies = {}
    for j, b in enumerate(hm08.tolist()):
        copies.setdefault(b, []).append(j)
    masks = []
    for region, prefixes in FACE_REGIONS:
        base = set()
        files = 0
        for stem, path in index.items():
            if any(stem.startswith(p) for p in prefixes):
                files += 1
                base.update(i for i in target_moved_vertices(path) if i < BASEMESH_BODY_VERTS)
        if files == 0 or not base:
            raise SystemExit("маска «%s»: ни одного файла цели с префиксами %s"
                             % (region, prefixes))
        verts = sorted(j for b in base for j in copies.get(b, []))
        if not verts:
            raise SystemExit("маска «%s»: ни одна вершина basemesh не нашлась в .glb" % region)
        masks.append((region, len(base), files, verts))
    return masks


def write_face_masks(path, masks, glb_verts):
    with open(path, "w", encoding="utf-8") as f:
        f.write("# МАСКИ ОБЛАСТЕЙ ЛИЦА для судьи dfn_face_scale (docs/research/FACE_CANON.md §1).\n"
                "# Пишет tools/make_body_targets.py; маска области = объединение целей MPFB,\n"
                "# которые её двигают, в НОМЕРАХ ВЕРШИН .dfo (порядок .glb, %d вершин; копии\n"
                "# по швам UV входят все). Строка: <область>: <номер> <номер> ...\n"
                % glb_verts)
        f.write("verts %d\n" % glb_verts)
        for region, base_count, files, verts in masks:
            f.write("# %s: %d вершин basemesh из %d файлов целей, %d вершин .dfo\n"
                    % (region, base_count, files, len(verts)))
            f.write("%s: %s\n" % (region, " ".join(str(v) for v in verts)))


# ------------------------------------------------ 0c. ПЕРЕЗАПИСЬ ПОЛОС -------

def read_morf(path):
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] != b"DFMF":
        raise SystemExit("%s: не .morf (магия)" % path)
    ver, verts, count = struct.unpack_from("<III", d, 4)
    if ver != 1:
        raise SystemExit("%s: версия %d, знаю 1" % (path, ver))
    off = 16
    targets = []
    for _ in range(count):
        (n,) = struct.unpack_from("<I", d, off)
        off += 4
        name = d[off:off + n].decode("utf-8")
        off += n
        lo, hi, dn = struct.unpack_from("<ffI", d, off)
        off += 12
        raw = d[off:off + dn * 16]
        off += dn * 16
        targets.append([name, lo, hi, raw])
    if off != len(d):
        raise SystemExit("%s: хвост файла %d байт" % (path, len(d) - off))
    return verts, targets


def write_morf(path, verts, targets):
    targets = sorted(targets, key=lambda t: t[0])
    with open(path, "wb") as f:
        f.write(b"DFMF")
        f.write(struct.pack("<III", 1, verts, len(targets)))
        for name, lo_v, hi_v, raw in targets:
            enc = name.encode("utf-8")
            f.write(struct.pack("<I", len(enc)))
            f.write(enc)
            f.write(struct.pack("<ffI", lo_v, hi_v, len(raw) // 16))
            f.write(raw)


def apply_bands(targets, bands, manifest_rows):
    """Полосы: у ручки лица — калиброванная, если есть, иначе манифестная;
    калибровка ОБЯЗАНА лежать внутри манифестной (сужать, не расширять)."""
    by_name = {r["name"]: r for r in manifest_rows}
    changed = 0
    for t in targets:
        row = by_name.get(t[0])
        if row is None:
            continue
        lo_v, hi_v = row["lo"], row["hi"]
        if t[0] in bands:
            b_lo, b_hi = bands[t[0]]
            if b_lo < lo_v - 1e-6 or b_hi > hi_v + 1e-6 or not (b_lo < b_hi):
                raise SystemExit("калибровка %s [%g, %g] шире манифеста [%g, %g] или пуста"
                                 % (t[0], b_lo, b_hi, lo_v, hi_v))
            lo_v, hi_v = b_lo, b_hi
        if (t[1], t[2]) != (lo_v, hi_v):
            changed += 1
        t[1], t[2] = lo_v, hi_v
    return changed


def reband(opt):
    path = abs_path(opt["out"])
    verts, targets = read_morf(path)
    rows = read_face_manifest(abs_path(opt["face"]))
    bands = read_face_bands(abs_path(opt["bands"])) if opt["bands"] else {}
    changed = apply_bands(targets, bands, rows)
    write_morf(path, verts, targets)
    log("полосы перезаписаны: %s, %d целей, %d полос сменились, калибровка: %s"
        % (path, len(targets), changed,
           ("%d ручек из %s" % (len(bands), opt["bands"])) if bands else "нет"))


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
    if opt["reband"]:
        reband(opt)
        return
    if bpy is None:
        raise SystemExit("выпечка целей идёт под Blender (см. Usage); без него — только --reband")
    mhb = load_body_module()
    body_opt = dict(mhb.DEFAULTS)
    only = [x for x in opt["only"].split(",") if x]
    # ЛИЦО — ИЗ МАНИФЕСТА, ТЕЛО — ОТСЮДА. Один список ручек на выпечку, чтобы
    # .morf был ОДНИМ файлом: dfn_morph attach заменяет секцию целиком.
    face_rows = read_face_manifest(abs_path(opt["face"])) if opt["face"] else []
    face_names = {r["name"] for r in face_rows}
    for s in SLIDERS:
        if s["name"] in face_names:
            raise SystemExit("ручка «%s» названа и телом, и манифестом лица" % s["name"])
    sliders = [s for s in SLIDERS + face_rows if not only or s["name"] in only]
    if not sliders:
        raise SystemExit("--only не оставил ни одной ручки")
    log("ручек: %d тела + %d лица (манифест %s)"
        % (sum(1 for s in sliders if s["name"] not in face_names),
           sum(1 for s in sliders if s["name"] in face_names), opt["face"]))

    rest, inv_blend = read_rest(opt["rest"])
    glb_path = abs_path(opt["glb"])
    dfo_path = abs_path(opt["dfo"])
    glb = read_glb_positions(glb_path)
    bind = read_dfo_bind_positions(dfo_path)
    if len(glb) != len(rest) or len(bind) != len(rest):
        raise SystemExit("%s: %d вершин, .dfo: %d, rest.bin — %d: это не одно тело"
                         % (glb_path, len(glb), len(bind), len(rest)))
    log("rest.bin: %d вершин; .glb: %d вершин; .dfo: %d вершин" % (len(rest), len(glb), len(bind)))
    hm08 = read_glb_hm08(glb_path)
    if len(hm08) != len(glb):
        raise SystemExit("%s: _HM08_INDEX на %d вершинах из %d" % (glb_path, len(hm08), len(glb)))

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
    # ДВА ПУТИ К НОМЕРУ BASEMESH ОБЯЗАНЫ СОЙТИСЬ: атрибут _HM08_INDEX, которым
    # .glb несёт номер сам, и карта «координата → вершина» этого скрипта. Маски
    # лица пишутся по атрибуту, дельты — по карте; разойдись они, судья мерил
    # бы не те вершины, которые двигает ручка.
    mismatch = int((orig[glb_to_body] != hm08).sum())
    if mismatch != 0:
        raise SystemExit("_HM08_INDEX расходится с картой координат на %d вершинах .glb" % mismatch)
    log("_HM08_INDEX сверен с картой координат: %d вершин, расхождений 0" % len(glb))

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
    floor = float(rest[:, 1].min())
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
        if spec["name"] in face_names:
            below = keep[(rest[keep, 1] - floor) / rest_h < FACE_FLOOR_FRAC]
            if len(below) > 0:
                log("%-22s ОТБРОШЕНО ниже пола лица %.2fH: %d вершин, макс %.2f мм (чужие индексы в цели MPFB)"
                    % (spec["name"], FACE_FLOOR_FRAC, len(below), length[below].max() * 1000.0))
                keep = keep[(rest[keep, 1] - floor) / rest_h >= FACE_FLOOR_FRAC]
        if len(keep) == 0:
            raise SystemExit("цель \"%s\" не сдвинула НИ ОДНОЙ вершины — ОТКАЗ" % spec["name"])
        ys = (rest[keep, 1] - floor) / rest_h
        log("%-22s вершин %5d  макс %6.1f мм  полоса высот %.3f..%.3f"
            % (spec["name"], len(keep), length[keep].max() * 1000.0, ys.min(), ys.max()))
        diag.append((spec["name"], len(keep), float(length[keep].max()),
                     float(ys.min()), float(ys.max()), spec["note"]))
        default = (-1.0, 1.0) if spec["two"] else (0.0, 1.0)
        lo_v, hi_v = RANGES.get(spec["name"], default)
        raw = b"".join(struct.pack("<Ifff", int(i), float(d_rest[i][0]),
                                   float(d_rest[i][1]), float(d_rest[i][2]))
                       for i in keep)
        out_targets.append([spec["name"], lo_v, hi_v, raw])

    # ПОЛОСЫ ЛИЦА: манифест, суженный калибровкой судей (если файл есть).
    bands = read_face_bands(abs_path(opt["bands"])) if opt["bands"] else {}
    apply_bands(out_targets, bands, face_rows)
    log("полосы лица: %d ручек по манифесту, калибровка судьями: %s"
        % (len(face_rows), ("%d ручек из %s" % (len(bands), opt["bands"])) if bands
           else "НЕТ (файла калибровки нет — полосы манифеста, предложение)"))

    path = abs_path(opt["out"])
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    write_morf(path, len(rest), out_targets)
    total = sum(len(t[3]) // 16 for t in out_targets)
    log("написано %s: %d целей, %d дельт, %.1f КБ"
        % (path, len(out_targets), total, os.path.getsize(path) / 1024.0))

    # МАСКИ ЛИЦА — по атрибуту .glb, сверенному выше с картой координат.
    if face_rows and opt["masks"]:
        targets_dir = os.path.dirname(os.path.dirname(
            importlib.import_module(mhb.MPFB_PKG + ".services.targetservice")
            .TargetService.target_full_path("head-oval")))
        masks = build_face_masks(targets_dir, hm08)
        masks_path = abs_path(opt["masks"])
        write_face_masks(masks_path, masks, len(glb))
        log("маски лица: %s — %s" % (masks_path, ", ".join(
            "%s %d" % (r, len(v)) for r, _, _, v in masks)))

    if opt["dump-diag"]:
        with open(opt["dump-diag"], "w", encoding="utf-8") as f:
            f.write("name\tverts\tmax_mm\ty_lo\ty_hi\tnote\n")
            for row in diag:
                f.write("%s\t%d\t%.2f\t%.4f\t%.4f\t%s\n"
                        % (row[0], row[1], row[2] * 1000.0, row[3], row[4], row[5]))


if __name__ == "__main__":
    main()
