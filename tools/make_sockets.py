#!/usr/bin/env python3
"""
Module: tools
File: tools/make_sockets.py

Responsibility:
- СОКЕТЫ ПЕРСОНАЖА — точки крепления жёсткой мелочи (кольца, браслеты, амулет,
  пояс, оружие в руке, за спиной, головной убор) как ДАННЫЕ: имя → кость DEF
  + точка покоя в системе glb (Y вверх, лицом в −Z) + локальные оси-подсказки.
  Считается из скелета HumanBase.glb (положения суставов в покое), пишется в
  assets/characters/sockets.json; движок при загрузке переводит точку в
  систему кости (CLOTHING_AND_CLOTH.md §1, первая волна одежды).

Key items:
- SOCKETS: имя → (кость, смещение от сустава в метрах по осям glb).
- main(): читает glb (без Blender), пишет json.

Dependencies:
- Uses: assets/objects/characters/HumanBase.glb.
- Used by: CharacterFactory (прикрепление — волна лида), экран одевания.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Смещения — анатомия, не вкус: середина фаланги, запястье, грудина, поясница.
- Не запускать через venv/uv — обычный python3 (правило 25.08).
"""

import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLB = os.path.join(ROOT, "assets", "objects", "characters", "HumanBase.glb")
OUT = os.path.join(ROOT, "assets", "characters", "sockets.json")

# имя: (кость, к какой точке кости — head|mid, смещение x,y,z в системе glb, подпись)
# Оси glb: +X — левая сторона тела, +Y — вверх, −Z — вперёд (лицо).
SOCKETS = {
    "ring.L": ("DEF-f_ring.02.L", "mid", (0.0, 0.0, 0.0), "кольцо, безымянный палец левой"),
    "ring.R": ("DEF-f_ring.02.R", "mid", (0.0, 0.0, 0.0), "кольцо, безымянный палец правой"),
    "bracelet.L": ("DEF-hand.L", "head", (0.0, 0.0, 0.0), "браслет, левое запястье"),
    "bracelet.R": ("DEF-hand.R", "head", (0.0, 0.0, 0.0), "браслет, правое запястье"),
    "amulet": ("DEF-spine.003", "head", (0.0, 0.10, -0.10), "амулет на груди, у грудины"),
    "belt": ("DEF-hips", "head", (0.0, 0.05, 0.0), "пояс, поясница"),
    "belt.charm": ("DEF-hips", "head", (0.12, 0.03, -0.06), "брелок на поясе слева"),
    "weapon.R": ("DEF-hand.R", "mid", (0.0, 0.0, 0.0), "оружие в правой ладони"),
    "weapon.L": ("DEF-hand.L", "mid", (0.0, 0.0, 0.0), "щит или оружие в левой ладони"),
    "back": ("DEF-spine.003", "head", (0.0, 0.08, 0.14), "за спиной, между лопаток"),
    "head.top": ("DEF-head", "mid", (0.0, 0.08, 0.0), "головной убор"),
    "neck": ("DEF-neck", "head", (0.0, 0.02, -0.04), "ожерелье у основания шеи"),
}


def load_glb(path):
    b = open(path, "rb").read()
    ln = struct.unpack_from("<I", b, 12)[0]
    return json.loads(b[20:20 + ln])


def joint_world(doc):
    nodes = doc["nodes"]
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i

    def qmul(a, b):
        x1, y1, z1, w1 = a
        x2, y2, z2, w2 = b
        return (w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2)

    def rotv(q, v):
        qc = (-q[0], -q[1], -q[2], q[3])
        r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qc)
        return r[:3]

    out = {}
    for i, n in enumerate(nodes):
        chain = []
        x = i
        while x is not None:
            chain.append(x)
            x = parent.get(x)
        pos, rot = (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)
        for x in reversed(chain):
            t = nodes[x].get("translation", [0, 0, 0])
            r = nodes[x].get("rotation", [0, 0, 0, 1])
            tv = rotv(rot, t)
            pos = (pos[0] + tv[0], pos[1] + tv[1], pos[2] + tv[2])
            rot = qmul(rot, tuple(r))
        out[n.get("name", "")] = pos
    children = {n.get("name", ""): [nodes[c].get("name", "") for c in n.get("children", [])]
                for n in nodes}
    return out, children


def main():
    doc = load_glb(GLB)
    world, children = joint_world(doc)
    result = {"source": os.path.relpath(GLB, ROOT), "space": "glb: +X left, +Y up, -Z forward",
              "sockets": {}}
    for name, (bone, at, off, label) in SOCKETS.items():
        if bone not in world:
            raise SystemExit("no joint %s in %s" % (bone, GLB))
        p = world[bone]
        if at == "mid":
            kids = [c for c in children.get(bone, []) if c in world]
            tip = world[kids[0]] if kids else p
            p = ((p[0] + tip[0]) / 2, (p[1] + tip[1]) / 2, (p[2] + tip[2]) / 2)
        point = [round(p[0] + off[0], 4), round(p[1] + off[1], 4), round(p[2] + off[2], 4)]
        result["sockets"][name] = {"bone": bone, "rest_point_m": point, "label": label}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print("wrote", os.path.relpath(OUT, ROOT), "with", len(result["sockets"]), "sockets")
    for name, s in result["sockets"].items():
        print("  %-12s %-18s %s" % (name, s["bone"], s["rest_point_m"]))


if __name__ == "__main__":
    main()
