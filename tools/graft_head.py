#!/usr/bin/env python3
#
# File: tools/graft_head.py
#
# Responsibility:
# - ПЕРЕСАДКА ГОЛОВЫ НА HumanBase, ОДНИМ ВЫЗОВОМ И ЦЕЛИКОМ: срезает с тела
#   безликий овоид по выведенной линии, лепит на его же вершинах ЛИЦО по числам
#   docs/research/FACE_CANON.md, строит между телом и головой ШЕЮ-ПЕРЕХОДНИК,
#   сваривает швы в один меш, раздаёт веса (голова — DEF-head, шея — градиент
#   DEF-head/DEF-neck) и кладёт результат обратно в .glb со всеми 46 клипами.
#   Голова, выбранная владельцем в галерее 01.09 (own-sculpt), — это ЭТОТ ЖЕ
#   рецепт: скрипт artifacts/3D/heads/_tools/sculpt_ovoid.py перенесён сюда
#   целиком и дополнен тем, что владелец назвал кривым (посадка, шов, остаток
#   подбородка у туловища).
#
# Usage (headless; Blender живёт в скрэтчпаде сессии):
#     <blender> --background --python tools/graft_head.py -- \
#         --body assets/objects/characters/HumanBaseNoHead.glb \
#         --out  assets/objects/characters/HumanBase.glb
#   ВХОД — БЕЗГОЛОВЫЙ ИСХОДНИК, ВЫХОД — ДРУГОЙ ФАЙЛ, и это не вкусовщина.
#   Пересадка не идемпотентна по своей природе: подай ей уже пересаженное тело,
#   и она срежет НОВУЮ голову по той же линии и посадит на неё ещё одну. Поэтому
#   в git лежат оба: HumanBaseNoHead.glb — исходное тело Quaternius как есть
#   (лицензия QUATERNIUS_LICENSE.txt рядом), HumanBase.glb — результат этого
#   скрипта. Сторож ниже отказывает вслух, если на входе уже голова.
#   Ключи: --level N (дробление овоида, по умолчанию 3), --cuts N (колец лофта),
#   --measure (печатать таблицы мерок и не писать файл), --report FILE (json),
#   --head-glb FILE (положить одну голову отдельным файлом, для кадров).
#
# ДЕТЕРМИНИРОВАН: те же ключи — тот же .glb до вершины. Ни одного случайного
# числа; каждая кисть — строка с координатами в миллиметрах.
#
# ДВЕ ЛИНИИ, И ОБЕ ЗАМЕРЕНЫ НА САМОМ ТЕЛЕ, А НЕ ВЗЯТЫ ИЗ КАНОНА. Тело с 01.09
# печётся СЫРЫМ (решение владельца: только равномерный масштаб к 1.75 м, без
# --fit-canon и без --reshape), поэтому судья пропорций для него справочный, а
# пропорции головы задаёт не канон, а сам ассет.
#
# ЛИНИЯ СРЕЗА, и почему НЕ 0.871H, по которой резала галерея. 0.871H — низ
# овоида в контракте галереи, но овоид на ней НЕ КОНЧАЕТСЯ: замер
# средне-сагиттального переднего обвода (печатается всегда, таблица в начале
# прогона) даёт вынос вперёд −42.4 мм на высоте 1473.5, −49.0 на 1479.6, −62.1
# на 1485.8, −85.8 на 1491.9 — то есть ниже подбородочной линии тело ещё почти
# пятьдесят миллиметров ВЫПИРАЕТ ВПЕРЁД черепом. Это и есть «у туловища ещё
# есть часть подбородка» из вердикта владельца. Скат кончается на 1479.6 мм =
# 0.8455H: ниже наклон обвода мягче 1 мм на мм, выше — до 3.9. Отсюда 0.845H.
# СОВПАДЕНИЕ, которое стоит записать, но которое ничего не доказывает: ровно
# там же лежит середина канонической шеи (плечо 0.818H, подбородок 0.870H).
#
# ЛИНИЯ ПОДБОРОДКА 0.870H — не канон, а ТРЕБОВАНИЕ РАВЕНСТВА: голова обязана
# занять ровно тот объём, который занимал овоид, иначе пересадка меняет ещё и
# пропорции тела, которые владелец принял. Макушка остаётся на 1.000H, низ
# головы — на линии овоида; высота головы выходит 227.5 мм при росте 1.75.
#
# ЧТО ТАКОЕ ШЕЯ-ПЕРЕХОДНИК И ПОЧЕМУ ОНА НЕ «ВОРОТНИК». Между кольцом тела на
# 0.845H (замер 191 × 163 мм — это трапеция, а не шея) и кольцом головы на
# 0.870H стоит лофт в `--cuts` колец, построенный мостом по обводам ОБОИХ
# колец, а не приставленным цилиндром: вершины моста — общие с телом и с
# головой, поэтому шва как разрыва не существует, он есть только как полоса
# четырёхугольников. Кольцо головы при этом не «что получилось», а НАЗНАЧЕНО:
# последние 45 мм лепки сведены к эллипсу 112 × 120 мм с центром на 20 мм
# позади лицевой оси — это сечение живой шеи под челюстью, и это же число
# просил заказ. Подбородок при этом всплывает на ~12 мм выше 0.870H; судью
# пропорций это не трогает (он мерит макушку минус СУСТАВ головы, а не минус
# подбородок), и в отчёте это сказано вслух.
#
# ПОЧЕМУ ВЕСА НАЗНАЧАЮТСЯ, А НЕ НАСЛЕДУЮТСЯ. У скелета Rigify есть DEF-neck,
# и это единственная причина, по которой шея может гнуться отдельно от головы.
# Голова получает DEF-head = 1 ЦЕЛИКОМ — это и есть требование «лицо не
# деформируется клипами»: жёстко привязанная к одной кости сетка не может
# дрожать, что бы ни делал клип. Кольца шеи получают гладкий градиент
# smoothstep от DEF-neck к DEF-head, тело ниже среза не трогается вовсе.
#
# Dependencies:
# - Uses: bpy (Blender 4.2 LTS); числа канона читаются из docs/NUMBERS.md
#   (правило 14 — не переписывать константы), мерки лица — из
#   docs/research/FACE_CANON.md.
# - Used by: рука; результат печёт dfn_import_gltf (цель dfn_characters), цели
#   тела после него ОБЯЗАНЫ быть перепечены tools/make_body_targets.py — номера
#   вершин изменились.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - НИ ОДНОЙ ЧУЖОЙ ВЕРШИНЫ: голова выведена из овоида самого HumanBase.

import json
import math
import os
import re
import sys

import bmesh
import bpy
import mathutils
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --------------------------------------------------------------- канон (правило 14)

def numbers_row(name, default):
    """Читает долю из docs/NUMBERS.md — реестра, а не второй копии."""
    path = os.path.join(ROOT, "docs", "NUMBERS.md")
    try:
        with open(path, encoding="utf-8") as f:
            m = re.search(r"\|\s*`%s`\s*\|\s*([0-9.]+)\s*\|" % re.escape(name), f.read())
    except OSError:
        return default
    return float(m.group(1)) if m else default


NECK_FRAC = numbers_row("BODY_NECK_HEIGHT_FRAC", 0.870)   # сверка, не источник
HEAD_FRAC = numbers_row("BODY_HEAD_HEIGHT_FRAC", 0.130)
SHOULDER_FRAC = numbers_row("BODY_SHOULDER_HEIGHT_FRAC", 0.818)

FIG_MM = 1750.0                       # рост, в котором нормируются все мерки
CHIN_FRAC = 0.870                     # низ головы = низ овоида (см. шапку)
CUT_FRAC = 0.845                      # конец ската переднего обвода (см. шапку)
HEAD_MM = (1.0 - CHIN_FRAC) * FIG_MM  # 227.5 — высота головы, замер по телу
CHIN_MM = CHIN_FRAC * FIG_MM          # 1522.5
CUT_MM = CUT_FRAC * FIG_MM            # 1478.75

# Кольцо шеи, назначенное голове (мм): полуширина, полуглубина, отступ назад.
NECK_A_MM = 56.0
NECK_B_MM = 60.0
NECK_CY_MM = 20.0
NECK_BLEND_MM = 34.0                  # на какой высоте лепка начинает сходиться к нему
# Показатель развала шеи: 1.0 — прямой конус от кольца головы к кольцу тела,
# больше — шея дольше держит свой радиус и расходится к трапеции только внизу.
NECK_FLARE_POW = 2.0
# Сглаживание лепки: сколько шагов лапласиана и с каким шагом.
NECK_HEAD_FLOOR = 0.35
RELAX_ITERS = 2
RELAX_LAMBDA = 0.35

# --------------------------------------------------------------- мерки лица (FACE_CANON)
Z_EYE = 0.50 * HEAD_MM                # линия глаз — половина высоты головы
IPD_MM = 64.0                         # межзрачковое нейтрали MakeHuman
Z_NOSE = Z_EYE - 40.1                 # основание носа = линия глаз минус длина носа
Z_MOUTH = 0.22 * HEAD_MM              # линия рта
FACE_W_MM = 108.6                     # ширина лица на линии глаз
EX = IPD_MM / 2.0


def log(msg):
    print("[graft] %s" % msg)
    sys.stdout.flush()


# --------------------------------------------------------------- лепка

def smoothstep(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


class Sculpt:
    """Лепка лица на вершинах овоида. Координаты — миллиметры при росте 1.75,
    начало в центре подбородочной линии, лицо смотрит в −Y, макушка в +Z."""

    def __init__(self, P):
        self.X = P[:, 0].copy()
        self.Y = P[:, 1].copy()
        self.Z = P[:, 2].copy()

    def pack(self):
        return np.column_stack([self.X, self.Y, self.Z])

    def brush(self, cx, cz, rx, rz, dy=0.0, dx=0.0, side="front", pw=2.0):
        """Гауссова кисть по (x,z); ход по Y (вперёд = минус) или по X."""
        X, Y, Z = self.X, self.Y, self.Z
        sel = (Y < 0) if side == "front" else (Y > 0)
        a = ((X - cx) / rx) ** 2 + ((Z - cz) / rz) ** 2
        g = np.exp(-(a ** (pw / 2.0)))
        depth = np.clip(-Y / 110.0, 0, 1) if side == "front" else np.clip(Y / 110.0, 0, 1)
        g = g * depth ** 0.5
        g[~sel] = 0.0
        if dy:
            self.Y = Y + g * dy
        if dx:
            self.X = X + g * np.sign(X) * dx

    def ring(self, cz, rz, dy=0.0, da=0.0):
        """Кольцевая кисть по всей окружности на высоте cz: da — доля радиуса."""
        g = np.exp(-(((self.Z - cz) / rz) ** 2))
        if dy:
            self.Y = self.Y + g * dy
        if da:
            self.X = self.X * (1.0 + g * da)
            self.Y = self.Y * (1.0 + g * da)

    # ------------------------------------------------------------ шаги

    def skull(self):
        """Пропорции черепа: овоид шире и глубже головы; челюсть сходится."""
        self.X *= 0.90
        front = self.Y < 0
        self.Y[front] *= 0.78
        tj = np.clip((70.0 - self.Z) / 70.0, 0, 1) ** 1.4      # ниже 70 мм — челюсть
        self.X *= (1.0 - 0.30 * tj)
        self.Y *= (1.0 - 0.22 * tj)
        tc = np.clip((self.Z - 185.0) / 41.0, 0, 1) ** 1.2     # выше 185 мм — темя
        self.X *= (1.0 - 0.10 * tc)
        self.Y *= (1.0 - 0.10 * tc)

    def face_plane(self):
        """Плоскость лица: профиль по средней линии плюс отход к вискам."""
        PZ = np.array([0, 18, 32, 46, 58, 74, 88, 100, 113, 126, 138, 152, 170, 190, 210, 226])
        PY = np.array([-88, -84, -88, -90, -88, -91, -93, -92, -89, -92, -95, -93, -88, -78, -62, -40], float)
        base = np.interp(self.Z, PZ, PY)
        curv = 20.0 / (58.0 ** 2)          # на |x| = 58 мм лицо уходит на 20 мм назад
        tgt = base + curv * self.X ** 2
        wx = 1.0 - smoothstep((np.abs(self.X) - 42.0) / 34.0)
        wz = smoothstep((self.Z - 18.0) / 26.0) * (1.0 - smoothstep((self.Z - 150.0) / 48.0))
        w = wx * wz
        w[self.Y >= 0] = 0.0
        self.Y = np.where(self.Y < 0, self.Y * (1 - w) + tgt * w, self.Y)

    def features(self):
        """Тридцать три кисти черт. Каждая строка — миллиметры канона."""
        b = self.brush
        # лоб, надбровные дуги, переносица, виски
        b(0, 150, 52, 22, dy=-4.0)
        b(-EX, 130, 26, 10, dy=-7.0); b(EX, 130, 26, 10, dy=-7.0)
        b(0, 128, 12, 9, dy=-3.0)
        b(-62, 140, 18, 26, dy=+5.0); b(62, 140, 18, 26, dy=+5.0)
        # глазницы: провал, яблоко, верхнее и нижнее веко как СКЛАДКИ
        b(-EX, Z_EYE + 2, 25, 15, dy=+15.0); b(EX, Z_EYE + 2, 25, 15, dy=+15.0)
        b(-EX, Z_EYE, 14, 8.5, dy=-11.0);    b(EX, Z_EYE, 14, 8.5, dy=-11.0)
        b(-EX, Z_EYE + 6.5, 13, 4.0, dy=+6.0); b(EX, Z_EYE + 6.5, 13, 4.0, dy=+6.0)
        b(-EX, Z_EYE - 7.5, 12, 4.0, dy=+4.0); b(EX, Z_EYE - 7.5, 12, 4.0, dy=+4.0)
        # ВЕКИ КАК СКЛАДКИ (правка волны пересадки): узкая борозда над яблоком
        # и валик под ним — без них глаз читается вдавленным шаром.
        b(-EX, Z_EYE + 10.5, 12.5, 2.2, dy=+3.5, pw=2.6)
        b(EX, Z_EYE + 10.5, 12.5, 2.2, dy=+3.5, pw=2.6)
        b(-EX, Z_EYE - 11.0, 11.5, 2.4, dy=-2.6, pw=2.6)
        b(EX, Z_EYE - 11.0, 11.5, 2.4, dy=-2.6, pw=2.6)
        # нос: спинка, кончик, крылья, ноздревая тень
        b(0, Z_EYE - 6, 8.5, 15, dy=-9.0)
        b(0, (Z_EYE + Z_NOSE) / 2 - 2, 8.0, 18, dy=-15.0)
        b(0, Z_NOSE + 6, 9.5, 8.5, dy=-22.0)
        b(-15.5, Z_NOSE + 1, 7.0, 6.0, dy=-13.0); b(15.5, Z_NOSE + 1, 7.0, 6.0, dy=-13.0)
        b(-11.5, Z_NOSE - 3, 5.0, 4.0, dy=+7.0);  b(11.5, Z_NOSE - 3, 5.0, 4.0, dy=+7.0)
        # фильтр и губы
        b(0, Z_NOSE - 10, 11, 6, dy=+6.0)
        b(0, Z_MOUTH + 7.0, 24, 6.0, dy=-8.0)
        b(0, Z_MOUTH, 23, 3.6, dy=+8.0)
        b(0, Z_MOUTH - 7.5, 20, 6.0, dy=-7.0)
        b(-27, Z_MOUTH, 8, 7, dy=+4.0); b(27, Z_MOUTH, 8, 7, dy=+4.0)
        # ГУБЫ КАК СКЛАДКИ (правка волны): кайма верхней и нижней губы
        b(0, Z_MOUTH + 3.2, 21, 1.8, dy=-2.4, pw=2.6)
        b(0, Z_MOUTH - 3.4, 19, 1.8, dy=-2.0, pw=2.6)
        # скулы, щёки, подбородок, подгубная складка
        b(-46, 98, 20, 20, dy=-6.0); b(46, 98, 20, 20, dy=-6.0)
        b(-36, 72, 16, 16, dy=+4.0); b(36, 72, 16, 16, dy=+4.0)
        b(0, 22, 22, 14, dy=-9.0)
        b(0, 36, 16, 5, dy=+4.0)

    def ears(self):
        """УШИ ХОТЯ БЫ НАМЁКОМ, И ДРУГИМ ИНСТРУМЕНТОМ, ЧЕМ ОСТАЛЬНОЕ ЛИЦО.
        Прежняя лепка звала ту же кисть brush(dx=...) — и на кадрах галереи ушей
        НЕТ ВООБЩЕ. Причина в самой кисти: её сила умножена на
        (|Y|/110)**0.5, то есть падает до нуля там, где поверхность смотрит
        ВБОК, а ухо живёт ровно там. Здесь кисть своя: гауссиана по (Y, Z) —
        то есть по боковой проекции, где ухо и рисуют, — и ход строго по X
        наружу, с окном по |X|, чтобы щёки не поехали.

        Размеры живого уха при росте 1.75: длина 60 мм (z 75…135), ширина 32,
        вынос 15. Три кисти: раковина наружу, завиток по краю, чаша внутрь."""
        aX = np.abs(self.X)
        gate = smoothstep((aX - 38.0) / 16.0)
        for cy, cz, ry, rz, dx, pw in ((32.0, 106.0, 20.0, 34.0, -5.0, 2.0),   # ров вокруг
                                       (14.0, 106.0, 15.0, 30.0, 17.0, 3.0),   # раковина
                                       (20.0, 108.0, 6.0, 26.0, 6.0, 3.0),     # завиток
                                       (8.0, 100.0, 7.0, 13.0, -9.0, 2.4)):    # чаша
            a = ((self.Y - cy) / ry) ** 2 + ((self.Z - cz) / rz) ** 2
            g = np.exp(-(a ** (pw / 2.0)))
            self.X = self.X + np.sign(self.X) * dx * g * gate

    def relax(self, nbr_idx, nbr_off, iters, lam):
        """СГЛАЖИВАНИЕ ПО СОБСТВЕННОЙ ТОПОЛОГИИ. Единственная претензия галереи
        к своей же лепке — «поверхность местами рябит»: тридцать гауссиан на
        плотной сетке складываются в мелкую волну, которую на глине видно
        сразу. Лапласиан с шагом lam, применённый к КООРДИНАТАМ, снимает волну
        и почти не трогает крупную форму (её длина волны в двадцать раз
        больше шага сетки). Кольцо шеи и края не двигаются: neck_base идёт
        после и переписывает их назначенным эллипсом."""
        P = np.column_stack([self.X, self.Y, self.Z])
        cnt = np.diff(nbr_off).astype(float)[:, None]
        for _ in range(iters):
            acc = np.zeros_like(P)
            np.add.at(acc, np.repeat(np.arange(len(P)), np.diff(nbr_off)), P[nbr_idx])
            P = P + lam * (acc / np.maximum(cnt, 1.0) - P)
        self.X, self.Y, self.Z = P[:, 0], P[:, 1], P[:, 2]

    def neck_base(self):
        """Последние миллиметры сводятся к НАЗНАЧЕННОМУ кольцу шеи: эллипс
        112 × 120 мм, центр на 20 мм позади лицевой оси. Делается последним —
        поверх подбородка, — потому что ниже челюсти головы уже нет, там шея."""
        # ПОКАЗАТЕЛЬ 2.2 — ЧТОБЫ ПОДБОРОДОК ВЫЖИЛ. При линейном сходе кольцо
        # съедало челюсть целиком и голова садилась на шею без подбородка
        # (кадр профиля первого прогона). При 2.2 на высоте 25 мм вес 0.02,
        # на 12 мм — 0.35, на нуле — 1: подбородок остаётся, шея начинается
        # под ним.
        w = smoothstep((NECK_BLEND_MM - self.Z) / NECK_BLEND_MM) ** 2.2
        ang = np.arctan2(self.Y - NECK_CY_MM, self.X + 1e-9)
        tx = NECK_A_MM * np.cos(ang)
        ty = NECK_CY_MM + NECK_B_MM * np.sin(ang)
        self.X = self.X * (1 - w) + tx * w
        self.Y = self.Y * (1 - w) + ty * w

    def symmetrize(self, order):
        """Зеркальная симметрия ПО ПОСТРОЕНИЮ: пара вершин, найденная на
        исходном овоиде (он симметричен до 0.001 мм), усредняется. Кисти
        симметричны и сами, но одна опечатка в координате делает лицо кривым
        молча — этот шаг превращает симметрию в свойство файла."""
        self.X = 0.5 * (self.X - self.X[order])
        self.Y = 0.5 * (self.Y + self.Y[order])
        self.Z = 0.5 * (self.Z + self.Z[order])


def mirror_order(P):
    """Для каждой вершины — индекс её зеркальной пары (по X)."""
    kd = mathutils.kdtree.KDTree(len(P))
    for i, p in enumerate(P):
        kd.insert(mathutils.Vector((float(p[0]), float(p[1]), float(p[2]))), i)
    kd.balance()
    order = np.zeros(len(P), dtype=np.int64)
    worst = 0.0
    for i, p in enumerate(P):
        _, idx, dist = kd.find(mathutils.Vector((-float(p[0]), float(p[1]), float(p[2]))))
        order[i] = idx
        worst = max(worst, dist)
    return order, worst


# --------------------------------------------------------------- мерки

def face_table(P):
    """Десять мерок FACE_CANON, снятых с НАШЕЙ геометрии. Маски MPFB2 к нашей
    топологии не приложимы, поэтому окна берутся из тех же чисел канона, по
    которым лепилось: это проверка «вылеплено ли то, что заказано», и полоса
    у неё — вокруг СВОЕЙ нейтрали, а не вокруг медицинской таблицы."""
    X, Y, Z = P[:, 0], P[:, 1], P[:, 2]
    front = Y < 0
    out = {}

    def win(cx, cz, rx, rz):
        return front & (np.abs(X - cx) < rx) & (np.abs(Z - cz) < rz)

    le = win(-EX, Z_EYE, 13, 9)
    re = win(EX, Z_EYE, 13, 9)
    out["IPD"] = float(X[re].mean() - X[le].mean()) if le.any() and re.any() else 0.0
    band = front & (np.abs(Z - Z_EYE) < 6) & (np.abs(X) < 60)   # уши исключены
    out["face_w"] = float(np.ptp(X[band])) if band.any() else 0.0
    out["head_h"] = float(np.ptp(Z))
    out["eye_line"] = float(Z_EYE / np.ptp(Z))
    inner = win(-14, Z_EYE, 5, 6)
    outer = win(-46, Z_EYE, 6, 6)
    out["fissure_w"] = float(abs(X[outer].mean() - X[inner].mean())) if inner.any() and outer.any() else 0.0
    out["five_eyes"] = out["face_w"] / out["fissure_w"] if out["fissure_w"] > 1e-6 else 0.0
    nb = win(0, Z_NOSE, 10, 5)
    out["nose_len"] = float(Z_EYE - Z[nb].mean()) if nb.any() else 0.0
    tip = win(0, Z_NOSE + 6, 8, 5)
    out["nose_out"] = float(Y[nb].mean() - Y[tip].mean()) if nb.any() and tip.any() else 0.0
    wings = front & (np.abs(Z - (Z_NOSE + 1)) < 5) & (np.abs(X) < 26) & (Y < -70)
    out["wing_w"] = float(np.ptp(X[wings])) if wings.any() else 0.0
    corners = front & (np.abs(Z - Z_MOUTH) < 5) & (np.abs(X) < 40) & (Y < -70)
    out["mouth_w"] = float(np.ptp(X[corners])) if corners.any() else 0.0
    return out


FACE_ASKED = {           # чего просил канон в этих же единицах
    "IPD": 64.0, "face_w": 108.6, "head_h": HEAD_MM, "eye_line": 0.50,
    "fissure_w": 23.9, "five_eyes": 4.54, "nose_len": 40.1, "nose_out": 7.0,
    "wing_w": 31.0, "mouth_w": 54.0,
}


def profile_table(me, z0, H, K):
    """Средне-сагиттальный передний обвод тела: тот замер, из которого выведена
    линия среза (см. шапку). Сечение берётся РЕЗОМ ПЛОСКОСТЬЮ, а не полосой
    вершин: полоса на редкой сетке ловит то затылок, то ничего."""
    rows = []
    base = bmesh.new()
    base.from_mesh(me)
    for frac in np.arange(0.820, 0.8801, 0.005):
        z = z0 + float(frac) * H
        cut = base.copy()
        bmesh.ops.bisect_plane(cut, geom=list(cut.verts) + list(cut.edges) + list(cut.faces),
                               plane_co=(0, 0, z), plane_no=(0, 0, 1),
                               clear_inner=True, clear_outer=True)
        Q = np.array([list(v.co) for v in cut.verts]) if len(cut.verts) else None
        cut.free()
        if Q is None or not len(Q):
            continue
        mid = Q[np.abs(Q[:, 0]) < 0.010 / K]
        if not len(mid):
            continue
        rows.append((round(float(frac), 4), round(float(frac) * FIG_MM, 1),
                     round(float(mid[:, 1].min() * K * 1000), 1),
                     round(float(np.ptp(Q[:, 0]) * K * 1000), 1)))
    base.free()
    return rows


# --------------------------------------------------------------- сборка

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opt = {"body": os.path.join(ROOT, "assets/objects/characters/HumanBaseNoHead.glb"),
           "out": "", "level": "3", "cuts": "4", "report": "", "head_glb": ""}
    flags = {"measure": False}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--measure":
            flags["measure"] = True
            i += 1
            continue
        key = a.lstrip("-").replace("-", "_")
        if key in opt and i + 1 < len(argv):
            opt[key] = argv[i + 1]
            i += 2
        else:
            raise SystemExit("неизвестный ключ %s" % a)
    level = int(opt["level"])
    cuts = int(opt["cuts"])

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=opt["body"])
    sc = bpy.context.scene
    for o in list(sc.objects):
        if o.name == "Icosphere":
            bpy.data.objects.remove(o, do_unlink=True)
    body = sc.objects["Mannequin"]
    me = body.data
    if len(me.vertices) > 12000:
        raise SystemExit("на входе %d вершин — это уже пересаженное тело; "
                         "вход должен быть HumanBaseNoHead.glb" % len(me.vertices))
    if any(abs(body.matrix_world[r][c] - (1.0 if r == c else 0.0)) > 1e-6
           for r in range(4) for c in range(4)):
        raise SystemExit("тело не в единичной матрице — посадка была бы кривой")

    P = np.array([list(v.co) for v in me.vertices])
    z0 = float(P[:, 2].min())
    H = float(np.ptp(P[:, 2]))
    K = 1.75 / H                       # метры при росте 1.75 на единицу файла
    U = 0.001 / K                      # единица файла в одном миллиметре
    log("тело: %d вершин, H = %.4f ед., K = %.6f, срез 0.845H = %.4f ед."
        % (len(P), H, K, z0 + CUT_FRAC * H))

    prof = profile_table(me, z0, H, K)
    log("передний обвод (доля, мм над подошвой, вынос вперёд мм, ширина мм):")
    for r in prof:
        log("    %.3f  %7.1f  %7.1f  %7.1f" % r)

    # --- 1. исходный овоид: всё выше подбородочной линии
    z_chin = z0 + CHIN_FRAC * H
    z_cut = z0 + CUT_FRAC * H
    src = bmesh.new()
    src.from_mesh(me)
    bmesh.ops.bisect_plane(src, geom=list(src.verts) + list(src.edges) + list(src.faces),
                           plane_co=(0, 0, z_chin), plane_no=(0, 0, 1),
                           clear_inner=False, clear_outer=False)
    src.verts.ensure_lookup_table()
    bmesh.ops.delete(src, geom=[v for v in src.verts if v.co.z < z_chin - 1e-6],
                     context="VERTS")
    # СВАРКА ОВОИДА ДО ДРОБЛЕНИЯ. В .glb тело лежит двумя примитивами, и на их
    # стыке одна точка тела записана двумя вершинами (шапка make_body_targets.py,
    # раздел про швы). Внутри овоида таких стыков полтора десятка, а дробление
    # Catmull-Clark считает СВОБОДНЫЙ КРАЙ границей и стягивает его внутрь: не
    # сварив, мы получили бы на черепе трещины шириной в миллиметр. Порог тот
    # же, каким сваривает импортёр, — 1/20000 м.
    n_raw = len(src.verts)
    b0 = sum(1 for e in src.edges if len(e.link_faces) < 2)
    bmesh.ops.remove_doubles(src, verts=list(src.verts), dist=(1.0 / 20000.0) / K)
    # ЛОСКУТ НА ЗАТЫЛКЕ. После сварки в затылке остаётся ОДНО ребро с ТРЕМЯ
    # гранями: две — симметричная пара по средней линии, третья — вырожденный
    # четырёхугольник, целиком лежащий в плоскости x = 0 (замер: вершины
    # (0, .0976, .8887) (0, .0986, .8823) (0, .1058, .870) (0, .1062, .870),
    # площадь 37.8 мм² против 222.3 у соседей). Это нулевой толщины лоскут
    # исходного ассета: на самом теле его не видно, а дробление превращает его
    # в складку и рвёт по ней затылок на 24 мм. Снимается самая мелкая грань
    # такого ребра — и обвод головы становится ровно одной петлёй (замер: 54
    # ребра, все вершины степени 2, разброс по высоте 0.0 мм).
    flap = []
    for e in src.edges:
        if len(e.link_faces) > 2:
            flap.append(sorted(e.link_faces, key=lambda f: f.calc_area())[0])
    if flap:
        bmesh.ops.delete(src, geom=flap, context="FACES_ONLY")
        loose = [e for e in src.edges if len(e.link_faces) == 0]
        if loose:
            bmesh.ops.delete(src, geom=loose, context="EDGES")
        alone = [v for v in src.verts if not v.link_faces]
        if alone:
            bmesh.ops.delete(src, geom=alone, context="VERTS")
        src.verts.ensure_lookup_table()
    b1 = sum(1 for e in src.edges if len(e.link_faces) < 2)
    log("овоид: свободных рёбер до сварки %d, после сварки и снятия %d "
        "лоскутов %d" % (b0, len(flap), b1))
    ovoid = bpy.data.meshes.new("Ovoid")
    src.to_mesh(ovoid)
    src.free()
    head_ob = bpy.data.objects.new("Head", ovoid)
    sc.collection.objects.link(head_ob)
    n_ovoid = len(ovoid.vertices)

    # --- 2. дробление и лепка
    bpy.context.view_layer.objects.active = head_ob
    md = head_ob.modifiers.new("sub", "SUBSURF")
    md.levels = level
    md.render_levels = level
    md.boundary_smooth = "PRESERVE_CORNERS"
    bpy.ops.object.modifier_apply(modifier="sub")
    hm = head_ob.data

    Q = np.array([list(v.co) for v in hm.vertices])
    cx = float((Q[:, 0].min() + Q[:, 0].max()) / 2)
    cy = float((Q[:, 1].min() + Q[:, 1].max()) / 2)
    zc = float(Q[:, 2].min())
    k_mm = HEAD_MM / (np.ptp(Q[:, 2]) * K * 1000)       # довести до 227.5 мм канона
    M = np.column_stack([(Q[:, 0] - cx) * K * 1000 * k_mm,
                         (Q[:, 1] - cy) * K * 1000 * k_mm,
                         (Q[:, 2] - zc) * K * 1000 * k_mm])
    order, mirror_err = mirror_order(M)
    log("овоид: %d -> %d вершин (дробление %d), зеркальная невязка %.4f мм, "
        "довод до канона x%.5f" % (n_ovoid, len(M), level, mirror_err, k_mm))

    # соседи по рёбрам — для сглаживания
    ed = np.array([[e.vertices[0], e.vertices[1]] for e in hm.edges], dtype=np.int64)
    both = np.concatenate([ed, ed[:, ::-1]])
    order_e = np.argsort(both[:, 0], kind="stable")
    both = both[order_e]
    nbr_idx = both[:, 1]
    nbr_off = np.zeros(len(M) + 1, dtype=np.int64)
    np.add.at(nbr_off, both[:, 0] + 1, 1)
    nbr_off = np.cumsum(nbr_off)

    s = Sculpt(M)
    s.skull()
    s.face_plane()
    s.features()
    s.relax(nbr_idx, nbr_off, RELAX_ITERS, RELAX_LAMBDA)
    s.ears()
    s.neck_base()
    s.symmetrize(order)
    S = s.pack()
    # ВЫСОТА ВОССТАНАВЛИВАЕТСЯ ПОСЛЕ СГЛАЖИВАНИЯ. Лапласиан двигает и Z, а
    # значит опускает макушку и утягивает кольцо среза из своей плоскости
    # (замер: 0.0001 мм на кольце, 0.006 мм на макушке). Кольцо возвращается на
    # ноль точно — по нему ищется обвод под лофт, — а высота растягивается
    # обратно в 0…HEAD_MM, чтобы посадка не зависела от числа шагов сглаживания.
    on_ring = M[:, 2] <= 0.05          # мм: дробление кладёт край в ту же плоскость
    S[on_ring, 2] = 0.0
    zlo, zhi = float(S[:, 2].min()), float(S[:, 2].max())
    S[:, 2] = (S[:, 2] - zlo) * (HEAD_MM / (zhi - zlo))
    _, sym_after = mirror_order(S)
    log("симметрия после лепки: %.5f мм" % sym_after)

    face = face_table(S)
    log("мерки лица (мм / доли):")
    for k in ("IPD", "face_w", "head_h", "eye_line", "fissure_w", "five_eyes",
              "nose_len", "nose_out", "wing_w", "mouth_w"):
        asked = FACE_ASKED[k]
        got = face[k]
        d = (got - asked) / asked * 100.0 if abs(asked) > 1e-9 else 0.0
        log("    %-10s %8.2f   канон %8.2f   %+6.1f %%" % (k, got, asked, d))

    ring_head = S[S[:, 2] < 0.5]
    seam = {"head_ring_w": float(np.ptp(ring_head[:, 0])),
            "head_ring_d": float(np.ptp(ring_head[:, 1]))}
    log("кольцо головы: %.1f x %.1f мм (заказ ~110 x 120)"
        % (seam["head_ring_w"], seam["head_ring_d"]))

    # обратно в единицы файла: подбородочная линия на 0.870H, ось вертикальна,
    # центр макушки над центром кольца — по построению, лепка не трогает X-центр.
    # МАСШТАБ ГОЛОВЫ БЕРЁТСЯ ОТ МАКУШКИ ТЕЛА, А НЕ ОТ КРУГЛОГО ЧИСЛА 227.5.
    # Разница между «0.130 × рост» и «макушка ровно там, где была» — шесть
    # микрон, и она стоила красного теста: dfn_import_gltf нормирует ВЕСЬ меш
    # по общей высоте, поэтому сдвинутая на микрон макушка ужимает и кисть, а
    # приёмка стойки ищет гарду как САМУЮ широкую поперёк оси вершину клинка —
    # у неё почти ничья, и 0.4 микрона на ладони перекидывают ответ на 18 мм
    # (замер: гарда 14.61 см против 16.41). Собственная высота головы от этого
    # 227.494 мм вместо 227.5; собственная приёмка — «рост не изменился ни на
    # бит», и она важнее круглого числа.
    U_head = (z0 + H - z_chin) / HEAD_MM
    back = np.column_stack([S[:, 0] * U_head + cx,
                            S[:, 1] * U_head + cy,
                            S[:, 2] * U_head + z_chin])
    for i, v in enumerate(hm.vertices):
        v.co = (float(back[i, 0]), float(back[i, 1]), float(back[i, 2]))
    crown = float(back[:, 2].max())
    log("макушка %.4f ед. (тело было %.4f), голова %.1f мм"
        % (crown, z0 + H, (crown - z_chin) * K * 1000))

    if opt["head_glb"]:
        bpy.ops.object.select_all(action="DESELECT")
        head_ob.select_set(True)
        bpy.context.view_layer.objects.active = head_ob
        bpy.ops.export_scene.gltf(filepath=opt["head_glb"], export_format="GLB",
                                  use_selection=True, export_yup=True,
                                  export_animations=False)

    # --- 3. срез тела по 0.845H
    bm = bmesh.new()
    bm.from_mesh(me)
    dvert_lay = bm.verts.layers.deform.verify()
    bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                           plane_co=(0, 0, z_cut), plane_no=(0, 0, 1),
                           clear_inner=False, clear_outer=False)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z > z_cut + 1e-6],
                     context="VERTS")
    bm.verts.ensure_lookup_table()
    # СВАРКА ТОЛЬКО ШЕИ, И ТОЛЬКО ПОТОМУ, ЧТО БЕЗ НЕЁ КОЛЬЦО СРЕЗА — НЕ КОЛЬЦО.
    # Замер: на линии среза сходятся обе примитивы .glb, и обвод распадается на
    # четыре дуги (24 + 20 + 8 + 8 рёбер) вместо одной замкнутой петли; мост,
    # которому дали четыре дуги, возвращает FINISHED и не строит ничего. Варится
    # ПОЯС В 30 мм ПОД СРЕЗОМ, ниже тело не тронуто ни одной вершиной.
    lo = z_cut - 0.030 / K
    near = [v for v in bm.verts if v.co.z > lo]
    n_seam = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=near, dist=(1.0 / 20000.0) / K)
    bm.verts.ensure_lookup_table()
    log("сварка пояса шеи: %d -> %d вершин" % (n_seam, len(bm.verts)))
    ring_body = np.array([list(v.co) for v in bm.verts
                          if abs(v.co.z - z_cut) < 0.0002 / K])
    log("кольцо тела на срезе: %d вершин, %.1f x %.1f мм"
        % (len(ring_body), np.ptp(ring_body[:, 0]) * K * 1000,
           np.ptp(ring_body[:, 1]) * K * 1000))
    bm.to_mesh(me)
    bm.free()
    me.update()

    # --- 4. голова в тот же объект и мост между двумя обводами
    bpy.ops.object.select_all(action="DESELECT")
    head_ob.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    for p in head_ob.data.polygons:
        p.material_index = 0
    head_offset = len(body.data.vertices)
    bpy.ops.object.join()
    me = body.data
    for p in me.polygons:
        p.use_smooth = True

    # ЛОФТ СТРОИТСЯ РУКАМИ, А НЕ ОПЕРАТОРОМ МОСТА. bpy.ops.mesh.bridge_edge_loops
    # на этой паре обводов возвращает FINISHED и оставляет 104 незакрытых ребра
    # ровно на линии подбородка (замер): у него своё представление о том, какие
    # рёбра образуют петлю, и проверить его нечем. Здесь петли обходятся явно,
    # кольца ставятся по РАДИУСУ от собственного центра, а низ сшивается
    # застёжкой — то есть каждая грань шва названа в коде, и дыра невозможна.
    n_before_bridge = len(me.vertices)
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    tol = 0.0002 / K

    def loop_at(z, lo=None, hi=None):
        """Обвод: свободные рёбра, отобранные по высоте или по номерам вершин
        (голова после join лежит хвостом), обойденные в порядке цепи."""
        if lo is None:
            es = [e for e in bm.edges
                  if len(e.link_faces) < 2
                  and abs(e.verts[0].co.z - z) < tol and abs(e.verts[1].co.z - z) < tol]
        else:
            es = [e for e in bm.edges
                  if len(e.link_faces) < 2
                  and e.verts[0].index >= lo and e.verts[1].index >= lo]
            zz = [v.co.z for e in es for v in e.verts]
            log("обвод головы: %d рёбер, разброс по высоте %.4f мм"
                % (len(es), (max(zz) - min(zz)) * K * 1000))
        if not es:
            raise SystemExit("на высоте %.4f нет свободного обвода" % z)
        adj = {}
        for e in es:
            adj.setdefault(e.verts[0], []).append(e)
            adj.setdefault(e.verts[1], []).append(e)
        # ОБРЕЗКА ОТРОСТКОВ. У обвода головы из плоскости среза торчит остаток
        # затылочной щели: вершина степени 1 и вершина степени 3 на 55 рёбрах.
        # Отросток снимается с конца, пока не останется чистый цикл; если после
        # этого цикл не один — работа прекращается вслух, а не молча кривится.
        pruned = 0
        while True:
            leaf = [v for v, l in adj.items() if len(l) == 1]
            if not leaf:
                break
            for v in leaf:
                e = adj[v][0]
                for w in e.verts:
                    adj[w].remove(e)
                es.remove(e)
                pruned += 1
            adj = {v: l for v, l in adj.items() if l}
        if pruned:
            log("обвод: снято %d рёбер отростка" % pruned)
        bad = [v for v, l in adj.items() if len(l) != 2]
        if bad:
            raise SystemExit("обвод на %.4f не петля: %d вершин со степенью != 2"
                             % (z, len(bad)))
        start = es[0].verts[0]
        chain = [start]
        prev_e = None
        cur = start
        while True:
            nxt = None
            for e in adj[cur]:
                if e is not prev_e:
                    nxt = e
                    break
            prev_e = nxt
            cur = nxt.other_vert(cur)
            if cur is start:
                break
            chain.append(cur)
        if len(chain) != len(es):
            raise SystemExit("обвод на %.4f распался: %d из %d"
                             % (z, len(chain), len(es)))
        return chain

    head_loop = loop_at(z_chin, lo=head_offset)
    body_loop = loop_at(z_cut)

    def polar(chain):
        P = np.array([[v.co.x, v.co.y] for v in chain])
        c = P.mean(0)
        d = P - c
        ang = np.arctan2(d[:, 1], d[:, 0])
        rad = np.hypot(d[:, 0], d[:, 1])
        return c, ang, rad

    ch, ang_h, rad_h = polar(head_loop)
    cb, ang_b, rad_b = polar(body_loop)
    # обе петли — по часовой или против, приводим к возрастанию угла
    def ascending(chain, ang, rad):
        step = np.diff(np.unwrap(ang))
        if step.sum() < 0:
            chain = chain[::-1]
            ang = ang[::-1]
            rad = rad[::-1]
        k = int(np.argmin(ang))
        return chain[k:] + chain[:k], np.roll(ang, -k), np.roll(rad, -k)

    head_loop, ang_h, rad_h = ascending(head_loop, ang_h, rad_h)
    body_loop, ang_b, rad_b = ascending(body_loop, ang_b, rad_b)
    if np.any(np.diff(ang_h) <= 0) or np.any(np.diff(ang_b) <= 0):
        raise SystemExit("обвод не звёздчатый относительно своего центра — "
                         "радиальный лофт на нём неверен")
    log("обводы: голова %d вершин, тело %d вершин" % (len(head_loop), len(body_loop)))

    # радиус тела на углах головы (кольцевая интерполяция)
    ext_a = np.concatenate([ang_b - 2 * np.pi, ang_b, ang_b + 2 * np.pi])
    ext_r = np.concatenate([rad_b, rad_b, rad_b])
    rad_b_at_h = np.interp(ang_h, ext_a, ext_r)

    rings = [head_loop]
    for k in range(1, cuts + 1):
        t = k / float(cuts + 1)
        wc = float(smoothstep(t))                      # центр едет ровно
        wr = t ** NECK_FLARE_POW                       # радиус держит шею и
        cxk = ch + (cb - ch) * wc                      # расходится к трапеции
        rk = rad_h * (1.0 - wr) + rad_b_at_h * wr
        zk = z_chin + (z_cut - z_chin) * t
        rings.append([bm.verts.new((float(cxk[0] + rk[i] * math.cos(ang_h[i])),
                                    float(cxk[1] + rk[i] * math.sin(ang_h[i])),
                                    float(zk))) for i in range(len(head_loop))])
    bm.verts.ensure_lookup_table()

    # ОБХОД ГРАНЕЙ ВЫВЕДЕН, А НЕ ПОДОБРАН: у четырёхугольника (верх_i, низ_i,
    # низ_j, верх_j) при обходе колец против часовой стрелки нормаль смотрит
    # НАРУЖУ — проверено на цилиндре аналитически, поэтому пересчёт нормалей
    # оператором тут не нужен и не запускается (он трогал бы и тело).
    new_faces = []
    for a, b in zip(rings, rings[1:]):
        n = len(a)
        for i in range(n):
            j = (i + 1) % n
            new_faces.append(bm.faces.new((a[i], b[i], b[j], a[j])))
    # ЗАСТЁЖКА: нижнее кольцо (углы головы) на обвод тела, шаг по меньшему углу.
    low = rings[-1]
    na, nb = len(low), len(body_loop)
    ia = ib = 0
    while ia < na or ib < nb:
        ta = (float(ang_h[(ia + 1) % na]) + (2 * np.pi if ia + 1 >= na else 0.0)) \
            if ia < na else 1e9
        tb = (float(ang_b[(ib + 1) % nb]) + (2 * np.pi if ib + 1 >= nb else 0.0)) \
            if ib < nb else 1e9
        if ia < na and (ib >= nb or ta <= tb):
            new_faces.append(bm.faces.new(
                (low[(ia + 1) % na], low[ia % na], body_loop[ib % nb])))
            ia += 1
        else:
            new_faces.append(bm.faces.new(
                (low[ia % na], body_loop[ib % nb], body_loop[(ib + 1) % nb])))
            ib += 1
    for f in new_faces:
        f.smooth = True
        f.material_index = 0
    log("лофт: %d граней" % len(new_faces))
    bm.to_mesh(me)
    bm.free()
    me.update()

    n_loft = len(me.vertices) - n_before_bridge
    log("лофт шеи: %d колец, %d новых вершин" % (cuts, n_loft))

    # ДЫР В ШВЕ БЫТЬ НЕ ДОЛЖНО. Считается только пояс от линии среза до макушки:
    # свободные рёбра НИЖЕ среза — это родные швы примитивов тела, они были до
    # нас и остаются (импортёр сваривает их сам), и записывать их себе в
    # достижение или в брак одинаково нечестно.
    chk = bmesh.new()
    chk.from_mesh(me)
    holes = sum(1 for e in chk.edges
                if len(e.link_faces) < 2
                and min(e.verts[0].co.z, e.verts[1].co.z) > z_cut - 1e-4)
    holes_body = sum(1 for e in chk.edges if len(e.link_faces) < 2) - holes
    if holes:
        zz = sorted(round((min(e.verts[0].co.z, e.verts[1].co.z) - z0) / H, 4)
                    for e in chk.edges if len(e.link_faces) < 2
                    and min(e.verts[0].co.z, e.verts[1].co.z) > z_cut - 1e-4)
        import collections as _c
        log("дыры по высоте: %s" % sorted(_c.Counter(zz).items())[:20])
    chk.free()

    # --- 5. веса: голова жёстко на DEF-head, шея — градиент к DEF-neck
    gh = body.vertex_groups.get("DEF-head")
    gn = body.vertex_groups.get("DEF-neck")
    if gh is None or gn is None:
        raise SystemExit("у скелета нет DEF-head/DEF-neck — пересадка невозможна")
    span = z_chin - z_cut
    wm = bmesh.new()
    wm.from_mesh(me)
    wm.verts.ensure_lookup_table()
    lay = wm.verts.layers.deform.verify()
    ih, inck = gh.index, gn.index
    n_head_w = 0
    n_loft_w = 0
    for v in wm.verts:
        z = v.co.z
        if z >= z_chin - 1e-6:
            v[lay].clear()
            v[lay][ih] = 1.0
            n_head_w += 1
        elif z > z_cut + 1e-6 and v.index >= n_before_bridge:
            # ПОЛ ГРАДИЕНТА НЕ НОЛЬ, И ЭТО ВИДНО НА КАДРЕ, А НЕ В ТАБЛИЦЕ.
            # --skin-palette красит вершину по её САМОМУ ТЯЖЁЛОМУ суставу, а
            # DEF-neck у нашего скелета не привязан ни к одной кости рига и
            # наследует цвет ТУЛОВИЩА (рубахи). При градиенте от нуля рубаха
            # начиналась под челюстью — на кадре стенда это читается как
            # воротник. Пол NECK_HEAD_FLOOR опускает границу цвета к основанию
            # шеи, где воротник и живёт, и одновременно сближает вес кольца
            # среза с весом соседних вершин ТЕЛА (у них DEF-head 0.25…0.5 по
            # замеру исходника) — то есть шов не только не рвётся, но и не
            # ломается по весу.
            t = NECK_HEAD_FLOOR + (1.0 - NECK_HEAD_FLOOR) * float(
                smoothstep((z - z_cut) / span))
            v[lay].clear()
            v[lay][ih] = t
            v[lay][inck] = 1.0 - t
            n_loft_w += 1
    wm.to_mesh(me)
    wm.free()
    me.update()
    log("веса: голова %d вершин на DEF-head=1, шея %d вершин градиентом"
        % (n_head_w, n_loft_w))

    log("итог: %d вершин, %d треугольников; дыр в шве %d, родных швов тела %d"
        % (len(me.vertices), sum(len(p.vertices) - 2 for p in me.polygons),
           holes, holes_body))

    if opt["report"]:
        rep = {"face": face, "face_asked": FACE_ASKED, "seam": seam,
               "profile": prof, "verts": len(me.vertices),
               "tris": sum(len(p.vertices) - 2 for p in me.polygons),
               "ovoid_verts": n_ovoid, "head_verts": len(M), "loft_verts": n_loft,
               "cut_frac": CUT_FRAC, "chin_frac": CHIN_FRAC, "level": level,
               "cuts": cuts, "symmetry_mm": sym_after, "seam_holes": holes,
               "body_seam_edges": holes_body, "ovoid_raw_verts": n_raw,
               "body_ring_mm": [float(np.ptp(ring_body[:, 0]) * K * 1000),
                                float(np.ptp(ring_body[:, 1]) * K * 1000)]}
        os.makedirs(os.path.dirname(os.path.abspath(opt["report"])), exist_ok=True)
        with open(opt["report"], "w", encoding="utf-8") as f:
            json.dump(rep, f, ensure_ascii=False, indent=1, sort_keys=True)

    if flags["measure"] or not opt["out"]:
        log("--measure: файл не пишется")
        return

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=opt["out"], export_format="GLB",
                              export_animations=True, export_animation_mode="ACTIONS",
                              export_nla_strips=True, export_skins=True,
                              export_yup=True, use_selection=True)
    log("записано %s" % opt["out"])


main()
