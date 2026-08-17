#
# Created: 17:08:2026 - 15:52:18
# Last updated: 17:08:2026 - 17:35:16
# Module: tools
# File: tools/gen_showcase_signs.py
#
# Responsibility:
# - ПОДПИСИ ОБРАЗЦОВ ВИТРИНЫ (user, 17.08: «чтобы у каждого объекта на стендах
#   были подписи что это за объект»). Turns every `note` already written on
#   assets/scenes/showcase.scene into a REAL BOARD standing in front of the
#   sample it names: writes assets/signs/showcase.signs and appends the label
#   placements back into the scene.
#
# WHY IT READS THE SCENE INSTEAD OF CARRYING ITS OWN LIST. The words are
# already there — the showcase was written with a human sentence on every
# sample. A second list in this file would be a second truth, and the day
# somebody fixes a wall's description in the scene the board would go on saying
# the old thing. One sentence, one place, two forms: the `note` an agent reads
# and the board a player reads.
#
# WHY IT APPENDS INSTEAD OF REWRITING THE SCENE. showcase.scene is HAND-BUILT
# and stays that way; this tool owns only the block below its sentinel line and
# rewrites exactly that block, every run. Re-running is safe and idempotent.
#
# Usage:
#     python3 tools/gen_showcase_signs.py          (writes .signs and the block)
#     python3 tools/gen_showcase_signs.py --print  (prints, writes nothing)
#     ./build_lead/dfn_signs --flat assets/signs/showcase.signs \
#         assets/objects/signs                     (bakes the boards)
#
# Dependencies:
# - Uses: assets/scenes/showcase.scene (the sentences and the positions).
# - Used by: dfn_signs, the app, the acceptance frames.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - YAW IS RADIANS and +Z IS SOUTH (see tools/gen_house_demo.py's header and
#   assets/scenes/panels/joint-box-4x4.scene). A label stands SOUTH of its
#   sample with yaw 0, which turns its lettered face toward the visitor.
# - THE TEXT IS CONTENT (Rule 5): it lives in the scene, not here. This file
#   may wrap a sentence onto two lines; it may not write one.
#
# UPD:
# - 17:08:2026 - 15:52:18: Создан — вторая половина работы 5 заказа 17.08 (физические
#   подписи образцов витрины вместо подписей только в note).
# - 17:08:2026 - 17:35:16: Перевыпущены после правки образца «пологий скат»: на витрине
#   стоял roof-thatch-12x8x4 (уклон 18.4 град), а подпись обещала 27. Заменён на
#   8x8x4 = 26.6 град — то есть чинилась ДЕТАЛЬ под обещанное число, а не число
#   под деталь: 27 назван в HOUSES.md как конструкция односкатной кровли.
#

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCENE = os.path.join(ROOT, "assets", "scenes", "showcase.scene")
SIGNS = os.path.join(ROOT, "assets", "signs", "showcase.signs")

SENTINEL = "# --- ПОДПИСИ ОБРАЗЦОВ: этот блок пишет tools/gen_showcase_signs.py ---"

# The label's letter height and how wide a line may run. 6 cm cap is
# SIGN_CAP_MEDIUM_M — the exhibit-label size SignForge.h derives from reading
# distance. The LINE is short on purpose: a sentence on one 30-character line
# makes a board 1.13 m wide and 0.10 m tall, and a 11:1 sliver reads as a
# floating string of letters with no board under it (measured by eye on the
# first cut). Sixteen characters puts most notes on 2-3 lines, which is a
# board of about 3:1 — a museum label — and narrow enough for ONE post.
CAP_M = 0.06
LINE_CHARS = 16
# How far south of its sample the board stands. Far enough not to hide the
# sample, close enough that the eye pairs them: the rows are 14 m apart, so
# 1.6 m reads as "this one" and not "the row behind".
STAND_OFF_M = 1.6


def wrap(text, width=LINE_CHARS):
    """The sentence broken on WORD boundaries, never mid-word."""
    words = text.split()
    lines = []
    cur = ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = f"{cur} {w}".strip()
    if cur:
        lines.append(cur)
    return lines


def read_samples():
    """Every [place] of the showcase that carries a note, with where it stands."""
    with open(SCENE) as f:
        text = f.read()
    body = text.split(SENTINEL)[0]
    out = []
    for i, block in enumerate(body.split("[place]")[1:]):
        fields = dict(re.findall(r"^(\w+) = (.*)$", block, re.M))
        note = fields.get("note")
        if not note:
            continue
        pos = [float(v) for v in fields["pos"].split()]
        out.append({"index": i, "pos": pos, "object": fields["object"],
                    "note": note.strip()})
    if not out:
        sys.exit("[labels] в " + SCENE + " нет ни одного note — нечего подписывать")
    return out


def label_name(sample):
    """Stable name for a sample's board: its ordinal in the scene.

    The ordinal and not the part's name, because one part appears twice in the
    showcase (the square joint stands both under a wall and in the joints row)
    and two boards may never collide on one file name."""
    return f"label-{sample['index']:02d}"


def signs_text(samples):
    stamp = "17:08:2026 - 17:35:16"
    out = ["#", "# Created: " + stamp, "# Last updated: " + stamp,
           "# Module: assets", "# File: assets/signs/showcase.signs",
           "#",
           "# Responsibility:",
           "# - Подписи 44 образцов витрины: что это за объект, человеческими",
           "#   словами. Текст — КОНТЕНТ (правило 5) и живёт в note сцены",
           "#   assets/scenes/showcase.scene; здесь он только разложен по строкам.",
           "#",
           "# Dependencies:",
           "# - Uses: dfn_signs / render::read_signs_file (печёт .dfo).",
           "#   Used by: assets/scenes/showcase.scene.",
           "#",
           "# AI Agents Notice (must follow):",
           "# - СГЕНЕРИРОВАНО tools/gen_showcase_signs.py из note сцены — правки",
           "#   вносить в СЦЕНУ, иначе следующий прогон их сотрёт.",
           "#",
           "# UPD:",
           "# - 17:08:2026 - 15:52:18: Создан генератором из подписей витрины.",
           f"# - {stamp}: Перевыпущены после правки образца «пологий скат»: на",
           "#   витрине стоял roof-thatch-12x8x4 (уклон 18.4 град), а подпись",
           "#   обещала 27. Заменён на 8x8x4 = 26.6 град — чинилась ДЕТАЛЬ под",
           "#   обещанное число, а не число под деталь.",
           "#", ""]
    for s in samples:
        out += ["[sign]", f"name = {label_name(s)}", "shape = post",
                f"cap = {CAP_M:.2f}", "board = timber", "ink = dark", "wear = 0.3"]
        out += [f"line = {line}" for line in wrap(s["note"])]
        out.append("")
    return "\n".join(out) + "\n"


def scene_block(samples):
    out = [SENTINEL,
           "# По одной доске на каждый образец: стоит в 1.6 м ЮЖНЕЕ (+Z) своего",
           "# образца лицом к пришедшему (yaw 0 — лицевая сторона смотрит на +Z).",
           "# Текст доски — тот же note, что стоит на самом образце.", ""]
    for s in samples:
        x, y, z = s["pos"]
        out += ["[place]", f"object = {label_name(s)}",
                f"pos = {x:.3f} {y:.3f} {z + STAND_OFF_M:.3f}",
                "yaw = 0.000000", "scale = 1",
                f"note = подпись образца: {s['object']}", ""]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--print", action="store_true")
    args = ap.parse_args()
    samples = read_samples()
    with open(SCENE) as f:
        scene = f.read()
    body = scene.split(SENTINEL)[0].rstrip() + "\n\n"
    block = scene_block(samples)
    if args.print:
        print(signs_text(samples))
        print(block)
        return
    with open(SIGNS, "w") as f:
        f.write(signs_text(samples))
    with open(SCENE, "w") as f:
        f.write(body + block)
    print(f"[labels] {len(samples)} подписей -> {SIGNS} и в конец витрины")


if __name__ == "__main__":
    main()
