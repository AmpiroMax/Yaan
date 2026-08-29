"""
Module: tools/quality
File: tools/quality/check_furn_gabarit.py

Responsibility:
- СТОРОЖ ПЕРЕПЕЧКИ ПОЛКИ УБРАНСТВА. Печёт полку ДВАЖДЫ одним и тем же
  двоичным файлом — с нулевой фаской и со штатной — и сверяет ТРИ вещи:
    * габарит (x / z / низ / верх) — фаска режет внутрь и не растит;
    * БЮДЖЕТ ТЕЛА (треугольники) — иначе сторож пропустит случай «габарит
      сохранён, а тело стало вчетверо тяжелее»: у плоского ковра из двух плит
      пятно осталось 2.00 x 1.40, а трис выросли 24 -> 96 (замер волны
      номенклатуры, случай не гипотетический);
    * ХЭШИ ДВУХ ОДИНАКОВЫХ ВЫПЕЧЕК — перепечка подряд обязана совпасть.

ЗАЧЕМ ИМЕННО ДВЕ ВЫПЕЧКИ, А НЕ СВЕРКА С ПРОШЛЫМ КОММИТОМ (замечание
координатора, 28.08). Перечень прошлого коммита стареет вместе с деревом
(правило 54): сверка с ним мерила бы заодно всё, что за сутки поменялось
помимо фаски, и молчала бы о причине расхождения. И наоборот — контроль
«перепечка дважды = одинаково» габарит не ловит вовсе: две одинаково неверные
выпечки проходят его обе. Нужны ОБЕ руки, и они здесь обе.

ГАБАРИТ — НЕ УКРАШЕНИЕ ПЕРЕЧНЯ. Это колонка, по которой раскладка садит
предметы на столешницы и полки: молча уехавший сантиметр — это уехавшая
посуда, и увидят её через неделю на кадре, а не в тот день, когда она уехала.

ЧТО ИМЕННО ЗДЕСЬ ЗАПРЕЩЕНО, И ПОЧЕМУ НЕ «НИ ОДНОЙ ДЕСЯТОЙ». Фаска на
ПРЯМОМ угле габарит не двигает вовсе: крайнюю точку там держит ГРАНЬ, а не
вершина, и срез углов её не касается — так стоят 33 предмета из 35, кубок и
корзина в их числе. Но у тела, чья крайняя точка — ОСТРАЯ ВЕРШИНА (голова
зверя, косо поставленная ножка табурета), фаска эту вершину срезает, и это не
дефект, а определение фаски: срезанный угол короче несрезанного. Отсюда три
правила вместо одного:
  * габарит НЕ РАСТЁТ никогда (фаска режет внутрь) — допуск 1 мм;
  * усадка не больше ШИРИНЫ ФАСКИ на каждый конец оси — то есть до двух
    ширин у пятна x/z (у него срезаны оба конца) и до одной у «низа» и
    «верха» (там конец один);
  * всякая усадка ПЕЧАТАЕТСЯ поимённо — молчаливой она быть не может.
Плюс два поимённых образца (кубок и корзина): волна номенклатуры измерила у
них усадку 0.11 -> 0.10 и 0.50 -> 0.49 до починки, и они обязаны сходиться
ровно, иначе починка отката не заметит.

Usage:
    python3 tools/quality/check_furn_gabarit.py <путь к dfn_furn_objects>
                                                [--tol <метры>]
                                                [--limit <метры>]

  --limit — ОТРИЦАТЕЛЬНОЕ ПЛЕЧО (правило 30): предел усадки, заданный снаружи.
  При --limit 0 рука обязана ПОКРАСНЕТЬ на голове зверя и табурете — двух
  телах, чью крайнюю точку держит острая вершина. Рука, которую ничто не
  валит, — не рука, а описание.

  НЕ РАБОТАЕТ КАК ПЛЕЧО ШИРИНА ФАСКИ, и причина тоньше, чем «печь её не
  примет». Замер по всей полке, --bevel 0.06 против штатных 0.010: габарит
  СОШЁЛСЯ у всех 35 тел, число треугольников СОШЛОСЬ у всех 35, а байты
  разошлись у 23. То есть шестикратная ширина проходит на крупных телах
  (кровать, стол, кресло, комод) — те же грани стоят в других точках, — и не
  проходит только на двенадцати мелких и тонких (books, bowl, candlestick,
  chandelier, cup, fish, herbs, hide, plaque, rail, rug, tapestry), где потолок
  по куску её и режет.

  ПОЧЕМУ ЭТО ВСЁ РАВНО НЕ ПЛЕЧО: величина, которую сторожит эта рука, —
  ГАБАРИТ, а он на 60 мм не двигается. Плечо обязано менять ту величину, в
  которой живёт дефект (правило 48, третья часть), поэтому предел усадки
  задаётся снаружи ключом --limit, а не шириной.

  ОСТОРОЖНО СО СЛОВОМ «УПЁРТА» (поправка волны номенклатуры, 28.08). Первая
  редакция этой шапки утверждала, что полка при 0.06 байт в байт та же, что
  при штатной, — и это было ОБОБЩЕНИЕ С ДВЕНАДЦАТИ ТЕЛ НА ТРИДЦАТЬ ПЯТЬ.
  Проверялась колонка треугольников, а она к ширине НЕЧУВСТВИТЕЛЬНА ПО
  ПОСТРОЕНИЮ: грани те же, координаты другие. Прибор, не меняющийся вдоль того
  измерения, в котором живёт вопрос, отвечает «да» независимо от ответа.

  Запускать из корня репозитория. Ненулевой выход при расхождении.

Dependencies:
- Uses: dfn_furn_objects (ключ --bevel), assets/houses/furn-*.dfh.
- Used by: рукав ctest furn_gabarit_bevel.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- РУКА ПЕЧАТАЕТ ЧИСЛА, ПО КОТОРЫМ СУДИТ. «Габарит сошёлся» без таблицы
  расхождений не отличается от руки, которая не нашла ни одного предмета.
"""

import os
import subprocess
import sys
import tempfile

TOL_DEFAULT = 0.001  # метр: миллиметр, и он же шаг округления самого перечня
BEVEL_W = 0.010     # HOUSE_BEVEL_W_DEFAULT: глубже неё фаска резать не может
# Предел усадки по колонкам. x и z — ПЯТНО, то есть размах между двумя
# срезанными концами: две ширины. «низ» и «верх» — по одному концу: одна.
LIMIT = (2.0 * BEVEL_W, 2.0 * BEVEL_W, BEVEL_W, BEVEL_W)
# Бюджет тела. Полностью обфасоченная коробка — это 60 треугольников против
# 12, и шесть здесь не запас «на всякий», а ТОТ ЖЕ ПРЕДЕЛ с округлением вверх:
# предмет, выросший больше чем в шесть раз, вырос НЕ ОТ ФАСКИ. Общая доля
# ниже, потому что многогранники (кубок, бочка, миска) фаску продольных рёбер
# не берут вовсе.
TRIS_ITEM_MAX = 6.0
TRIS_TOTAL_MAX = 4.5

def read_index(path):
    """имя -> (x, z, низ, верх, треугольники, хэш)"""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if not line.startswith("furn-"):
                continue
            p = line.split()
            out[p[0]] = (float(p[1]), float(p[2]), float(p[3]), float(p[4]),
                         int(p[5]), p[7])
    return out

def bake(binary, out_dir, bevel):
    r = subprocess.run([binary, out_dir, "--bevel", str(bevel)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        raise SystemExit(f"[габарит] печь вернула {r.returncode} при фаске {bevel}")
    return read_index(os.path.join(out_dir, "INDEX.md"))

def main():
    args = sys.argv[1:]
    tol = TOL_DEFAULT
    limit = None
    if "--tol" in args:
        i = args.index("--tol")
        tol = float(args[i + 1])
        del args[i:i + 2]
    if "--limit" in args:
        i = args.index("--limit")
        limit = float(args[i + 1])
        del args[i:i + 2]
    if not args:
        raise SystemExit("usage: check_furn_gabarit.py <dfn_furn_objects> [--tol <м>]")
    binary = args[0]

    with tempfile.TemporaryDirectory(prefix="bevel_gab_") as tmp:
        sharp = bake(binary, os.path.join(tmp, "sharp"), 0.0)
        cut = bake_default(binary, os.path.join(tmp, "cut"))
        again = bake_default(binary, os.path.join(tmp, "cut-again"))

    if not sharp or len(sharp) != len(cut):
        raise SystemExit(f"[габарит] полки разной длины: {len(sharp)} и {len(cut)}")

    cols = ("x", "z", "низ", "верх")
    lim = LIMIT if limit is None else (limit,) * 4
    grown, over, shrank = [], [], []
    print(f"{'предмет':18}{'габарит без фаски':30}{'с фаской':30}"
          f"{'трис':>7}{'->':>4}{'':>7}")
    for name in sorted(sharp):
        a, b = sharp[name], cut[name]
        # x/z/верх — размеры от нуля предмета: рост это b > a. «низ»
        # отрицателен (тело ныряет под ноль), и рост габарита там — b < a.
        delta = [b[0] - a[0], b[1] - a[1], a[2] - b[2], b[3] - a[3]]
        worst_grow = max(delta)
        worst_shrink = -min(delta)
        mark = ""
        if worst_grow > tol:
            grown.append((name, worst_grow,
                          [cols[i] for i in range(4) if delta[i] > tol]))
            mark = "  <<< ВЫРОС"
        elif worst_shrink > tol:
            shrank.append((name, worst_shrink,
                           [cols[i] for i in range(4) if -delta[i] > tol]))
            mark = f"  усадка {worst_shrink * 1000:.0f} мм"
            worst_over = max(-delta[i] - lim[i] for i in range(4))
            if worst_over > tol:
                over.append((name, worst_shrink))
                mark = f"  <<< УСАДКА {worst_shrink * 1000:.0f} мм СВЕРХ ФАСКИ"
        print(f"{name:18}{str(a[:4]):30}{str(b[:4]):30}"
              f"{a[4]:7d}{'->':>4}{b[4]:7d}{mark}")

    named = []
    for name in ("furn-cup", "furn-basket"):
        if name not in sharp:
            named.append((name, "нет на полке"))
            continue
        d = max(abs(sharp[name][i] - cut[name][i]) for i in range(4))
        if d > tol:
            named.append((name, f"{d * 1000:.1f} мм"))

    heavy = []
    for name in sorted(sharp):
        r = cut[name][4] / max(sharp[name][4], 1)
        if r > TRIS_ITEM_MAX:
            heavy.append((name, r))
    total_ratio = sum(v[4] for v in cut.values()) / max(
        sum(v[4] for v in sharp.values()), 1)

    twice = [n for n in sorted(cut) if cut[n][5] != again[n][5]]
    print(f"[габарит] предметов {len(sharp)}, допуск {tol * 1000:.0f} мм, "
          f"ширина фаски {BEVEL_W * 1000:.0f} мм")
    print(f"[габарит] трис без фаски {sum(v[4] for v in sharp.values())}, "
          f"с фаской {sum(v[4] for v in cut.values())}")
    print(f"[габарит] усадок всего {len(shrank)}, из них сверх фаски {len(over)}")
    print(f"[габарит] бюджет тела: полка x{total_ratio:.2f} "
          f"(предел x{TRIS_TOTAL_MAX}), тяжелее x{TRIS_ITEM_MAX} предметов "
          f"{len(heavy)}")
    for name, r in heavy:
        print(f"[габарит] ТЕЛО ВЫРОСЛО {name}: x{r:.2f}")
    for name, worst, which in grown:
        print(f"[габарит] ВЫРОС {name}: {worst * 1000:.1f} мм по {', '.join(which)}")
    for name, worst in over:
        print(f"[габарит] УСАДКА СВЕРХ ФАСКИ {name}: {worst * 1000:.1f} мм")
    for name, why in named:
        print(f"[габарит] ПОИМЕННЫЙ ОБРАЗЕЦ {name} разошёлся: {why}")
    if twice:
        print(f"[габарит] ПЕРЕПЕЧКА ДВАЖДЫ РАЗОШЛАСЬ: {', '.join(twice)}")
    if grown or over or named or twice or heavy or total_ratio > TRIS_TOTAL_MAX:
        if total_ratio > TRIS_TOTAL_MAX:
            print(f"[габарит] ПОЛКА ПЕРЕВЕСИЛА БЮДЖЕТ: x{total_ratio:.2f}")
        return 1
    print("[габарит] габарит не растит и не режет глубже своей ширины; тело в "
          "бюджете; перепечка дважды совпала")
    return 0

def bake_default(binary, out_dir):
    r = subprocess.run([binary, out_dir], stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        raise SystemExit(f"[габарит] печь вернула {r.returncode} при штатной фаске")
    return read_index(os.path.join(out_dir, "INDEX.md"))

if __name__ == "__main__":
    sys.exit(main())
