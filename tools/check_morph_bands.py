#!/usr/bin/env python3
#
# File: tools/check_morph_bands.py
#
# Responsibility:
# - ПРИЁМКА ПОЛЗУНКОВ ТЕЛА, три утверждения и ни одного лишнего:
#     1. на ОБОИХ концах КАЖДОГО ползунка судья dfn_human_scale держит все
#        двадцать ориентиров внутри канона;
#     2. на конце ползунок ДЕЙСТВИТЕЛЬНО что-то двигает (ползунок, у которого
#        полоса ужата до нуля, проходит пункт 1 идеально и не является
#        ползунком);
#     3. выпечка из одного пресета дважды даёт ПОБАЙТОВО один файл.
#
# ПОЧЕМУ ПУНКТ 2 СТОИТ РЯДОМ С ПУНКТОМ 1, А НЕ ОТДЕЛЬНО. Они друг друга держат.
# Полоса ползунка сужается, пока судья не замолчит; без второй проверки самый
# дешёвый способ пройти приёмку — сузить полосу до нуля, и получившийся зелёный
# отчёт был бы отчётом о двадцати неподвижных ручках. Ровно эту ошибку записка
# ресёрчера поймала у чужого генератора целей: «52 цели готово», ноль сдвигов.
#
# Usage:
#     python3 tools/check_morph_bands.py --morph <dfn_morph> --judge <dfn_human_scale>
#         --body assets/objects/characters/HumanBase.dfo [--threshold 0.002]
#
# Dependencies:
# - Uses: собранные dfn_morph и dfn_human_scale; тело с секцией MORF.
# - Used by: ctest (цель morph_bands), рука, отчёт волны.
#
# ОТ ЧЕГО СУДЬЯ ОТСЧИТЫВАЕТ (правка 01.09, решение владельца). Отгружаемое
# тело печётся СЫРЫМ и само лежит мимо канона пятнадцатью строками, поэтому
# канон отверг бы и НЕЙТРАЛЬ — то есть каждая полоса ужалась бы в ноль, и
# пункт 2 выше поймал бы это как «двадцать неподвижных ручек». Судья зовётся с
# --baseline нейтрали и с КАНОНИЧЕСКОЙ ШИРИНОЙ полосы (5 % суставы, 15 %
# силуэт — умолчания судьи): утверждение стало «ползунок не уводит тело от
# СВОЕЙ нейтрали дальше, чем канон разрешает уходить от канона». Ширина
# осталась канонической и в ДОЛЯХ — ползунок, доехавший до края, обязан всё
# ещё выглядеть человеком.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - КРАСНАЯ СТРОКА ЗДЕСЬ ЗНАЧИТ «СУЗЬ ПОЛОСУ В tools/make_body_targets.py»,
#   а не «ослабь допуск судьи». Судья — полоса вокруг нейтрали, ширина полосы —
#   канон, значение полосы ползунка — наше решение.

import argparse
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def read_targets(morph, body):
    """Имена целей и их полосы — из самого файла, а не из второго списка.
    Второй список разошёлся бы с первым ровно в тот день, когда цель
    переименовали."""
    out = run([morph, "report", body])
    if out.returncode != 0:
        sys.stderr.write(out.stdout + out.stderr)
        raise SystemExit("не читается MORF у %s" % body)
    rows = []
    for line in out.stdout.splitlines():
        m = re.match(r"\s+(\S+)\s+(\d+)\s+\S+\s+\S+\s+\S+\s+\[(\S+), (\S+)\]", line)
        if m:
            rows.append((m.group(1), float(m.group(3)), float(m.group(4))))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--morph", required=True)
    ap.add_argument("--judge", required=True)
    ap.add_argument("--body", required=True)
    # ЗАМЕР НЕЙТРАЛИ, от которого отсчитываются крайние положения. Без него
    # судья мерил бы от канона, а канон отгружаемое тело не пропускает вовсе.
    ap.add_argument("--baseline", default="")
    # СКОЛЬКО ДОЛЖЕН ДВИГАТЬ ЖИВОЙ ПОЛЗУНОК, метры. 2 мм на фигуре 1.75 м — это
    # чуть больше сантиметра на реальном человеке: меньше не увидит ни глаз, ни
    # судья, и такой ползунок правильнее убрать, чем оставить.
    ap.add_argument("--threshold", type=float, default=0.002)
    opt = ap.parse_args()

    # Ширина полосы — каноническая (умолчания судьи 5 % / 15 %), точка отсчёта
    # — baseline нейтрали, если он дан.
    judge_bands = ["--tolerance", "0.05", "--silhouette-tolerance", "0.15"]
    if opt.baseline:
        judge_bands = ["--baseline", opt.baseline] + judge_bands
    rows = read_targets(opt.morph, opt.body)
    if not rows:
        raise SystemExit("у %s нет ни одной морф-цели — приёмке нечего мерить"
                         % opt.body)
    tmp = tempfile.mkdtemp(prefix="morph-bands-")
    bad = []
    print("[bands] %d ползунков, %d крайних положений" % (len(rows), 2 * len(rows)))
    for name, lo, hi in rows:
        for edge, value in (("lo", lo), ("hi", hi)):
            out_path = os.path.join(tmp, "%s-%s.dfo" % (name, edge))
            b = run([opt.morph, "bake", opt.body, "--out", out_path,
                     "--set", "%s=%g" % (name, value)])
            if b.returncode != 0:
                bad.append("%s=%g: выпечка отказала\n%s" % (name, value, b.stderr))
                continue
            j = run([opt.judge, out_path] + judge_bands)
            outs = [l.strip() for l in j.stdout.splitlines() if "<-- OUT" in l]
            moved = 0.0
            r = run([opt.morph, "report", opt.body, "--threshold",
                     "%g" % opt.threshold])
            for line in r.stdout.splitlines():
                m = re.match(r"\s+(\S+)\s+(\d+)\s+(\S+)", line)
                if m and m.group(1) == name:
                    moved = float(m.group(3)) / 1000.0
            status = "OK  "
            if j.returncode != 0:
                status = "ПОЛОСА"
                bad.append("%s=%g вне полосы нейтрали: %s" % (name, value, "; ".join(outs)))
            if value != 0.0 and moved * abs(value) < opt.threshold:
                status = "МЁРТВ"
                bad.append("%s=%g двигает %.1f мм — это не ползунок"
                           % (name, value, moved * abs(value) * 1000.0))
            print("  %-5s %-12s %-3s %+6.2f  ход %5.1f мм"
                  % (status, name, edge, value, moved * abs(value) * 1000.0))

    # --- ВЫПЕЧКА ИЗ ПРЕСЕТА ВОСПРОИЗВОДИМА ПОБАЙТОВО ------------------------
    # Тот же JSON дважды подряд обязан дать тот же файл. Это не проверка
    # детерминизма вообще: она сторожит ОДНУ вещь — порядок сложения дельт.
    # Сложение float не ассоциативно, и стоит цели поменяться местами (сортировка
    # по имени в dfn_morph attach), как два прогона разойдутся в последнем бите
    # каждой вершины, а выглядеть это будет как «иногда чуть другое тело».
    preset = os.path.join(tmp, "preset.json")
    sliders = ", ".join('"%s": %g' % (n, (lo + hi) / 3.0) for n, lo, hi in rows)
    with open(preset, "w", encoding="utf-8") as f:
        f.write('{"version": 1, "sliders": {%s}}\n' % sliders)
    a = os.path.join(tmp, "preset-a.dfo")
    b = os.path.join(tmp, "preset-b.dfo")
    for path in (a, b):
        r = run([opt.morph, "bake", opt.body, "--out", path, "--preset", preset])
        if r.returncode != 0:
            bad.append("выпечка пресета отказала:\n" + r.stderr)
    if os.path.exists(a) and os.path.exists(b):
        if filecmp.cmp(a, b, shallow=False):
            print("  OK    пресет из %d ползунков -> два побайтово равных файла"
                  % len(rows))
        else:
            bad.append("две выпечки одного пресета РАЗОШЛИСЬ побайтово")

    shutil.rmtree(tmp, ignore_errors=True)
    if bad:
        print("\n[bands] ОТКАЗ:")
        for line in bad:
            print("  " + line)
        return 1
    print("[bands] все крайние положения в полосе нейтрали, все ползунки живые, "
          "выпечка воспроизводима")
    return 0


if __name__ == "__main__":
    sys.exit(main())
