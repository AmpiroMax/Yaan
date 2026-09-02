#!/usr/bin/env python3
#
# File: tools/check_morph_bands.py
#
# Responsibility:
# - ПРИЁМКА ПОЛЗУНКОВ ТЕЛА И ЛИЦА, три утверждения и ни одного лишнего:
#     1. на ОБОИХ концах КАЖДОГО ползунка судья dfn_human_scale держит все
#        двадцать ориентиров внутри канона, а на концах ЛИЦЕВЫХ ползунков ещё
#        и судья лица dfn_face_scale держит одиннадцать мерок в полосе
#        нейтрали (FACE_CANON.md);
#     2. на конце ползунок ДЕЙСТВИТЕЛЬНО что-то двигает (ползунок, у которого
#        полоса ужата до нуля, проходит пункт 1 идеально и не является
#        ползунком);
#     3. выпечка из одного пресета дважды даёт ПОБАЙТОВО один файл.
#   И КАЛИБРОВКА (--calibrate <face.bands>): для каждой лицевой ручки — видит
#   ли её судья (сдвиг хоть одной мерки на конце манифестной полосы), и если
#   видит — двоичный поиск края полосы, где оба судьи ещё зелёные. Ручки, к
#   которым судья слеп (черты: веко, ноздри, губа), ходят весь ход манифеста
#   и помечаются blind — экран рисует им полый ромб (CHARGEN_UI.md, Р3).
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
#         [--face-judge <dfn_face_scale> --face-manifest face.targets
#          --masks face.masks --face-baseline HumanBase.face.json]
#         [--calibrate assets/characters/targets/face.bands]
#
# Dependencies:
# - Uses: собранные dfn_morph, dfn_human_scale, dfn_face_scale; тело с секцией
#   MORF; tools/make_body_targets.py (разбор манифеста лица — одно правило имён).
# - Used by: ctest (цель morph_bands), рука (калибровка), отчёт волны.
#
# ОТ ЧЕГО СУДЬЯ ОТСЧИТЫВАЕТ (правка 01.09, решение владельца). Отгружаемое
# тело печётся СЫРЫМ и само лежит мимо канона пятнадцатью строками, поэтому
# канон отверг бы и НЕЙТРАЛЬ — то есть каждая полоса ужалась бы в ноль, и
# пункт 2 выше поймал бы это как «двадцать неподвижных ручек». Судья зовётся с
# --baseline нейтрали и с КАНОНИЧЕСКОЙ ШИРИНОЙ полосы (5 % суставы, 15 %
# силуэт — умолчания судьи): утверждение стало «ползунок не уводит тело от
# СВОЕЙ нейтрали дальше, чем канон разрешает уходить от канона». Ширина
# осталась канонической и в ДОЛЯХ — ползунок, доехавший до края, обязан всё
# ещё выглядеть человеком. У лица то же: baseline нейтрали, ширина — ±12 %
# (±2σ межзрачкового) и ±0.03 линии глаз (умолчания dfn_face_scale).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - КРАСНАЯ СТРОКА ЗДЕСЬ ЗНАЧИТ «СУЗЬ ПОЛОСУ» (тело — RANGES в
#   tools/make_body_targets.py, лицо — перекалибруй face.bands и перезапиши
#   полосы --reband), а не «ослабь допуск судьи». Судья — полоса вокруг
#   нейтрали, ширина полосы — канон, значение полосы ползунка — наше решение.

import argparse
import filecmp
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


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


def read_moves(morph, body, threshold):
    """Ход каждой цели на единичном весе, метры (dfn_morph report)."""
    moves = {}
    r = run([morph, "report", body, "--threshold", "%g" % threshold])
    for line in r.stdout.splitlines():
        m = re.match(r"\s+(\S+)\s+(\d+)\s+(\S+)", line)
        if m:
            moves[m.group(1)] = float(m.group(3)) / 1000.0
    return moves


def read_face_manifest(path):
    """Ручки лица — тем же разбором, что у экспортёра: одно правило имён."""
    spec = importlib.util.spec_from_file_location(
        "make_body_targets", os.path.join(ROOT, "tools", "make_body_targets.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return {r["name"]: (r["lo"], r["hi"]) for r in mod.read_face_manifest(path)}


# ЧТО ЗНАЧИТ «СУДЬЯ ВИДИТ РУЧКУ»: хоть одна мерка на конце манифестной полосы
# ушла от нейтрали на процент (длины) или на 0.005 (линия глаз). Процент —
# это 0.6 мм на межзрачковом, то есть ниже всякой полосы, но выше шума
# рест-позы (перепечка воспроизводит мерки в тысячные).
SEE_REL = 0.01
SEE_ABS = 0.005


class FaceJudge:
    def __init__(self, exe, masks, baseline, tolerance, eyeline):
        self.cmd = [exe, "--masks", masks, "--baseline", baseline,
                    "--tolerance", "%g" % tolerance, "--eyeline-tolerance", "%g" % eyeline]

    def judge(self, dfo):
        """(зелёный?, список красных строк, наибольший сдвиг мерки как доля
        своего порога видимости)."""
        j = run(self.cmd[:1] + [dfo] + self.cmd[1:])
        outs = [l.strip() for l in j.stdout.splitlines() if "<-- OUT" in l]
        seen = 0.0
        for line in j.stdout.splitlines():
            m = re.search(r"\s([+-]\d+\.\d+)(%?)\s*(<-- OUT|\(канон мимо\))?\s*$", line)
            if not m or not line.startswith("        "):
                continue
            dev = abs(float(m.group(1)))
            ratio = dev / 100.0 / SEE_REL if m.group(2) == "%" else dev / SEE_ABS
            seen = max(seen, ratio)
        return j.returncode == 0, outs, seen


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
    # ТО ЖЕ ДЛЯ ЛИЦА: 0.5 мм. На портретном кадре лицо занимает 0.54 высоты
    # (CHARGEN_FACE_FILL): 0.24 м головы на 580 px FullHD — 0.4 мм на пиксель,
    # и полмиллиметра — это пиксель. Ширина крыльев ходит 1.7 мм на единицу;
    # порог тела (2 мм) объявил бы её мёртвой, а на кадре она видна.
    ap.add_argument("--face-threshold", type=float, default=0.0005)
    ap.add_argument("--face-judge", default="")
    ap.add_argument("--face-manifest", default="")
    ap.add_argument("--masks", default="")
    ap.add_argument("--face-baseline", default="")
    ap.add_argument("--face-tolerance", type=float, default=0.12)
    ap.add_argument("--eyeline-tolerance", type=float, default=0.03)
    # КАЛИБРОВКА: куда писать полосы лица (assets/characters/targets/face.bands).
    ap.add_argument("--calibrate", default="")
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
    face = {}
    face_judge = None
    if opt.face_judge:
        if not (opt.face_manifest and opt.masks and opt.face_baseline):
            raise SystemExit("--face-judge требует --face-manifest, --masks и --face-baseline")
        face = read_face_manifest(opt.face_manifest)
        face_judge = FaceJudge(opt.face_judge, opt.masks, opt.face_baseline,
                               opt.face_tolerance, opt.eyeline_tolerance)
        absent = sorted(n for n in face if n not in {r[0] for r in rows})
        if absent:
            raise SystemExit("манифест лица называет ручки, которых нет в теле: %s"
                             % ", ".join(absent))
    tmp = tempfile.mkdtemp(prefix="morph-bands-")
    moves_body = read_moves(opt.morph, opt.body, opt.threshold)
    moves_face = read_moves(opt.morph, opt.body, opt.face_threshold) if face else {}

    def bake(name, value):
        out_path = os.path.join(tmp, "%s-%+.4f.dfo" % (name, value))
        b = run([opt.morph, "bake", opt.body, "--out", out_path,
                 "--set", "%s=%g" % (name, value)])
        if b.returncode != 0:
            return None, b.stderr
        return out_path, ""

    def verdict(name, value):
        """(тело зелёное?, лицо зелёное?, красные строки, видимость лица)."""
        dfo, err = bake(name, value)
        if dfo is None:
            return False, False, ["выпечка отказала: " + err.strip()], 0.0
        j = run([opt.judge, dfo] + judge_bands)
        outs = ["тело: " + l.strip() for l in j.stdout.splitlines() if "<-- OUT" in l]
        body_ok = j.returncode == 0
        face_ok, seen = True, 0.0
        if face_judge is not None and name in face:
            face_ok, fouts, seen = face_judge.judge(dfo)
            outs += ["лицо: " + l for l in fouts]
        return body_ok, face_ok, outs, seen

    bad = []

    # --- КАЛИБРОВКА ПОЛОС ЛИЦА ----------------------------------------------
    if opt.calibrate:
        if face_judge is None:
            raise SystemExit("--calibrate требует судью лица (--face-judge ...)")
        lines = []
        print("[bands] калибровка %d ручек лица: манифест -> судьи -> face.bands" % len(face))
        for name in sorted(face):
            lo_m, hi_m = face[name]
            edges = {}
            seen_max = 0.0
            limiter = []
            for side, edge in (("lo", lo_m), ("hi", hi_m)):
                if edge == 0.0:
                    edges[side] = 0.0
                    continue
                body_ok, face_ok, outs, seen = verdict(name, edge)
                seen_max = max(seen_max, seen)
                if body_ok and face_ok:
                    edges[side] = edge
                    continue
                limiter += outs[:2]
                # ДВОИЧНЫЙ ПОИСК КРАЯ: между нулём (нейтраль — зелёная по
                # построению) и краем манифеста; восемь делений дают сотую хода.
                good, bad_v = 0.0, edge
                for _ in range(8):
                    mid = 0.5 * (good + bad_v)
                    b_ok, f_ok, _, _ = verdict(name, mid)
                    if b_ok and f_ok:
                        good = mid
                    else:
                        bad_v = mid
                # К НУЛЮ, до сотой: край полосы обязан быть зелёным сам, а не
                # «почти».
                edges[side] = (int(abs(good) * 100.0) / 100.0) * (1.0 if good > 0 else -1.0)
            measured = seen_max >= 1.0
            lo_v, hi_v = edges["lo"], edges["hi"]
            if measured and not (lo_v < hi_v):
                bad.append("%s: судья видит ручку, а полоса схлопнулась [%g, %g]" % (name, lo_v, hi_v))
            note = "; ".join(sorted(set(l.split("<--")[0].strip() for l in limiter)))[:160]
            flag = "measured" if measured else "blind"
            what = "судья видит (сдвиг x%.1f порога), полоса %s" % (
                seen_max, "полная" if (lo_v, hi_v) == (lo_m, hi_m) else "сужена: " + note) \
                if measured else "судья слеп (сдвиг x%.2f порога), весь ход манифеста" % seen_max
            print("  %-22s [%+.2f, %+.2f] %-8s %s" % (name, lo_v, hi_v, flag, what))
            lines.append("%-22s %+.2f %+.2f %-8s  # %s" % (name, lo_v, hi_v, flag, what))
        with open(opt.calibrate, "w", encoding="utf-8") as f:
            f.write("# ПОЛОСЫ ЛИЦЕВЫХ РУЧЕК, ОТКАЛИБРОВАННЫЕ СУДЬЯМИ (tools/check_morph_bands.py\n"
                    "# --calibrate): строка «имя lo hi measured|blind # упор». measured — судья\n"
                    "# (dfn_human_scale по baseline тела 5 %%/15 %%, dfn_face_scale по baseline лица\n"
                    "# ±%.0f %% длины / ±%.2f линии глаз) видит ручку, полоса найдена двоичным\n"
                    "# поиском от нейтрали к краю манифеста; blind — судья слеп, ручка ходит весь\n"
                    "# манифест, экран рисует ей полый ромб. Читают: tools/make_body_targets.py\n"
                    "# (--reband -> полосы в .morf) и engine/app FaceManifest (ромб).\n"
                    % (opt.face_tolerance * 100.0, opt.eyeline_tolerance))
            f.write("\n".join(lines) + "\n")
        print("[bands] записано %s: %d ручек, measured %d, blind %d" % (
            opt.calibrate, len(lines), sum(1 for l in lines if " measured" in l),
            sum(1 for l in lines if " blind" in l)))
        shutil.rmtree(tmp, ignore_errors=True)
        if bad:
            print("\n[bands] ОТКАЗ:")
            for line in bad:
                print("  " + line)
            return 1
        return 0

    # --- ПРИЁМКА КРАЙНИХ ПОЛОЖЕНИЙ ------------------------------------------
    print("[bands] %d ползунков (%d лицевых), %d крайних положений"
          % (len(rows), sum(1 for r in rows if r[0] in face), 2 * len(rows)))
    for name, lo, hi in rows:
        is_face = name in face
        threshold = opt.face_threshold if is_face else opt.threshold
        moved = (moves_face if is_face else moves_body).get(name, 0.0)
        for edge, value in (("lo", lo), ("hi", hi)):
            body_ok, face_ok, outs, _ = verdict(name, value)
            status = "OK  "
            if not body_ok or not face_ok:
                status = "ПОЛОСА"
                bad.append("%s=%g вне полосы нейтрали: %s" % (name, value, "; ".join(outs)))
            if value != 0.0 and moved * abs(value) < threshold:
                status = "МЁРТВ"
                bad.append("%s=%g двигает %.2f мм — это не ползунок"
                           % (name, value, moved * abs(value) * 1000.0))
            print("  %-5s %-22s %-3s %+6.2f  ход %5.1f мм%s"
                  % (status, name, edge, value, moved * abs(value) * 1000.0,
                     "  (лицо)" if is_face else ""))

    # --- ВЫПЕЧКА ИЗ ПРЕСЕТА ВОСПРОИЗВОДИМА ПОБАЙТОВО ------------------------
    # Тот же JSON дважды подряд обязан дать тот же файл. Это не проверка
    # детерминизма вообще: она сторожит ОДНУ вещь — порядок сложения дельт.
    # Сложение float не ассоциативно, и стоит целям поменяться местами (сортировка
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
