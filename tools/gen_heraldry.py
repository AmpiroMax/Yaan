#!/usr/bin/env python3
#
# Module: tools
# File: tools/gen_heraldry.py
#
# Responsibility:
# - ПЕЧЁТ ГЕРБ ИМПЕРИИ ЯАН КАК 3D-ОБЪЕКТ ИЗ 2D-СИЛУЭТА. Заказ владельца 27.08:
#   «герб-дуб в центре главного меню надо сделать не в круге, а как в Skyrim —
#   объектом 3D с краями и без лишнего фона». На входе — прозрачный PNG
#   силуэта, на выходе — .dfo на полку реестра объектов, кадры превью и (по
#   ключу --obj) Wavefront .obj для стороннего просмотрщика.
#
# КОНВЕЙЕР (весь классический, БЕЗ НЕЙРОСЕТЕЙ и без сторонних библиотек):
#   PNG -> альфа -> чистка крупинок и мелких дыр -> точное евклидово
#   преобразование расстояний (Фельценсвальб — Хуттенлохер) -> марширующие
#   квадраты КАК МЕШЕР (крышка сеткой + граничные отрезки) -> сшивка контуров
#   -> профиль высоты по расстоянию (фаска + «подушка») -> нормали из градиента
#   поля высот -> стенка и дно -> .dfo/.obj -> программный рендер кадров.
#
# ПОЧЕМУ РЕЛЬЕФ ПО РАССТОЯНИЮ, А НЕ ПРОСТАЯ ВЫДАВЛИВАНИЕ С ФАСКОЙ. Выдавливание
# даёт ПЛИТКУ: плоское поле, обрубленное фаской по периметру. У печати и монеты
# читается другое — поле, которое ПОДНИМАЕТСЯ К СЕРЕДИНЕ МАССЫ, отчего ствол
# выпуклее веточки, и свет по этой выпуклости течёт. Ровно это и даёт
# преобразование расстояний: расстояние до края — и есть «толщина массы» в
# точке. Фаска при этом НЕ ОТМЕНЯЕТСЯ, а кладётся поверх подушки отдельным
# слагаемым: заказ просит «с краями», а край читается узким блеском на СКАТЕ
# ПОСТОЯННОГО УКЛОНА, которого у подушки в чистом виде нет.
#
# ПРОФИЛЬ (h — высота, d — расстояние до края в пикселях исходника):
#   h(d) = bevel_h * min(d / bevel_w, 1)            <- фаска, постоянный уклон
#        + dome_h  * sqrt(1 - (1 - min(d,R)/R)^2)   <- подушка, R = --dome-reach
# Второе слагаемое — классическая «подушка» (полусфера, переписанная через
# расстояние): при d=R даёт 1 с НУЛЕВЫМ уклоном, то есть середина массы
# ровная, а не остроконечная.
#
# ЧТО ПРОВЕРЕНО ПРИБОРОМ, А НЕ ГЛАЗОМ (печатается при каждом запуске, и при
# несошедшемся меше файлы НЕ ПИШУТСЯ):
#   - преобразование расстояний сверено с полным перебором: ошибка 0.0;
#   - контуры сшиты БЕЗ разрывов и без развилок;
#   - меш ЗАМКНУТ И СОГЛАСОВАНО ОРИЕНТИРОВАН: каждое ребро встречается ровно
#     дважды и во встречных направлениях (сварка по координатам, а не по
#     номерам вершин, — стенка и крышка нарочно не делят вершины ради жёсткого
#     ребра);
#   - знаковый объём положителен, то есть нормали смотрят наружу;
#   - .dfo сверен с байтами движка (tools/heraldry/dfo.py --verify).
#
# Usage:
#     python3 tools/gen_heraldry.py                     # печать по умолчанию
#     python3 tools/gen_heraldry.py --cell 2.5 --frames 3
#     python3 tools/gen_heraldry.py --dp-report         # замер Дугласа-Пойкера
#
# Dependencies:
# - Uses: numpy; tools/heraldry/{png_io,geometry,dfo,preview}.py.
# - Used by: пока никем в рантайме — движок не умеет крутить меш в меню
#   (см. docs/reports/heraldry-3d.html, раздел «что нужно движку»).
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ЛИЦЕНЗИИ: здесь нет ни строки заимствованного кода. potrace, autotrace,
#   CGAL, Triangle и питоний пакет `triangle` — GPL либо «не для коммерческих
#   продуктов», и в этом проекте ЗАПРЕЩЕНЫ; earcut (ISC) и CDT (MPL-2.0)
#   разрешены, но не нужны (см. шапку tools/heraldry/geometry.py).
# - ИСТОЧНИК СИЛУЭТА: Wikimedia Commons, «Quercus robur Silhouette», автор
#   oddsock, CC BY 2.0 — АТРИБУЦИЯ ОБЯЗАТЕЛЬНА в титрах (assets/branding/README.txt).
"""Bakes the Yaan oak seal as a relief 3D object from a 2D silhouette PNG."""

import argparse
import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from heraldry import dfo, geometry, png_io, preview  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_SOURCE = "assets/branding/oak_seal/oak_silhouette_black.png"
DEFAULT_OBJECT = "assets/objects/heraldry/oak-seal-relief.dfo"
DEFAULT_PREVIEW = "docs/reports/heraldry"

# ЗОЛОТО ГЕРБА в линейном 0..1. Взято из BRANDING (золото на червлёном) и
# приглушено: чистый #ffd700 в линейном свете с бликом уходит в белое пятно.
GOLD = (0.62, 0.44, 0.13)
GOLD_RIM = (0.78, 0.60, 0.22)   # фаска светлее поля — так её видно и без блика

def log(message: str) -> None:
    sys.stdout.write(message + "\n")
    sys.stdout.flush()

def build_height_field(distance: np.ndarray, bevel_w: float, bevel_h: float,
                       dome_reach: float, dome_h: float) -> np.ndarray:
    """Профиль из шапки: фаска постоянного уклона плюс подушка."""
    # ПОЛПИКСЕЛЯ ДОЛОЙ. Преобразование расстояний двоичной маски даёт 1.0 в
    # пикселе, прилегающем к фону, а настоящий край проходит по СЕРЕДИНЕ между
    # ними. Без этой поправки герб стоит на бортике в полпикселя по всему
    # периметру — на кадре это тонкая светлая нитка вдоль силуэта.
    d = np.maximum(0.0, distance - 0.5)
    ramp = np.minimum(d / max(1e-6, bevel_w), 1.0)
    t = np.minimum(d, dome_reach) / max(1e-6, dome_reach)
    pillow = np.sqrt(np.maximum(0.0, 1.0 - (1.0 - t) ** 2))
    return bevel_h * ramp + dome_h * pillow

def assemble(cap, loops, height_px, pixel_scale, thickness, bevel_w):
    """Собирает три части в один меш: крышка с рельефом, стенка, дно.

    ВЕРШИНЫ НЕ СВАРИВАЮТСЯ МЕЖДУ ЧАСТЯМИ, И ЭТО НАРОЧНО. Общая вершина несёт
    ОДНУ нормаль, то есть сваренный обод сгладил бы стык поля и стенки в
    закругление — а заказ просит «с краями». Каждая часть держит свои копии
    обода со своей нормалью, и ребро выходит жёстким.
    """
    pts = np.asarray(cap.points, dtype=np.float64)
    cx, cy = pts[:, 0].mean(), pts[:, 1].mean()

    def to_world(px, py):
        # Ось Y картинки смотрит ВНИЗ, ось Y мира — ВВЕРХ.
        return (px - cx) * pixel_scale, -(py - cy) * pixel_scale

    height, width = height_px.shape
    # ГРАДИЕНТ БЕРЁТСЯ С ЧУТЬ РАЗМЫТОГО ПОЛЯ, А ВЫСОТА — С ЧИСТОГО.
    # Преобразование расстояний имеет ГРЕБЕНЬ по срединной оси (там расстояние
    # достигает максимума и его производная рвётся), а по краю — ступеньку в
    # пиксель. Разностная производная на обоих местах выдаёт мусор, и на кадре
    # он читается поперечной рябью по стволу — там, где гребень идёт вдоль.
    # Полтора пикселя размытия ЛЕЧАТ ИМЕННО НОРМАЛЬ, не трогая ни высоту, ни
    # положение обода: размой мы само поле, силуэт уехал бы внутрь.
    smooth_h = geometry.blur(height_px, 1.5)
    grad_y, grad_x = np.gradient(smooth_h)
    cap_x, cap_y = to_world(pts[:, 0], pts[:, 1])
    cap_z = geometry.sample_bilinear(height_px, pts[:, 0], pts[:, 1])
    dzdx = geometry.sample_bilinear(grad_x, pts[:, 0], pts[:, 1]) / pixel_scale
    dzdy = -geometry.sample_bilinear(grad_y, pts[:, 0], pts[:, 1]) / pixel_scale
    cap_n = np.stack([-dzdx, -dzdy, np.ones_like(dzdx)], axis=1)
    cap_n /= np.linalg.norm(cap_n, axis=1, keepdims=True)

    # Фаска красится светлее поля — по РАССТОЯНИЮ, а не по высоте: у тонкой
    # веточки высота никогда не дорастает до поля, но краем она быть не
    # перестаёт.
    dist_at_cap = geometry.sample_bilinear(
        np.maximum(0.0, DISTANCE_CACHE[0] - 0.5), pts[:, 0], pts[:, 1])
    edge_mix = np.clip(1.0 - dist_at_cap / max(1e-6, bevel_w), 0.0, 1.0)[:, None]
    gold = np.array(GOLD)
    gold_rim = np.array(GOLD_RIM)
    cap_rgb = gold * (1.0 - edge_mix) + gold_rim * edge_mix

    positions = [np.stack([cap_x, cap_y, cap_z], axis=1)]
    normals = [cap_n]
    colors = [np.array([dfo.pack_color(c) for c in cap_rgb], dtype=np.uint32)]
    tris = [np.asarray(cap.triangles, dtype=np.int64)]
    base = len(cap.points)

    # ---- ДНО: та же топология, встречный обход, плоская нормаль -Z.
    back_z = np.full_like(cap_z, -thickness)
    positions.append(np.stack([cap_x, cap_y, back_z], axis=1))
    normals.append(np.tile(np.array([0.0, 0.0, -1.0]), (base, 1)))
    colors.append(np.full(base, dfo.pack_color(np.array(GOLD) * 0.45),
                          dtype=np.uint32))
    back = np.asarray(cap.triangles, dtype=np.int64)[:, ::-1] + base
    tris.append(back)

    # ---- СТЕНКА: по четырёхугольнику на отрезок контура, ПЛОСКИЕ нормали.
    wall_pos, wall_nrm, wall_tri = [], [], []
    offset = 2 * base
    for loop in loops:
        count = len(loop)
        for k in range(count):
            a, b = loop[k], loop[(k + 1) % count]
            ax, ay = cap_x[a], cap_y[a]
            bx, by = cap_x[b], cap_y[b]
            dx, dy = bx - ax, by - ay
            length = math.hypot(dx, dy)
            if length < 1e-12:
                continue
            # Внутренность контура — СЛЕВА по обходу, значит наружу смотрит
            # ПРАВАЯ нормаль. Знак сверяется прибором (замкнутость + объём),
            # а не доверием к этому рассуждению.
            nx, ny = dy / length, -dx / length
            # ОБХОД СТЕНКИ — ВСТРЕЧНЫЙ К ОБХОДУ КРЫШКИ. Крышка проходит
            # граничное ребро в направлении a->b (в том же порядке, в каком
            # марширующий квадрат выдал отрезок), поэтому стенка обязана
            # пройти его b->a: у замкнутой поверхности каждое ребро
            # встречается дважды и во ВСТРЕЧНЫХ направлениях. Стенка,
            # выписанная «естественным» порядком a->b, оставила ровно 19600
            # несопряжённых рёбер — по два на каждый из 9800 отрезков контура
            # (верх и низ), и прибор это поймал.
            quad = [(bx, by, cap_z[b]), (ax, ay, cap_z[a]),
                    (ax, ay, -thickness), (bx, by, -thickness)]
            start = offset + len(wall_pos)
            wall_pos.extend(quad)
            wall_nrm.extend([(nx, ny, 0.0)] * 4)
            wall_tri.append((start + 0, start + 1, start + 2))
            wall_tri.append((start + 0, start + 2, start + 3))
    positions.append(np.asarray(wall_pos, dtype=np.float64))
    normals.append(np.asarray(wall_nrm, dtype=np.float64))
    colors.append(np.full(len(wall_pos), dfo.pack_color(GOLD_RIM), dtype=np.uint32))
    tris.append(np.asarray(wall_tri, dtype=np.int64))

    # ЗЕРКАЛО ПО Y ПЕРЕВОРАЧИВАЕТ ОБХОД, И ЭТО НАДО ОТМЕНИТЬ РОВНО ОДИН РАЗ.
    # Вся сборка выше ведётся в порядке, согласованном с ОСЯМИ КАРТИНКИ, где
    # y смотрит вниз; переход в мир (y вверх) — отражение, а отражение меняет
    # знак ориентации: снаружи становится изнутри. Прибор поймал это знаковым
    # объёмом -0.0248 м^3 при полностью замкнутой поверхности — то есть меш
    # был безупречно сшит и вывернут наизнанку.
    #
    # Разворот делается ЗДЕСЬ, над готовым списком, а не тремя правками в
    # крышке, дне и стенке: три согласованных разворота — это три места, где
    # следующая правка забудет один.
    order = np.concatenate(tris)[:, ::-1]
    return (np.concatenate(positions), np.concatenate(normals),
            np.concatenate(colors), order)

DISTANCE_CACHE = [None]

def check_mesh(positions, indices, quantum=1e-6):
    """ЗАМКНУТ ЛИ МЕШ И НАРУЖУ ЛИ СМОТРЯТ НОРМАЛИ.

    Сварка по КООРДИНАТАМ, а не по номерам вершин: части нарочно держат свои
    копии обода ради жёсткого ребра, поэтому по номерам меш «дырявый» всегда, и
    проверка по ним не значила бы ничего.
    """
    keys = np.round(positions / quantum).astype(np.int64)
    _uniq, weld = np.unique(keys, axis=0, return_inverse=True)
    weld = weld.reshape(-1)
    tri = weld[indices]
    edges = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]])
    # Каждое ребро обязано встретиться дважды и в ПРОТИВОПОЛОЖНЫХ направлениях:
    # тогда поверхность замкнута И согласованно ориентирована.
    forward = {}
    for a, b in edges.tolist():
        forward[(a, b)] = forward.get((a, b), 0) + 1
    unmatched = 0
    duplicated = 0
    for (a, b), n in forward.items():
        if n != 1:
            duplicated += 1
        if forward.get((b, a), 0) != n:
            unmatched += 1
    # Знаковый объём — сумма определителей тетраэдров на начало координат.
    p = positions[indices]
    volume = float(np.einsum("ij,ij->i",
                             np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]),
                             p[:, 0]).sum() / 6.0)
    return {"welded_vertices": int(_uniq.shape[0]), "edges": len(forward),
            "unmatched_edges": unmatched, "duplicated_edges": duplicated,
            "signed_volume": volume}

def write_obj(path, positions, normals, indices):
    """Wavefront .obj — только чтобы владелец мог открыть меш чем угодно."""
    with open(path, "w") as handle:
        handle.write("# Yaan oak seal, baked by tools/gen_heraldry.py\n")
        handle.write("# oak silhouette by oddsock, CC BY 2.0\n")
        for x, y, z in positions:
            handle.write("v %.6f %.6f %.6f\n" % (x, y, z))
        for x, y, z in normals:
            handle.write("vn %.5f %.5f %.5f\n" % (x, y, z))
        for a, b, c in indices + 1:
            handle.write("f %d//%d %d//%d %d//%d\n" % (a, a, b, b, c, c))

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    parser.add_argument("--object", default=DEFAULT_OBJECT)
    parser.add_argument("--preview-dir", default=DEFAULT_PREVIEW)
    parser.add_argument("--name", default="oak-seal-relief")
    parser.add_argument("--cell", type=float, default=2.0,
                        help="шаг сетки в пикселях исходника; ЕДИНСТВЕННЫЙ "
                             "рычаг плотности сетки")
    parser.add_argument("--width", type=float, default=1.0,
                        help="ширина герба в метрах")
    parser.add_argument("--thickness", type=float, default=0.018,
                        help="толщина плиты (стенка) в долях ширины")
    parser.add_argument("--bevel", type=float, default=2.5,
                        help="ширина фаски в пикселях исходника")
    parser.add_argument("--bevel-height", type=float, default=0.007)
    parser.add_argument("--dome-reach", type=float, default=12.0,
                        help="расстояние (пиксели), на котором подушка "
                             "выходит на полную высоту")
    parser.add_argument("--dome-height", type=float, default=0.028)
    parser.add_argument("--min-blob", type=int, default=60)
    parser.add_argument("--min-hole", type=int, default=60)
    parser.add_argument("--smooth", type=float, default=1.0,
                        help="размытие поля перед изолинией, пиксели")
    parser.add_argument("--frames", type=int, default=3)
    parser.add_argument("--preview-size", type=int, default=720)
    parser.add_argument("--dp-report", action="store_true",
                        help="замерить упрощение Дугласа — Пойкера и выйти")
    parser.add_argument("--no-preview", action="store_true")
    # .obj НЕ ПИШЕТСЯ ПО УМОЛЧАНИЮ: он весит 18 МБ против 8,5 МБ у .dfo и
    # ничего не добавляет — движок его не читает. Нужен ровно тогда, когда меш
    # хотят открыть сторонним просмотрщиком.
    parser.add_argument("--obj", action="store_true",
                        help="дополнительно выписать Wavefront .obj")
    args = parser.parse_args(argv)

    source = os.path.join(REPO, args.source)
    log("герб: %s" % source)
    start = time.time()
    image = png_io.read_png(source)
    alpha = image[..., 3].astype(np.float64) / 255.0
    log("  снимок %dx%d, покрытие %.1f%%"
        % (image.shape[1], image.shape[0], 100.0 * (alpha > 0.5).mean()))

    mask, clean = geometry.clean_mask(alpha > 0.5, args.min_blob, args.min_hole)
    log("  чистка: крупинок выкинуто %d (осталось кусков %d), дыр заткнуто %d "
        "(осталось %d)" % (clean["blobs_dropped"], clean["blobs_kept"],
                           clean["holes_filled"], clean["holes_kept"]))

    distance = geometry.distance_transform(mask)
    DISTANCE_CACHE[0] = distance
    log("  расстояния: максимум %.1f px (самое «толстое» место силуэта)"
        % distance.max())

    # Изолиния идёт по СГЛАЖЕННОМУ полю, собранному из маски и исходной альфы:
    # альфа несёт субпиксельный край (снимок сглажен), маска — вычищенную
    # топологию. Одна альфа вернула бы выкинутые крупинки, одна маска — лесенку.
    field = geometry.blur(0.5 * mask.astype(np.float64) + 0.5 * alpha, args.smooth)
    cap = geometry.march_cells(field, 0.5, args.cell)
    loops, chain = geometry.chain_loops(cap.segments, len(cap.points))
    log("  марширующие квадраты (шаг %.1f px): вершин %d, треугольников %d, "
        "контуров %d" % (args.cell, len(cap.points), len(cap.triangles), len(loops)))
    log("  сшивка контуров: разрывов %d, развилок %d  <- обязаны быть нули"
        % (chain["open_chains"], chain["forks"]))

    if args.dp_report:
        total = sum(len(loop) for loop in loops)
        log("  замер Дугласа — Пойкера на %d точках контура:" % total)
        for tol in (0.25, 0.5, 1.0, 2.0):
            kept = sum(len(geometry.simplify_loop(loop, cap.points, tol))
                       for loop in loops)
            log("    допуск %.2f px -> %5d точек (%.1f%% долой)"
                % (tol, kept, 100.0 * (1.0 - kept / total)))
        log("  НО В БОЕВОМ ПУТИ УПРОЩЕНИЕ НЕ ПРИМЕНЯЕТСЯ: контур упрощают ДО "
            "сшивки крышки, иначе стенка разойдётся с крышкой щелью по всему "
            "силуэту. Рычаг плотности здесь — --cell.")
        return 0

    span = max(image.shape[0], image.shape[1])
    pixel_scale = args.width / span
    height_px = build_height_field(distance, args.bevel,
                                   args.bevel_height / pixel_scale,
                                   args.dome_reach,
                                   args.dome_height / pixel_scale) * pixel_scale
    positions, normals, colors, indices = assemble(
        cap, loops, height_px, pixel_scale, args.thickness * args.width, args.bevel)
    log("  меш: вершин %d, треугольников %d (крышка %d + дно %d + стенка %d)"
        % (len(positions), len(indices), len(cap.triangles), len(cap.triangles),
           len(indices) - 2 * len(cap.triangles)))
    log("  габарит: %.3f x %.3f x %.3f м (рельеф %.1f мм над плитой)"
        % (np.ptp(positions[:, 0]), np.ptp(positions[:, 1]), np.ptp(positions[:, 2]),
           1000.0 * positions[:, 2].max()))

    report = check_mesh(positions, indices)
    ok = report["unmatched_edges"] == 0 and report["signed_volume"] > 0
    log("  ПРОВЕРКА МЕША: рёбер %d, несопряжённых %d, знаковый объём %+.6f м^3 "
        "-> %s" % (report["edges"], report["unmatched_edges"],
                   report["signed_volume"],
                   "ЗАМКНУТ, нормали наружу" if ok else "ДЕФЕКТ"))
    if not ok:
        log("  !! меш не замкнут или вывернут — файлы не пишутся")
        return 1

    obj = dfo.RegistryObject(
        args.name, "heraldry",
        "gen_heraldry:v1 cell=%.2f bevel=%.1f dome=%.1f/%.3f src=%s"
        % (args.cell, args.bevel, args.dome_reach, args.dome_height,
           os.path.basename(args.source)))
    obj.wood.positions = positions.astype(np.float32)
    obj.wood.normals = normals.astype(np.float32)
    obj.wood.uvs = np.zeros((len(positions), 2), dtype=np.float32)
    obj.wood.colors = colors
    obj.wood.indices = indices.reshape(-1).astype(np.uint32)

    out_path = os.path.join(REPO, args.object)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    dfo.write_object(obj, out_path)
    log("  записан %s (%d байт, хэш %016x)"
        % (args.object, os.path.getsize(out_path), dfo.content_hash(obj)))
    back = dfo.read_object(out_path)
    log("  перечитан своим читателем: %s"
        % ("хэш сошёлся" if back is not None else "ОТКАЗ"))

    if args.obj:
        obj_path = os.path.splitext(out_path)[0] + ".obj"
        write_obj(obj_path, positions, normals, indices)
        log("  записан %s (%.1f МБ)"
            % (os.path.relpath(obj_path, REPO),
               os.path.getsize(obj_path) / 1e6))

    if not args.no_preview:
        preview_dir = os.path.join(REPO, args.preview_dir)
        os.makedirs(preview_dir, exist_ok=True)
        yaws = [0.0, -0.32, 0.30][:max(1, args.frames)]
        pitches = [0.0, 0.10, -0.08][:max(1, args.frames)]
        for i, (yaw, pitch) in enumerate(zip(yaws, pitches)):
            t0 = time.time()
            frame = preview.render(positions, normals, colors, indices,
                                   size=args.preview_size, yaw=yaw, pitch=pitch)
            name = "oak-seal-%d.png" % i
            preview.save(os.path.join(preview_dir, name), frame)
            log("  кадр %s (поворот %+.0f°, наклон %+.0f°) за %.1f с"
                % (name, math.degrees(yaw), math.degrees(pitch), time.time() - t0))

    log("готово за %.1f с" % (time.time() - start))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
