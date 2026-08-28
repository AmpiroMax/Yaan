#!/usr/bin/env python3
#
# Created: 28:08:2026 - 22:05:00
# Last updated: 29:08:2026 - 03:10:00
# File: tools/flora_sow.py
#
# Responsibility:
# - ЗАКОН ПОСЕВА ЯРУСОВ (записка №2 ресёрчера, docs/reports/trees-tiers,
#   пункты 1.2 / 1.3 / 1.4 / 3.1). ОДИН файл на четыре закона, потому что все
#   четыре — про одну и ту же землю и обязаны знать друг о друге:
#     §1 КУРТИНЫ подлеска: центры + плотность с рваной границей, а НЕ рассев.
#     §2 КОВЁР: сплошной и дешёвый, мох против травы ПО ВИДИМОСТИ НЕБА.
#     §3 АКЦЕНТЫ: редкие, высокие, группами — своя плотность, свой порог света.
#     §4 ОПУШКА: подлесок только на бортик тропы, деревья — дальше кустов.
#
# ЧТО ЗДЕСЬ НЕ ЗАВОДИТСЯ. Ни одного нового объекта (кузницы уже есть) и ни
# одной новой величины света: «видимость неба» — та же величина, которой
# движок гейтит интерьерный свет (VoxelMeshView::sky_visibility), и здесь она
# СЧИТАЕТСЯ ОТ ПОЛОГА, потому что композиционная сцена — единственное место,
# где полог вообще известен: в вершинах земли этот канал сегодня пуст (255 =
# «не знаю»), и заполнить его от деревьев сцены некому. Если/когда движок
# начнёт печь полог в вершины, ЭТОТ расчёт обязан быть заменён чтением канала,
# а не оставлен рядом с ним — две видимости неба на одной земле были бы ровно
# тем «своим каналом», который контракт зоны запрещает.
#
# ДЕТЕРМИНИЗМ. Ни одного вызова random: каждое решение — хэш от (соль, ячейка,
# номер), поэтому посев не зависит ни от порядка обхода, ни от того, сколько
# растений посеяли раньше. Тот же семя — тот же файл до байта.
#
# Dependencies:
# - Uses: Python stdlib; tools/dfo_read.py (габариты объектов полки).
# - Used by: tools/gen_trees_v2.py (смотровая площадка), tools/gen_city.py
#   (боевые города — вторая волна ярусов).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ЧИСЛА ЗАКОНА ЖИВУТ ЗДЕСЬ И НИГДЕ БОЛЬШЕ. У генератора карты не может быть
#   своей плотности куртины: две плотности — два закона, и кадр перестанет
#   быть доводом о законе.
#
# UPD:
# - 28:08:2026 - 22:05:00: Создан — первая волна ярусов (пункты 1, 2, 3 и 5
#   очереди записки №2).
# - 29:08:2026 - 03:10:00: ВТОРАЯ ВОЛНА (закон посева — в города). Ни одного
#   нового числа и ни одного изменённого: у акцентов и подроста появилась
#   ручка region, которая у подлеска и ковра была с первого дня. Причина —
#   город: у него ярусам место в лесу и рощах, а не во дворе, на мостовой и
#   в теле дома, и сказать это можно только областью. Область приходит
#   СНАРУЖИ (маска зовущего), потому что закон не знает ни про стенд, ни про
#   город — и это ровно то свойство, ради которого он лежит отдельным файлом.

import math

TAU = 6.283185307179586

# =====================================================================
# ЧИСЛА ЗАКОНА. Каждое — с доводом; менять с доводом же.
# =====================================================================

# --- §1 КУРТИНЫ ПОДЛЕСКА ---------------------------------------------
# Ячейка решётки центров. 16 м — потому что куртина радиусом ~4 м должна
# читаться как ОТДЕЛЬНАЯ, а не сливаться с соседней: расстояние между центрами
# не меньше двух радиусов, и между куртинами остаётся «просто трава».
CLUMP_CELL_M = 16.0
CLUMP_JITTER = 0.42          # доля ячейки; центр гуляет, решётка не видна
CLUMP_OCCUPANCY = 0.70       # доля ячеек, несущих куртину вообще
CLUMP_R_MIN_M = 2.5          # меньше — это одно растение, а не куртина
CLUMP_R_MAX_M = 5.5          # больше — это снова «везде»
# РВАНАЯ ГРАНИЦА: радиус — функция угла, две гармоники. Куртина с круглой
# границей читается как клумба; полоса 0.54..1.46 R0 — это лопасти и заливы.
CLUMP_LOBE3 = 0.30
CLUMP_LOBE5 = 0.16
# Кандидаты внутри куртины: шаг решётки и закон принятия.
CLUMP_STEP_M = 1.05          # 0.907 кандидата на м^2 — шаг НЕСПЛОШНОГО подлеска
# ШАГ КУРТИНЫ — СВОЙСТВО ГОСПОДСТВУЮЩЕГО ВИДА, А НЕ ОДНО ЧИСЛО НА ВСЕХ.
# Папоротник несплошной: сквозь него ходят, и заросль папоротника — это заросль.
# Куст сплошной, и два куста в одной яме — находка судьи (no-overlap, зазор
# 0.5 м). Поэтому куртина кустов РЕДЖЕ куртины папоротника ПО ЗАКОНУ, а не по
# отбраковке: шаг = поперечник следа минус судейский зазор.
CLUMP_SOLID_SLACK_M = 0.35   # с запасом против судейских 0.5
CLUMP_FALLOFF = 0.55         # p(r) = (1 - (r/R)^2)^FALLOFF: плотное ядро
CLUMP_DOMINANT = 0.80        # доля господствующего вида в куртине

# --- §2 КОВЁР --------------------------------------------------------
CARPET_CELL_M = 3.0          # шаг укладки плат ковра
CARPET_JITTER_M = 0.55
CARPET_BARE = 0.12           # доля ПРОПУЩЕННЫХ ячеек: «видна пятнистая земля»
# ПОРОГ ПО СВЕТУ и его размытие. Ниже MOSS — мох, выше GRASS — трава, между —
# ДИТЕРИНГ по хэшу ячейки: именно он делает границу мягкой и рваной, а не
# контурной. Полоса 0.42..0.58 — шириной в шаг ковра на типичном краю кроны.
CARPET_MOSS_BELOW = 0.42
CARPET_GRASS_ABOVE = 0.58

# --- §3 АКЦЕНТЫ ------------------------------------------------------
# СВОЯ решётка и своя плотность: акцент — другая сущность, а не редкий ковёр.
ACCENT_CELL_M = 17.0
ACCENT_JITTER = 0.45
ACCENT_OCCUPANCY = 0.30
ACCENT_R_MIN_M = 1.2
ACCENT_R_MAX_M = 2.6
ACCENT_STEP_M = 1.35         # 0.549 кандидата на м^2
ACCENT_FALLOFF = 0.80
# Порог света для акцентов: цветы и высокие злаки — в световых пятнах, грибы —
# в тени. Это вторая половина п.1.3 записки, сказанная про акцент.
ACCENT_LIGHT_MIN = 0.55      # злаки/цветы только светлее этого
ACCENT_SHADE_MAX = 0.45      # грибы только темнее этого

# --- §4 ОПУШКА -------------------------------------------------------
# Метры от ИЗНОШЕННОГО КРАЯ полотна, наружу (та же величина, которой судья
# ловит [off-path]; его порог — 0.5 м, взято с запасом).
EDGE_UNDER_CLEAR_M = 0.60    # ближе — подлеска нет вовсе
EDGE_TREE_CLEAR_M = 2.50     # деревья держат дистанцию БОЛЬШЕ кустов
EDGE_CARPET_CLEAR_M = 0.45
EDGE_BAND_M = 2.50           # полоса бортика
EDGE_BAND_GAIN = 1.8         # во столько раз подлесок на бортике гуще

# --- §0 ВИДИМОСТЬ НЕБА -----------------------------------------------
# Сколько неба закрывает ОДНА зрелая крона в своём центре. 0.72 — то есть под
# одиночным деревом видно 28% неба; две сомкнутые кроны дают 0.078, три — 0.02,
# и «сомкнутый полог» получается сам, без второго порога.
CANOPY_SHADE_MAX = 0.72
# Мерка кроны берётся из габарита объекта, но габарит считает самый дальний
# лист; тень же кладёт масса. 0.85 — доля габарита, работающая как крона.
CANOPY_R_FRAC = 0.85
# ОБОД КРОНЫ — доля радиуса, на которой тень гаснет. Крона НЕ конус тени с
# вершиной у ствола: она затеняет почти всю свою площадь и рвётся только по
# краю. Мягкий профиль (тень ~ расстоянию до ствола) был первой редакцией и
# ОКАЗАЛСЯ ИЗМЕРИМО НЕВЕРЕН: в сомкнутой роще 7x7 деревьев он давал видимость
# неба 0.93 — то есть «полога нет», — потому что точка между четырьмя стволами
# лежит у КРАЯ каждой кроны, а по мягкому профилю край не затеняет. С ободом
# 0.30 та же точка получает 0.2-0.35, и сомкнутый полог наконец существует.
CANOPY_RIM = 0.30
# Подрост и кусты тени в этом смысле не кладут: полог — это ярус 1.
CANOPY_MIN_HEIGHT_M = 6.0


# =====================================================================
# ДЕТЕРМИНИРОВАННЫЙ ХЭШ. Тот же приём, что clump_detail::mix64 в
# engine/core/math/sources/FloraField.h — здесь он нужен на питоне, и это
# СОЗНАТЕЛЬНАЯ вторая реализация одного и того же смесителя, а не второй закон
# посева: закон — это пороги ниже, а смеситель лишь раздаёт равномерные числа.
# =====================================================================
_M = (1 << 64) - 1


def mix64(x):
    x = (x + 0x9E3779B97F4A7C15) & _M
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & _M
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & _M
    return x ^ (x >> 31)


def h01(*parts):
    """Равномерное [0,1) от произвольного набора целых."""
    h = 0xCBF29CE484222325
    for p in parts:
        h = mix64(h ^ (int(p) & _M))
    return (h >> 11) / float(1 << 53)


def _smoothstep(t):
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
    return t * t * (3.0 - 2.0 * t)


# =====================================================================
# §0 ВИДИМОСТЬ НЕБА ОТ ПОЛОГА
# =====================================================================
class Canopy:
    """Полог сцены: круги теней от деревьев яруса 1.

    Величина одна на все три применения (мох/трава, акценты, подрост), и
    считается она ЗДЕСЬ ОДИН РАЗ — иначе три правила разойдутся на границе.
    """

    def __init__(self, trees, cell_m=16.0):
        """trees — список (x, z, crown_radius_m, height_m)."""
        self.discs = [(x, z, r) for (x, z, r, h) in trees
                      if h >= CANOPY_MIN_HEIGHT_M and r > 0.5]
        self.cell = cell_m
        self.grid = {}
        for i, (x, z, r) in enumerate(self.discs):
            x0 = int(math.floor((x - r) / cell_m))
            x1 = int(math.floor((x + r) / cell_m))
            z0 = int(math.floor((z - r) / cell_m))
            z1 = int(math.floor((z + r) / cell_m))
            for cz in range(z0, z1 + 1):
                for cx in range(x0, x1 + 1):
                    self.grid.setdefault((cx, cz), []).append(i)

    def openness(self, x, z):
        """1 — открытое небо, 0 — запечатано. ПРОИЗВЕДЕНИЕ пропусканий крон:
        так две наложенные кроны темнее одной, а не «тоже одна»."""
        cx = int(math.floor(x / self.cell))
        cz = int(math.floor(z / self.cell))
        v = 1.0
        for i in self.grid.get((cx, cz), ()):
            dx, dz, r = self.discs[i]
            d2 = (x - dx) ** 2 + (z - dz) ** 2
            if d2 >= r * r:
                continue
            # Тень гаснет ТОЛЬКО НА ОБОДЕ, а не от самого ствола (CANOPY_RIM).
            t = (1.0 - math.sqrt(d2) / r) / CANOPY_RIM
            v *= 1.0 - CANOPY_SHADE_MAX * _smoothstep(t)
        return v


# =====================================================================
# §4 ОПУШКА: поле троп, выгруженное судьёй (--path-field)
# =====================================================================
class PathField:
    """dist_from_worn_edge на решётке. Отсутствие поля = мир без троп, и
    тогда правило опушки просто не срабатывает (как у судьи)."""

    FAR = 99.0

    def __init__(self, step=0.0, values=None):
        self.step = step
        self.values = values or {}

    @classmethod
    def parse(cls, text):
        step = 0.0
        vals = {}
        for line in text.splitlines():
            if line.startswith("[path-field] step"):
                step = float(line.split()[2])
            elif line.startswith("pf "):
                _, ix, iz, d = line.split()
                vals[(int(ix), int(iz))] = float(d)
        return cls(step, vals)

    def clearance(self, x, z):
        if self.step <= 0.0:
            return self.FAR
        # Билинейный минимум ни к чему: нужен КОНСЕРВАТИВНЫЙ ответ, поэтому
        # берётся минимум четырёх узлов ячейки — растение не сядет на полотно
        # из-за интерполяции.
        ix = int(math.floor(x / self.step))
        iz = int(math.floor(z / self.step))
        worst = self.FAR
        for dz in (0, 1):
            for dx in (0, 1):
                worst = min(worst, self.values.get((ix + dx, iz + dz), self.FAR))
        return worst

    def band_gain(self, x, z):
        """БОРТИК: полоса подлеска вдоль тропы гуще открытого места. Это не
        украшение — именно она делает тропу читаемой как тропу (§3.1)."""
        d = self.clearance(x, z)
        if d >= self.FAR or d > EDGE_BAND_M:
            return 1.0
        if d < EDGE_UNDER_CLEAR_M:
            return 0.0
        return EDGE_BAND_GAIN


# =====================================================================
# ОБЩАЯ МАШИНА КУРТИНЫ. Один процесс, две настройки (§1 и §3): куртина — это
# «центры + плотность вокруг с рваной границей», и разными их делают ЧИСЛА, а
# не два разных кода.
# =====================================================================
def _clump_centres(salt, span, cell, jitter, occupancy):
    n = int(math.ceil(span / cell)) + 1
    for cz in range(-1, n + 1):
        for cx in range(-1, n + 1):
            if h01(salt, cx, cz, 1) >= occupancy:
                continue
            x = (cx + 0.5 + (h01(salt, cx, cz, 2) - 0.5) * 2.0 * jitter) * cell
            z = (cz + 0.5 + (h01(salt, cx, cz, 3) - 0.5) * 2.0 * jitter) * cell
            yield (cx, cz, x, z)


def _ragged_radius(r0, phi1, phi2, ang):
    return r0 * (1.0 + CLUMP_LOBE3 * math.sin(3.0 * ang + phi1)
                 + CLUMP_LOBE5 * math.sin(5.0 * ang + phi2))


def _clump_members(salt, cx, cz, x0, z0, r0, step, falloff, span, gain_at):
    """Кандидаты внутри одной куртины, принятые по закону плотности."""
    phi1 = h01(salt, cx, cz, 4) * TAU
    phi2 = h01(salt, cx, cz, 5) * TAU
    rmax = r0 * (1.0 + CLUMP_LOBE3 + CLUMP_LOBE5)
    ix0 = int(math.floor((x0 - rmax) / step))
    ix1 = int(math.floor((x0 + rmax) / step))
    iz0 = int(math.floor((z0 - rmax) / step))
    iz1 = int(math.floor((z0 + rmax) / step))
    k = 0
    for iz in range(iz0, iz1 + 1):
        for ix in range(ix0, ix1 + 1):
            px = (ix + h01(salt, ix, iz, 6)) * step
            pz = (iz + h01(salt, ix, iz, 7)) * step
            if not (0.0 <= px <= span and 0.0 <= pz <= span):
                continue
            dx = px - x0
            dz = pz - z0
            d = math.hypot(dx, dz)
            if d > rmax:
                continue
            rr = _ragged_radius(r0, phi1, phi2, math.atan2(dz, dx))
            if rr <= 0.01 or d >= rr:
                continue
            u = d / rr
            p = (1.0 - u * u) ** falloff
            p *= gain_at(px, pz)
            if h01(salt, ix, iz, 8) >= min(1.0, p):
                continue
            yield (px, pz, k, h01(salt, ix, iz, 9))
            k += 1


# =====================================================================
# ПОЛКА: габариты объектов теми же мерками, какими их читает СУДЬЯ.
# Без этого сеятель и судья мерят разное, и «0 находок» становится удачей.
# =====================================================================
# СЛЕД ПОВЁРНУТОГО СЛЕДА. Судья берёт коробку объекта, ПОВОРАЧИВАЕТ её на yaw
# и обводит осевой коробкой (Scene.cpp: «a turned box is not a box»). У квадрата
# с полустороной r обводка растёт до r*(|cos|+|sin|), то есть до r*sqrt(2) на
# 45 градусах. Сеятель, считавший просто r, был на 41% МЯГЧЕ судьи — и это
# ровно те 356 находок [no-overlap] и 5 [off-path], которыми первый прогон
# ярусов ответил на «0 находок». Множитель не подгонка, а та же геометрия.
YAW_ENVELOPE = 1.4142135623730951


class Shelf:
    """name -> (radius, solid_radius, solid). Радиусы отдаются УЖЕ в мерке
    судьи (с обводкой поворота), чтобы сеятель и судья не мерили разное:
    «0 находок» обязано быть свойством закона, а не удачей угла."""

    def __init__(self, objects, scales=None):
        # МАСШТАБ РАЗМЕЩЕНИЯ ВХОДИТ В МЕРКУ. Судья множит габарит на scale
        # (Scene.cpp), и сеятель обязан делать то же: посаженный вдвое крупнее
        # папоротник вдвое ближе к тропе и вдвое сильнее лезет в соседа.
        scales = scales or {}
        self.scales = scales
        self.sizes = {}
        for name, o in objects.items():
            k = scales.get(name, 1.0)
            self.sizes[name] = (o.radius * k, o.solid_radius * k, o.solid)

    def scale(self, name):
        return self.scales.get(name, 1.0)

    def radius(self, name):
        return self.sizes.get(name, (0.5, 0.0, False))[0] * YAW_ENVELOPE

    def solid_radius(self, name):
        return self.sizes.get(name, (0.5, 0.0, False))[1] * YAW_ENVELOPE

    def is_solid(self, name):
        return self.sizes.get(name, (0.5, 0.0, False))[2]


def bounds_ok(shelf, name, x, z, span, margin=2.0):
    """ВНУТРИ КАРТЫ с тем же запасом, каким мерит судья (edge_margin_m = 2.0
    плюс радиус). Порог берётся у судьи, а не назначается здесь."""
    r = shelf.radius(name) + margin
    return r <= x <= span - r and r <= z <= span - r


def path_ok(paths, shelf, name, x, z, need):
    """Пять проб по следу — ТОТ ЖЕ набор точек, каким судья мерит [off-path]:
    центр и четыре угла. Растение, чей край свесился на полотно, стоит на
    полотне, даже если его начало в стороне."""
    r = shelf.radius(name)
    for (px, pz) in ((x, z), (x - r, z - r), (x + r, z - r),
                     (x - r, z + r), (x + r, z + r)):
        if paths.clearance(px, pz) < need:
            return False
    return True


def thin_solids(placed, shelf, existing=(), slack=0.5, cell=4.0):
    """ПРОРЕЖИВАНИЕ СПЛОШНЫХ по правилу судьи (no-overlap).

    Куртина — это ПЛОТНАЯ группа, но плотность у папоротника и у куста разная,
    и разная она не по вкусу: два куста в одной яме — находка судьи, а два
    папоротника — заросль (папоротник несплошной, сквозь него ходят). Поэтому
    прореживание считает ТОТ ЖЕ проникающий размер, что и судья, и трогает
    ТОЛЬКО сплошные. Возвращает (оставленные, сколько снято)."""
    grid = {}

    def add(nm, x, z):
        r = shelf.solid_radius(nm)
        cx0, cx1 = int((x - r) // cell), int((x + r) // cell)
        cz0, cz1 = int((z - r) // cell), int((z + r) // cell)
        for cz in range(cz0, cz1 + 1):
            for cx in range(cx0, cx1 + 1):
                grid.setdefault((cx, cz), []).append((x, z, r))

    def clashes(nm, x, z):
        r = shelf.solid_radius(nm)
        cx0, cx1 = int((x - r) // cell), int((x + r) // cell)
        cz0, cz1 = int((z - r) // cell), int((z + r) // cell)
        for cz in range(cz0, cz1 + 1):
            for cx in range(cx0, cx1 + 1):
                for (ox, oz, orr) in grid.get((cx, cz), ()):
                    # Проникновение осевых коробок — min по осям, как у судьи.
                    ox_ov = (r + orr) - abs(x - ox)
                    oz_ov = (r + orr) - abs(z - oz)
                    if min(ox_ov, oz_ov) > slack:
                        return True
        return False

    for (nm, x, z) in existing:
        if shelf.is_solid(nm):
            add(nm, x, z)
    kept = []
    dropped = 0
    for rec in placed:
        nm, x, z = rec[0], rec[1], rec[2]
        if not shelf.is_solid(nm):
            kept.append(rec)
            continue
        if clashes(nm, x, z):
            dropped += 1
            continue
        add(nm, x, z)
        kept.append(rec)
    return kept, dropped


# =====================================================================
# §1 ПОДЛЕСОК КУРТИНАМИ
# =====================================================================
def sow_undergrowth(span, seed, canopy, paths, shelf, palettes, uniform=False,
                    region=None):
    """Возвращает список (name, x, z, yaw, note).

    palettes — {'shade': [имена], 'light': [имена], 'any': [имена]}: господ-
    ствующий вид куртины выбирается ПО СВЕТУ её центра, а не по случаю, —
    папоротник растёт в тени, ягодный куст на просвете.

    uniform=True — КОНТРОЛЬНОЕ ПЛЕЧО (правило 30): тот же счёт растений,
    рассеянный РАВНОМЕРНО. Не для сдачи, а для кадра-довода и для замера:
    без него «куртина читается лучше рассева» — утверждение без контроля.
    """
    salt = (seed * 0x1000003) ^ 0x5EED_C10D
    out = []
    for (cx, cz, x0, z0) in _clump_centres(salt, span, CLUMP_CELL_M,
                                           CLUMP_JITTER, CLUMP_OCCUPANCY):
        if not (0.0 <= x0 <= span and 0.0 <= z0 <= span):
            continue
        if region is not None and not region(x0, z0):
            continue
        if paths.clearance(x0, z0) < EDGE_UNDER_CLEAR_M:
            continue
        r0 = CLUMP_R_MIN_M + (CLUMP_R_MAX_M - CLUMP_R_MIN_M) * h01(salt, cx, cz, 10)
        light = canopy.openness(x0, z0)
        pool = palettes["shade"] if light < 0.5 else palettes["light"]
        companion = palettes["any"]
        dom = pool[int(h01(salt, cx, cz, 11) * len(pool)) % len(pool)]

        def gain(px, pz):
            return paths.band_gain(px, pz)

        step = CLUMP_STEP_M
        if shelf.is_solid(dom):
            step = max(step, 2.0 * shelf.solid_radius(dom) - CLUMP_SOLID_SLACK_M)

        for (px, pz, k, r) in _clump_members(salt, cx, cz, x0, z0, r0,
                                             step, CLUMP_FALLOFF,
                                             span, gain):
            if r < CLUMP_DOMINANT:
                name = dom
            else:
                # СПУТНИК НЕ КРУПНЕЕ ГОСПОДСТВУЮЩЕГО. Куртина сеется с шагом
                # господина; спутник шире него встал бы в него самого.
                name = companion[int(r * 997) % len(companion)]
                if shelf.solid_radius(name) > shelf.solid_radius(dom) + 0.05:
                    name = dom
            if not (bounds_ok(shelf, name, px, pz, span)
                    and path_ok(paths, shelf, name, px, pz, EDGE_UNDER_CLEAR_M)):
                continue
            yaw = h01(salt, int(px * 16), int(pz * 16), 12) * TAU
            out.append((name, px, pz, yaw,
                        "подлесок: куртина (%d,%d) R=%.1f м, свет %.2f"
                        % (cx, cz, r0, light)))
    if uniform:
        out = _uniformise(out, span, salt, paths, shelf, EDGE_UNDER_CLEAR_M)
    return out


def _uniformise(placed, span, salt, paths, shelf, clear_m):
    """КОНТРОЛЬНОЕ ПЛЕЧО: тот же счёт и тот же состав, РОВНЫМ рассевом по
    площади. Состав сохраняется нарочно — иначе кадры сравнивали бы не законы
    посева, а наборы растений."""
    n = len(placed)
    out = []
    i = 0
    guard = 0
    while len(out) < n and guard < n * 40:
        guard += 1
        px = h01(salt, guard, 101) * span
        pz = h01(salt, guard, 102) * span
        name = placed[i % n][0]
        if not (bounds_ok(shelf, name, px, pz, span)
                and path_ok(paths, shelf, name, px, pz, clear_m)):
            continue
        i += 1
        out.append((name, px, pz, h01(salt, guard, 103) * TAU,
                    "КОНТРОЛЬ: равномерный рассев (плечо правила 30)"))
    return out


# =====================================================================
# §2 КОВЁР
# =====================================================================
def sow_carpet(span, seed, canopy, paths, shelf, moss_names, grass_names,
               region=None):
    """Сплошной ковёр платами. Мох против травы — ПОРОГ ПО ВИДИМОСТИ НЕБА с
    дитерингом в переходной полосе: контурная граница читается как граница
    текстуры, дитеринг — как смена растительности."""
    salt = (seed * 0x1000193) ^ 0xC0FFEE_1D
    out = []
    n = int(math.ceil(span / CARPET_CELL_M))
    for cz in range(n):
        for cx in range(n):
            if h01(salt, cx, cz, 20) < CARPET_BARE:
                continue          # проплешина: сквозь ковёр видна земля
            x = (cx + 0.5) * CARPET_CELL_M \
                + (h01(salt, cx, cz, 21) - 0.5) * 2.0 * CARPET_JITTER_M
            z = (cz + 0.5) * CARPET_CELL_M \
                + (h01(salt, cx, cz, 22) - 0.5) * 2.0 * CARPET_JITTER_M
            if region is not None and not region(x, z):
                continue
            light = canopy.openness(x, z)
            if light <= CARPET_MOSS_BELOW:
                moss = True
            elif light >= CARPET_GRASS_ABOVE:
                moss = False
            else:
                t = (light - CARPET_MOSS_BELOW) / (CARPET_GRASS_ABOVE
                                                   - CARPET_MOSS_BELOW)
                moss = h01(salt, cx, cz, 23) >= t
            pool = moss_names if moss else grass_names
            name = pool[int(h01(salt, cx, cz, 24) * len(pool)) % len(pool)]
            # Плата ковра — крупный след; судья мерит его УГЛЫ, поэтому и
            # порог опушки берётся по углам, а не по началу.
            if not (bounds_ok(shelf, name, x, z, span)
                    and path_ok(paths, shelf, name, x, z, EDGE_CARPET_CLEAR_M)):
                continue
            out.append((name, x, z, h01(salt, cx, cz, 25) * TAU,
                        "ковёр: %s, свет %.2f" % ("мох" if moss else "трава",
                                                  light)))
    return out


# =====================================================================
# §3 АКЦЕНТЫ
# =====================================================================
def sow_accents(span, seed, canopy, paths, shelf, light_names, shade_names,
                region=None):
    """Редкие ВЫСОКИЕ акценты группами. Своя решётка, своя плотность, свой
    порог света — «ковёр и акцент разные сущности» (§1.4) выражено числами,
    а не словом.

    region — ГДЕ этому ярусу вообще место (та же ручка, что у подлеска и
    ковра). Ярусу нужна не только плотность, но и ОБЛАСТЬ: у стенда это лес,
    у города — лес и рощи, а не двор и не мостовая. Маска приходит снаружи,
    потому что закон про стенд и город не знает и знать не должен."""
    salt = (seed * 0x100015B) ^ 0xACCE_1177
    out = []
    for (cx, cz, x0, z0) in _clump_centres(salt, span, ACCENT_CELL_M,
                                           ACCENT_JITTER, ACCENT_OCCUPANCY):
        if not (0.0 <= x0 <= span and 0.0 <= z0 <= span):
            continue
        if region is not None and not region(x0, z0):
            continue
        light = canopy.openness(x0, z0)
        if light >= ACCENT_LIGHT_MIN:
            pool, kind = light_names, "световое пятно"
        elif light <= ACCENT_SHADE_MAX:
            pool, kind = shade_names, "тень полога"
        else:
            continue          # полутень акцента не несёт: он и должен быть редок
        r0 = ACCENT_R_MIN_M + (ACCENT_R_MAX_M - ACCENT_R_MIN_M) \
            * h01(salt, cx, cz, 30)
        dom = pool[int(h01(salt, cx, cz, 31) * len(pool)) % len(pool)]
        for (px, pz, k, r) in _clump_members(salt, cx, cz, x0, z0, r0,
                                             ACCENT_STEP_M, ACCENT_FALLOFF,
                                             span, lambda a, b: 1.0):
            name = dom if r < CLUMP_DOMINANT else pool[int(r * 991) % len(pool)]
            if not (bounds_ok(shelf, name, px, pz, span)
                    and path_ok(paths, shelf, name, px, pz, EDGE_UNDER_CLEAR_M)):
                continue
            out.append((name, px, pz, h01(salt, int(px * 16), int(pz * 16), 32) * TAU,
                        "акцент: %s, группа (%d,%d), свет %.2f"
                        % (kind, cx, cz, light)))
    return out


# =====================================================================
# ЯРУС ПОДРОСТА
# =====================================================================
SAPLING_CELL_M = 13.0
SAPLING_JITTER = 0.44
# Подрост гуще на ОПУШКЕ и в световых пятнах, реже под сомкнутым пологом —
# это тот же порог света, и он же есть §3.4 записки, прочитанный снизу.
SAPLING_P_LIGHT = 0.42
SAPLING_P_SHADE = 0.14
SAPLING_CLEAR_M = 3.0        # молодое деревце всё же дерево: держит дистанцию
SAPLING_TREE_GAP_M = 2.6     # не в стволе взрослого


def sow_saplings(span, seed, canopy, paths, shelf, names, trees, region=None):
    """Ярус подроста: молодые деревца 1-3 м под пологом и на опушке.

    Не куртинами: подрост — это ОДИНОЧКИ и двойки там, где до земли дошёл
    свет. Куртинный процесс дал бы «питомник», а не лес.

    region — та же ручка области, что у прочих ярусов (см. sow_accents)."""
    salt = (seed * 0x1000201) ^ 0x5AB1_1465
    out = []
    n = int(math.ceil(span / SAPLING_CELL_M))
    for cz in range(n):
        for cx in range(n):
            x = (cx + 0.5 + (h01(salt, cx, cz, 40) - 0.5) * 2 * SAPLING_JITTER) \
                * SAPLING_CELL_M
            z = (cz + 0.5 + (h01(salt, cx, cz, 41) - 0.5) * 2 * SAPLING_JITTER) \
                * SAPLING_CELL_M
            if not (2.0 <= x <= span - 2.0 and 2.0 <= z <= span - 2.0):
                continue
            if region is not None and not region(x, z):
                continue
            light = canopy.openness(x, z)
            p = SAPLING_P_SHADE + (SAPLING_P_LIGHT - SAPLING_P_SHADE) * light
            if h01(salt, cx, cz, 42) >= p:
                continue
            near = False
            for (tx, tz, tr, th) in trees:
                if (x - tx) ** 2 + (z - tz) ** 2 < SAPLING_TREE_GAP_M ** 2:
                    near = True
                    break
            if near:
                continue
            name = names[int(h01(salt, cx, cz, 43) * len(names)) % len(names)]
            if not (bounds_ok(shelf, name, x, z, span)
                    and path_ok(paths, shelf, name, x, z, SAPLING_CLEAR_M)):
                continue
            out.append((name, x, z, h01(salt, cx, cz, 44) * TAU,
                        "подрост: свет %.2f" % light))
    return out
