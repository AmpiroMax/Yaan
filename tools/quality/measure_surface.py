"""
Module: tools/quality
File: tools/quality/measure_surface.py

Responsibility:
- ПРИБОР ПРИЁМКИ ПОВЕРХНОСТИ, критерии К1-К6 ТЗ материалов
  (docs/reports/materials-spec/index.html §2). Мерит УЧАСТОК ОДНОГО ВЕЩЕСТВА
  на кадре, а не кадр целиком: иначе яркий очаг вытянет среднее и спрячет
  плоскую стену рядом.
    К1 СТРУКТУРА    ДЕТАЛЬ >= 4.0 (матовое вещество) | поведение блика (гладкое)
    К2 ТОН          энтропия яркости >= 6.3 бит
    К3 ЦВЕТНОСТЬ    >= 200 цветов на 1000 пикселей
    К4 РЕБРО        доля рёбер с фаской = 100 % — ГЕОМЕТРИЯ, не кадр:
                    приходит отчётом dfn_bevel_check (--k4-report)
    К5 ПОСАДКА      контактное затемнение: полоса 3 см темнее той же
                    поверхности в 30 см не менее чем на 15 %
    К6 РАЗБРОС      два экземпляра одного рецепта расходятся по средней
                    яркости не менее чем на 4 %

АВТОРСТВО. Величины, полосы и сам метод (декодер PNG на zlib, ДЕТАЛЬ как
средний модуль отклонения от окрестности 3x3) — ресёрчер, записка №3 от
28.08.2026, там же таблица двенадцати участков, по которой выведены пороги.
Волна фаски перевела черновик в штатный прибор: разбор ключей, рукав в ctest,
контрольные руки, ветвь гладких веществ и К4 из геометрии.

РАСХОЖДЕНИЕ Р1 (docs/reports/materials-divergence, принято координатором
28.08) — ПОЧЕМУ У К1 ДВЕ ВЕТВИ. Порог ДЕТАЛЬ >= 4.0 выведен на МАТОВЫХ
участках и честен только там. У гладкого вещества — полированного золота,
стекла, льда, стоячей воды, свежей побелки — локальной детальности почти нет
ПО ПРИРОДЕ: такой участок провалит К1 ИМЕННО ПОТОМУ, ЧТО СДЕЛАН ПРАВИЛЬНО, а
исполнитель, глядя на красное число, добавит золоту зерно, которого там быть
не должно. Поэтому К1 применяется к веществам с шероховатостью >= 0.5, а
гладкие судятся ПОВЕДЕНИЕМ БЛИКА — тремя замерами, которые уже делает
прототип материалов (MATERIALS.md §5.3, кадр герба): блик ЕСТЬ; он НЕСЁТ ЦВЕТ
вещества; он НЕ ВЫБИТ В БЕЛЫЙ. Второго прибора не заведено нарочно: две руки,
меряющие «структуру» по-разному, разошлись бы в первый же день.

Usage:
    python3 tools/quality/measure_surface.py <кадр.png> --patch <имя>:<x0,y0,x1,y1>
                                             [--gloss <имя>] [--pair <имя>=<имя>]
                                             [--contact <имя>=<имя>]
                                             [--k4 <отчёт dfn_bevel_check>]
                                             [--require <К1,К2,...>]
    python3 tools/quality/measure_surface.py --self-test

  Рамка участка — доли ширины и высоты кадра (0..1), как в записке ресёрчера.
  Кадр — FullHD БЕЗ масштабирования: пересчёт при уменьшении сам добавляет и
  детальность на пиксель, и число цветов, то есть завышает оценку.
  --require просит ненулевой выход, если названные критерии не сошлись.

Dependencies:
- Uses: tools/quality/png_read.py (декодер PNG на zlib), numpy СИСТЕМНОГО
  питона (окружение music не требуется: прибор зовётся из тестов).
- Used by: рукав ctest quality_surface_selftest, приёмка волн материалов,
  отчёты docs/reports/*.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КАЖДЫЙ КРИТЕРИЙ ЕДЕТ СО СВОЕЙ КОНТРОЛЬНОЙ РУКОЙ (правило 30): --self-test
  прогоняет участки, которые обязаны ПРОВАЛИТЬСЯ, и участки, которые обязаны
  ПРОЙТИ. Критерий, который ничто не валит, — не критерий, а описание.
- К1-К3 МЕРЯЮТСЯ ПО ВЕЩЕСТВУ, А НЕ ПО КАДРУ. Участок меньше 100x100 пикселей
  прибор отвергает: на нём разброс величины больше самой величины.
- ЕДИНИЦА ИЗМЕРЕНИЯ — ЧАСТЬ ПО ВЕЩЕСТВУ, А НЕ ОБЪЕКТ (замер номенклатуры по
  всем 35 предметам полки, принят координатором 28.08). 23 объекта из 35
  несут два вещества и больше в одном теле — окованный сундук это дуб И
  железо, — и средняя детальность такого объекта не описывает ни дуб, ни
  железо, а ветвь «матовое/гладкое» на нём считалась бы на ничьём. Участок
  `--patch` обязан лежать ВНУТРИ одной части: в .dfo части уже разложены
  бакетами по паре (mat, tone), и резать надо по ним.
- СПИСОК ГЛАДКИХ ВЕДЁТСЯ ПОИМЁННО ПО ЧАСТЯМ И НЕ ВЫВОДИТСЯ ИЗ ОРДИНАЛА
  ВЕЩЕСТВА. Соблазн «mat=3 — значит камень, значит гладкое» ломается на
  первом же образце: земляной ком в furn-plant — тот же mat=3 и обязан
  остаться матовым. Обратный случай тоже есть: у furn-jug глазурь названа в
  отчёте словами, а в рецепте стоит голая глина. Список пересматривается
  после каждой волны веществ, и до тех пор он — довод в отчёте, а не
  умолчание прибора.
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import png_read as png  # noqa: E402  (декодер лежит рядом; путь ставится выше)

# ---------------------------------------------------------------------------
# Пороги. ВСЕ ЧИСЛА — ИЗ §2 ЗАПИСКИ РЕСЁРЧЕРА, и ни одно не выбрано здесь.
# ---------------------------------------------------------------------------
K1_DETAIL = 4.0        # референсная полоса 4.96-9.31; ниже 3.0 — брак
K2_ENTROPY = 6.3       # бит; референс 5.87-7.62
K3_COLOURS = 200.0     # цветов на 1000 пикселей; референс 133-640
K4_SHARE = 1.0         # доля рёбер с фаской
K5_CONTACT = 0.15      # затемнение в стыке, доля
K6_SPREAD = 0.04       # расхождение двух экземпляров, доля
MIN_PATCH_PX = 100 * 100

# Ветвь гладких веществ (расхождение Р1). Блик ЕСТЬ: верхний процентиль
# участка ярче медианы во столько раз. НЕСЁТ ЦВЕТ: насыщенность блика не ниже
# доли от насыщенности самого вещества. НЕ ВЫБИТ: доля пикселей на упоре
# (>= 250 по всем каналам) не выше.
GLOSS_HILIGHT = 1.8
GLOSS_CHROMA = 0.35
GLOSS_CLIPPED = 0.02

def luminance(px):
    return 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]

def crop(rgb, w, h, frame):
    x0, y0, x1, y1 = frame
    return [[rgb[y][x] for x in range(int(x0 * w), int(x1 * w))]
            for y in range(int(y0 * h), int(y1 * h))]

def stats(patch):
    """Пять величин участка: среднее, СКО, ДЕТАЛЬ, энтропия, цветность."""
    lum = [[luminance(p) for p in row] for row in patch]
    flat = [v for row in lum for v in row]
    n = len(flat)
    if n == 0:
        raise SystemExit("[поверхность] пустой участок")
    mean = sum(flat) / n
    sd = math.sqrt(sum((v - mean) ** 2 for v in flat) / n)
    acc, cnt = 0.0, 0
    for y in range(1, len(lum) - 1):
        for x in range(1, len(lum[0]) - 1):
            m = sum(lum[y + dy][x + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)) / 9.0
            acc += abs(lum[y][x] - m)
            cnt += 1
    detail = acc / max(cnt, 1)
    hist = [0] * 256
    for v in flat:
        hist[min(255, int(v))] += 1
    ent = -sum((c / n) * math.log2(c / n) for c in hist if c)
    uniq = len({p for row in patch for p in row})
    return {"mean": mean, "sd": sd, "detail": detail, "entropy": ent,
            "colours": uniq * 1000.0 / n, "px": n, "lum": flat}

def gloss_stats(patch):
    """Три замера блика — ветвь К1 для гладких веществ (расхождение Р1)."""
    lum = sorted(luminance(p) for row in patch for p in row)
    n = len(lum)
    median = lum[n // 2]
    top = lum[int(n * 0.99)]
    hilight = top / max(median, 1.0)

    def chroma(px):
        hi, lo = max(px), min(px)
        return 0.0 if hi == 0 else (hi - lo) / hi

    thr = lum[int(n * 0.95)]
    bright = [p for row in patch for p in row if luminance(p) >= thr]
    body = [p for row in patch for p in row if luminance(p) < median]
    c_bright = sum(chroma(p) for p in bright) / max(len(bright), 1)
    c_body = sum(chroma(p) for p in body) / max(len(body), 1)
    clipped = sum(1 for p in bright if min(p) >= 250) / max(len(bright), 1)
    return {"hilight": hilight, "chroma_ratio": c_bright / max(c_body, 1e-6),
            "clipped": clipped}

def verdict(ok):
    return "ЗЕЛЁНЫЙ" if ok else "КРАСНЫЙ"

def measure(frame_path, patches, gloss, pairs, contacts, k4_report):
    w, h, rgb = png.load(frame_path)
    rows, results = [], {}
    for name, box in patches.items():
        patch = crop(rgb, w, h, box)
        s = stats(patch)
        if s["px"] < MIN_PATCH_PX:
            raise SystemExit(f"[поверхность] участок {name}: {s['px']} пкс, "
                             f"нужно не меньше {MIN_PATCH_PX} — прибор отказывает")
        results[name] = s
        if name in gloss:
            g = gloss_stats(patch)
            s.update(g)
            k1 = (g["hilight"] >= GLOSS_HILIGHT
                  and g["chroma_ratio"] >= GLOSS_CHROMA
                  and g["clipped"] <= GLOSS_CLIPPED)
            note = (f"блик x{g['hilight']:.2f} цвет {g['chroma_ratio']:.2f} "
                    f"выбито {g['clipped'] * 100:.1f}%")
        else:
            k1 = s["detail"] >= K1_DETAIL
            note = "матовое"
        s["k1"] = k1
        rows.append((name, s, note))

    print(f"{'участок':26}{'сред':>7}{'СКО':>7}{'ДЕТАЛЬ':>8}{'энтроп':>8}"
          f"{'цв/1000':>9}{'пкс':>9}  К1")
    for name, s, note in rows:
        print(f"{name:26}{s['mean']:7.1f}{s['sd']:7.2f}{s['detail']:8.3f}"
              f"{s['entropy']:8.3f}{s['colours']:9.1f}{s['px']:9d}  "
              f"{verdict(s['k1'])} ({note})")

    got = {}
    got["К1"] = all(s["k1"] for _, s, _ in rows)
    got["К2"] = all(s["entropy"] >= K2_ENTROPY for _, s, _ in rows)
    got["К3"] = all(s["colours"] >= K3_COLOURS for _, s, _ in rows)

    if k4_report:
        share = None
        with open(k4_report, encoding="utf-8") as f:
            for line in f:
                if line.startswith("k4_share"):
                    share = float(line.split()[1])
        if share is None:
            raise SystemExit(f"[поверхность] в {k4_report} нет строки k4_share")
        got["К4"] = share >= K4_SHARE - 1e-6
        print(f"К4 РЕБРО      доля рёбер с фаской {share * 100:.2f}%  "
              f"{verdict(got['К4'])}  (геометрия, {k4_report})")

    for near, far in contacts.items():
        a, b = results[near]["mean"], results[far]["mean"]
        drop = (b - a) / max(b, 1e-6)
        ok = drop >= K5_CONTACT
        got["К5"] = got.get("К5", True) and ok
        print(f"К5 ПОСАДКА    {near} темнее {far} на {drop * 100:.1f}%  {verdict(ok)}")

    for one, two in pairs.items():
        a, b = results[one]["mean"], results[two]["mean"]
        spread = abs(a - b) / max(a, b, 1e-6)
        ok = spread >= K6_SPREAD
        got["К6"] = got.get("К6", True) and ok
        print(f"К6 РАЗБРОС    {one} и {two} расходятся на {spread * 100:.2f}%  "
              f"{verdict(ok)}")
    return got

# ---------------------------------------------------------------------------
# КОНТРОЛЬНЫЕ РУКИ (правило 30). Фикстуры рисуются здесь же, а не лежат
# файлами: участок, который обязан провалить К1, — это ровная заливка, и
# хранить её картинкой значило бы хранить то, что можно назвать одной строкой.
# ---------------------------------------------------------------------------
def synth(kind, size=140, seed=1):
    import random
    rnd = random.Random(seed)
    out = []
    for y in range(size):
        row = []
        for x in range(size):
            if kind == "flat":
                row.append((120, 110, 96))
            elif kind == "structured":
                # Волокно + зерно + шов: то, чего нашим поверхностям и не
                # хватает по замеру ресёрчера.
                base = 120 + 40 * math.sin(x * 0.9) + 25 * math.sin(y * 0.31)
                base += rnd.uniform(-38, 38)
                if x % 37 < 2:
                    base -= 55
                v = int(max(0, min(255, base)))
                row.append((v, int(v * 0.92), int(v * 0.76)))
            elif kind == "gold":
                # Гладкое вещество: почти нет локальной структуры, но есть
                # цветной блик — К1 матовой ветвью провалится, глянцевой нет.
                s = math.exp(-((x - 70) ** 2 + (y - 70) ** 2) / 900.0)
                v = 70 + 150 * s
                row.append((int(min(255, v * 1.0)), int(min(255, v * 0.78)),
                            int(min(255, v * 0.30))))
            elif kind == "gold-blown":
                s = math.exp(-((x - 70) ** 2 + (y - 70) ** 2) / 900.0)
                v = 70 + 400 * s
                g = int(min(255, v))
                row.append((g, g, g))
            else:
                raise SystemExit(f"нет фикстуры {kind}")
        out.append(row)
    return out

def self_test():
    bad = []

    def expect(what, got, want):
        mark = "ok" if got == want else "ПРОВАЛ"
        if got != want:
            bad.append(what)
        print(f"  {what:52} {verdict(got):8} ждали {verdict(want):8} {mark}")

    print("[поверхность] контрольные руки К1 (правило 30)")
    flat = stats(synth("flat"))
    expect("ровная заливка обязана провалить К1", flat["detail"] >= K1_DETAIL, False)
    good = stats(synth("structured"))
    expect("волокно+зерно+шов обязаны пройти К1", good["detail"] >= K1_DETAIL, True)
    print(f"  ДЕТАЛЬ: заливка {flat['detail']:.3f}, структура {good['detail']:.3f}, "
          f"порог {K1_DETAIL}")

    print("[поверхность] контрольные руки К1, ветвь гладких (расхождение Р1)")
    gold = synth("gold")
    gm, gg = stats(gold), gloss_stats(gold)
    expect("полированное золото ПРОВАЛИТ матовую ветвь", gm["detail"] >= K1_DETAIL,
           False)
    expect("оно же обязано пройти ветвь блика",
           gg["hilight"] >= GLOSS_HILIGHT and gg["chroma_ratio"] >= GLOSS_CHROMA
           and gg["clipped"] <= GLOSS_CLIPPED, True)
    blown = gloss_stats(synth("gold-blown"))
    expect("блик, выбитый в белый, обязан провалить ветвь блика",
           blown["hilight"] >= GLOSS_HILIGHT
           and blown["chroma_ratio"] >= GLOSS_CHROMA
           and blown["clipped"] <= GLOSS_CLIPPED, False)
    print(f"  блик: золото x{gg['hilight']:.2f} цвет {gg['chroma_ratio']:.2f} "
          f"выбито {gg['clipped'] * 100:.1f}%; пересвет x{blown['hilight']:.2f} "
          f"цвет {blown['chroma_ratio']:.2f} выбито {blown['clipped'] * 100:.1f}%")

    print("[поверхность] контрольные руки К6")
    a = stats(synth("structured", seed=1))
    b = stats(synth("structured", seed=1))
    same = abs(a["mean"] - b["mean"]) / max(a["mean"], b["mean"])
    expect("два побитово одинаковых стула обязаны провалить К6",
           same >= K6_SPREAD, False)
    c = stats(synth("structured", seed=7))
    for row in c:
        pass
    shifted = stats([[(min(255, int(p[0] * 1.12)), min(255, int(p[1] * 1.12)),
                       min(255, int(p[2] * 1.12))) for p in row]
                     for row in synth("structured", seed=1)])
    diff = abs(a["mean"] - shifted["mean"]) / max(a["mean"], shifted["mean"])
    expect("экземпляры со сдвигом тона обязаны пройти К6", diff >= K6_SPREAD, True)
    print(f"  расхождение: клоны {same * 100:.2f}%, сдвинутый {diff * 100:.2f}%, "
          f"порог {K6_SPREAD * 100:.0f}%")

    print("[поверхность] контрольная рука К5")
    lit, dark = 140.0, 140.0 * (1.0 - 0.22)
    expect("контактное затемнение 22% обязано пройти К5",
           (lit - dark) / lit >= K5_CONTACT, True)
    expect("затемнение 5% обязано провалить К5",
           (lit - lit * 0.95) / lit >= K5_CONTACT, False)

    ref = os.path.join("images_examples", "houses_indoors", "image copy 9.png")
    if os.path.exists(ref):
        print("[поверхность] референсный кадр владельца — участок половиц")
        w, h, rgb = png.load(ref)
        s = stats(crop(rgb, w, h, (.18, .86, .34, .96)))
        expect("эталон Скайрима обязан пройти К1", s["detail"] >= K1_DETAIL, True)
        print(f"  ДЕТАЛЬ {s['detail']:.3f}, энтропия {s['entropy']:.3f}, "
              f"цветность {s['colours']:.1f}")
    else:
        # КАДРЫ ВЛАДЕЛЬЦА В GIT НЕ ИДУТ (.gitignore). Молчаливый пропуск был бы
        # хуже отсутствия руки: рука, которой нет, обязана СКАЗАТЬСЯ.
        print(f"[поверхность] ПРОПУЩЕНО: нет {ref} (кадры владельца вне git)")

    if bad:
        print(f"[поверхность] контрольные руки ПРОВАЛИЛИСЬ: {len(bad)}")
        for b in bad:
            print(f"  - {b}")
        return 1
    print("[поверхность] все контрольные руки сошлись")
    return 0

def main():
    args = sys.argv[1:]
    if "--self-test" in args:
        return self_test()
    if not args:
        raise SystemExit(__doc__.split("Usage:")[1].split("Dependencies")[0])

    frame, patches, gloss, pairs, contacts, k4, require = args[0], {}, set(), {}, {}, None, []
    i = 1
    while i < len(args):
        a = args[i]
        if a == "--patch":
            name, box = args[i + 1].split(":")
            patches[name] = tuple(float(v) for v in box.split(","))
            i += 2
        elif a == "--gloss":
            gloss.add(args[i + 1])
            i += 2
        elif a == "--pair":
            one, two = args[i + 1].split("=")
            pairs[one] = two
            i += 2
        elif a == "--contact":
            near, far = args[i + 1].split("=")
            contacts[near] = far
            i += 2
        elif a == "--k4":
            k4 = args[i + 1]
            i += 2
        elif a == "--require":
            require = args[i + 1].split(",")
            i += 2
        else:
            raise SystemExit(f"[поверхность] неизвестный ключ {a}")

    got = measure(frame, patches, gloss, pairs, contacts, k4)
    print("[поверхность] " + ", ".join(f"{k} {verdict(v)}" for k, v in sorted(got.items())))
    missed = [k for k in require if not got.get(k, False)]
    if missed:
        print(f"[поверхность] НЕ СОШЛИСЬ: {', '.join(missed)}")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
