#
# Created: 17:08:2026 - 14:46:25
# Last updated: 17:08:2026 - 14:46:25
# Module: tools
# File: tools/gen_house_demo.py
#
# Responsibility:
# - Generates the HOUSE DEMO stand (user, 17.08: «жду демку из нескольких домов
#   разной этажности и архитектуры») and its passports: assets/scenes/demo.scene
#   plus assets/signs/demo.signs. Three houses — a one-storey log cabin, a
#   storey-and-a-half timber frame, a two-storey stone-and-timber — each with a
#   board in front of it saying WHAT it is and WHY it is built that way.
#
# WHY A GENERATOR AND NOT A HAND-WRITTEN SCENE (the lead's ruling, 17.08:
# «ставит генератор витрины, не руки»). A house is ~40 placements and every one
# of them obeys the same three rules — a panel is measured post-centre to
# post-centre, a panel sits with its MID-THICKNESS on the axis, and a roof
# slope's origin is its eaves corner. Typed by hand those rules hold until the
# first tired line; written once here they hold for every house, and moving a
# house is editing one number.
#
# Usage:
#     python3 tools/gen_house_demo.py           (writes both files)
#     python3 tools/gen_house_demo.py --print   (prints the scene, writes nothing)
#
# Dependencies:
# - Uses: the kit's own naming rule (kind-material-LxWxH-...), assets/objects/parts.
# - Used by: dfn_signs (the .signs file), the app (the .scene), and the
#   acceptance frames.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - THE PASSPORT TEXTS ARE CONTENT and they live in PASSPORTS below, one place,
#   in the language the user reads. They are the half of this work the user
#   asked for BY NAME: «почему такая архитектура, почему выбраны именно такие
#   материалы».
# - EVERY PANEL ENDS AT A POST (HOUSES.md §3): a panel that touches a panel is
#   the defect the judge exists to find. Run tools/check_scene after editing.
#
# UPD:
# - 17:08:2026 - 14:46:25: Создан — работы 5-6 заказа 17.08 (демка этажности + паспорта домов).
#

import argparse
import math
import os

GRID = 0.25
WALL_T = 0.25          # panel thickness: 1u
POST_D = 0.50          # joint post across-flats: d50
PAD_Y = 25.5           # the stand's own flat shelf
SPAN = 8.0             # house length along X (two 16u panels)
DEPTH = 4.0            # house depth along Z (one 16u panel per side)
STOREY = 3.25          # 13u: the main dwelling wall (HOUSES.md §6)

# THE PASSPORTS. Content, in the user's own language, and every line answers
# one of his three questions: what is this, why this architecture, why these
# materials. Kept to ~34 characters so a 4 cm cap height fits a 1.5 m board.
PASSPORTS = {
    "log": [
        "ДОМ ОДНОЭТАЖНЫЙ, СРУБ",
        "стена 13u = 3.25 м, потолок 2.90",
        "бревно держит стену и тепло сразу",
        "цоколь камень: бревно не в земле",
        "солома: её кроют своими руками",
    ],
    "frame": [
        "ПОЛУТОРАЭТАЖНЫЙ, ФАХВЕРК",
        "каркас несёт, глина заполняет",
        "дерева мало, а лес далеко",
        "крутой марш 45: этаж за свою длину",
        "антресоль под коньком, тепло вверху",
    ],
    "stone": [
        "ДВУХЭТАЖНЫЙ, КАМЕНЬ И ДЕРЕВО",
        "низ тёсаный камень: сырость, удар",
        "верх фахверк: легче и теплее",
        "камень внизу, дерево вверху —",
        "тяжёлое стоит на земле",
        "черепица: дом побогаче соседей",
    ],
}


def yaw_for(dx, dz):
    """Yaw that turns a part's local +X toward (dx, dz).

    The app rotates by (x*cos + z*sin, -x*sin + z*cos), so local +X lands on
    (cos, -sin) — which makes +Z (south) a yaw of -90 and not +90. Derived from
    the transform rather than guessed, because a guess here mirrors every
    house and the judge would only see it as a seat error."""
    return round(math.degrees(math.atan2(-dz, dx)))


def seat(axis_x, axis_z, yaw_deg, thickness=WALL_T):
    """Panel position from the POST AXIS it starts at (HOUSES.md §3.2).

    The panel's own origin lies on its FACE plane, so the composer offsets it
    by half a thickness along the rotated lateral — otherwise the far corner of
    the end hangs a whole thickness off the axis and the judge goes red."""
    r = math.radians(yaw_deg)
    return (axis_x - math.sin(r) * thickness * 0.5,
            axis_z - math.cos(r) * thickness * 0.5)


class Scene:
    def __init__(self):
        self.items = []

    def place(self, obj, pos, yaw=0, group=None, note=None):
        self.items.append((obj, pos, yaw, group, note))

    def text(self):
        out = []
        for obj, pos, yaw, group, note in self.items:
            out.append("[place]")
            out.append(f"object = {obj}")
            out.append(f"pos = {pos[0]:.3f} {pos[1]:.3f} {pos[2]:.3f}")
            out.append(f"yaw = {yaw}")
            out.append("scale = 1")
            if group:
                out.append(f"group = {group}")
            if note:
                out.append(f"note = {note}")
            out.append("")
        return "\n".join(out)


def wall_run(scene, x0, z0, dx, dz, length, panel, post, group, storey_y):
    """One side of a house: a post at each end of every panel, the panel
    between them. The post is placed FIRST and the panel counted from it —
    that order is the convention, not a preference (§3.2)."""
    yaw = yaw_for(dx, dz)
    px, pz = seat(x0, z0, yaw)
    scene.place(post, (x0, storey_y, z0), 0, group)
    scene.place(panel, (px, storey_y, pz), yaw, group)
    scene.place(post, (x0 + dx * length, storey_y, z0 + dz * length), 0, group)


def house(scene, ox, oz, style, storeys, roof, gable, group):
    """One house on the stand. `ox, oz` is the SOUTH-WEST post axis; the ridge
    runs along X, so the gables face east and west."""
    post = "joint-timber-d50-n4-h13-w03" if style != "ashlar" \
        else "joint-stone-d50-n4-h13-w03"
    # The footing: the house stands on stone, always. Its top is the floor.
    for i in range(2):
        scene.place("footing-stone-16x2x4-w05",
                    (ox - 0.25, PAD_Y - 0.5, oz - 0.25 + i * (DEPTH + 0.5)), 0, group)
    for level in range(storeys):
        y = PAD_Y + level * STOREY
        st = style if level == 0 else ("framex-plaster" if style == "ashlar-stone"
                                       else style)
        south = f"wall-{st}-16x1x13-{{}}-w05"
        # South face: door on the ground floor of the west bay, window east.
        wall_run(scene, ox, oz, 1, 0, 4.0,
                 south.format("door" if level == 0 else "win1"), post, group, y)
        wall_run(scene, ox + 4.0, oz, 1, 0, 4.0, south.format("win1"), post, group, y)
        # North face (yaw 180: its outer face looks north).
        wall_run(scene, ox + 4.0, oz + DEPTH, -1, 0, 4.0,
                 south.format("win1"), post, group, y)
        wall_run(scene, ox + 8.0, oz + DEPTH, -1, 0, 4.0,
                 south.format("blind"), post, group, y)
        # The two gable walls, one panel each, running north (yaw -90 / 90).
        wall_run(scene, ox, oz + DEPTH, 0, -1, 4.0, south.format("blind"), post,
                 group, y)
        wall_run(scene, ox + SPAN, oz, 0, 1, 4.0, south.format("win1"), post,
                 group, y)
    eaves = PAD_Y + storeys * STOREY
    # THE ROOF: two slopes meeting over the ridge, each cut into pieces of the
    # catalogue's own depth (max 12u = 3 m along the ridge) — 3 + 3 + 2 = 8 m.
    run = DEPTH * 0.5
    for piece, (start, depth_u) in enumerate([(0.0, 12), (3.0, 12), (6.0, 8)]):
        depth = depth_u * GRID * 4 / 4  # u -> m is GRID; 12u = 3 m
        depth = depth_u * GRID
        name = roof.format(depth_u)
        # South slope: rises from the south eaves toward the ridge (+Z), so its
        # local +X is +Z (yaw -90) and its depth runs -X.
        scene.place(name, (ox + start + depth, eaves, oz - 0.35), -90, group)
        # North slope: mirror, rising toward -Z.
        scene.place(name, (ox + start, eaves, oz + DEPTH + 0.35), 90, group)
    # The gables close the triangle at both ends.
    for gx, gyaw in ((ox, -90), (ox + SPAN, 90)):
        scene.place(gable, (gx, eaves, oz + (0 if gyaw < 0 else DEPTH)), gyaw, group)
    if storeys > 1 or style.startswith("frame"):
        # The steep flight, inside, against the west gable: 13 steps of 1u rise
        # reach the next floor within their own run (b0ce19e).
        scene.place("stair-steep-timber-1x4x13-w03",
                    (ox + 0.9, PAD_Y, oz + 0.9), 0, group,
                    "крутой марш 45: этаж за собственную длину")
    return run


def passport(scene, ox, oz, key, group):
    """The board in front of the house, facing the visitor's approach (south).

    Two metres clear of the wall: close enough to read at a walk, far enough
    that the house is in the same view — a passport you have to stand inside
    the porch to read is a passport nobody reads."""
    scene.place(f"PASSPORT:{key}", (ox + SPAN * 0.5, PAD_Y, oz - 2.5), 180, group,
                "паспорт дома: что это, почему такая архитектура и материалы")


def build():
    scene = Scene()
    houses = [
        # (x, style, storeys, roof pattern, gable, key, note)
        (96.0, "log-timber", 1, "roof-thatch-8x{}x8-w03", "gable-timber-16x1x8-w03",
         "log"),
        (120.0, "framex-clay", 1, "roof-shingle-8x{}x12-w03",
         "gable-timber-16x1x8-w03", "frame"),
        (144.0, "ashlar-stone", 2, "roof-tile-8x{}x8-w05",
         "gable-plaster-16x1x8-w03", "stone"),
    ]
    for ox, style, storeys, roof, gable, key in houses:
        group = f"house-{key}"
        house(scene, ox, 138.0, style, storeys, roof, gable, group)
        passport(scene, ox, 138.0, key, group)
    return scene


HEADER = """# Daggerfall N scene — ДЕМКА ЭТАЖНОСТИ И АРХИТЕКТУРЫ (зона домов, работа 6).
# СГЕНЕРИРОВАНО tools/gen_house_demo.py — правки вносить в генератор, иначе
# следующий прогон их сотрёт.
# Свой стенд (Gallery, 1 чанк). Три дома с юга на север по одной линии:
# одноэтажный сруб, полутораэтажный фахверк, двухэтажный камень+дерево.
# У каждого — паспорт-табличка на столбике в 2.5 м перед южной стеной.
map = houses/demo
world_span_m = 256
spawn = 128 0 172
spawn_yaw = 0

[pad]
center = 128 142
half_extents = 60 26
blend = 10
height = 25.5
note = ровная площадка демки: три дома сравниваются взглядом, а не рельефом

"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--print", action="store_true")
    args = ap.parse_args()
    scene = build()
    body = HEADER + scene.text()
    signs = ["# Паспорта домов демки — СГЕНЕРИРОВАНО tools/gen_house_demo.py.",
             "# Текст — контент (правило 5): что это за дом, почему такая",
             "# архитектура, почему такие материалы.", ""]
    for key, lines in PASSPORTS.items():
        signs += ["[sign]", f"name = passport-{key}", "shape = post", "cap = 0.04",
                  "board = timber", "ink = dark", "wear = 0.3"]
        signs += [f"line = {t}" for t in lines]
        signs.append("")
    if args.print:
        print(body)
        print("\n".join(signs))
        return
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.makedirs(os.path.join(root, "assets", "signs"), exist_ok=True)
    with open(os.path.join(root, "assets", "signs", "demo.signs"), "w") as f:
        f.write("\n".join(signs) + "\n")
    # The passport placeholders become real object names only after the signs
    # are baked, and the baked name is the one the .signs file declares.
    body = body.replace("PASSPORT:log", "passport-log")
    body = body.replace("PASSPORT:frame", "passport-frame")
    body = body.replace("PASSPORT:stone", "passport-stone")
    with open(os.path.join(root, "assets", "scenes", "demo.scene"), "w") as f:
        f.write(body)
    print("[demo] assets/scenes/demo.scene + assets/signs/demo.signs")


if __name__ == "__main__":
    main()
