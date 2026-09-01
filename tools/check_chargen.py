#!/usr/bin/env python3
"""
Module: tools
File: tools/check_chargen.py

Responsibility:
- ПРИЁМКА ЭКРАНА СОЗДАНИЯ ПЕРСОНАЖА, ПРОЙДЕННАЯ ДО КОНЦА: гоняет НАСТОЯЩУЮ
  игру дозами (DFN_CHARGEN=1, DFN_MORPH=..., DFN_CHARGEN_DONE=1), то есть
  ползунки крутит тот же код, что и рука игрока, и получает те же два файла,
  которыми персонаж уезжает в мир. Каждое выпеченное тело показывается судье
  пропорций (dfn_human_scale) и меряется по росту.

Key items:
- Полоса роста судится на ОБОИХ концах и в каноне: масштаб пропорций не
  трогает по построению, и это утверждение надо проверить, а не объявить.
- Контрольная рука: рост ЗА полосой обязан зажаться, а не проехать.

Dependencies:
- Uses: собранные dfn_app и dfn_human_scale; пишет в
  assets/characters/presets/ (туда же, куда пишет «Готово»).
- Used by: рукой, при сдаче волны; в ctest НЕ регистрируется — каждый прогон
  поднимает окно и стоит секунды, а ползунки телосложения уже судятся
  набором morph_bands (шаг 1) на тех же полосах.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Числа — из вывода судьи, не отсюда.
"""

import re
import subprocess
import sys
from pathlib import Path

BAKED = Path("assets/characters/presets/player.dfo")
PRESET = Path("assets/characters/presets/player.json")


def run_chargen(app: str, morph: str) -> None:
    env = {
        "DFN_CHARGEN": "1",
        "DFN_CHARGEN_DONE": "1",
        "DFN_MORPH": morph,
    }
    import os

    full = dict(os.environ)
    full.update(env)
    out = subprocess.run([app], env=full, capture_output=True, text=True, timeout=300)
    for line in out.stderr.splitlines():
        if "[создание]" in line:
            print("   " + line.strip())


def judge(tool: str, path: Path):
    out = subprocess.run([tool, str(path)], capture_output=True, text=True, timeout=120)
    text = out.stdout + out.stderr
    m = re.search(r"figure height ([0-9.]+) m .*?([0-9.]+) heads", text)
    height = float(m.group(1)) if m else -1.0
    heads = float(m.group(2)) if m else -1.0
    # Судья печатает отклонения в процентах; красным считается всё, что он сам
    # назвал вне полосы — строки с "!" в конце или "out of band".
    bad = [l for l in text.splitlines() if "OUT" in l.upper() or "!" in l]
    return height, heads, bad, out.returncode


def main(argv):
    if len(argv) < 3:
        raise SystemExit("usage: check_chargen.py <dfn_app> <dfn_human_scale>")
    app, tool = argv[1], argv[2]
    cases = [
        ("канон", "stature=1.75"),
        ("низ полосы", "stature=1.66"),
        ("верх полосы", "stature=1.84"),
        ("ниже полосы (зажим)", "stature=1.20"),
        ("выше полосы (зажим)", "stature=2.40"),
        ("всё на упоре + верх роста",
         "weight=0.65,belly=0.45,muscle=1.0,hips=1.0,shoulders=1.0,"
         "deltoid=1.0,buttocks=1.0,torso-depth=0.25,arm-length=1.0,"
         "leg-length=1.0,age=0.55,stature=1.84"),
    ]
    failures = 0
    for name, morph in cases:
        print(f"[{name}] DFN_MORPH={morph}")
        run_chargen(app, morph)
        if not BAKED.exists():
            print("   ОТКАЗ: выпеченного тела нет")
            failures += 1
            continue
        height, heads, bad, code = judge(tool, BAKED)
        state = "в каноне" if code == 0 else "ВНЕ КАНОНА"
        print(f"   судья: рост {height:.3f} м, {heads:.2f} головы — {state}")
        for line in bad[:6]:
            print("   " + line.strip())
        if code != 0:
            failures += 1
    print(f"итог: {len(cases) - failures} из {len(cases)} прогонов в каноне")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
