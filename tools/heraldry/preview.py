#!/usr/bin/env python3
#
# Module: tools
# File: tools/heraldry/preview.py
#
# Responsibility:
# - ОФЛАЙН-РЕНДЕР МЕША В PNG: программный растеризатор с буфером глубины,
#   ламбертом, зеркальным бликом Блинна — Фонга и подсветкой контура. Нужен для
#   ОДНОЙ вещи: показать владельцу будущий герб до того, как движок научится
#   крутить меш в меню.
#
# ПОЧЕМУ ЭТО НЕ «ЛИШНИЙ РЕНДЕРЕР». Он ничего не решает в игре и не попадает в
# рантайм: это ПРИБОР, доказывающий, что запечённый меш — действительно объём с
# краями, а не картинка. Меш, показанный числом треугольников, не доказывает
# ничего: рельеф оценивают глазом по бликам, и других глаз, кроме кадра, у
# заказа нет.
#
# ПОЧЕМУ БЛИК, А НЕ ОДИН ЛИШЬ ЛАМБЕРТ. Заказ звучит как «как в Skyrim»: там
# эмблема читается ровно потому, что фаска ловит УЗКИЙ блик и отделяет край от
# поля. Чистый ламберт даёт ту же геометрию плоской и увёл бы приёмку в
# «рельефа не видно» — при том, что рельеф есть.
#
# Dependencies:
# - Uses: numpy, tools/heraldry/png_io.py.
# - Used by: tools/gen_heraldry.py.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - Освещение здесь — НЕ контракт с движком, а витрина. Не переносите эти
#   числа в шейдер как «утверждённые»: свет меню назначает зона app.
"""Tiny software rasteriser: z-buffer, Lambert + Blinn-Phong, for preview PNGs."""

import numpy as np

from . import png_io

def _normalize(vec):
    vec = np.asarray(vec, dtype=np.float64)
    return vec / max(1e-12, float(np.linalg.norm(vec)))

def render(positions, normals, colors, indices, size=768, yaw=0.0, pitch=0.0,
           background=None, supersample=2,
           light_dir=(-0.45, 0.55, 0.70), fill_dir=(0.6, -0.2, 0.45),
           specular=0.55, shininess=48.0, ambient=0.13, margin=0.88):
    """Рисует меш и возвращает RGBA8 (size, size, 4).

    Камера ортографическая: у эмблемы в меню нет глубины сцены, а перспектива
    на плоском объекте только кривит силуэт. Сглаживание — суперсэмплингом:
    краевая фаска шириной в пиксель без него превращается в лесенку, то есть
    ровно в тот дефект, из-за которого заказ и начался.
    """
    res = size * supersample
    positions = np.asarray(positions, dtype=np.float64)
    normals = np.asarray(normals, dtype=np.float64)
    indices = np.asarray(indices, dtype=np.int64).reshape(-1, 3)

    cos_y, sin_y = np.cos(yaw), np.sin(yaw)
    cos_p, sin_p = np.cos(pitch), np.sin(pitch)
    rot_y = np.array([[cos_y, 0, sin_y], [0, 1, 0], [-sin_y, 0, cos_y]])
    rot_x = np.array([[1, 0, 0], [0, cos_p, -sin_p], [0, sin_p, cos_p]])
    rot = rot_x @ rot_y
    view_pos = positions @ rot.T
    view_nrm = normals @ rot.T

    # Кадрирование по НЕПОВЁРНУТОМУ силуэту: иначе герб дышал бы от кадра к
    # кадру, и поворот читался бы как наезд камеры.
    half = float(np.abs(positions[:, :2]).max())
    scale = (res * 0.5 * margin) / max(1e-9, half)
    sx = view_pos[:, 0] * scale + res * 0.5
    sy = res * 0.5 - view_pos[:, 1] * scale
    sz = view_pos[:, 2]

    depth = np.full((res, res), -1e30)
    accum = np.zeros((res, res, 3))
    cover = np.zeros((res, res), dtype=bool)

    key = _normalize(light_dir)
    fill = _normalize(fill_dir)
    eye = np.array([0.0, 0.0, 1.0])
    half_key = _normalize(key + eye)

    tri_x = sx[indices]
    tri_y = sy[indices]
    tri_z = sz[indices]
    # ОТСЕВ ЗАДНИХ ГРАНЕЙ ЗНАКОМ ПЛОЩАДИ. Он же ловит вырожденные треугольники,
    # которых на границе изолинии всегда несколько.
    area = ((tri_x[:, 1] - tri_x[:, 0]) * (tri_y[:, 2] - tri_y[:, 0])
            - (tri_x[:, 2] - tri_x[:, 0]) * (tri_y[:, 1] - tri_y[:, 0]))
    visible = np.nonzero(area < -1e-9)[0]

    lo_x = np.maximum(0, np.floor(tri_x.min(axis=1)).astype(int))
    hi_x = np.minimum(res - 1, np.ceil(tri_x.max(axis=1)).astype(int))
    lo_y = np.maximum(0, np.floor(tri_y.min(axis=1)).astype(int))
    hi_y = np.minimum(res - 1, np.ceil(tri_y.max(axis=1)).astype(int))

    rgb = np.stack([(colors & 0xFF), (colors >> 8) & 0xFF,
                    (colors >> 16) & 0xFF], axis=1).astype(np.float64) / 255.0

    for t in visible.tolist():
        x0, x1, y0, y1 = lo_x[t], hi_x[t], lo_y[t], hi_y[t]
        if x1 < x0 or y1 < y0:
            continue
        ax, bx, cx_ = tri_x[t]
        ay, by, cy_ = tri_y[t]
        inv_area = 1.0 / area[t]
        px = np.arange(x0, x1 + 1) + 0.5
        py = np.arange(y0, y1 + 1) + 0.5
        gx = px[None, :]
        gy = py[:, None]
        w0 = ((bx - ax) * (gy - ay) - (gx - ax) * (by - ay)) * inv_area
        w1 = ((gx - ax) * (cy_ - ay) - (cx_ - ax) * (gy - ay)) * inv_area
        w2 = 1.0 - w0 - w1
        hit = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not hit.any():
            continue
        # w2 -> вершина 0, w1 -> вершина 1, w0 -> вершина 2 (см. вывод выше).
        zz = w2 * tri_z[t, 0] + w1 * tri_z[t, 1] + w0 * tri_z[t, 2]
        window = depth[y0:y1 + 1, x0:x1 + 1]
        closer = hit & (zz > window)
        if not closer.any():
            continue
        idx = indices[t]
        nrm = (w2[..., None] * view_nrm[idx[0]] + w1[..., None] * view_nrm[idx[1]]
               + w0[..., None] * view_nrm[idx[2]])
        nrm /= np.maximum(1e-9, np.linalg.norm(nrm, axis=-1, keepdims=True))
        base = (w2[..., None] * rgb[idx[0]] + w1[..., None] * rgb[idx[1]]
                + w0[..., None] * rgb[idx[2]])
        lam = np.clip(nrm @ key, 0.0, 1.0)
        fil = np.clip(nrm @ fill, 0.0, 1.0) * 0.32
        spec = np.clip(nrm @ half_key, 0.0, 1.0) ** shininess * specular
        # Подсветка контура: грани, отвёрнутые от камеры, — это край объёма, и
        # именно она отделяет герб от чёрного фона меню.
        rim = (1.0 - np.clip(np.abs(nrm[..., 2]), 0.0, 1.0)) ** 3 * 0.30
        shade = base * (ambient + lam + fil)[..., None] + (spec + rim)[..., None]
        window[closer] = zz[closer]
        accum[y0:y1 + 1, x0:x1 + 1][closer] = shade[closer]
        cover[y0:y1 + 1, x0:x1 + 1][closer] = True

    image = np.clip(accum, 0.0, 1.0) ** (1.0 / 2.2)  # линейный свет -> sRGB
    image *= cover[..., None]
    alpha = cover.astype(np.float64)
    if supersample > 1:
        shape = (size, supersample, size, supersample)
        image = image.reshape(size, supersample, size, supersample, 3).mean((1, 3))
        alpha = alpha.reshape(shape).mean((1, 3))
    # ЦВЕТ ДЕЛИТСЯ НА ПОКРЫТИЕ (снятие предумножения). Краевой пиксель накрыт
    # гербом наполовину, и сумма цвета в нём вдвое темнее самого герба; отдать
    # её как есть значит обвести силуэт тёмной ниткой, которая проступит на
    # любом фоне светлее чёрного. Поделив на покрытие, отдаём ЧИСТЫЙ цвет
    # герба и отдельно честную альфу — такой кадр верно ложится и на чёрное
    # меню, и на белую страницу отчёта.
    safe = np.maximum(alpha, 1e-6)[..., None]
    image = np.where(alpha[..., None] > 0, image / safe, 0.0)
    if background is not None:
        bg = np.array(background, dtype=np.float64) / 255.0
        image = image * alpha[..., None] + bg * (1.0 - alpha[..., None])
    out = np.empty((size, size, 4), dtype=np.uint8)
    out[..., :3] = np.round(image * 255.0)
    out[..., 3] = np.round(np.clip(alpha, 0, 1) * 255.0)
    return out

def save(path: str, image: np.ndarray) -> None:
    png_io.write_png(path, image)
