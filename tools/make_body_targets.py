#!/usr/bin/env python3
#
# File: tools/make_body_targets.py
#
# Responsibility:
# - ПЕЧЁТ ПОЛЗУНКИ ТЕЛА: переносит именованные цели и макро-параметры
#   MPFB2/MakeHuman на ВЕРШИНЫ НАШЕГО HumanBase и кладёт их в переносной файл
#   целей (.morf), который dfn_morph attach вписывает в .dfo секцией MORF.
#   Тело у нас своё и эталонное (вердикт владельца 01.09), ползунки — чужие.
#
# Usage (headless; Blender живёт в скрэтчпаде сессии, постоянной установки в
# системе нет — это отдельная задача лида):
#     <scratchpad>/blender/Blender.app/Contents/MacOS/Blender --background \
#         --python tools/make_body_targets.py -- \
#         --rest <rest.bin> --out assets/objects/characters/HumanBase.morf
#   где rest.bin выдаёт `dfn_morph rest assets/objects/characters/HumanBase.dfo`.
#   Ключи: --only имя,имя (печь не весь набор), --smooth N, --quantum МЕТРЫ,
#   --guard МЕТРЫ (порог доверия к проекции), --dump-diag файл.
#
# ЧТО ЗДЕСЬ ПРОИСХОДИТ, В ПЯТИ СТРОКАХ.
#   1. MakeHuman лепится ДВАЖДЫ на каждый ползунок — на одном конце его полосы
#      и на другом (у односторонних — нейтраль и конец).
#   2. Оба тела сажаются в НАШУ рест-позу: подобие по СУСТАВАМ (Умеяма) плюс
#      разворот конечностей линейным блендингом по весам метарига, потому что
#      MakeHuman стоит с руками врозь, а наш человек — с руками вдоль тела.
#   3. Каждая НАША вершина проецируется на поверхность НЕЙТРАЛЬНОГО MakeHuman
#      один раз: запоминается треугольник и барицентрика.
#   4. Дельта = та же барицентрика на теле «hi» минус она же на теле «lo».
#      Средняя отстройка «наше тело против чужого» (замер ресёрчера 0.152 м) в
#      разность НЕ входит — она вычитается вместе с разницей стиля.
#   5. Дельта режется маской региона, сглаживается по НАШЕЙ топологии, УСРЕДНЯЕТСЯ
#      ПО ШВАМ и прореживается порогом.
#
# ПОЧЕМУ ПРОЕКЦИЯ СЧИТАЕТСЯ ОДИН РАЗ, А НЕ ПО РАЗУ НА ЦЕЛЬ. Все тела MakeHuman —
# одна топология (13380 вершин): барицентрика, снятая на нейтрали, годится для
# любого варианта. Это и дешевле, и ПРАВИЛЬНЕЕ: считай мы проекцию отдельно на
# «hi» и на «lo», разность двух РАЗНЫХ соответствий добавила бы к цели шум
# перепроецирования, который никакая маска не отличит от самой цели.
#
# ПОЧЕМУ МАСКА ОБЯЗАТЕЛЬНА, А НЕ «на всякий случай». Замер ресёрчера: у цели
# «живот» 46 вершин уезжали больше чем на сантиметр ВНЕ полосы туловища — на
# стопах, где наша топология легла на другое место MakeHuman. Ползунок живота,
# шевелящий стопу, — это не мелкий дефект, это ползунок, после которого судья
# пропорций красный, а причина не видна.
#
# ПОЧЕМУ ШВЫ УСРЕДНЯЮТСЯ. В .glb две примитивы (3389 + 5157 вершин), и на их
# стыке лежат СОВПАДАЮЩИЕ точки — одна точка тела, две вершины файла. Импортёр
# уже сваривает их при --reshape (tools/import_gltf.cpp, тем же квантом
# 1/20000 м); дельта обязана делать то же, иначе на ползунке между рукой и
# рукавом раскрывается волосяная щель фона.
#
# Dependencies:
# - Uses: bpy (Blender 4.2 LTS), расширение MPFB 2.0.17, аддон rigify; вход —
#   rest.bin от `dfn_morph rest`.
# - Used by: рука; результат потребляет `dfn_morph attach`.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ДЕТЕРМИНИРОВАН: те же ключи — тот же .morf до байта. Ни одного случайного
#   числа, ни одного обхода по множеству (везде отсортированные списки).
# - ЦЕЛЬ, НЕ СДВИНУВШАЯ НИ ОДНОЙ ВЕРШИНЫ, — ОТКАЗ, а не пустая запись. Ровно на
#   этом записка поймала чужой генератор ARKit: 52 цели, 0 сдвигов, «успех».

import importlib
import math
import os
import struct
import sys

import bpy  # type: ignore
import numpy as np  # type: ignore

PKG = "bl_ext.user_default.mpfb"

# ------------------------------------------------------------------ ключи ---

DEFAULTS = {
    "rest": "",
    "out": "assets/objects/characters/HumanBase.morf",
    "only": "",
    # СГЛАЖИВАНИЕ ДЕЛЬТЫ ПО НАШЕЙ ТОПОЛОГИИ, проходов Лапласа. Это «спад к
    # краю» из записки: дельта считается по геометрии и не знает, что на поясе
    # кончается одна деталь и начинается другая, — без сглаживания на стыке
    # видна складка. Два прохода: одного мало на квадратной сетке, четыре
    # заметно съедают саму цель (замер печатается ниже как «максимум сдвига»).
    "smooth": "2",
    # КВАНТ СВАРКИ ШВОВ, метры. 1/20000 — ровно тот, которым сваривает
    # импортёр (tools/import_gltf.cpp): два разных кванта дали бы две разные
    # группы швов, то есть щель ровно там, где её чинят.
    "quantum": "0.00005",
    # ДОКУДА МЫ ВЕРИМ ПРОЕКЦИИ, метры (на фигуре 1.75 м). Сеть ВТОРОГО эшелона:
    # анатомию сторожит соответствие частей тела (our_part / their_part), а этот
    # порог ловит остаток — внутренность рта, зубы, вершины без веса. Он ЩЕДРЫЙ
    # нарочно: замеренная невязка «наше тело против MakeHuman» и есть 4-13 см
    # (у ресёрчера через чужую обёртку выходило 15 см), и порог, поставленный по
    # ней, выключал бы не брак, а работу.
    "guard": "0.20",
    # ПОРОГ РАЗРЕЖЕННОСТИ, метры. Ниже 0.1 мм сдвиг не виден ни в кадре, ни
    # судье, а в файле стоит 16 байт.
    "epsilon": "0.0001",
    "dump-diag": "",
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
# Одиннадцать ручек. Каждая — ИМЕНОВАННАЯ цель MakeHuman или его макро-параметр,
# то есть та же величина, которую называет анатомический обмер; ни одна не
# крутится «на глаз», и ход каждой виден судье dfn_human_scale.
#
# ПОЛЯ:
#   kind    "macro" (решётка готовых тел, дельта — разность двух выпечек) либо
#           "target" (именованные файлы .target MakeHuman).
#   two     двусторонняя ли ручка. У двусторонней дельта считается как
#           ПОЛУРАЗНОСТЬ концов ((hi − lo)/2), полоса [-1, +1] и нейтраль ровно
#           посередине: так линеаризация симметрична, а не завалена в одну
#           сторону, и −1 действительно даёт «тощего», а не «минус толстого».
#   band    (низ, верх, растушёвка) в ДОЛЯХ РОСТА от подошвы. Маска региона.
#   out_x   |x| ≥ доля роста: маска «снаружи туловища», для рук.
#   note    что это за ручка на человеческом языке (идёт в отчёт).
#
# ПОЛОСЫ РОСТА И ДЛИН НАРОЧНО УЗКИЕ. Судья держит суставы в ±5 %, силуэт в
# ±15 %; ползунок, которым можно выйти за полосу, — это ползунок, после
# которого приёмка красная, а виноват интерфейс. Числа полос — ЗДЕСЬ, потому
# что здесь они и проверяются приёмкой на крайних положениях (пункт 5 заказа).
#
# ОТ ЧЕГО ОТСЧИТЫВАЕТСЯ ПОЛОСА (перемер 01.09). Первый раз полосы мерились от
# КАНОНА, и тело под ними было прогнано через --fit-canon и --reshape. Владелец
# сравнил его с сырым ассетом и оставил сырой; сырое тело само лежит мимо
# канона (7.01 головы, пятнадцать строк), так что канон отверг бы и НЕЙТРАЛЬ —
# каждая полоса ужалась бы в ноль. Отсчёт теперь от BASELINE нейтрали
# (assets/objects/characters/HumanBase.scale.json), ШИРИНА полосы осталась
# канонической: 5 % на суставах, 15 % на силуэте. Результат перемера
# показателен: на своей нейтрали ползунки ходят ШИРЕ, чем ходили вокруг чужой
# — семь из одиннадцати проходят полный ход, и упор остался только у трёх.

MACRO_NEUTRAL = {
    "gender": 0.9, "age": 0.5385, "muscle": 0.40, "weight": 0.58,
    "proportions": 0.5, "height": 0.5, "cupsize": 0.5, "firmness": 0.5,
    "race": {"asian": 0.0, "caucasian": 1.0, "african": 0.0},
}

ALL_BUT_HANDS = ("head", "neck", "torso", "pelvis", "upperarm.L", "upperarm.R",
                 "forearm.L", "forearm.R", "thigh.L", "thigh.R", "shin.L", "shin.R",
                 "foot.L", "foot.R")
ARM_PARTS = ("upperarm.L", "upperarm.R", "forearm.L", "forearm.R", "hand.L", "hand.R")
LEG_PARTS = ("pelvis", "thigh.L", "thigh.R", "shin.L", "shin.R", "foot.L", "foot.R")

# ПОЛОСЫ (range) — НЕ ПРИДУМАНЫ, А ИЗМЕРЕНЫ. Каждая цель прогнана судьёй
# dfn_human_scale двоичным поиском по своему ходу (tools/check_morph_bands.py
# делает то же самое на приёмке): полоса — это ПОСЛЕДНЕЕ значение, на котором
# все девять суставных ориентиров и одиннадцать силуэтных ещё внутри канона.
# Где полосы нет, ручка проходит весь ход. В скобках — кто краснеет первым, и
# это самое полезное число здесь: оно говорит, во что ручка упирается.
SLIDERS = [
    dict(name="weight", kind="macro", two=True, key="weight", lo=0.30, hi=0.86,
         parts=ALL_BUT_HANDS,
         note="полнота: от сухого к плотному (полный ход: 01.09 упор в глубину "
              "груди снялся вместе с --reshape, который её и раздувал)"),
    dict(name="muscle", kind="macro", two=True, key="muscle", lo=0.12, hi=0.78,
         parts=ALL_BUT_HANDS,
         note="мускулатура (полный ход, 01.09)"),
    dict(name="age", kind="macro", two=False, key="age", lo=0.5385, hi=0.95,
         parts=ALL_BUT_HANDS, range=(0.0, 0.75),
         note="возраст: сутулость и оплывший силуэт (упирается в ГОЛОВЫ НА "
              "ФИГУРУ: сутулость опускает макушку, а сустав шеи стоит)"),
    dict(name="belly", kind="target", two=False,
         hi_targets=("stomach-pregnant-incr",),
         parts=("torso", "pelvis"), band=(0.545, 0.700, 0.055),
         range=(0.0, 0.43),
         note="вынос живота (упирается в СВОЙ же ориентир — глубину на пупке)"),
    dict(name="shoulders", kind="target", two=True,
         hi_targets=("measure-shoulder-dist-incr",),
         lo_targets=("measure-shoulder-dist-decr",),
         parts=("torso", "neck", "upperarm.L", "upperarm.R"),
         band=(0.640, 0.930, 0.060), note="ширина плеч"),
    dict(name="deltoid", kind="target", two=True,
         hi_targets=("l-upperarm-shoulder-muscle-incr",
                     "r-upperarm-shoulder-muscle-incr"),
         lo_targets=("l-upperarm-shoulder-muscle-decr",
                     "r-upperarm-shoulder-muscle-decr"),
         parts=("upperarm.L", "upperarm.R", "torso"),
         band=(0.640, 0.910, 0.055), note="масса плечевого пояса"),
    dict(name="hips", kind="target", two=True,
         hi_targets=("hip-scale-horiz-incr",), lo_targets=("hip-scale-horiz-decr",),
         parts=("pelvis", "torso", "thigh.L", "thigh.R"),
         band=(0.410, 0.640, 0.060), range=(-0.85, 0.85),
         note="ширина таза (упирается в СВОЙ ориентир — ширину таза по мешу, "
              "и теперь СИММЕТРИЧНО: нейтраль своя, а не чужая)"),
    dict(name="buttocks", kind="target", two=True,
         hi_targets=("buttocks-volume-incr",), lo_targets=("buttocks-volume-decr",),
         parts=("pelvis", "thigh.L", "thigh.R"),
         band=(0.400, 0.620, 0.055), note="объём ягодиц"),
    dict(name="torso-depth", kind="target", two=True,
         hi_targets=("torso-scale-depth-incr",), lo_targets=("torso-scale-depth-decr",),
         parts=("torso", "pelvis", "neck"),
         band=(0.420, 0.910, 0.070),
         note="глубина туловища (полный ход: +11 % к канону база даёт и сырая, "
              "но полоса теперь считается от НЕЁ, а не от канона)"),
    # ДЛИНЫ — «ЧУТЬ-ЧУТЬ» ПО ЗАКАЗУ, и величина этого «чуть-чуть» стоит в
    # amount: цель берётся не на единице, а на 0.35 своего хода. Длина звена —
    # это то, что судья мерит В ПЕРВУЮ ОЧЕРЕДЬ (девять суставных ориентиров
    # ±5 %), и полный ход именованной цели выводит из полосы сразу.
    dict(name="arm-length", kind="target", two=True, amount=0.35,
         hi_targets=("measure-upperarm-length-incr", "measure-lowerarm-length-incr"),
         lo_targets=("measure-upperarm-length-decr", "measure-lowerarm-length-decr"),
         parts=ARM_PARTS,
         note="длина руки (полный ход; прежний упор вниз был в длину кисти по "
              "канону, а кисть сырого тела мерится от своей же нейтрали)"),
    dict(name="leg-length", kind="target", two=True, amount=0.35,
         hi_targets=("measure-upperleg-height-incr", "measure-lowerleg-height-incr"),
         lo_targets=("measure-upperleg-height-decr", "measure-lowerleg-height-decr"),
         parts=LEG_PARTS, band=(0.000, 0.600, 0.050), note="длина ноги (узко)"),

    # РОСТ — НЕ ПЕЧЁТСЯ, И ЭТО ЗАМЕР, А НЕ ЛЕНЬ. Цель готова и работает (макро
    # `height` 0.40..0.60, максимум сдвига 80 мм), но судья пропускает её только
    # в полосе [-0.20, +0.04] хода — то есть ±1.6 см вниз и 3 мм вверх. Причина
    # структурная и стоит того, чтобы её назвать: МОРФ ДВИГАЕТ МЕШ, А НЕ
    # СУСТАВЫ. Высота головы у судьи — это «макушка меша минус сустав шеи»;
    # подними макушку на 7 см, не тронув сустав, и голова станет вдвое выше, а
    # на кадре она поедет отдельно от черепа при первом же клипе. Ползунок роста
    # обязан двигать СКЕЛЕТ (то, что делает --fit-canon у импортёра), и это
    # другой механизм, а не другая цель. Ход к шагу 2; включить строку и снять
    # range, как только скелет поедет вместе с мешем.
    # dict(name="stature", kind="macro", two=True, key="height", lo=0.40, hi=0.60,
    #      range=(-0.20, 0.04), note="рост в узкой полосе канона"),
]

# -------------------------------------- 2. КОСТИ: НАШИ ИМЕНА И ИМЕНА MPFB ----
#
# Соответствие ровно то же, что у tools/make_human_body.py: наш скелет — копия
# прежнего по именам, метариг MPFB — источник геометрии.

TRUNK_FIT = [  # (наш сустав, кость метарига) — по ГОЛОВЕ кости
    ("DEF-hips", "spine"),
    ("DEF-spine.001", "spine.001"),
    ("DEF-spine.002", "spine.002"),
    ("DEF-spine.003", "spine.003"),
    ("DEF-neck", "spine.005"),
    ("DEF-head", "spine.006"),
    ("DEF-shoulder.L", "shoulder.L"),
    ("DEF-shoulder.R", "shoulder.R"),
    ("DEF-thigh.L", "thigh.L"),
    ("DEF-thigh.R", "thigh.R"),
]

# Цепи, которые доворачиваются под нашу позу. Порядок — родитель раньше ребёнка.
LIMB_CHAINS = [
    [("upper_arm.%s", "DEF-upper_arm.%s", "DEF-forearm.%s"),
     ("forearm.%s", "DEF-forearm.%s", "DEF-hand.%s"),
     ("hand.%s", "DEF-hand.%s", "DEF-f_middle.01.%s")],
    [("thigh.%s", "DEF-thigh.%s", "DEF-shin.%s"),
     ("shin.%s", "DEF-shin.%s", "DEF-foot.%s"),
     ("foot.%s", "DEF-foot.%s", "DEF-toe.%s")],
]

# Куда лить веса групп, у которых своей кости в цепи нет. Пальцы едут с кистью,
# носок — со стопой, скручивающие звенья — со своим родителем: доворот цепи
# обязан двигать ВСЮ конечность, иначе кисть отрывается от предплечья.
def group_to_bone(group, chain_bones):
    g = group
    if g.startswith("DEF-"):
        g = g[4:]
    for suffix in (".001", ".002", ".003"):
        if g.endswith(suffix):
            g = g[:-len(suffix)]
    if g in chain_bones:
        return g
    for side in ("L", "R"):
        if g.endswith("." + side):
            stem = g[:-2]
            if stem.startswith(("f_index", "f_middle", "f_pinky", "f_ring",
                                "thumb", "palm")):
                return "hand." + side
            if stem.startswith(("toe", "heel")):
                return "foot." + side
    return None


# ------------------------------------------------ 3. ЧТЕНИЕ rest.bin ---------

def read_rest(path):
    """Рест-поза нашего скина: позиции, ГЛАВНЫЙ СУСТАВ каждой вершины, обратные
    линейные части блендов, индексы и суставы. Пишет `dfn_morph rest`."""
    with open(path, "rb") as f:
        buf = f.read()
    if buf[:4] != b"DFRS":
        raise SystemExit("%s: не rest.bin (магия)" % path)
    ver, vcount, icount = struct.unpack_from("<III", buf, 4)
    if ver != 1:
        raise SystemExit("%s: версия %d, знаю 1" % (path, ver))
    off = 16
    stride = 13  # pos(3) + сустав(1) + обратная линейная часть(9)
    words = np.frombuffer(buf, dtype="<u4", count=vcount * stride, offset=off)
    words = words.reshape(vcount, stride)
    floats = words.view("<f4")
    pos = floats[:, 0:3].astype(np.float64)
    vjoint = words[:, 3].astype(np.int64)
    inv = floats[:, 4:13].astype(np.float64).reshape(vcount, 3, 3).transpose(0, 2, 1)
    off += vcount * stride * 4
    idx = np.frombuffer(buf, dtype="<u4", count=icount, offset=off).astype(np.int64)
    off += icount * 4
    (jcount,) = struct.unpack_from("<I", buf, off)
    off += 4
    joints = {}
    order = []
    for _ in range(jcount):
        (n,) = struct.unpack_from("<I", buf, off)
        off += 4
        name = buf[off:off + n].decode("utf-8")
        off += n
        joints[name] = np.array(struct.unpack_from("<3f", buf, off), dtype=np.float64)
        order.append(name)
        off += 12
    return pos.copy(), vjoint, inv, idx, joints, order


# ------------------------------------- ЧАСТИ ТЕЛА: ОБЩИЙ СЛОВАРЬ ДЛЯ ДВУХ ----
#
# ПОЧЕМУ СООТВЕТСТВИЕ ИЩЕТСЯ ВНУТРИ ЧАСТИ, А НЕ ПРОСТО «ПОБЛИЖЕ». Наш человек
# стоит с руками, прижатыми к бокам (замер: сустав кисти на |x| = 0.21 м,
# ровно под плечевым), и ближайшая точка чужой поверхности от вершины ладони —
# бедро, а не ладонь. Расстояние их не различает: и то и другое в паре
# сантиметров. Различают их ВЕСА СКИНА — они и есть ответ на вопрос «какая это
# часть тела», данный тем, кто модель привязывал.
#
# Стороны в имени части НАРОЧНО: левая ладонь не имеет права соответствовать
# правой, даже когда фигура симметрична, — иначе цель, асимметричная по
# построению, тихо зеркалится.

def our_part(name):
    n = name
    if n.startswith("DEF-"):
        n = n[4:]
    side = ""
    if n.endswith(".L") or n.endswith(".R"):
        side = "." + n[-1]
        n = n[:-2]
    if n in ("root", "hips"):
        return "pelvis"
    if n.startswith("spine"):
        return "torso"
    if n == "neck":
        return "neck"
    if n == "head":
        return "head"
    if n == "shoulder":
        return "torso"
    if n == "upper_arm":
        return "upperarm" + side
    if n == "forearm":
        return "forearm" + side
    if n == "hand" or n.startswith(("f_index", "f_middle", "f_pinky", "f_ring",
                                    "thumb", "palm")):
        return "hand" + side
    if n == "thigh":
        return "thigh" + side
    if n == "shin":
        return "shin" + side
    if n == "foot" or n.startswith(("toe", "heel")):
        return "foot" + side
    return "torso"


def their_part(name):
    """То же для метарига MPFB. Отдельная функция, а не общая с нашей: имена
    хребта у метарига на одну кость длиннее, и подмена одного словаря другим —
    это молчаливый сдвиг шеи на позвонок."""
    n = name
    if n.startswith("DEF-"):
        n = n[4:]
    for suffix in (".001", ".002", ".003"):
        if n.endswith(suffix) and not n.startswith("spine"):
            n = n[:-len(suffix)]
    side = ""
    if n.endswith(".L") or n.endswith(".R"):
        side = "." + n[-1]
        n = n[:-2]
    if n in ("spine", "pelvis"):
        return "pelvis"
    if n in ("spine.001", "spine.002", "spine.003", "spine.004", "breast"):
        return "torso"
    if n == "spine.005":
        return "neck"
    if n == "spine.006" or n.startswith(("face", "eye", "nose", "ear", "cheek",
                                         "brow", "lip", "jaw", "chin", "temple",
                                         "teeth", "tongue", "forehead", "lid")):
        return "head"
    if n in ("shoulder",) or n.startswith("shoulder"):
        return "torso"
    if n.startswith("upper_arm"):
        return "upperarm" + side
    if n.startswith("forearm"):
        return "forearm" + side
    if n.startswith(("hand", "f_index", "f_middle", "f_pinky", "f_ring", "thumb",
                     "palm")):
        return "hand" + side
    if n.startswith("thigh"):
        return "thigh" + side
    if n.startswith("shin"):
        return "shin" + side
    if n.startswith(("foot", "toe", "heel")):
        return "foot" + side
    return "torso"


# СОСЕДНИЕ ЧАСТИ. Вершина ровно на границе (локоть, запястье, пах) законно
# ложится и туда и сюда; запрет соседа сделал бы шов там, где его нет.
NEIGHBOURS = {
    "pelvis": ("torso", "thigh.L", "thigh.R"),
    "torso": ("pelvis", "neck", "upperarm.L", "upperarm.R"),
    "neck": ("torso", "head"),
    "head": ("neck",),
    "upperarm.L": ("torso", "forearm.L"), "upperarm.R": ("torso", "forearm.R"),
    "forearm.L": ("upperarm.L", "hand.L"), "forearm.R": ("upperarm.R", "hand.R"),
    "hand.L": ("forearm.L",), "hand.R": ("forearm.R",),
    "thigh.L": ("pelvis", "shin.L"), "thigh.R": ("pelvis", "shin.R"),
    "shin.L": ("thigh.L", "foot.L"), "shin.R": ("thigh.R", "foot.R"),
    "foot.L": ("shin.L",), "foot.R": ("shin.R",),
}


# ------------------------------------------------ 4. ТЕЛА MakeHuman ----------

def enable_addons():
    for mod in ("rigify", PKG):
        try:
            bpy.ops.preferences.addon_enable(module=mod)
        except Exception as exc:  # noqa: BLE001
            raise SystemExit("cannot enable %s: %s" % (mod, exc))


def wipe():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def shapekeyed_positions(ob):
    """Вершины С ПРИМЕНЁННЫМИ КЛЮЧАМИ ФОРМЫ, но БЕЗ модификаторов.

    Форма тела MakeHuman живёт целиком в ключах: базовая сетка — нейтральный
    манекен, и снятые с неё числа суть числа НЕ ТОГО тела. А вот граф
    зависимостей брать нельзя: на теле висит маска «Hide helpers», и
    вычисленный меш — это 13380 вершин из 19158, ПОДМНОЖЕСТВО с другой
    нумерацией. Барицентрика, снятая на одном теле и применённая к другому,
    держится ровно на том, что нумерация у всех тел ОДНА; подмножество с
    неявным отображением — самый дешёвый способ это сломать молча."""
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    out = co.reshape(n, 3).copy()
    keys = me.shape_keys
    if keys is None:
        return out
    blocks = list(keys.key_blocks)
    basis = np.empty(n * 3, dtype=np.float64)
    blocks[0].data.foreach_get("co", basis)
    basis = basis.reshape(n, 3)
    out = basis.copy()
    for kb in blocks[1:]:
        if kb.value == 0.0:
            continue
        kco = np.empty(n * 3, dtype=np.float64)
        kb.data.foreach_get("co", kco)
        out += (kco.reshape(n, 3) - basis) * kb.value
    return out


def body_triangles(ob):
    """Треугольники ТЕЛА, без вспомогательной геометрии. MPFB прячет помощников
    маской по группе `body` (13380 вершин из 19158); нам они мешают вдвойне —
    ближайшей поверхностью для нашей вершины оказалась бы не кожа, а habit
    одежды, натянутый вокруг неё."""
    body = ob.vertex_groups.get("body")
    if body is None:
        raise SystemExit("у тела MakeHuman нет группы `body` — не с чем сверять")
    inside = np.zeros(len(ob.data.vertices), dtype=bool)
    for v in ob.data.vertices:
        for ge in v.groups:
            if ge.group == body.index:
                inside[v.index] = True
    ob.data.calc_loop_triangles()
    tris = [list(t.vertices) for t in ob.data.loop_triangles
            if inside[t.vertices[0]] and inside[t.vertices[1]] and inside[t.vertices[2]]]
    return np.array(tris, dtype=np.int64), inside


def make_body(macro, targets):
    """Одно тело MakeHuman: макро плюс список (имя цели, значение)."""
    wipe()
    hs = importlib.import_module(PKG + ".services.humanservice").HumanService
    ob = hs.create_human(mask_helpers=True, detailed_helpers=False,
                         extra_vertex_groups=False, feet_on_ground=True,
                         scale=0.1, macro_detail_dict=macro)
    if targets:
        ts = importlib.import_module(PKG + ".services.targetservice").TargetService
        ts.bulk_load_targets(ob, [{"target": n, "value": v} for n, v in targets])
    return ob


# ------------------------------------------- 5. ПОСАДКА В НАШУ РЕСТ-ПОЗУ -----

def umeyama(src, dst):
    """Подобие src -> dst (поворот, масштаб, перенос) методом наименьших
    квадратов. Не «примерно по коробке»: коробка чужого тела включает его позу,
    и подгонка по ней сажает нашу фигуру на ЧУЖОЙ размах рук."""
    sm, dm = src.mean(0), dst.mean(0)
    sc, dc = src - sm, dst - dm
    cov = (dc.T @ sc) / len(src)
    u, s, vt = np.linalg.svd(cov)
    d = np.sign(np.linalg.det(u @ vt))
    r = u @ np.diag([1.0, 1.0, d]) @ vt
    var = (sc ** 2).sum() / len(src)
    scale = float((s * [1.0, 1.0, d]).sum() / var) if var > 1e-12 else 1.0
    t = dm - scale * (r @ sm)
    return r, scale, t


def rotation_between(a, b):
    """Кратчайший поворот, переводящий единичный a в единичный b."""
    v = np.cross(a, b)
    c = float(np.dot(a, b))
    s = float(np.linalg.norm(v))
    if s < 1e-12:
        if c > 0.0:
            return np.eye(3)
        # ровно противоположны: любой перпендикуляр годится
        axis = np.array([1.0, 0.0, 0.0])
        if abs(a[0]) > 0.9:
            axis = np.array([0.0, 1.0, 0.0])
        v = np.cross(a, axis)
        v /= np.linalg.norm(v)
        return -np.eye(3) + 2.0 * np.outer(v, v)
    k = np.array([[0.0, -v[2], v[1]], [v[2], 0.0, -v[0]], [-v[1], v[0], 0.0]])
    return np.eye(3) + k + k @ k * ((1.0 - c) / (s * s))


def limb_pose(bone_heads, bone_tails, weights, chain_bones, joints, points):
    """ЛИНЕЙНЫЙ БЛЕНДИНГ ПО ВЕСАМ МЕТАРИГА: доворачивает конечности чужого тела
    под НАШУ рест-позу.

    ЗАЧЕМ ЭТО ВООБЩЕ. MakeHuman стоит с руками врозь (замер: полуразмах 0.557
    при росте 1.73), наш человек в рест-позе — с руками вдоль тела (полуширина
    0.258, и это плечо, а не кисть). Ближайшая точка поверхности от нашей кисти
    на таком теле — не кисть, а рёбра: «длина руки» тянула бы за бок.

    ПОЧЕМУ НЕТОЧНОСТЬ ЭТОГО ДОВОРОТА ПОЧТИ НЕ ВРЕДИТ. Дельта — РАЗНОСТЬ двух
    тел, повёрнутых ОДНИМ И ТЕМ ЖЕ преобразованием: ошибка позы сокращается
    вместе с ним и остаётся только там, где решает, КАКОЙ вершине что
    соответствует, — то есть ровно там, ради чего доворот и делается."""
    out = points.copy()
    acc = {}
    for chain in LIMB_CHAINS:
        for side in ("L", "R"):
            parent_t = np.eye(4)
            for bone_fmt, head_j, tail_j in chain:
                bone = bone_fmt % side
                if bone not in bone_heads:
                    continue
                ours_h = joints.get(head_j % side)
                ours_t = joints.get(tail_j % side)
                if ours_h is None or ours_t is None:
                    continue
                head = parent_t[:3, :3] @ bone_heads[bone] + parent_t[:3, 3]
                cur = parent_t[:3, :3] @ (bone_tails[bone] - bone_heads[bone])
                nl = np.linalg.norm(cur)
                want = ours_t - ours_h
                wl = np.linalg.norm(want)
                if nl < 1e-9 or wl < 1e-9:
                    acc[bone] = parent_t
                    parent_t = acc[bone]
                    continue
                r = rotation_between(cur / nl, want / wl)
                m = np.eye(4)
                m[:3, :3] = r
                m[:3, 3] = head - r @ head
                total = m @ parent_t
                acc[bone] = total
                parent_t = total
    if not acc:
        return out
    # p' = Σ w_b · (T_b · p) + (1 − Σ w_b) · p
    moved = np.zeros_like(points)
    total_w = np.zeros(len(points))
    for bone, t in sorted(acc.items()):
        w = weights.get(bone)
        if w is None:
            continue
        hit = w > 0.0
        if not hit.any():
            continue
        p = points[hit]
        moved[hit] += (p @ t[:3, :3].T + t[:3, 3]) * w[hit, None]
        total_w[hit] += w[hit]
    keep = 1.0 - np.clip(total_w, 0.0, 1.0)
    return moved + points * keep[:, None]


# ------------------------------------------- 6. ПРОЕКЦИЯ И БАРИЦЕНТРИКА ------

def closest_on_triangles(points, point_part, verts, tris, tri_part, cell=0.05):
    """Для каждой точки — ближайший треугольник ЕЁ ЧАСТИ ТЕЛА и барицентрика.

    СВОЯ СЕТКА, А НЕ BVH БЛЕНДЕРА, по двум причинам. Первая: нам нужна НЕ
    ближайшая точка, а треугольник И барицентрика в НЁМ, чтобы ту же
    барицентрику снять с другого тела; вернуть точку и восстанавливать её
    обратным поиском значит писать ту же арифметику дважды. Вторая: поиск
    обязан идти ВНУТРИ ЧАСТИ ТЕЛА (см. our_part выше), а чужому дереву про
    наши части знать неоткуда."""
    a = verts[tris[:, 0]]
    b = verts[tris[:, 1]]
    c = verts[tris[:, 2]]
    centre = (a + b + c) / 3.0
    lo = np.minimum(verts.min(0), points.min(0)) - cell
    keys = np.floor((centre - lo) / cell).astype(np.int64)
    buckets = {}
    for i in range(len(tris)):
        buckets.setdefault((tri_part[i], keys[i, 0], keys[i, 1], keys[i, 2]),
                           []).append(i)

    ab = b - a
    ac = c - a
    d00 = (ab * ab).sum(1)
    d01 = (ab * ac).sum(1)
    d11 = (ac * ac).sum(1)
    denom = d00 * d11 - d01 * d01
    denom = np.where(np.abs(denom) < 1e-18, 1e-18, denom)

    def bary_dist(idx, p):
        ap = p - a[idx]
        d20 = (ab[idx] * ap).sum(1)
        d21 = (ac[idx] * ap).sum(1)
        v = (d11[idx] * d20 - d01[idx] * d21) / denom[idx]
        w = (d00[idx] * d21 - d01[idx] * d20) / denom[idx]
        # ЗАЖИМ В ТРЕУГОЛЬНИК. Проекция на ПЛОСКОСТЬ грани лежит вне её у
        # каждой второй пары «точка — соседний треугольник»; без зажима
        # ближайшей объявлялась бы дальняя грань с удачной плоскостью.
        v = np.clip(v, 0.0, 1.0)
        w = np.clip(w, 0.0, 1.0)
        ss = v + w
        over = ss > 1.0
        v = np.where(over, v / np.where(over, ss, 1.0), v)
        w = np.where(over, w / np.where(over, ss, 1.0), w)
        q = a[idx] + ab[idx] * v[:, None] + ac[idx] * w[:, None]
        return v, w, np.linalg.norm(q - p, axis=1)

    n = len(points)
    best_tri = np.zeros(n, dtype=np.int64)
    best_v = np.zeros(n)
    best_w = np.zeros(n)
    best_d = np.full(n, 1e30)
    pk = np.floor((points - lo) / cell).astype(np.int64)
    for i in range(n):
        parts = (point_part[i],) + NEIGHBOURS.get(point_part[i], ())
        cand = []
        radius = 1
        while not cand and radius <= 8:
            for part in parts:
                for dx in range(-radius, radius + 1):
                    for dy in range(-radius, radius + 1):
                        for dz in range(-radius, radius + 1):
                            cand.extend(buckets.get((part, pk[i, 0] + dx,
                                                     pk[i, 1] + dy,
                                                     pk[i, 2] + dz), ()))
            radius += 2
        if not cand:
            # ЧАСТИ НЕТ У ЧУЖОГО ТЕЛА ВОВСЕ — молчать нельзя, но и падать не за
            # что: вершина остаётся без соответствия, и её дельта будет нулевой.
            best_d[i] = 1e30
            continue
        idx = np.array(sorted(set(cand)), dtype=np.int64)
        p = np.repeat(points[i][None, :], len(idx), axis=0)
        v, w, d = bary_dist(idx, p)
        k = int(np.argmin(d))
        best_tri[i] = idx[k]
        best_v[i] = v[k]
        best_w[i] = w[k]
        best_d[i] = d[k]
    return best_tri, best_v, best_w, best_d


def sample(verts, tris, tri_idx, v, w):
    a = verts[tris[tri_idx, 0]]
    b = verts[tris[tri_idx, 1]]
    c = verts[tris[tri_idx, 2]]
    return a + (b - a) * v[:, None] + (c - a) * w[:, None]


# ---------------------------------------------- 7. МАСКА, СПАД, ШВЫ ---------

def smoothstep(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def region_mask(spec, rest, floor, height, our_vpart, nb):
    """МАСКА РЕГИОНА со спадом к краю: ЧАСТИ ТЕЛА плюс полоса высот.

    ЧАСТИ — ПЕРВЫЕ, и это главное отличие от полосы. Полоса высот у живота
    (0.52..0.80 роста) честно накрывает и предплечья, прижатые к бокам, — а
    предплечью от живота двигаться незачем. Часть тела названа ВЕСАМИ СКИНА,
    то есть тем же, чем движок гнёт эту вершину; полоса — геометрическая
    догадка поверх. Обе нужны: часть не отличает груди от паха, полоса — да.

    ГРАНИЦА ЧАСТИ РАЗМЫВАЕТСЯ ПО ТОПОЛОГИИ (Лаплас по рёбрам), а не оставляется
    ступенькой. Ступенька в маске — это ступенька в дельте, то есть складка на
    теле ровно там, где кончается ползунок: тот самый дефект «складка на поясе»,
    который записка назвала обязательной доводкой."""
    m = np.ones(len(rest))
    parts = spec.get("parts")
    if parts is not None:
        allowed = set(parts)
        hard = np.array([1.0 if p in allowed else 0.0 for p in our_vpart])
        m *= laplacian(hard[:, None], nb, 3)[:, 0]
    band = spec.get("band")
    if band is not None:
        lo, hi, fade = band
        y = (rest[:, 1] - floor) / height
        m *= smoothstep((y - lo) / max(fade, 1e-6))
        m *= smoothstep((hi - y) / max(fade, 1e-6))
    return m


def build_adjacency(idx, n):
    pairs = set()
    for t in range(0, len(idx) - 2, 3):
        a, b, c = int(idx[t]), int(idx[t + 1]), int(idx[t + 2])
        for u, v in ((a, b), (b, c), (c, a)):
            pairs.add((u, v))
            pairs.add((v, u))
    nb = [[] for _ in range(n)]
    for u, v in sorted(pairs):
        nb[u].append(v)
    return nb


def laplacian(delta, nb, passes):
    """Спад к краю по НАШЕЙ топологии. Тот же smoothstep, что у чужого
    генератора лицевых целей, только по рёбрам, а не по радиусу: радиус не
    знает, что рука и бок — соседи в пространстве и не соседи на теле."""
    out = delta
    for _ in range(passes):
        nxt = out.copy()
        for i, ring in enumerate(nb):
            if not ring:
                continue
            nxt[i] = 0.5 * out[i] + 0.5 * out[ring].mean(0)
        out = nxt
    return out


def weld_groups(rest, quantum):
    """Группы совпадающих вершин — те же, что сваривает импортёр."""
    keys = np.rint(rest / quantum).astype(np.int64)
    groups = {}
    for i in range(len(rest)):
        groups.setdefault(tuple(keys[i]), []).append(i)
    return [g for g in (groups[k] for k in sorted(groups)) if len(g) > 1]


# ------------------------------------------------------------------ main ----

def main():
    opt = parse_args()
    rest, vjoint, inv_blend, idx, joints, joint_order = read_rest(opt["rest"])
    floor = float(rest[:, 1].min())
    height = float(rest[:, 1].max() - floor)
    log("наше тело: %d вершин, рост %.4f м, %d суставов"
        % (len(rest), height, len(joints)))

    enable_addons()
    neutral_ob = make_body(MACRO_NEUTRAL, ())
    hs = importlib.import_module(PKG + ".services.humanservice").HumanService
    rig = hs.add_builtin_rig(neutral_ob, "rigify.human", import_weights=True)
    bone_heads = {b.name: np.array(b.head_local, dtype=np.float64)
                  for b in rig.data.bones}
    bone_tails = {b.name: np.array(b.tail_local, dtype=np.float64)
                  for b in rig.data.bones}
    chain_bones = {fmt % side for chain in LIMB_CHAINS for fmt, _, _ in chain
                   for side in ("L", "R")}
    # Веса метарига по вершинам — один раз: топология у всех вариантов одна.
    nverts = len(neutral_ob.data.vertices)
    weights = {b: np.zeros(nverts) for b in sorted(chain_bones)}
    gname = {g.index: g.name for g in neutral_ob.vertex_groups}
    unmapped = set()
    for v in neutral_ob.data.vertices:
        for ge in v.groups:
            name = gname.get(ge.group)
            if name is None:
                continue
            bone = group_to_bone(name, chain_bones)
            if bone is None:
                unmapped.add(name)
                continue
            weights[bone][v.index] += ge.weight
    log("групп без кости цепи: %d (туловище и голова — так и надо)" % len(unmapped))

    # Треугольники чужого тела — один раз, топология у всех вариантов общая.
    tris, in_body = body_triangles(neutral_ob)
    # ЧАСТЬ ТЕЛА У КАЖДОГО ЧУЖОГО ТРЕУГОЛЬНИКА — по главной группе его вершин.
    # Считается ЗДЕСЬ, а не в поиске: это свойство чужой модели, и пересчитывать
    # его на каждый запрос значило бы 8546 раз отвечать на один вопрос.
    vpart = ["torso"] * nverts
    vbest = np.full(nverts, -1.0)
    for v in neutral_ob.data.vertices:
        for ge in v.groups:
            name = gname.get(ge.group)
            if name is None or name in ("body", "HelperGeometry", "JointCubes"):
                continue
            if ge.weight > vbest[v.index]:
                vbest[v.index] = ge.weight
                vpart[v.index] = their_part(name)
    tri_part = []
    for t in tris:
        parts = [vpart[t[0]], vpart[t[1]], vpart[t[2]]]
        tri_part.append(max(set(parts), key=parts.count))
    from collections import Counter
    log("чужое тело: %d вершин базы, %d в теле, %d треугольников; части: %s"
        % (nverts, int(in_body.sum()), len(tris),
           ", ".join("%s=%d" % kv for kv in sorted(Counter(tri_part).items()))))
    our_vpart = [our_part(joint_order[j]) if 0 <= j < len(joint_order) else "torso"
                 for j in vjoint]
    log("наши части: %s"
        % ", ".join("%s=%d" % kv for kv in sorted(Counter(our_vpart).items())))

    def raw_body(macro, targets):
        ob = make_body(macro, targets)
        pts = shapekeyed_positions(ob)
        if len(pts) != nverts:
            raise SystemExit("тело выехало на %d вершин вместо %d — топология "
                             "вариантов обязана совпадать" % (len(pts), nverts))
        return pts

    neutral_raw = raw_body(MACRO_NEUTRAL, ())
    # Подобие считается ОДИН раз, по НЕЙТРАЛИ, и применяется ко всем телам:
    # иначе ползунок роста, честно поднявший макушку, был бы обратно ужат
    # собственной подгонкой и не делал бы ничего.
    src = []
    dst = []
    for ours, theirs in TRUNK_FIT:
        if ours in joints and theirs in bone_heads:
            src.append(bone_heads[theirs])
            dst.append(joints[ours])
    if len(src) < 4:
        raise SystemExit("подгонка по суставам: совпало только %d — мало" % len(src))
    rot, scale, trans = umeyama(np.array(src), np.array(dst))
    log("подобие по %d суставам: масштаб %.4f" % (len(src), scale))
    fit = lambda p: (p @ rot.T) * scale + trans  # noqa: E731

    heads_fit = {k: fit(v[None, :])[0] for k, v in bone_heads.items()}
    tails_fit = {k: fit(v[None, :])[0] for k, v in bone_tails.items()}

    def to_ours(raw):
        return limb_pose(heads_fit, tails_fit, weights, chain_bones, joints, fit(raw))

    neutral = to_ours(neutral_raw)
    flat = fit(neutral_raw)
    log("чужое тело в нашей раме: рост %.4f м; полуразмах до доворота %.3f, после %.3f, "
        "у нас %.3f" % (neutral[:, 1].max() - neutral[:, 1].min(),
                        float(np.abs(flat[:, 0]).max()),
                        float(np.abs(neutral[:, 0]).max()),
                        float(np.abs(rest[:, 0]).max())))

    tri_idx, bv, bw, dist = closest_on_triangles(rest, our_vpart, neutral, tris,
                                                 tri_part)
    guard = float(opt["guard"])
    trusted = dist <= guard
    log("проекция: средняя невязка %.4f м, медиана %.4f, доверено %d из %d (порог %.3f)"
        % (dist.mean(), float(np.median(dist)), int(trusted.sum()), len(rest), guard))
    # РАСКЛАДКА НЕВЯЗКИ ПО ВЫСОТЕ — прибор, а не украшение: одно среднее не
    # отличает «мы всюду на сантиметр толще» от «туловище легло идеально, а
    # кисти улетели», а лечится это разными вещами.
    yfrac = (rest[:, 1] - floor) / height
    for lo_b in (0.0, 0.2, 0.4, 0.6, 0.8):
        sel = (yfrac >= lo_b) & (yfrac < lo_b + 0.2)
        if sel.any():
            log("  высота %.1f..%.1f: вершин %4d, медиана %.4f, макс %.4f"
                % (lo_b, lo_b + 0.2, int(sel.sum()), float(np.median(dist[sel])),
                   float(dist[sel].max())))

    nb = build_adjacency(idx, len(rest))
    welds = weld_groups(rest, float(opt["quantum"]))
    log("швов (групп совпадающих вершин): %d" % len(welds))

    wanted = [s.strip() for s in opt["only"].split(",") if s.strip()]
    eps = float(opt["epsilon"])
    passes = int(opt["smooth"])
    out_targets = []
    diag = []
    for spec in SLIDERS:
        if wanted and spec["name"] not in wanted:
            continue
        amount = float(spec.get("amount", 1.0))
        if spec["kind"] == "macro":
            hi_macro = dict(MACRO_NEUTRAL, **{spec["key"]: spec["hi"]})
            lo_macro = dict(MACRO_NEUTRAL,
                            **{spec["key"]: spec["lo"] if spec["two"]
                               else MACRO_NEUTRAL[spec["key"]]})
            p_hi = to_ours(raw_body(hi_macro, ()))
            p_lo = to_ours(raw_body(lo_macro, ())) if spec["two"] else neutral
        else:
            hi_t = [(n, amount) for n in spec["hi_targets"]]
            p_hi = to_ours(raw_body(MACRO_NEUTRAL, hi_t))
            if spec["two"]:
                lo_t = [(n, amount) for n in spec["lo_targets"]]
                p_lo = to_ours(raw_body(MACRO_NEUTRAL, lo_t))
            else:
                p_lo = neutral
        half = 0.5 if spec["two"] else 1.0
        field = (sample(p_hi, tris, tri_idx, bv, bw)
                 - sample(p_lo, tris, tri_idx, bv, bw)) * half
        raw_worst = float(np.linalg.norm(field, axis=1).max())
        field[~trusted] = 0.0
        # ПОРЯДОК ВАЖЕН: сглаживание СНАЧАЛА, маска ПОТОМ. Наоборот — и Лаплас
        # растаскивает дельту обратно за край маски (замер: у таза полоса
        # уезжала с 0.410 на 0.323 роста), то есть приёмка «дельта не течёт из
        # региона» проверяла бы сглаживание, а не маску.
        if passes > 0:
            field = laplacian(field, nb, passes)
        mask = region_mask(spec, rest, floor, height, our_vpart, nb)
        field *= mask[:, None]
        for g in welds:
            field[g] = field[g].mean(0)
        length = np.linalg.norm(field, axis=1)
        keep = np.nonzero(length > eps)[0]
        if len(keep) == 0:
            raise SystemExit("цель \"%s\" не сдвинула НИ ОДНОЙ вершины — ОТКАЗ"
                             % spec["name"])
        ys = (rest[keep, 1] - floor) / height
        log("%-12s вершин %5d  макс %6.1f мм (сырьё %6.1f)  полоса высот %.3f..%.3f"
            % (spec["name"], len(keep), length[keep].max() * 1000.0,
               raw_worst * 1000.0, ys.min(), ys.max()))
        diag.append((spec["name"], len(keep), float(length[keep].max()),
                     float(ys.min()), float(ys.max()), spec["note"]))
        # РЕСТ -> в файл. Перевод в пространство привязки делает dfn_morph attach:
        # он и только он знает скелет, а знание скелета в двух местах — это два
        # разных ответа на «где стоит эта вершина».
        lo_v, hi_v = spec.get("range", (-1.0, 1.0) if spec["two"] else (0.0, 1.0))
        out_targets.append((spec["name"], lo_v, hi_v,
                            [(int(i), field[i]) for i in keep]))

    out_targets.sort(key=lambda t: t[0])
    path = opt["out"]
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


main()
