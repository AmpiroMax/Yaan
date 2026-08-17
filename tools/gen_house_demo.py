#
# Created: 17:08:2026 - 14:46:25
# Last updated: 17:08:2026 - 17:01:54
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
# THE THREE CONVENTIONS THIS FILE OBEYS, each one measured and not guessed:
# 1. YAW IS RADIANS. engine/world/sources/Scene.h says so in one word
#    (`float yaw; ///< radians`) and assets/scenes/panels/joint-box-4x4.scene
#    proves it: the same box written in DEGREES turns 6 findings red.
# 2. PANEL MID-THICKNESS ON THE POST AXIS: pos = axis - lateral*T/2, with
#    lateral = (sin yaw, cos yaw). Without the shift the same control box goes
#    red by 0.025 m — exactly half the panel thickness.
# 3. +Z IS SOUTH. `spawn_yaw = 0` looks north and forward is (sin, 0, -cos),
#    so the visitor arriving at the stand walks toward -Z: the DOOR and the
#    PASSPORT belong on the +Z face, or the demo shows three backs.
#
# Usage:
#     python3 tools/gen_house_demo.py           (writes both files)
#     python3 tools/gen_house_demo.py --print   (prints the scene, writes nothing)
#
# Dependencies:
# - Uses: assets/objects/parts/INDEX.txt — every name is CHECKED against the
#   baked catalogue before a line is written (see require()).
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
# - NEVER INVENT A PART NAME. require() reads INDEX.txt and refuses an unknown
#   one HERE, where it is a one-line fix, instead of in the judge's report.
#
# UPD:
# - 17:08:2026 - 14:46:25: Создан — работы 5-6 заказа 17.08 (демка этажности + паспорта домов).
# - 17:08:2026 - 14:48:55: .signs получил шапку по правилу 17 — файл в git, значит у него есть
#   происхождение и журнал.
# - 17:08:2026 - 15:22:41: Переписан по трём измеренным конвенциям (yaw — радианы, панель
#   серединой на ось, +Z — юг): дом собирается обходом четырёх углов, скаты
#   сходятся на коньке ровно, имена проверяются по INDEX.txt, дома сняты с троп
#   стенда. Пролёт 12u и скат 12x12x12 вместо 16u/8x?x8 — только при них 45°
#   гейбла, свес 1 м и конёк сходятся ОДНОВРЕМЕННО.
# - 17:08:2026 - 16:31:07: ПОЛЫ И ПОЛОТНА ДВЕРЕЙ (пользователь: «нет ни одного объекта в
#   демке полов», «дома не целые»): доски 16u поперёк дома по лежням, шаг лежня
#   1.5 м; полотно закрывает единственный сквозной проём.
# - 17:08:2026 - 17:01:54: КОНЬКОВЫЙ ПРОГОН — 22 находки roof-seat судьи были настоящими:
#   ни у одного из трёх домов не было конька, скаты и фронтоны держались ни за
#   что. Прогон идёт по линии, где сходятся верхние грани обоих скатов и
#   вершины обоих фронтонов; СРАЩЁН по 12u на пролёт (полка держит лежень не
#   длиннее 16u = 4 м, а дома 6 и 9 м), круглый d50. Судья: 214 расстановок,
#   0 находок.
#

import argparse
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- THE GRID AND THE HOUSE ------------------------------------------------
GRID = 0.25            # 1u
WALL_T = 0.25          # panel thickness: 1u
PAD_Y = 25.5           # the stand's own flat shelf
DEPTH = 4.0            # 16u across the house: the GABLE's largest size, §6
BAY = 3.0              # 12u: one panel, one roof piece, one bay of the ridge
STOREY = 3.25          # 13u: the main dwelling wall (HOUSES.md §6)
EAVES_OUT = 1.0        # how far the eaves stand out past the wall axis
RIDGE_RISE = 2.0       # 8u: the gable's rise, and so the roof's, at 45 degrees

# WHY 12u BAYS AND A 12x12x12 SLOPE, when the previous cut used 16u bays and an
# 8x?x8 slope. Three things have to be true at once: the roof must meet the
# GABLE's pitch (16x1x8 = 2 m rise over 2 m half-depth = 45 deg), the two
# slopes must meet EXACTLY on the ridge, and the eaves must stand out past the
# wall. At 45 degrees the slope's run must equal half-depth + overhang: with a
# 2 m run (8u) the overhang is zero and the eaves die on the wall axis, and any
# outboard shift leaves a hole at the ridge as wide as the shift. A 3 m run
# (12u) with a 3 m rise gives 2 + 1: the slopes meet on the ridge line and the
# eaves stand a metre proud. The catalogue has that piece only at depth 12u —
# which is why the bay is 12u and the house's length is a multiple of 3 m.

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


# --- THE CATALOGUE ---------------------------------------------------------
def catalogue():
    """Every part name the shelf actually holds.

    Read, not remembered: an invented name is the defect that cost the previous
    cut eight red findings (footing-stone-16x2x4, roof-shingle-8x8x12), and the
    only cure is asking the shelf."""
    path = os.path.join(ROOT, "assets", "objects", "parts", "INDEX.txt")
    if not os.path.exists(path):
        sys.exit("[demo] нет " + path + " — сперва испеки полку: ./build_lead/dfn_kit")
    names = set()
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            names.add(line.split()[0])
    return names


KNOWN = catalogue()


def require(name):
    """A part name, refused HERE if the shelf does not have it.

    A PASSPORT: placeholder passes through — it is not a part but a sign, baked
    from assets/signs/demo.signs by dfn_signs, and its name is settled below
    once the text that names it is written."""
    if name.startswith("PASSPORT:"):
        return name
    if name not in KNOWN:
        sys.exit(f"[demo] в каталоге нет детали \"{name}\" — правь генератор")
    return name


def worn(stem, steps=("w03", "w05", "w08")):
    """The least worn step of `stem` the shelf actually holds."""
    for s in steps:
        if f"{stem}-{s}" in KNOWN:
            return f"{stem}-{s}"
    sys.exit(f"[demo] в каталоге нет детали \"{stem}\" ни в одной степени износа")


# --- THE SCENE -------------------------------------------------------------
def yaw_for(dx, dz):
    """Yaw (RADIANS) that turns a part's local +X toward (dx, dz).

    The app rotates by (x*cos + z*sin, -x*sin + z*cos), so local +X lands on
    (cos, -sin): +X is yaw 0, +Z (south) is -pi/2, -X is pi, -Z is +pi/2.
    Derived from the transform, not guessed — a guess mirrors every house and
    the judge would only report it as a seat error."""
    return math.atan2(-dz, dx)


def seat(axis_x, axis_z, yaw, thickness=WALL_T):
    """Panel position from the POST AXIS it starts at (HOUSES.md §3.2).

    The panel's own origin lies on its FACE plane, so the composer offsets it
    by half a thickness along the rotated lateral — otherwise the far corner of
    the end hangs a whole thickness off the axis and the judge goes red."""
    return (axis_x - math.sin(yaw) * thickness * 0.5,
            axis_z - math.cos(yaw) * thickness * 0.5)


class Scene:
    def __init__(self):
        self.items = []

    def place(self, obj, pos, yaw=0.0, group=None, note=None):
        self.items.append((require(obj), pos, yaw, group, note))

    def text(self):
        out = []
        for obj, pos, yaw, group, note in self.items:
            out.append("[place]")
            out.append(f"object = {obj}")
            out.append(f"pos = {pos[0]:.3f} {pos[1]:.3f} {pos[2]:.3f}")
            out.append(f"yaw = {yaw:.6f}")
            out.append("scale = 1")
            if group:
                out.append(f"group = {group}")
            if note:
                out.append(f"note = {note}")
            out.append("")
        return "\n".join(out)


def plinth(scene, ox, oz, span, group):
    """The stone footing ring under the walls: the house stands on stone.

    Its top is 0.25 m above the shelf — a step the player clears (0.35 m,
    docs/NUMBERS.md) so the doorway stays walkable, and enough stone to read as
    a plinth under a log wall."""
    piece = require("footing-stone-12x2x2-w03")
    short = require("footing-stone-4x2x2-w03")
    y = PAD_Y - 0.25
    for k in range(int(span / BAY)):
        x = ox + k * BAY
        scene.place(piece, (x, y, oz - 0.25), 0.0, group)
        scene.place(piece, (x, y, oz + DEPTH - 0.25), 0.0, group)
    # The two ends run along Z: 3 m + 1 m makes the 4 m depth.
    for x in (ox, ox + span):
        scene.place(piece, (x + 0.25, y, oz), yaw_for(0, 1), group)
        scene.place(short, (x + 0.25, y, oz + BAY), yaw_for(0, 1), group)


def floor(scene, ox, oz, span, level, group):
    """ПОЛ: доски поперёк дома по лежням (HOUSES.md §3.2).

    Boards run across the DEPTH, which is 4 m and exactly the longest plank the
    shelf holds (16u), so every board is one piece bearing on the plinth at
    both ends. Under them, joists every 1.5 m: on the ground floor they lie in
    the shelf and nobody sees them, on the upper floor they ARE the ceiling of
    the room below — the same rule either way, which is why it is one rule.

    The deck's top sits 6 cm above the wall's base, so the doorway is a step of
    6 cm and not of half a metre: a floor the player cannot walk onto is not a
    floor (PLAYER_STEP_HEIGHT 0.35, docs/NUMBERS.md)."""
    y = PAD_Y + level * STOREY
    joist = worn("beam-timber-16x2x1")
    board = worn("plank-timber-16x2x1")
    # Joists across the depth, 1.5 m apart, their tops flush with the deck's
    # underside.
    n_joists = max(2, int(span / 1.5))
    for k in range(n_joists):
        x = ox + (span / n_joists) * (k + 0.5)
        scene.place(joist, (x, y - 0.25, oz), yaw_for(0, 1), group)
    # The boards: 2u wide each, laid west to east, centred on their own line.
    boards = int(span / 0.5)
    for k in range(boards):
        scene.place(board, (ox + 0.25 + k * 0.5, y, oz), yaw_for(0, 1), group)


def walls(scene, ox, oz, span, level, style, group):
    """One storey: a post on every bay line, a panel between two posts.

    The four sides are walked as ONE loop around the corners A->B->C->D, which
    is what keeps every panel's local +Z pointing INTO the house — mixing the
    directions would flip the door's face on one wall out of four."""
    y = PAD_Y + level * STOREY
    post = require(("joint-stone-d50-n4-h13-w03" if style.endswith("stone")
                    else "joint-timber-d50-n4-h13-w03"))
    bays = int(span / BAY)
    # Posts: every bay line on both long walls, each placed ONCE.
    for k in range(bays + 1):
        for z in (oz, oz + DEPTH):
            scene.place(post, (ox + k * BAY, y, z), 0.0, group)

    def panel(opening, length_u):
        return require(f"wall-{style}-{length_u}x1x13-{opening}-w05")

    # North wall (A->B, +X): the back of the house — blind and one window.
    for k in range(bays):
        opening = "win1" if k == bays - 1 else "blind"
        yaw = yaw_for(1, 0)
        px, pz = seat(ox + k * BAY, oz, yaw)
        scene.place(panel(opening, 12), (px, y, pz), yaw, group)
    # East gable wall (B->C, +Z): one 16u panel, the full depth.
    yaw = yaw_for(0, 1)
    px, pz = seat(ox + span, oz, yaw)
    scene.place(panel("win1" if level == 0 else "blind", 16), (px, y, pz), yaw, group)
    # South wall (C->D, -X): the face the visitor meets — door on the ground
    # floor, windows beside it.
    for k in range(bays):
        first = k == 0
        opening = "door" if (level == 0 and first) else "win1"
        yaw = yaw_for(-1, 0)
        px, pz = seat(ox + span - k * BAY, oz + DEPTH, yaw)
        scene.place(panel(opening, 12), (px, y, pz), yaw, group)
    # West gable wall (D->A, -Z).
    yaw = yaw_for(0, -1)
    px, pz = seat(ox, oz + DEPTH, yaw)
    scene.place(panel("blind", 16), (px, y, pz), yaw, group)


def door_leaf(scene, ox, oz, span, group):
    """ПОЛОТНО В ДВЕРНОЙ ПРОЁМ — the kit's one sanctioned through-hole, closed.

    The wall part cuts a 1.0 x 2.05 m opening in the MIDDLE of its bay
    (PartForgeWalls.holes_of), and the panel carrying it is the first bay of
    the south run, so the opening's centre is 1.5 m in from the house's east
    corner. The leaf is 1.25 x 2.25 — the kit has no leaf the size of the hole
    — so it is hung 2 cm PROUD of the wall face (no coplanar z-fight, and a
    plank door was hung on the face) and dropped 0.2 m, which puts its head on
    the lintel and buries the surplus in the plinth instead of standing it
    above the door like a tombstone."""
    leaf = worn("door-timber-5x1x9")
    yaw = yaw_for(-1, 0)  # the south wall's own yaw: the leaf faces +Z with it
    scene.place(leaf, (ox + span - 0.875, PAD_Y - 0.2, oz + DEPTH + 0.145), yaw,
                group, "полотно закрывает единственный сквозной проём дома")


def roof(scene, ox, oz, span, eaves, cover, gable_mat, group):
    """Two slopes meeting on the ridge, and a gable closing each end.

    A slope's origin is its EAVES CORNER, its +X climbs toward the ridge and
    its +Z runs along the ridge. Both slopes start a metre outboard and a metre
    LOW — at 45 degrees that is the same metre — so the eaves overhang and the
    two runs still end on the ridge line together."""
    # THE WEAR STEP IS THE SHELF'S TO CHOOSE. Most coverings are baked at
    # 0.3/0.8, tile only at 0.5 — asking for a step the shelf does not hold is
    # the same invented name as asking for a size it does not hold.
    piece = worn(f"roof-{cover}-12x12x12")
    base = eaves - EAVES_OUT
    # КОНЬКОВЫЙ ПРОГОН. Both slopes' upper edges and both gables' apexes land
    # on ONE line — x from ox to ox+span at z = oz + DEPTH/2, y = eaves +
    # RIDGE_RISE — and until this run there was nothing on that line at all:
    # the judge reported 22 roof-seat findings on this stand, six on the log
    # house and eight on each of the others, and every one of them was true.
    # Скаты и фронтоны держались НИ ЗА ЧТО.
    #
    # SPLICED, NOT ONE STICK: the shelf's longest sleeper is 16u = 4 m and the
    # houses are 6 and 9 m, so the purlin runs one 12u piece per 12u bay,
    # jointed over the bay line. That is how a purlin is actually built, and it
    # is also what the seat rule wants: a slope's upper edge spans exactly one
    # bay, so it lies wholly within one piece rather than half on each.
    #
    # ROUND (nr) and d50: round because a ridge log takes the rafters at
    # whatever pitch the roof happens to be (a faceted purlin would put the
    # facet rule between the composer and every pitch that is not its step),
    # d50 because the seat tolerance is r_in - 0.02 = 0.23 m and the eaves
    # purlins of a later canopy will want that slack. Timber even under the
    # stone house: nobody cuts a ridge out of ashlar.
    ridge_y = eaves + RIDGE_RISE
    ridge_z = oz + DEPTH * 0.5
    purlin = worn("sleeper-timber-d50-nr-12u")
    purlin_r = 0.25
    for k in range(int(span / BAY)):
        scene.place(purlin, (ox + k * BAY, ridge_y - purlin_r, ridge_z),
                    yaw_for(1, 0), group,
                    "коньковый прогон: на нём сходятся оба ската и оба фронтона"
                    if k == 0 else None)
    for k in range(int(span / BAY)):
        # South slope: eaves on the +Z side, climbing toward -Z; its depth runs
        # +X, so the piece starts at the bay's west line.
        scene.place(piece, (ox + k * BAY, base, oz + DEPTH + EAVES_OUT),
                    yaw_for(0, -1), group)
        # North slope: the mirror. Its depth runs -X, so it starts at the
        # bay's EAST line.
        scene.place(piece, (ox + (k + 1) * BAY, base, oz - EAVES_OUT),
                    yaw_for(0, 1), group)
    gable = require(f"gable-{gable_mat}-16x1x8-w03")
    # The gable is 1u thick and its thickness runs off its own +Z, so each one
    # is offset half a thickness to sit CENTRED on the wall plane it closes.
    scene.place(gable, (ox + 0.125, eaves, oz), yaw_for(0, 1), group)
    scene.place(gable, (ox + span - 0.125, eaves, oz + DEPTH), yaw_for(0, -1), group)


def house(scene, ox, oz, span, storeys, style, upper, cover, gable_mat, group):
    """One house on the stand. `ox, oz` is the NORTH-WEST post axis; the ridge
    runs along X, so the gables face east and west and the door faces +Z."""
    plinth(scene, ox, oz, span, group)
    for level in range(storeys):
        floor(scene, ox, oz, span, level, group)
        walls(scene, ox, oz, span, level, style if level == 0 else upper, group)
    door_leaf(scene, ox, oz, span, group)
    eaves = PAD_Y + storeys * STOREY
    roof(scene, ox, oz, span, eaves, cover, gable_mat, group)
    if storeys > 1 or style.startswith("frame"):
        # The steep flight, inside, along the north wall: 13 steps of 1u rise
        # reach the next floor within their own run (b0ce19e). It climbs +X and
        # is 1 m wide, so it clears both gable walls and the doorway.
        scene.place(require("stair-steep-timber-1x4x13-w03"),
                    (ox + 0.5, PAD_Y, oz + 0.5), 0.0, group,
                    "крутой марш 45: этаж за собственную длину")


def passport(scene, ox, oz, span, key, group):
    """The board in front of the house, facing the visitor's approach (+Z).

    Two and a half metres clear of the wall: close enough to read at a walk,
    far enough that the house is in the same view — a passport you have to
    stand in the porch to read is a passport nobody reads. Yaw 0 turns the
    board's lettered face toward +Z, which is where the visitor stands."""
    scene.place(f"PASSPORT:{key}", (ox + span * 0.5, PAD_Y, oz + DEPTH + 2.5),
                0.0, group,
                "паспорт дома: что это, почему такая архитектура и материалы")


# The three houses. X positions are chosen off the stand's own path network:
# Gallery's road runs the diagonal from (72,112) to (132,172), and at the
# houses' latitude (z 136..146) it occupies x 92..112 — measured by probing
# path_clearance on a 4 m grid, not by eye.
HOUSES = [
    # key,     ox,    span, storeys, style,          upper,            cover,    gable
    ("log",   118.0,  6.0,  1, "log-timber",     "log-timber",      "thatch",  "timber"),
    ("frame", 130.0,  9.0,  1, "framex-clay",    "framex-clay",     "shingle", "timber"),
    ("stone", 147.0,  9.0,  2, "ashlar-stone",   "framex-plaster",  "tile",    "plaster"),
]
HOUSE_Z = 136.0


def build():
    scene = Scene()
    for key, ox, span, storeys, style, upper, cover, gable_mat in HOUSES:
        group = f"house-{key}"
        house(scene, ox, HOUSE_Z, span, storeys, style, upper, cover, gable_mat, group)
        passport(scene, ox, HOUSE_Z, span, key, group)
    return scene


HEADER = """# Daggerfall N scene — ДЕМКА ЭТАЖНОСТИ И АРХИТЕКТУРЫ (зона домов, работа 6).
# СГЕНЕРИРОВАНО tools/gen_house_demo.py — правки вносить в генератор, иначе
# следующий прогон их сотрёт.
# Свой стенд (Gallery, 1 чанк). Три дома в ряд с запада на восток:
# одноэтажный сруб 6 м, полутораэтажный фахверк 9 м, двухэтажный камень+дерево
# 9 м. У каждого — паспорт-табличка на столбике в 2.5 м перед южной стеной,
# лицом к пришедшему (спавн южнее, смотрит на север).
map = houses/demo
world_span_m = 256
spawn = 136 0 162
spawn_yaw = 0

[pad]
center = 128 142
half_extents = 60 26
blend = 10
height = 25.5
note = ровная площадка демки: три дома сравниваются взглядом, а не рельефом

"""

STAMP = "17:08:2026 - 15:22:41"

SIGNS_HEADER = [
    "#", f"# Created: 17:08:2026 - 14:46:25", f"# Last updated: {STAMP}",
    "# Module: assets", "# File: assets/signs/demo.signs",
    "#",
    "# Responsibility:",
    "# - Паспорта трёх домов демки. Текст — КОНТЕНТ (правило 5): что это за",
    "#   дом, ПОЧЕМУ такая архитектура, ПОЧЕМУ такие материалы.",
    "#",
    "# Dependencies:",
    "# - Uses: dfn_signs / render::read_signs_file (печёт .dfo).",
    "#   Used by: assets/scenes/demo.scene.",
    "#",
    "# AI Agents Notice (must follow):",
    "# - СГЕНЕРИРОВАНО tools/gen_house_demo.py — правки в генератор, иначе",
    "#   следующий прогон их сотрёт.",
    "#",
    "# UPD:",
    "# - 17:08:2026 - 14:46:25: Создан генератором вместе с demo.scene.",
    f"# - {STAMP}: Перевыпущен вместе со сценой (дома переставлены с троп).",
    "#", "",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--print", action="store_true")
    args = ap.parse_args()
    scene = build()
    body = HEADER + scene.text()
    signs = list(SIGNS_HEADER)
    for key, lines in PASSPORTS.items():
        signs += ["[sign]", f"name = passport-{key}", "shape = post", "cap = 0.04",
                  "board = timber", "ink = dark", "wear = 0.3"]
        signs += [f"line = {t}" for t in lines]
        signs.append("")
    if args.print:
        print(body)
        print("\n".join(signs))
        return
    os.makedirs(os.path.join(ROOT, "assets", "signs"), exist_ok=True)
    with open(os.path.join(ROOT, "assets", "signs", "demo.signs"), "w") as f:
        f.write("\n".join(signs) + "\n")
    # The passport placeholders become real object names only after the signs
    # are baked, and the baked name is the one the .signs file declares.
    for key in PASSPORTS:
        body = body.replace(f"PASSPORT:{key}", f"passport-{key}")
    with open(os.path.join(ROOT, "assets", "scenes", "demo.scene"), "w") as f:
        f.write(body)
    print(f"[demo] {len(scene.items)} расстановок -> assets/scenes/demo.scene"
          " + assets/signs/demo.signs")


if __name__ == "__main__":
    main()
