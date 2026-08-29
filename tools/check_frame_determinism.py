#!/usr/bin/env python3
"""
Module: tools
File: tools/check_frame_determinism.py

Responsibility:
- ПРИБОР ПОВТОРЯЕМОСТИ БЕСПИЛОТНОГО КАДРА: гоняет ОДИН рецепт ДВА раза одной
  сборкой и сравнивает кадры — md5 (побитово) и доля различающихся пикселей с
  порогом. Это тот прибор, на котором стоит всякое утверждение «до/после» по
  городу: пара кадров есть довод только если ДВА прогона без дозы дают один
  кадр (правило 30 — контроль, правило 47 — одна сборка, одна камера).

Key items:
- RECIPES: рецепты живут ЗДЕСЬ, а не в строке ctest и не в отчёте. Камера,
  переписанная руками во второй раз, — это два разных кадра под одной подписью.
- run_once(): один прогон, возвращает пути кадров.
- main(): --recipe <имя> [--unpin <что>] [--expect same|differ] [--threshold %]

Dependencies:
- Uses: python3 stdlib; tools/pngdiff.py (доля пикселей); сборка с dfn_app.
- Used by: ctest (frame_determinism_*), приёмка волны детерминизма тура.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КОНТРОЛЬНАЯ РУКА ОБЯЗАТЕЛЬНА. --unpin all возвращает прогону стенные часы
  (дверь DFN_UNPIN), и на нём этот прибор ОБЯЗАН краснеть. Зелёный прибор без
  красного контроля не отличим от прибора, который ничего не мерит.
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

# РЕЦЕПТЫ. Камера — абсолютная (DFN_EDITOR_CAM), час — заданный
# (DFN_TIME_OF_DAY): и то и другое ради того, чтобы единственной переменной
# между прогонами оставалась загрузка машины.
RECIPES = {
    # СТЕНД: маленькая свежая карта, стриминг короткий. Эта рука УЖЕ была
    # зелёной до волны — она здесь как контроль второго рода: починка не имеет
    # права её сломать.
    "stand": {
        "map": "trees/forest-v2",
        "cam": "132,41.6,140,1.5708,-0.16",
        "frames": "90",
    },
    # ГОРОД: та же камера, что у приёмочного кадра рощи Вайтрана
    # (tools/shoot_tiers_city.sh, плечо wr-grove).
    "city": {
        "map": "cities/whiterun",
        "cam": "30,27.6,199,1.30,-0.12",
        "frames": "120",
    },
    # ЖИТНОВ: роща у западного тракта.
    "cornhall": {
        "map": "cities/cornhall",
        "cam": "24,25.4,380,1.19,-0.10",
        "frames": "120",
    },
    # ОБЛЁТ: маршрут тура по городу (четыре стороны света вокруг сцены).
    # Здесь кадров несколько, и сравниваются ВСЕ.
    "tour": {
        "map": "cities/whiterun",
        "tour": "1",
        "frames": None,
    },
    "tour_cornhall": {
        "map": "cities/cornhall",
        "tour": "1",
        "frames": None,
    },
}

def run_once(app, recipe, out_dir, unpin, timeout_s):
    os.makedirs(out_dir, exist_ok=True)
    env = dict(os.environ)
    env.update({
        "DFN_OPEN_MAP": recipe["map"],
        "DFN_HUD": "0",
        "DFN_INTERNAL_RES": "1920x1080",
        "DFN_TIME_OF_DAY": "0.42",
        "DFN_NULL_AUDIO": "1",
    })
    if recipe.get("tour"):
        env["DFN_TOUR"] = "1"
        env["DFN_TOUR_DIR"] = out_dir
    else:
        env["DFN_EDITOR"] = "1"
        env["DFN_EDITOR_CAM"] = recipe["cam"]
        env["DFN_CAPTURE_DIR"] = out_dir
        env["DFN_CAPTURE_AFTER_FRAMES"] = recipe["frames"]
    if unpin:
        env["DFN_UNPIN"] = unpin
    log = out_dir + ".log"
    with open(log, "wb") as f:
        try:
            subprocess.run([app], env=env, stdout=f, stderr=subprocess.STDOUT,
                           cwd=ROOT, timeout=timeout_s, check=False)
        except subprocess.TimeoutExpired:
            print(f"ПРОГОН НЕ ЗАВЕРШИЛСЯ за {timeout_s} с — см. {log}",
                  file=sys.stderr)
    return sorted(n for n in os.listdir(out_dir) if n.endswith(".png"))

def md5(path):
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", default=os.path.join(ROOT, "build_tour",
                                                  "engine", "app", "dfn_app"))
    ap.add_argument("--recipe", required=True, choices=sorted(RECIPES))
    ap.add_argument("--unpin", default="",
                    help="значение двери DFN_UNPIN для контрольной руки")
    ap.add_argument("--expect", default="same", choices=["same", "differ"])
    ap.add_argument("--threshold", type=float, default=0.0,
                    help="допустимая доля различающихся пикселей, %%")
    ap.add_argument("--out", default="/tmp/tour_determinism")
    ap.add_argument("--timeout", type=float, default=300.0)
    args = ap.parse_args()

    if not os.path.exists(args.app):
        print(f"нет dfn_app: {args.app}", file=sys.stderr)
        return 2

    recipe = RECIPES[args.recipe]
    tag = args.recipe + ("_" + args.unpin.replace(",", "-") if args.unpin else "")
    base = os.path.join(args.out, tag)
    shutil.rmtree(base, ignore_errors=True)
    a = os.path.join(base, "run_a")
    b = os.path.join(base, "run_b")
    names_a = run_once(args.app, recipe, a, args.unpin, args.timeout)
    names_b = run_once(args.app, recipe, b, args.unpin, args.timeout)

    # ЭКРАН ЗАГРУЗКИ — НЕ ПРИЁМОЧНЫЙ КАДР. Он рисует ПРОГРЕСС, то есть по
    # построению является функцией стенных часов: сколько этапов успело пройти
    # к моменту снимка. Сравнивать его — значит требовать от индикатора не
    # индицировать. Кадры рецепта — capture_*.png (дверь дозы) и NN_*.png (тур).
    common = [n for n in names_a if n in names_b
              and not n.startswith("loading_")]
    skipped = [n for n in names_a if n.startswith("loading_")]
    if skipped:
        print(f"(не сравниваются, это индикатор прогресса: {', '.join(skipped)})")
    if not common:
        print(f"НИ ОДНОГО КАДРА не снято ({names_a} / {names_b}) — прогон, "
              f"не измеривший НИЧЕГО, не есть измеривший ноль", file=sys.stderr)
        return 2

    import pngdiff
    print(f"рецепт {args.recipe}"
          + (f"  DFN_UNPIN={args.unpin}" if args.unpin else "")
          + f"  кадров {len(common)}")
    print(f"{'кадр':24s} {'md5 прогон A':34s} {'md5 прогон B':34s} "
          f"{'пикселей врозь':>14s}")
    worst = 0.0
    equal = True
    for n in common:
        ha, hb = md5(os.path.join(a, n)), md5(os.path.join(b, n))
        if ha == hb:
            pct = 0.0
        else:
            equal = False
            wa, hgt, ca, pa = pngdiff.read_png(os.path.join(a, n))
            wb, hb2, cb, pb = pngdiff.read_png(os.path.join(b, n))
            if (wa, hgt, ca) != (wb, hb2, cb):
                pct = 100.0
            else:
                npx = wa * hgt
                cc = 3 if ca >= 3 else 1
                nd = 0
                for i in range(npx):
                    o = i * ca
                    if any(pa[o + k] != pb[o + k] for k in range(cc)):
                        nd += 1
                pct = 100.0 * nd / npx
        worst = max(worst, pct)
        print(f"{n:24s} {ha:34s} {hb:34s} {pct:13.4f} %")
    print(f"ХУДШИЙ КАДР: {worst:.4f} % пикселей врозь; "
          f"побитово {'СОВПАЛИ' if equal else 'РАЗОШЛИСЬ'}")

    if args.expect == "same":
        ok = equal if args.threshold <= 0.0 else worst <= args.threshold
        print("ВЕРДИКТ:", "повторяемо" if ok else "НЕ ПОВТОРЯЕТСЯ")
        return 0 if ok else 1
    ok = not equal
    print("ВЕРДИКТ (контроль):", "разошлись, как и обязаны" if ok
          else "НЕ РАЗОШЛИСЬ — контроль ничего не разделяет")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
