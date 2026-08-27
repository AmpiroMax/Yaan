#!/usr/bin/env python3
#
# Created: 27:08:2026 - 12:02:00
# Last updated: 27:08:2026 - 12:02:00
# Module: tools
# File: tools/heraldry/geometry.py
#
# Responsibility:
# - КЛАССИЧЕСКАЯ ГЕОМЕТРИЯ «2D-СИЛУЭТ -> 3D-РЕЛЬЕФ», без нейросетей и без
#   сторонних библиотек: точное преобразование расстояний (Фельценсвальб —
#   Хуттенлохер), сеточные марширующие квадраты как МЕШЕР (а не только как
#   обводчик), сшивка граничных отрезков в контуры, упрощение Дугласа — Пойкера.
#
# ПОЧЕМУ ВСЁ СВОЁ, А НЕ potrace/earcut. Разведка 27.08 по лицензиям:
# potrace, potracer, pypotrace, autotrace, CGAL, Triangle и питоний пакет
# `triangle` — GPL либо «не для коммерческих продуктов», то есть запрещены
# правилами проекта. Разрешённое (earcut — ISC, CDT — MPL-2.0, VTracer — MIT)
# пришлось бы ставить пакетами, которых в этой системе нет (нет даже Pillow).
# Алгоритмы же здесь — школьные, по опубликованным описаниям, и весь модуль
# короче, чем инструкция по установке зависимостей.
#
# ГЛАВНОЕ РЕШЕНИЕ ФАЙЛА: МАРШИРУЮЩИЕ КВАДРАТЫ КАК МЕШЕР.
# Обычный путь «обводим контур -> упрощаем -> триангулируем (earcut)» даёт
# ПЛОСКУЮ крышку из одних лишь граничных вершин: внутри детали нет, а рельеф —
# это смещение ВНУТРЕННИХ вершин, и смещать там нечего. Пришлось бы дробить
# треугольники, а дробление разной глубины у соседей рвёт сетку щелями
# (T-стыки). Поэтому крышка строится СРАЗУ сеткой: каждая ячейка шага L
# отсекается изолинией альфы, полученный выпуклый многоугольник веерно
# триангулируется. Это:
#   - бесщелевой ПО ПОСТРОЕНИЮ (соседние ячейки считают точку пересечения из
#     одной пары значений узлов одной формулой — байт в байт та же точка);
#   - сам разбирается с дырами (их 336 в кроне дуба) без разбора вложенности
#     контуров, которого earcut требует;
#   - даёт равномерную плотность, то есть ровно то, чего хочет смещение по
#     полю высот.
# Граничные отрезки тех же ячеек — это И ЕСТЬ контур, по которому ставится
# стенка, поэтому крышка и стенка делят вершины и разойтись не могут.
#
# ПОЧЕМУ УПРОЩЕНИЕ ДУГЛАСА — ПОЙКЕРА ЕСТЬ, НО НЕ В БОЕВОМ ПУТИ. Оно
# реализовано и меряется (--dp-report), однако упростить контур ПОСЛЕ того, как
# крышка сшита по нему, значит развести стенку с крышкой и получить щель по
# всему силуэту. Настоящий рычаг плотности здесь — шаг сетки L, и он один.
#
# Dependencies:
# - Uses: numpy.
# - Used by: tools/gen_heraldry.py.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - Ключи вершин — ЦЕЛОЧИСЛЕННЫЕ (узел сетки либо ребро сетки), никогда не
#   округлённые координаты: округление float — это сварка вершин, которые
#   алгоритм считал разными, и она проявляется дырой в сетке, а не ошибкой.
#
# UPD:
# - 27:08:2026 - 12:02:00: Создан — ядро генератора 3D-герба (заказ владельца
#   27.08: «объектом 3D с краями и без лишнего фона»).
#
"""Classical silhouette-to-relief geometry: EDT, marching-squares meshing, RDP."""

import numpy as np

# ---------------------------------------------------------------- расстояния

def _edt_1d(f: np.ndarray) -> np.ndarray:
    """Нижняя огибающая парабол вдоль ПОСЛЕДНЕЙ оси (Felzenszwalb & Huttenlocher,
    "Distance Transforms of Sampled Functions", TR2004-1963, Cornell).

    Реализовано по ОПИСАНИЮ алгоритма из статьи, не переносом кода. Точное
    евклидово преобразование за O(n) на строку; приближения вроде чамфера 3-4
    дают на пологом рельефе видимые грани-«лучи» от углов силуэта, поэтому
    берётся точное.
    """
    rows, n = f.shape
    d = np.empty_like(f)
    v = np.zeros((rows, n), dtype=np.int64)      # номера парабол огибающей
    z = np.empty((rows, n + 1), dtype=np.float64)  # границы их областей
    z[:, 0] = -np.inf
    z[:, 1] = np.inf
    k = np.zeros(rows, dtype=np.int64)
    idx = np.arange(rows)
    for q in range(1, n):
        # Сдвигаем вершину огибающей, пока новая парабола накрывает прежнюю.
        while True:
            vk = v[idx, k]
            s = ((f[:, q] + q * q) - (f[idx, vk] + vk * vk)) / (2.0 * q - 2.0 * vk)
            over = (s <= z[idx, k]) & (k > 0)
            if not over.any():
                break
            k[over] -= 1
        vk = v[idx, k]
        s = ((f[:, q] + q * q) - (f[idx, vk] + vk * vk)) / (2.0 * q - 2.0 * vk)
        k += 1
        v[idx, k] = q
        z[idx, k] = s
        z[idx, k + 1] = np.inf
    k[:] = 0
    for q in range(n):
        while True:
            ahead = z[idx, k + 1] < q
            if not ahead.any():
                break
            k[ahead] += 1
        vk = v[idx, k]
        d[:, q] = (q - vk) ** 2 + f[idx, vk]
    return d


def distance_transform(mask: np.ndarray) -> np.ndarray:
    """Точное евклидово расстояние (в пикселях) от каждого True до ближайшего
    False. Вне маски — ноль. Двухпроходное: параболы по строкам, затем по
    столбцам (сепарабельность квадрата евклидовой метрики).
    """
    big = 1e12
    f = np.where(mask, big, 0.0)
    d = _edt_1d(f)                       # по строкам
    d = _edt_1d(np.ascontiguousarray(d.T)).T   # по столбцам
    return np.sqrt(np.maximum(d, 0.0))


# ------------------------------------------------------------------- чистка

def _label(mask: np.ndarray):
    """Связные области (4-связность) без scipy: обход в ширину по очереди."""
    from collections import deque
    height, width = mask.shape
    labels = np.zeros((height, width), dtype=np.int32)
    sizes = [0]
    current = 0
    ys, xs = np.nonzero(mask)
    for sy, sx in zip(ys.tolist(), xs.tolist()):
        if labels[sy, sx]:
            continue
        current += 1
        count = 0
        queue = deque([(sy, sx)])
        labels[sy, sx] = current
        while queue:
            y, x = queue.popleft()
            count += 1
            for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= ny < height and 0 <= nx < width and mask[ny, nx] \
                        and not labels[ny, nx]:
                    labels[ny, nx] = current
                    queue.append((ny, nx))
        sizes.append(count)
    return labels, np.array(sizes)


def clean_mask(mask: np.ndarray, min_blob: int, min_hole: int):
    """Выкидывает крупинки и затыкает мелкие дыры. Возвращает (маска, отчёт).

    ЗАЧЕМ. У силуэта дуба 58 связных кусков, из которых 57 — крупинки по 2-6
    пикселей (мусор трассировки), и 336 дыр, из которых больше половины мельче
    сорока пикселей. На печати размером в треть экрана такая мелочь — не
    рисунок кроны, а шум, и каждая крупинка стоит собственной стенки по
    периметру: цена в треугольниках есть, читаемости нет.
    """
    padded = np.zeros((mask.shape[0] + 2, mask.shape[1] + 2), dtype=bool)
    padded[1:-1, 1:-1] = mask
    labels, sizes = _label(padded)
    keep = sizes >= min_blob
    keep[0] = False
    blobs_dropped = int((sizes[1:] < min_blob).sum())
    padded = keep[labels]
    holes, hole_sizes = _label(~padded)
    # Область фона, касающаяся рамки, — это внешний фон, а не дыра.
    outer = set(holes[0, :].tolist()) | set(holes[-1, :].tolist()) \
        | set(holes[:, 0].tolist()) | set(holes[:, -1].tolist())
    fill = np.zeros(len(hole_sizes), dtype=bool)
    for i in range(1, len(hole_sizes)):
        if i not in outer and hole_sizes[i] < min_hole:
            fill[i] = True
    holes_filled = int(fill.sum())
    holes_kept = int(sum(1 for i in range(1, len(hole_sizes))
                         if i not in outer and not fill[i]))
    padded |= fill[holes]
    report = {"blobs_dropped": blobs_dropped, "blobs_kept": int(keep.sum()),
              "holes_filled": holes_filled, "holes_kept": holes_kept}
    return padded[1:-1, 1:-1], report


def blur(field: np.ndarray, radius: float) -> np.ndarray:
    """Гауссово размытие через три прохода прямоугольным ядром (приближение,
    доказанное центральной предельной теоремой; три прохода дают ошибку ~3%).

    Нужно, чтобы изолиния шла по сглаженному полю: по сырой альфе марширующие
    квадраты повторяют лесенку растра, и она видна на фаске как рябь.
    """
    if radius <= 0:
        return field
    width = max(1, int(round(radius * 2.0)) | 1)  # нечётное
    half = width // 2
    out = field.astype(np.float64)
    for _ in range(3):
        padded = np.pad(out, half, mode="edge")
        cumulative = np.cumsum(padded, axis=1)
        out = (cumulative[:, width:] - cumulative[:, :-width]) / width
        out = np.pad(out, ((half, half), (0, 0)), mode="edge")
        cumulative = np.cumsum(out, axis=0)
        out = (cumulative[width:, :] - cumulative[:-width, :]) / width
    return out


# --------------------------------------------------- марширующие квадраты

class CapMesh:
    """Крышка (сетка внутри силуэта) плюс граничные отрезки для стенки."""

    def __init__(self):
        self.points = []      # [(x, y)] в пикселях исходной картинки
        self.triangles = []   # [(i, j, k)] против часовой в экранных осях
        self.segments = []    # [(i, j)] — отрезки изолинии, внутренность слева


def march_cells(field: np.ndarray, iso: float, step: float) -> CapMesh:
    """МЕШЕР: сетка шагом `step` по полю `field`, каждая ячейка отсекается
    изолинией `iso`; внутренняя часть ячейки триангулируется веером.

    Метод обхода вместо таблицы шестнадцати случаев: идём по четырём рёбрам
    ячейки по кругу и выдаём (а) угол, если он внутри, (б) точку пересечения,
    если ребро пересечено. Для четырнадцати случаев из шестнадцати это ровно
    отсечение квадрата полуплоскостью и даёт выпуклый многоугольник. Два
    седловых случая (внутри две ПРОТИВОПОЛОЖНЫЕ вершины) обходом дают
    самопересекающийся четырёхугольник, поэтому такая ячейка режется по
    диагонали между внутренними углами на два треугольника, и каждый
    обрабатывается тем же обходом.

    Точка пересечения линейно интерполируется между значениями узлов. У
    соседней ячейки то же ребро — та же пара значений и та же формула, значит
    бит в бит та же точка: сетка бесщелевая по построению.
    """
    height, width = field.shape
    nx = int(np.floor((width - 1) / step))
    ny = int(np.floor((height - 1) / step))
    gx = np.arange(nx + 1) * step
    gy = np.arange(ny + 1) * step
    # Билинейная выборка поля в узлах сетки — векторно, одним махом.
    x0 = np.clip(np.floor(gx).astype(int), 0, width - 2)
    y0 = np.clip(np.floor(gy).astype(int), 0, height - 2)
    tx = (gx - x0)[None, :]
    ty = (gy - y0)[:, None]
    f00 = field[np.ix_(y0, x0)]
    f10 = field[np.ix_(y0, x0 + 1)]
    f01 = field[np.ix_(y0 + 1, x0)]
    f11 = field[np.ix_(y0 + 1, x0 + 1)]
    node = (f00 * (1 - tx) * (1 - ty) + f10 * tx * (1 - ty)
            + f01 * (1 - tx) * ty + f11 * tx * ty)
    inside = node >= iso

    mesh = CapMesh()
    vertex_of = {}

    def node_vertex(i, j):
        key = ("n", i, j)
        got = vertex_of.get(key)
        if got is None:
            got = len(mesh.points)
            mesh.points.append((float(gx[i]), float(gy[j])))
            vertex_of[key] = got
        return got

    def edge_vertex(i0, j0, i1, j1):
        """Пересечение ребра узел(i0,j0)-узел(i1,j1). Ключ — КАНОНИЧЕСКИЙ
        (меньший узел первым), иначе два соседа заведут две вершины в одной
        точке и стенка разойдётся со швом."""
        if (i1, j1) < (i0, j0):
            i0, j0, i1, j1 = i1, j1, i0, j0
        key = ("e", i0, j0, i1, j1)
        got = vertex_of.get(key)
        if got is None:
            a, b = node[j0, i0], node[j1, i1]
            t = 0.5 if a == b else (iso - a) / (b - a)
            t = min(1.0, max(0.0, t))
            got = len(mesh.points)
            mesh.points.append((float(gx[i0] + (gx[i1] - gx[i0]) * t),
                                float(gy[j0] + (gy[j1] - gy[j0]) * t)))
            vertex_of[key] = got
        return got

    def clip_ring(ring):
        """Обход замкнутого списка узлов ячейки/треугольника. Возвращает
        (вершины внутренней части, отрезок изолинии или None).

        ОТРЕЗОК ОРИЕНТИРОВАН ПО ОБХОДУ МНОГОУГОЛЬНИКА, и различать «выход» и
        «вход» приходится явно. Складывать пересечения в один список и брать их
        по порядку нельзя: обход начинается с произвольного угла ячейки, так
        что первым попадётся то выход, то вход, — и половина отрезков в сетке
        смотрит против остальных. Именно на этом сшивка контуров сначала не
        дала НИ ОДНОГО замкнутого при 9800 отрезках.
        """
        out = []
        leave = enter = None
        count = len(ring)
        for a in range(count):
            i0, j0 = ring[a]
            i1, j1 = ring[(a + 1) % count]
            in0, in1 = inside[j0, i0], inside[j1, i1]
            if in0:
                out.append(node_vertex(i0, j0))
            if in0 != in1:
                crossing = edge_vertex(i0, j0, i1, j1)
                out.append(crossing)
                if in0:
                    leave = crossing
                else:
                    enter = crossing
        segment = None
        if leave is not None and enter is not None and leave != enter:
            segment = (leave, enter)
        return out, segment

    def emit(ring):
        out, segment = clip_ring(ring)
        if len(out) >= 3:
            for a in range(1, len(out) - 1):
                mesh.triangles.append((out[0], out[a], out[a + 1]))
        if segment is not None:
            mesh.segments.append(segment)

    inside_list = inside
    for j in range(ny):
        row_any = inside_list[j, :].any() or inside_list[j + 1, :].any()
        if not row_any:
            continue
        for i in range(nx):
            bl = inside_list[j, i]
            br = inside_list[j, i + 1]
            tr = inside_list[j + 1, i + 1]
            tl = inside_list[j + 1, i]
            total = int(bl) + int(br) + int(tr) + int(tl)
            if total == 0:
                continue
            if total == 4:
                v00, v10 = node_vertex(i, j), node_vertex(i + 1, j)
                v11, v01 = node_vertex(i + 1, j + 1), node_vertex(i, j + 1)
                mesh.triangles.append((v00, v10, v11))
                mesh.triangles.append((v00, v11, v01))
                continue
            if total == 2 and ((bl and tr) or (br and tl)):
                # СЕДЛО: режем по диагонали между внутренними углами.
                if bl and tr:
                    emit([(i, j), (i + 1, j), (i + 1, j + 1)])
                    emit([(i, j), (i + 1, j + 1), (i, j + 1)])
                else:
                    emit([(i, j), (i + 1, j), (i, j + 1)])
                    emit([(i + 1, j), (i + 1, j + 1), (i, j + 1)])
                continue
            emit([(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)])
    return mesh


def chain_loops(segments, point_count):
    """Сшивает граничные отрезки в замкнутые контуры по общим вершинам.

    Вершина изолинии принадлежит ровно двум отрезкам (она лежит на ребре
    сетки, а ребро делят две ячейки), поэтому сшивка однозначна и словаря
    «откуда->куда» хватает. Незамкнутые цепочки не выбрасываются молча: их
    число возвращается наверх, потому что открытый контур — это дыра в стенке,
    и узнать о ней надо от прибора, а не от кадра.
    """
    nxt = {}
    forks = 0
    for a, b in segments:
        if a in nxt:
            forks += 1     # вершина с двумя исходящими: изолиния сама себя коснулась
            continue
        nxt[a] = b
    loops = []
    open_chains = 0
    visited = set()
    for start in nxt:
        if start in visited:
            continue
        loop = []
        node = start
        closed = False
        while node not in visited:
            visited.add(node)
            loop.append(node)
            follow = nxt.get(node)
            if follow is None:
                break
            if follow == start:
                closed = True
                break
            node = follow
        if closed and len(loop) >= 3:
            loops.append(loop)
        elif loop:
            open_chains += 1
    return loops, {"open_chains": open_chains, "forks": forks}


# ------------------------------------------------------- Дуглас — Пойкер

def douglas_peucker(points, tolerance: float):
    """Упрощение полилинии (Douglas & Peucker, 1973), итеративно через стек.

    Рекурсия здесь опасна не теоретически: контур кроны — тысячи точек, и на
    почти прямом участке глубина рекурсии равна его длине, то есть питон
    падает по пределу стека на настоящем активе, а не на выдуманном.
    """
    count = len(points)
    if count < 3 or tolerance <= 0:
        return list(range(count))
    pts = np.asarray(points, dtype=np.float64)
    keep = np.zeros(count, dtype=bool)
    keep[0] = keep[count - 1] = True
    stack = [(0, count - 1)]
    while stack:
        first, last = stack.pop()
        if last <= first + 1:
            continue
        a, b = pts[first], pts[last]
        seg = b - a
        length = float(np.hypot(*seg))
        chunk = pts[first + 1:last]
        if length < 1e-12:
            dist = np.hypot(*(chunk - a).T)
        else:
            # Двумерное векторное произведение РАСПИСАНО, а не через np.cross:
            # numpy 2.x (здесь стоит 2.5.2) двумерный вариант убрал совсем.
            rel = chunk - a
            dist = np.abs(seg[0] * rel[:, 1] - seg[1] * rel[:, 0]) / length
        offset = int(np.argmax(dist))
        if dist[offset] > tolerance:
            split = first + 1 + offset
            keep[split] = True
            stack.append((first, split))
            stack.append((split, last))
    return np.nonzero(keep)[0].tolist()


def simplify_loop(loop, points, tolerance: float):
    """Дуглас — Пойкер на ЗАМКНУТОМ контуре: разрезаем в самой дальней от
    первой точке, упрощаем две половины отдельно и сшиваем. Разрез в
    произвольном месте прибил бы к контуру две случайные точки как якоря.
    """
    if len(loop) < 4:
        return list(loop)
    pts = np.asarray([points[i] for i in loop], dtype=np.float64)
    far = int(np.argmax(np.hypot(*(pts - pts[0]).T)))
    first = [loop[i] for i in douglas_peucker(pts[:far + 1], tolerance)]
    second = [loop[far + i] for i in douglas_peucker(pts[far:], tolerance)]
    return first + second[1:-1]


def sample_bilinear(field: np.ndarray, xs: np.ndarray, ys: np.ndarray) -> np.ndarray:
    """Билинейная выборка поля в произвольных точках (векторно)."""
    height, width = field.shape
    xs = np.clip(xs, 0.0, width - 1.001)
    ys = np.clip(ys, 0.0, height - 1.001)
    x0 = xs.astype(np.int64)
    y0 = ys.astype(np.int64)
    tx = xs - x0
    ty = ys - y0
    return (field[y0, x0] * (1 - tx) * (1 - ty)
            + field[y0, x0 + 1] * tx * (1 - ty)
            + field[y0 + 1, x0] * (1 - tx) * ty
            + field[y0 + 1, x0 + 1] * tx * ty)
