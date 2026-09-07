#!/usr/bin/env python3
"""
Module: tools
File: tools/skin_layers.py

Responsibility:
- СЛОИ КОЖИ ОФЛАЙН (docs/design/CHARACTER_SKIN_HAIR_FACE.md, волна 5): из
  альбедо кожи MPFB и спеки персонажа (*.skin, JSON) собрать лист альбедо с
  грязью и шрамами и лист нормалей (поры, рельеф по альбедо, рубцы шрамов).
  Области тела (кисти, предплечья, голени, стопы, голова…) — маски в UV,
  растеризованные из весов костей самого glb: никаких нарисованных масок.
- Выход идёт в выпечку (make_human_body.py --skin-albedo/--skin-normal), а та
  кладёт листы рядом с glb и пишет SHA256SUMS; сюда — только картинки.

Key items:
- region_masks(): растеризация треугольников glb в UV по доминирующей области.
- fbm(): шум значений, октавы — грязь и поры.
- scar_field(): высота и цвет рубца вдоль отрезка в UV.
- compose(): альбедо + грязь + шрамы; height → нормаль (tangent space, +Y вверх).
- preview: вырез лица (правый остров, повёрнут) для глаз агента и владельца.

Dependencies:
- Uses: numpy, Pillow (conda base/music: питон только conda, python-only-conda).
- Used by: make_human_body.py (через файлы), волна кожи.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Детерминировано: seed из спеки; один и тот же вход — побитово тот же лист.
- Не переписывать альбедо в дереве напрямую: sha256 в .dfo — контракт, лист
  меняется только через выпечку и импорт.
"""
import argparse
import json
import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REGIONS = ["head", "hand", "forearm", "upperarm", "torso", "thigh", "shin", "foot"]


def region_of_joint(name):
    n = name
    if "head" in n or "neck" in n:
        return "head"
    if "hand" in n or "f_" in n or "thumb" in n:
        return "hand"
    if "forearm" in n:
        return "forearm"
    if "upper_arm" in n or "shoulder" in n:
        return "upperarm"
    if "thigh" in n:
        return "thigh"
    if "shin" in n:
        return "shin"
    if "foot" in n or "toe" in n:
        return "foot"
    return "torso"


def read_glb(path):
    d = open(path, "rb").read()
    ln = struct.unpack("<I", d[12:16])[0]
    js = json.loads(d[20:20 + ln])
    bo = 20 + ln
    bl = struct.unpack("<I", d[bo:bo + 4])[0]
    bin_ = d[bo + 8:bo + 8 + bl]

    def acc(i):
        a = js["accessors"][i]
        bv = js["bufferViews"][a["bufferView"]]
        off = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        ct = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a["componentType"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        return np.frombuffer(bin_, dtype=ct, count=a["count"] * n, offset=off).reshape(a["count"], n)
    p = js["meshes"][0]["primitives"][0]
    uv = acc(p["attributes"]["TEXCOORD_0"]).astype(np.float64)
    J = acc(p["attributes"]["JOINTS_0"])
    W = acc(p["attributes"]["WEIGHTS_0"]).astype(np.float64)
    idx = acc(p["indices"]).reshape(-1, 3)
    names = [js["nodes"][j]["name"] for j in js["skins"][0]["joints"]]
    return uv, J, W, idx, names


def region_masks(glb, size):
    """Маски областей [0..1] размером size², по доминирующей области вершин."""
    uv, J, W, idx, names = read_glb(glb)
    jr = [REGIONS.index(region_of_joint(n)) for n in names]
    nv = len(uv)
    acc = np.zeros((nv, len(REGIONS)))
    for k in range(4):
        r = np.array([jr[int(j)] for j in J[:, k]])
        acc[np.arange(nv), r] += W[:, k]
    vr = acc.argmax(axis=1)
    imgs = {r: Image.new("L", (size, size), 0) for r in REGIONS}
    draws = {r: ImageDraw.Draw(imgs[r]) for r in REGIONS}
    for t in idx:
        rs = [vr[i] for i in t]
        r = REGIONS[max(set(rs), key=rs.count)]
        draws[r].polygon([(float(uv[i, 0]) * size, float(uv[i, 1]) * size) for i in t], fill=255)
    out = {}
    for r in REGIONS:
        im = imgs[r].filter(ImageFilter.GaussianBlur(size / 512.0))
        out[r] = np.asarray(im, dtype=np.float64) / 255.0
    return out


def value_noise(rng, size, cell):
    n = max(2, int(math.ceil(size / cell)) + 1)
    g = rng.random((n, n))
    im = Image.fromarray((g * 255).astype(np.uint8), "L").resize((size, size), Image.BICUBIC)
    return np.asarray(im, dtype=np.float64) / 255.0


def fbm(rng, size, cell, octaves):
    h = np.zeros((size, size))
    amp = 1.0
    tot = 0.0
    c = float(cell)
    for _ in range(octaves):
        h += amp * value_noise(rng, size, max(1.0, c))
        tot += amp
        amp *= 0.5
        c *= 0.5
    return h / tot


def scar_field(size, scar, rng):
    """Высота рубца (0..1) и маска цвета вдоль отрезка спеки."""
    u0, v0 = scar["uv"]
    L = scar["len"]
    a = math.radians(scar["angle_deg"])
    w = scar["width"]
    dx, dy = math.cos(a), math.sin(a)
    ys, xs = np.mgrid[0:size, 0:size]
    u = (xs + 0.5) / size
    v = (ys + 0.5) / size
    # окно вокруг отрезка — ради скорости
    pad = L + 6 * w
    win = (np.abs(u - u0) < pad) & (np.abs(v - v0) < pad)
    uu = u[win] - u0
    vv = v[win] - v0
    t = np.clip(uu * dx + vv * dy, -L / 2, L / 2)
    # лёгкая волнистость рубца и переменная ширина
    wob = 0.25 * w * np.sin(t / L * 5.0 + rng.random() * 6.28)
    dist = np.abs(-(uu - t * dx) * dy + (vv - t * dy) * dx - wob)
    w = w * (0.8 + 0.4 * np.abs(np.sin(t / L * 3.0 + 1.0)))
    along = 1.0 - np.clip((np.abs(t) - L / 2 + 3 * w) / (3 * w), 0.0, 1.0)
    ridge = np.exp(-(dist / w) ** 2) * along
    groove = -0.45 * np.exp(-(dist / (2.2 * w)) ** 2) * along
    h = np.zeros((size, size))
    c = np.zeros((size, size))
    h[win] = (ridge + groove) * scar["depth"]
    c[win] = np.exp(-(dist / (1.6 * w)) ** 2) * along * scar["depth"]
    return h, c


def height_to_normal(h, strength):
    dx = np.zeros_like(h)
    dy = np.zeros_like(h)
    dx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * 0.5
    dy[1:-1, :] = (h[2:, :] - h[:-2, :]) * 0.5
    nx = -dx * strength
    ny = dy * strength  # +Y вверх по листу (v растёт вниз) — стиль OpenGL
    nz = np.ones_like(h)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack([nx / n, ny / n, nz / n], axis=-1) * 0.5 + 0.5
    return (np.clip(rgb, 0, 1) * 255.0 + 0.5).astype(np.uint8)


def compose(albedo_path, glb, spec, out_dir, preview):
    im = Image.open(albedo_path).convert("RGB")
    size = im.size[0]
    alb = np.asarray(im, dtype=np.float64) / 255.0
    rng = np.random.default_rng(int(spec.get("seed", 7)))
    masks = region_masks(glb, size)
    lum = alb @ np.array([0.299, 0.587, 0.114])
    body = np.clip(sum(masks.values()), 0.0, 1.0)

    # ГРЯЗЬ: доля по областям × зерно шума × сгущение в складках (тёмное альбедо)
    dspec = spec["dirt"]
    amount = np.zeros((size, size))
    for r, a in dspec["regions"].items():
        amount += masks[r] * float(a)
    grain = fbm(rng, size, size / 24.0, 4)
    g = np.clip((grain - 0.42) / 0.35, 0.0, 1.0) ** 1.3
    crease = 1.0 + float(dspec["creases"]) * np.clip((0.62 - lum) / 0.3, 0.0, 1.0)
    d = np.clip(amount * (float(dspec["grain"]) * g + (1.0 - float(dspec["grain"])) * 0.5) * crease, 0.0, 1.0)
    tint = np.array(dspec["tint"])
    out = alb * (1.0 - d[..., None]) + (alb * tint[None, None, :] / max(tint.mean(), 1e-3) * 0.55)[...] * d[..., None]

    # ШРАМЫ: рубец в высоте и бледно-розовый цвет
    height = np.zeros((size, size))
    scar_col = np.zeros((size, size))
    for s in spec.get("scars", []):
        h, c = scar_field(size, s, rng)
        height += h
        scar_col = np.maximum(scar_col, c)
    # рубцовая ткань — бледнее и чуть розовее СВОЕЙ кожи, а не чужой цвет
    scar_rgb = np.clip(out * np.array([1.05, 0.97, 0.96])[None, None, :] + 0.05, 0.0, 1.0)
    k = 0.65 * scar_col[..., None]
    out = out * (1.0 - k) + scar_rgb * k

    # НОРМАЛЬ: поры + рельеф по альбедо + рубцы; вне тела — плоско
    pspec = spec["pores"]
    pores = fbm(rng, size, float(pspec["cell_px"]), int(pspec["octaves"])) - 0.5
    blur_r = float(spec["detail_from_albedo"]["radius_px"])
    lum_img = Image.fromarray((np.clip(lum, 0, 1) * 255).astype(np.uint8), "L")
    lum_blur = np.asarray(lum_img.filter(ImageFilter.GaussianBlur(blur_r)), dtype=np.float64) / 255.0
    detail = lum - lum_blur  # тёмные складки — впадины
    hmap = (float(pspec["amplitude"]) * pores * 0.35
            + float(spec["detail_from_albedo"]["amplitude"]) * detail * 4.0
            + height * 2.5) * body
    normal = height_to_normal(hmap, strength=float(spec.get("normal_strength", 6.0)))

    os.makedirs(out_dir, exist_ok=True)
    Image.fromarray((np.clip(out, 0, 1) * 255.0 + 0.5).astype(np.uint8), "RGB").save(os.path.join(out_dir, "albedo.png"), optimize=True)
    Image.fromarray(normal, "RGB").save(os.path.join(out_dir, "normal.png"), optimize=True)
    if preview:
        # лицо: правый остров, лежит на боку — повернуть, чтобы смотреть как на лицо
        x0, x1 = int(0.64 * size), int(0.995 * size)
        y0, y1 = int(0.20 * size), int(0.86 * size)
        for name, arr in (("albedo", (np.clip(out, 0, 1) * 255).astype(np.uint8)), ("normal", normal)):
            face = Image.fromarray(arr[y0:y1, x0:x1], "RGB").rotate(90, expand=True)
            face.save(os.path.join(out_dir, "preview_face_%s.png" % name))
        Image.fromarray((np.clip(d, 0, 1) * 255).astype(np.uint8), "L").resize((1024, 1024)).save(os.path.join(out_dir, "preview_dirt.png"))
    print("skin_layers: %s -> %s (albedo.png, normal.png; грязь средняя по телу %.3f, шрамов %d)"
          % (os.path.basename(albedo_path), out_dir, float((d * body).sum() / max(body.sum(), 1)), len(spec.get("scars", []))))


def main():
    ap = argparse.ArgumentParser(description="слои кожи офлайн: грязь, шрамы, поры")
    ap.add_argument("--albedo", required=True, help="альбедо кожи MPFB (PNG, sRGB)")
    ap.add_argument("--glb", required=True, help="тело (HumanBase.glb) — маски областей из весов костей")
    ap.add_argument("--spec", required=True, help="спека *.skin (JSON)")
    ap.add_argument("--out", required=True, help="каталог выхода: albedo.png, normal.png")
    ap.add_argument("--preview", action="store_true", help="плюс вырезы лица и карта грязи")
    a = ap.parse_args()
    spec = json.load(open(a.spec, encoding="utf-8"))
    compose(a.albedo, a.glb, spec, a.out, a.preview)


if __name__ == "__main__":
    sys.exit(main())
