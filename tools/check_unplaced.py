#!/usr/bin/env python3
#
# Created: 22:08:2026 - 22:40:00
# Last updated: 22:08:2026 - 22:40:00
# File: tools/check_unplaced.py
#
# Responsibility:
# - СОПОСТАВИТЬ КУЗНИЦУ С РАСКЛАДКОЙ: что испечено в assets/houses, но нигде не
#   стоит в сцене. Дефект этого класса не виден НИ ИЗ КОДА, НИ ИЗ СЦЕНЫ по
#   отдельности — только из их сравнения, поэтому глазами он и не находится.
#
# Usage:
#     python3 tools/check_unplaced.py [<scene>] [<houses_dir>]
#     (умолчания: assets/scenes/whiterun.scene, assets/houses)
#
# Dependencies:
# - Uses: Python stdlib.
# - Used by: раскладчик города и приёмка волны.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ПРИБОР СООБЩАЕТ, А НЕ РУГАЕТСЯ. Выход ВСЕГДА нулевой. За один прогон он дал
#   три попадания с ТРЕМЯ РАЗНЫМИ причинами: принятое решение (city-keep),
#   забытый замысел (марши) и задокументированная подмена (усадьба). Две из трёх
#   законны. Прибор, который на такое ругается, приучает себя игнорировать, а
#   хуже того — провоцирует «починку» законного: подстановка city-manor обратно
#   вернула бы крыло усадьбы на главную улицу (прогон v5-1).
# - НЕРАЗМЕЩЁННОЕ — ВОПРОС, А НЕ ПРИГОВОР. Ответ живёт в комментарии у
#   раскладки, а не в данных, и прибор его знать не может. Он обязан отделить
#   «причина известна» от «проверь, почему» — и не притворяться, что знает
#   больше.
#
# UPD:
# - 22:08:2026 - 22:40:00: Создан. Заведён по одобрению архитектора после того,
#   как разовый прогон нашёл три неразмещённых рецепта разного происхождения.
#
from __future__ import annotations

import os
import re
import sys

# ПОЧЕМУ РЕЦЕПТ МОЖЕТ ЗАКОННО НЕ СТОЯТЬ В ГОРОДЕ.
# Ключ — имя файла, значение — причина. Всё, чего здесь нет, попадает в
# «проверь, почему»: умолчание — вопрос, а не оправдание.
KNOWN = {
    "log-replica.dfh": "рецепт демо-карты houses/demo, в городе ему не место",
    "frame-replica.dfh": "рецепт демо-карты houses/demo",
    "stone-replica.dfh": "рецепт демо-карты houses/demo",
    "l-house.dfh": "рецепт демо-карты houses/demo",
    "u-house.dfh": "рецепт демо-карты houses/demo",
    "city-keep.dfh": "РЕШЕНИЕ архитектора: город держит city-keep-s "
                     "(43.6x30 против 26.6x14, донжон попадал внутрь пятна)",
    "city-manor.dfh": "ПОДМЕНА в KIND: 14x15 не влезает в участок 9x7, "
                      "крыло легло на осевую улицу (v5-1). Ставится "
                      "city-house-l. НЕ ЧИНИТЬ подстановкой обратно",
    "city-manor-old.dfh": "то же, что city-manor",
    "city-plaza12.dfh": "заменена мощением city-cobble: борт 0.22 м "
                        "отвесной стены, капсула на него взбиралась",
    "city-plaza20.dfh": "то же, что city-plaza12",
}


# ЗАКОННО НЕИСПОЛЬЗОВАННЫЕ ПО ОБРАЗЦУ ИМЕНИ. Семейство наклонных кусков — это
# МЕНЮ: раскладка берёт ближайший к уклону участка, и часть уклонов не
# понадобится по построению. Ругаться на них значит топить настоящие находки в
# шуме — city-stairs потерялся бы среди девяти неиспользованных уклонов.
KNOWN_PATTERNS = [
    (re.compile(r"^city-cobble\d+x?\d*-s\d+\.dfh$"),
     "запас семейства уклонов: раскладка берёт ближайший, остальные ждут"),
]


def reason_for(name: str):
    if name in KNOWN:
        return KNOWN[name]
    for rx, why in KNOWN_PATTERNS:
        if rx.match(name):
            return why
    return None


def placed_in(scene: str) -> set[str]:
    text = open(scene, encoding="utf-8").read()
    return set(re.findall(r"([A-Za-z0-9_-]+\.dfh)", text))


def main(scene: str, houses: str) -> int:
    placed = placed_in(scene)
    all_files = sorted(f for f in os.listdir(houses) if f.endswith(".dfh"))
    unplaced = [f for f in all_files if f not in placed]

    known = [f for f in unplaced if reason_for(f)]
    ask = [f for f in unplaced if not reason_for(f)]

    print(f"испечено рецептов: {len(all_files)}; "
          f"размещено: {len(all_files) - len(unplaced)}; "
          f"не размещено: {len(unplaced)}\n")

    if ask:
        print("ПРОВЕРЬ, ПОЧЕМУ (причина не записана):")
        for f in ask:
            print(f"  {f}")
        print("\n  Причина может быть законной — тогда впиши её в KNOWN,")
        print("  чтобы следующий не искал заново. Может быть и забытым")
        print("  замыслом: так нашлись city-stairs и city-stairs6, которые")
        print("  чертёж требует, а раскладка не ставила ни разу.\n")
    else:
        print("Неразмещённых без записанной причины нет.\n")

    if known:
        print("не размещено ЗАКОННО (причина записана):")
        for f in known:
            print(f"  {f}\n      {reason_for(f)}")
    return 0  # прибор СООБЩАЕТ, а не ругается


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1
                  else "assets/scenes/whiterun.scene",
                  sys.argv[2] if len(sys.argv) > 2 else "assets/houses"))
