#!/usr/bin/env python3
# Created: 21:08:2026 - 04:10:00
# Last updated: 22:08:2026 - 16:05:00
# Module: tools
# File: tools/gen_whiterun.py
#
# Responsibility:
# - ГЕНЕРАТОР КАРТЫ ВАЙТРАНА ИЗ ПЛАНА (заказ 21.08: «с нуля, начав со
#   схемы»). ЕДИНСТВЕННЫЙ источник композиции — docs/WHITERUN_PLAN.json,
#   который экспортирует чертёж tools/gen_whiterun_plan.py. Здесь ТОЛЬКО
#   перевод плана в термины движка: зоны -> пады, дороги -> тропы рельефа,
#   дома по kind -> .dfh с поворотом от дверей, дорожки -> цепочки плит
#   furn-walk2, зелень по видам. Править композицию — в чертеже, не тут.
#
# UPD:
# - 21:08:2026 - 04:10:00: Создан.
# - 21:08:2026 - 00:10:00: ЛЕСТНИЦЫ ЛОЖАТСЯ НА СКЛОНЫ, А НЕ НА ПЛАТО. Бот трижды
#   застревал в (118, 30.5, 124) — «невидимый блокер» оказался склоном рельефа:
#   подъёмы террас живут в town-land.relief пологими валами (T2: z120..130,
#   35 гр. в середине), а марши стояли целиком на плато. Профили промерены
#   честной пробой terrain_height (build_world_context на pads+relief сцены):
#   T2 -> (116,25,120); перешеек -> (164,32,64); замок -> (174,37,50).
#   smooth_keep_hump: ручной горб town-land (+3..5 м) на кромке замкового плато
#   гасится с каймой 5 м — он перекрывал единственный заход отвесом 45-70 гр.
#   Кольцо стен: восточная грань шла меридианом x166-168 ПО перешейку и резала
#   его лестницу; теперь обходит по кромке оврага x182-184, разрыв
#   (180,52)->(170,52) — проход под марш замка с башнями по флангам.
# - 21:08:2026 - 01:35:00: Просека лестницы T2 ([pad] 118/128 h25): марш стоит
#   над плоским дном — рисуемая земля не протыкает плиты (глаза, rec_00528).
#   Сам стоп в (118,·,123.93) оказался ЗАЖИМОМ КАПСУЛЫ НА КРОМКАХ ступеней —
#   починен рампой в HouseStairs.cpp; здесь только геометрия про склоны.
# - 21:08:2026 - 13:10:00: ЕСТЕСТВЕННЫЙ ГОРОД (заказ 21.08, шесть пунктов).
#   Сетка высот: двухпроходная схема gen -> dump_heights -> gen; ground()
#   билинейно семплит итоговую землю (пады+relief) — стены, башни, ворота,
#   мост и жилые дома садятся на неё, а не на номинал террасы. Кольцо стен —
#   неровный овал из 26 вершин вместо прямоугольника; wall_run кладёт сегменты
#   с вкопом, коротким шагом на крутых кусках, серединой в min (дамба через
#   русло) и ПРИЖИМАЕТ последний сегмент к вершине грани — его хвост залезал
#   в проём ворот, и бот упирался в вертикальную грань (char-трасса ворот:
#   нормаль (0.20, 0, 0.98)). Мост стоял на траве в шести метрах от русла
#   (глаза) — центр настила посажен на русло (126,163), торцы аппарелей на
#   берега, рукав продлён на юг до низовья новой [river], плита стыкует его
#   с улицей. Жилой слой houseJ: джиттер сдвига/поворота (не по осям) и пол
#   по ground. Рынок: шесть прилавков кольцом вокруг колодца. Ландшафт:
#   четыре холма и ложбина падами за стенами.
# - 21:08:2026 - 14:40:00: Дороги — ТРОПАМИ (механика ReliefPath, заказ 21.08):
#   road()/append_roads пишут восемь кривых path/pp в whiterun.relief, плиты
#   остались площадям. Жилой слой — пул rich/mid/poor (dwelling): три новых
#   типа кузницы и -old износы; усадьба НЕ в пуле фронтов — крыло манора
#   вылезало на осевую (бот v14 лез на его крышу), ставится только явно
#   (Серые Гривы). Расстановочные фиксы рабочего: 8 домов с русла, 5 из тела
#   стены, рынок 15 гр., подпор кромки с изломами. Круги v13/v15 чистые.
# - 21:08:2026 - 14:40:00: ДЕФЕКТЫ РАССТАНОВКИ по визуальной приёмке (кадры
#   глазами, 21.08). (1) Двухэтажный дом с красной черепицей стоял сваями В
#   РУСЛЕ: промер отпечатков (углы дома, а не точка pos — origin детали лежит
#   в её углу) нашёл восемь построек ближе 8 м к ломаным [river]; таверна
#   u-house, Г-образный дом, четыре city-house-s и хутор переставлены на берег
#   руками, координаты в вызовах ниже с пометкой «21.08: с русла». (2) Дома,
#   сидевшие В ТЕЛЕ кольца стен (кровля врезалась в кладку): четыре дома были
#   ближе 6 м к отрезкам ring — отодвинуты внутрь квартала. (3) Рынок
#   «прямоугольник строго по осям» — city-plaza20 повёрнута на 15 гр. вокруг
#   своего центра (114,148); origin пересчитан от центра, колодец и кольцо
#   прилавков (радиус 7.0) остались на плите. (4) «Две идеально прямые дороги»
#   — улицы больше не колонна плит по одной оси: street() кладёт city-plaza12
#   ЦЕНТРОМ на точки осевой ломаной с шагом 10 м (нахлёст 2 м), поворот каждой
#   плиты берётся у местной касательной; главная улица уходит от ворот к мосту
#   дугой 0->18 гр. изломами по 6 гр., улица средней террасы — 0->-6 гр.
#   Подпорные стенки кромки T2 (два прогона строго В-З) тоже получили излом
#   +-4 гр. и смещение +-0.9 м по z. Попутно: сруб средней террасы стоял
#   отпечатком в нефе храма Кинарет — тот же дефект, (96,58) -> (102,58).
# - 21:08:2026 - 20:35:00: ГОРОД ПО ДИЗАЙН-ГАЙДУ (docs/CITY_DESIGN_GUIDE.md,
#   заказ «плотно поисследовать и сделать по гайдлайнам»). Главное правило из
#   исследования: у настоящего Вайтрана ~20 зданий, сжимается не пространство,
#   а ПЛОТНОСТЬ — дома фасадными рядами ВПЛОТНУЮ вдоль улиц (фронт 6-10 м,
#   щели 0.35-0.7), ни один не параллелен соседнему, за каждым — дворик с
#   поленницей/бочкой и межевым плетнём (дворовый набор furn-* кузницы
#   мебели). street_front() кладёт ряды по осевым тех же троп-дорог; жилой
#   ковёр точечных домов снят, зданий ~30 (было 147 при эталоне 20);
#   Йоррваскр повёрнут на 45 («старше города»), рынок — колодец в центре
#   кольца прилавков на повёрнутой плите. Улицы упираются в доминанты:
#   главная — в мост и лестницу, улица Ветров — в Гилдергрин.
# - 21:08:2026 - 21:45:00: ВОЛНА «ЗАМОК, АККУРАТНОСТЬ, УБРАНСТВО» (заказы
#   21.08 вечера). Дома аккуратнее: дыхание ряда ±3, дворов ±6, посадка
#   sit_y по МИНИМУМУ пятна (центр+4 угла) — угол больше не висит на склоне.
#   Новый солидный замок (26х14, конёк 18.8 ярусами): донжоны выведены во
#   фланги двора, в подпоре плато прорезан проход под парадный марш, пад
#   «парадный двор» (197,54) сажает подножие марша на землю. Кузница у
#   ворот и алхимик сведены с осевых улиц. ВНУТРЕННЕЕ УБРАНСТВО v1:
#   FURN-раскладки по типам жилья (очаг у глухой стены, стол+лавки,
#   кровать, стеллаж; у «-old» беднее с бочкой), furnish() поворачивает
#   локальные позиции матрицей сцены, +0.12 на верх половой плиты — ~93
#   предмета. Круг v24 чистый (2943 кадра) под радиусным вотчдогом v2.
# - 22:08:2026 - 01:30:00: ПЕРЕПИСАН С НУЛЯ: читает docs/WHITERUN_PLAN.json
#   (экспорт чертежа tools/gen_whiterun_plan.py) — одна правда о городе.
#   Прежний путь «город руками в коде» с его историей выше заменён планом;
#   зоны -> пады, дороги -> тропы, дома по kind, дорожки цепочками плит.
# - 22:08:2026 - 02:00:00: РЕЛЬЕФ РАСТЕРИЗУЕТСЯ ИЗ ПОЛИГОНОВ ЧЕРТЕЖА
#   (эллипсы-пады дали паразитные горки — бот v5-3 утыкался у рынка): 256х256
#   point-in-polygon по зонам + микропятна + тройной box-блюр, в relief идут
#   дельты от натуральной земли (замер WHITERUN_BARE=1 -> dump natural).
#   Посадка домов ДВЕРНОЙ ГРАНЬЮ на красную линию чертежа с отодвигом от
#   полос дорог по нормали (rect_road_hit, рынок-буфер исключён); проверки
#   расстановки в генераторе. Круг v5-5 пройден целиком без застреваний.
# - 22:08:2026 - 14:20:00: ВОДА ЛЕЖИТ НА ЗЕМЛЕ (претензия критика: «мост стоит
#   на траве», тяжесть 3). Отметка воды задавалась формулой 23.6 - i*0.08 и
#   уходила на 3-4 м НИЖЕ земли, а ручной relief — он в compose_passes идёт
#   ПОСЛЕ apply_rivers — своими дельтами зарисовывал русло обратно: реки в
#   кадре не было вовсе. Теперь (1) river_levels снимает отметку с ФАКТИЧЕСКОЙ
#   земли — минимальная кромка двух берегов минус 0.40, монотонно НЕВОЗРАСТАЯ
#   вниз по течению (проверено: 37.9 у истока -> 24.6 к устью); (2) дельты
#   relief гасятся river_taper — тем же smoothstep, каким движок сводит дно с
#   берегом, поэтому вырез движка доживает до кадра (дно 23.60, вода 24.60,
#   кромка 25.0-27.6 на профиле z=130); (3) натуральная земля меряется
#   WHITERUN_BARE=1 БЕЗ [river] — иначе в nat уже сидит вырез старого русла и
#   дельта берега считается от дна. bank_m 5 -> 3.0: берег 1.4 м на 3 м (25
#   гр., ходибельно) и торцы моста выходят ЗА зону сведения. Мост: отметка
#   бралась с земли в 7 м восточнее (на дамбе) — теперь от воды, +1.05 к
#   origin; пролёт центрируется на осевой (точка [center] чертежа лежала в
#   0.66 м западнее, восточная аппарель висела метром выше земли), подходы
#   планируются grade_corridor — новый инструмент планировки поля высот.
# - 22:08:2026 - 16:05:00: УБРАНСТВО, СВЕТ ОЧАГОВ, ДВОРЫ (претензии критика
#   [7][10][29]). (1) furnish() возвращён из истории (2c397d3) и переписан
#   под новую посадку: раскладки на ВСЕ обитаемые рецепты (дом малый и
#   большой, лавка, кузница, длинный зал, храм, амбар, мельница, зал замка),
#   локальные точки крутятся матрицей сцены. Верх ПОЛОВОЙ ПЛИТЫ у рецептов
#   разный, а прежний furnish клал всё на +0.12 — в city-house-l (плита на
#   0.585) мебель тонула в полу на полметра; теперь таблица FLOOR по замеру.
#   (2) Очаг зажигает [light] 4.6 м тёплого; тени просят только длинный зал
#   и замок — слотов ровно два (MAX_SHADOW_POINT_LIGHTS). (3) Дворы вторым
#   проходом: поленница, бочки, межевой плетень с калиткой за глухой стеной,
#   с проверкой на тела, полосы дорог, площадь и русло. (4) Прилавки рынка
#   садились на отметку плиты, а стоят на земле рядом — ножки висели; каждый
#   сел по своему пятну, со своим поворотом и товаром (бочки на столешнице).
# - 22:08:2026 - 16:05:00: ГАБАРИТЫ ПО ЗАМЕРУ И РАЗВЕДЕНИЕ ТЕЛ (согласовано с
#   архитектором). KIND держал габариты «на глаз»: city-house-s 7х7 при факте
#   4.5х6.0, city-longhall 12х8 при 16х8, city-keep-s 25х9 при 26.6х14 —
#   расстановка раздвигала дома по фантомному телу (щели: медиана 5.1 м, ни
#   одной в норме гайда). Числа сняты с .dfh. Следом три вещи, без которых
#   правда о габаритах ломала расстановку: (а) sit_rect — посадка по минимуму
#   ПОВЁРНУТОГО пятна вместо квадрата half=max(w,d)/2: замок щупал землю в
#   18 м от центра, за кромкой плато, и стоял закопанным на 11 м (24.95
#   вместо 34.67); (б) площадки под здания крупнее 10 м режутся в поле высот
#   по медиане пятна — такое пятно не помещается на ровное; (в) resolve_frames
#   разводит тела совместно: одиночный отодвиг от дороги загонял лавку в
#   лавку и амбар в ферму (минимум щели -5.8 м, четыре наложения). Итог
#   замера: медиана щели 2.13 м, минимум 0.35, 11 из 26 в норме гайда ≤0.4.
#   Доминанты (замок, длинный зал, храм, мельница) в разведении неподвижны и
#   не уступают дороге — улицы этого чертежа упираются в них (гайд §11).

import json
import math
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAN = json.load(open(os.path.join(ROOT, "docs/WHITERUN_PLAN.json"), encoding="utf-8"))

H = []   # [house]
P = []   # [place]
LIGHTS = []   # [light]

def house(file, x, y, z, yaw_deg, note):
    H.append((f"assets/houses/{file}", x, y, z, math.radians(yaw_deg), note))

def place(obj, x, y, z, yaw_deg=0.0, note=None):
    P.append((obj, x, y, z, math.radians(yaw_deg), note))

def light(x, y, z, color, radius, note, shadow=False):
    LIGHTS.append((x, y, z, color, radius, shadow, note))

# --- сетка высот двухпроходного цикла (gen -> dump_heights -> gen) ----------
HEIGHTS_PATH = os.environ.get("WHITERUN_HEIGHTS", "/tmp/whiterun_heights.txt")
_H = None
if os.path.exists(HEIGHTS_PATH):
    _H = [[float(v) for v in line.split()]
          for line in open(HEIGHTS_PATH, encoding="utf-8") if not line.startswith("#")]

def ground(x, z):
    if _H is None:
        return None
    xi = min(max(x, 0.0), 254.999)
    zi = min(max(z, 0.0), 254.999)
    x0, z0 = int(xi), int(zi)
    fx, fz = xi - x0, zi - z0
    r0, r1 = _H[z0], _H[z0 + 1]
    return ((r0[x0]*(1-fx) + r0[x0+1]*fx) * (1-fz)
            + (r1[x0]*(1-fx) + r1[x0+1]*fx) * fz)

def sit(x, z, fallback, half=3.5):
    gs = [ground(x+dx, z+dz) for dx, dz in
          ((0,0),(half,half),(half,-half),(-half,half),(-half,-half))]
    gs = [g for g in gs if g is not None]
    return fallback if not gs else min(gs) - 0.05

def rect_points(cx, cz, w, d, yaw_deg, n=5):
    """Сетка n x n по ПОВЁРНУТОМУ пятну здания."""
    c = math.cos(math.radians(yaw_deg))
    sn = math.sin(math.radians(yaw_deg))
    out = []
    for i in range(n):
        lx = (-0.5 + i/(n-1)) * w
        for j in range(n):
            lz = (-0.5 + j/(n-1)) * d
            out.append((cx + lx*c + lz*sn, cz - lx*sn + lz*c))
    return out

def sit_rect(cx, cz, w, d, yaw_deg, fallback):
    """Посадка по МИНИМУМУ ПЯТНА, и пятно берётся повёрнутым. Прежняя проба
    «центр плюс четыре угла квадрата half=max(w,d)/2» для замка 26.6х14
    щупала землю в 18 м от центра — за кромкой замкового плато, и находила
    там 25 вместо 36: замок стоял закопанным по конёк одиннадцать метров."""
    gs = [g for g in (ground(px, pz) for px, pz in rect_points(cx, cz, w, d, yaw_deg))
          if g is not None]
    return fallback if not gs else min(gs) - 0.05

# --- kind -> рецепт и ЗАМЕРЕННЫЕ габариты (фронт w вдоль двери, глубина d) --
# Числа сняты с самих .dfh (габарит вершин), а не выписаны на глаз: прежняя
# таблица давала city-house-s 7х7 при факте 4.5х6.0 и city-longhall 12х8 при
# факте 16х8 — расстановка считала Йоррваскр на 4 м короче, чем он есть, а
# дворовый клаттер садился бы в стену. Габарит берётся ПО КОРПУСУ вместе с
# навесом/портиком: они тоже занимают землю.
KIND = {
    "keep":    ("city-keep-s.dfh", 26.6, 14.0),
    "wing":    (None, 0, 0),  # крылья входят в рецепт keep-s
    "donjon":  ("city-donjon.dfh", 5.1, 5.1),
    # city-manor реально 14х15 с крылом — на схемном пятне 9х7 крыло легло
    # на главную улицу (прогон v5-1). Усадьба плана = крупный дом.
    "manor":   ("city-house-l.dfh", 10.0, 8.0),
    "temple":  ("city-temple.dfh", 12.0, 10.2),
    "longhall":("city-longhall.dfh", 16.0, 8.0),
    "shop":    ("city-shop.dfh", 6.0, 8.0),
    "tavern":  ("city-house-l.dfh", 10.0, 8.0),
    "smithy":  ("city-shop-old.dfh", 6.0, 8.0),
    "old":     ("city-house-s-old.dfh", 4.5, 6.0),
    "farm":    ("city-house-s.dfh", 4.5, 6.0),
    "barn":    ("city-barn.dfh", 12.0, 9.0),
    "mill":    ("city-mill.dfh", 7.0, 7.0),
    "stable":  ("city-barn-old.dfh", 12.0, 9.0),
    "inn":     ("city-house-l.dfh", 10.0, 8.0),
    "":        ("city-house-s.dfh", 4.5, 6.0),
}

# --- ВНУТРЕННЕЕ УБРАНСТВО (возврат системы 21.08 21:45, вырезанной при
# переписывании 22.08) -------------------------------------------------------
# Локальные координаты рецепта: начало в УГЛУ, корпус занимает [0..w]x[0..d],
# ДВЕРЬ на стороне z=d. Отсюда общее правило раскладки: очаг у ГЛУХОЙ стены
# z=0 (у длинного зала — посреди пола), стол с лавками в середине, кровать у
# боковой стены в стороне от дверного проёма, стеллаж/бочки по стенам.
# Коридор от двери к очагу оставлен пустым (гайд §10: «коридор прохода
# свободен»). Верх ПОЛОВОЙ ПЛИТЫ у рецептов РАЗНЫЙ — прежний furnish клал
# всё на +0.12 и в city-house-l мебель тонула в полу на полметра.
FLOOR = {
    "city-house-s.dfh": 0.12, "city-house-s-old.dfh": 0.12,
    "city-house-l.dfh": 0.645, "city-house-l-old.dfh": 0.645,
    "city-shop.dfh": 0.12, "city-shop-old.dfh": 0.12,
    "city-longhall.dfh": 0.13, "city-temple.dfh": 0.125,
    "city-barn.dfh": 0.10, "city-barn-old.dfh": 0.10,
    "city-mill.dfh": 1.32, "city-keep-s.dfh": 1.72,
}
FURN = {
    # малый дом 4.5 x 6.0
    "city-house-s.dfh": [
        ("furn-hearth.dfh", 1.55, 0.35, 0), ("furn-table.dfh", 0.55, 2.50, 0),
        ("furn-bench.dfh", 0.65, 2.00, 0), ("furn-bench.dfh", 0.65, 3.55, 0),
        ("furn-bed.dfh", 3.15, 3.75, 0), ("furn-shelf.dfh", 0.30, 2.20, 90),
        ("furn-barrel.dfh", 3.60, 1.90, 0)],
    "city-house-s-old.dfh": [
        ("furn-hearth.dfh", 1.55, 0.35, 0), ("furn-table.dfh", 0.55, 2.50, 0),
        ("furn-bench.dfh", 0.65, 2.00, 0),
        ("furn-bed.dfh", 3.15, 3.75, 0), ("furn-barrel.dfh", 0.35, 4.90, 0),
        ("furn-barrel.dfh", 1.20, 4.95, 0)],
    # большой дом 10 x 8 (пол на 0.645)
    "city-house-l.dfh": [
        ("furn-hearth.dfh", 4.30, 0.40, 0), ("furn-table.dfh", 3.60, 3.00, 0),
        ("furn-bench.dfh", 3.70, 2.50, 0), ("furn-bench.dfh", 3.70, 4.20, 0),
        ("furn-bed.dfh", 0.45, 5.55, 0), ("furn-bed.dfh", 8.55, 5.55, 0),
        ("furn-shelf.dfh", 9.50, 3.40, 270), ("furn-column.dfh", 2.10, 3.90, 0),
        ("furn-column.dfh", 7.90, 3.90, 0), ("furn-barrel.dfh", 0.45, 0.60, 0)],
    "city-house-l-old.dfh": [
        ("furn-hearth.dfh", 4.30, 0.40, 0), ("furn-table.dfh", 3.60, 3.00, 0),
        ("furn-bench.dfh", 3.70, 2.50, 0), ("furn-bed.dfh", 0.45, 5.55, 0),
        ("furn-barrel.dfh", 8.80, 6.30, 0), ("furn-barrel.dfh", 8.80, 5.40, 0)],
    # лавка 6 x 6 (навес поверх z=6..8): прилавок к двери, товар по стенам
    "city-shop.dfh": [
        ("furn-hearth.dfh", 0.45, 0.40, 0), ("furn-table.dfh", 2.90, 4.60, 0),
        ("furn-shelf.dfh", 5.60, 1.20, 270), ("furn-shelf.dfh", 5.60, 2.80, 270),
        ("furn-bed.dfh", 0.40, 2.60, 0), ("furn-barrel.dfh", 4.60, 4.90, 0),
        ("furn-barrel.dfh", 3.75, 4.95, 0)],
    # кузница: горн у глухой стены, колода-наковальня, закалочные бочки
    "city-shop-old.dfh": [
        ("furn-hearth.dfh", 2.30, 0.40, 0), ("furn-table.dfh", 1.60, 2.60, 0),
        ("furn-barrel.dfh", 5.10, 0.60, 0), ("furn-barrel.dfh", 5.10, 1.70, 0),
        ("furn-woodpile.dfh", 4.30, 4.60, 0), ("furn-bench.dfh", 0.35, 4.80, 0),
        ("furn-shelf.dfh", 0.30, 2.20, 90)],
    # длинный зал 16 x 8: очаг ПОСРЕДИ пола, два ряда столов вдоль стен
    "city-longhall.dfh": [
        ("furn-hearth.dfh", 7.30, 3.30, 0),
        ("furn-table.dfh", 2.60, 1.00, 0), ("furn-bench.dfh", 2.70, 0.50, 0),
        ("furn-bench.dfh", 2.70, 2.10, 0),
        ("furn-table.dfh", 2.60, 5.90, 0), ("furn-bench.dfh", 2.70, 5.40, 0),
        ("furn-bench.dfh", 2.70, 7.00, 0),
        ("furn-table.dfh", 11.60, 1.00, 0), ("furn-bench.dfh", 11.70, 0.50, 0),
        ("furn-bench.dfh", 11.70, 2.10, 0),
        ("furn-table.dfh", 11.60, 5.90, 0), ("furn-bench.dfh", 11.70, 5.40, 0),
        ("furn-bench.dfh", 11.70, 7.00, 0),
        ("furn-column.dfh", 5.60, 3.85, 0), ("furn-column.dfh", 10.40, 3.85, 0),
        ("furn-shelf.dfh", 0.35, 3.40, 90), ("furn-barrel.dfh", 0.40, 6.60, 0)],
    # храм 12 x 8: алтарь у дальней стены, лавки рядами, жаровня
    "city-temple.dfh": [
        ("furn-table.dfh", 5.10, 0.70, 0), ("furn-hearth.dfh", 1.00, 0.60, 0),
        ("furn-hearth.dfh", 9.60, 0.60, 0),
        ("furn-column.dfh", 2.40, 2.40, 0), ("furn-column.dfh", 9.60, 2.40, 0),
        ("furn-bench.dfh", 3.20, 3.20, 0), ("furn-bench.dfh", 7.20, 3.20, 0),
        ("furn-bench.dfh", 3.20, 4.40, 0), ("furn-bench.dfh", 7.20, 4.40, 0),
        ("furn-bench.dfh", 3.20, 5.60, 0), ("furn-bench.dfh", 7.20, 5.60, 0),
        ("furn-shelf.dfh", 11.55, 3.00, 270)],
    # амбар/конюшня 12 x 9: БЕЗ огня, бочки и дрова по стенам
    "city-barn.dfh": [
        ("furn-barrel.dfh", 0.55, 0.60, 0), ("furn-barrel.dfh", 1.45, 0.60, 0),
        ("furn-barrel.dfh", 0.55, 1.50, 0),
        ("furn-woodpile.dfh", 10.20, 0.80, 0), ("furn-woodpile.dfh", 10.20, 2.20, 0),
        ("furn-woodpile.dfh", 10.20, 3.60, 0),
        ("furn-table.dfh", 0.60, 4.20, 0), ("furn-bench.dfh", 0.55, 6.00, 0)],
    "city-barn-old.dfh": [
        ("furn-barrel.dfh", 0.55, 0.60, 0), ("furn-barrel.dfh", 1.45, 0.60, 0),
        ("furn-woodpile.dfh", 10.20, 1.20, 0), ("furn-woodpile.dfh", 10.20, 2.60, 0),
        ("furn-bench.dfh", 0.55, 6.00, 0)],
    # мельница 7 x 7, пол на 1.32, дверь на x=0: мешки-бочки у восточной стены
    "city-mill.dfh": [
        ("furn-table.dfh", 2.40, 2.30, 0), ("furn-barrel.dfh", 5.70, 0.90, 0),
        ("furn-barrel.dfh", 5.70, 1.80, 0), ("furn-barrel.dfh", 5.70, 2.70, 0),
        ("furn-shelf.dfh", 6.55, 4.60, 270), ("furn-bench.dfh", 2.20, 5.80, 0),
        ("furn-hearth.dfh", 0.60, 0.50, 0)],
    # зал замка 15 x 8 на отметке 1.6: длинный стол, троноподобная скамья
    "city-keep-s.dfh": [
        ("furn-hearth.dfh", 6.80, 0.45, 0),
        ("furn-table.dfh", 4.20, 3.20, 0), ("furn-table.dfh", 6.20, 3.20, 0),
        ("furn-table.dfh", 8.20, 3.20, 0),
        ("furn-bench.dfh", 4.30, 2.70, 0), ("furn-bench.dfh", 6.30, 2.70, 0),
        ("furn-bench.dfh", 8.30, 2.70, 0),
        ("furn-bench.dfh", 4.30, 4.40, 0), ("furn-bench.dfh", 6.30, 4.40, 0),
        ("furn-bench.dfh", 8.30, 4.40, 0),
        ("furn-column.dfh", 2.60, 3.90, 0), ("furn-column.dfh", 12.40, 3.90, 0),
        ("furn-shelf.dfh", 14.55, 2.40, 270), ("furn-shelf.dfh", 14.55, 5.20, 270),
        ("furn-barrel.dfh", 0.50, 6.60, 0), ("furn-barrel.dfh", 1.40, 6.60, 0)],
}
# Очаг просит тень только в двух местах — слотов теней ровно два
# (MAX_SHADOW_POINT_LIGHTS), и их надо отдать залам, где очаг — сюжет.
HEARTH_SHADOW = ("city-longhall.dfh", "city-keep-s.dfh")
HEARTH_COLOR = (1.0, 0.52, 0.20)

def loc_to_world(ox, oz, yaw_deg, lx, lz):
    """Локальная точка детали -> мир. Конвенция сцены: +X_лок = (cos, -sin),
    +Z_лок = (sin, cos), начало детали в углу."""
    c = math.cos(math.radians(yaw_deg))
    sn = math.sin(math.radians(yaw_deg))
    return (ox + lx*c + lz*sn, oz - lx*sn + lz*c)

def furnish(rec, ox, oy, oz, yaw_deg, note):
    """Мебель дома: локальные позиции раскладки поворачиваются той же
    матрицей сцены, что и сам дом, и садятся на ВЕРХ ПОЛОВОЙ ПЛИТЫ этого
    рецепта. Очаг заодно зажигает точечный свет."""
    items = FURN.get(rec)
    if not items:
        return
    fy = oy + FLOOR.get(rec, 0.12)
    for ff, lx, lz, lyaw in items:
        mx, mz = loc_to_world(ox, oz, yaw_deg, lx, lz)
        house(ff, mx, fy, mz, (yaw_deg + lyaw) % 360.0, "убранство: " + note)
        if ff == "furn-hearth.dfh":
            # огонь в чаше очага: центр плиты 1.4х1.4, пламя на 0.55 над полом
            hx, hz = loc_to_world(ox, oz, yaw_deg, lx + 0.70, lz + 0.70)
            light(hx, fy + 0.55, hz, HEARTH_COLOR, 4.6,
                  "очаг: " + note, rec in HEARTH_SHADOW)
TREE = {"birch": ["birch-forge-a", "birch-forge-b"],
        "spruce": ["spruce-forge-a", "pine-forge-a", "spruce-forge-b"],
        "oak": ["oak-forge-a"], "bush": ["juniper-forge-a"]}

def door_dir(deg, door):
    """Мировое направление наружу от дверной стороны прямоугольника схемы."""
    base = {"S": (0, 1), "N": (0, -1), "E": (1, 0), "W": (-1, 0)}[door]
    r = math.radians(deg)
    return (base[0]*math.cos(r) - base[1]*math.sin(r),
            base[0]*math.sin(r) + base[1]*math.cos(r))

def house_frame(hs):
    """Геометрия посадки БЕЗ земли: (рецепт, центр тела, w, d, yaw, дверь).
    Отделена от put_house, потому что площадку под крупное здание надо
    спланировать в поле высот ДО того, как здание на неё сядет."""
    rec, w, d = KIND.get(hs["kind"], KIND[""])
    if rec is None:
        return None
    dx, dz = door_dir(hs["deg"], hs["door"])
    # у рецептов дверь на локальной стороне z=d: её наружная нормаль в мире
    # равна Z_loc=(sin yaw, cos yaw) -> yaw из направления двери схемы.
    yaw = math.degrees(math.atan2(dx, dz)) % 360.0
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    # ПОСАДКА ДВЕРНОЙ ГРАНЬЮ: дверная грань реального дома встаёт туда, где
    # дверная грань схемного пятна (красная линия чертежа). Рост реальной
    # глубины уходит НАЗАД во двор — посадка центром выдвигала фасады на
    # дороги (первый прогон v5: 15 пересечений с полосами).
    door_face = {"S": hs["d"]/2, "N": hs["d"]/2, "E": hs["w"]/2, "W": hs["w"]/2}[hs["door"]]
    fx = hs["x"] + dx * door_face
    fz = hs["z"] + dz * door_face
    ccx = fx - dx * (d/2)
    ccz = fz - dz * (d/2)
    return (rec, ccx, ccz, w, d, yaw, (fx, fz))

def nearest_on_roads(px, pz):
    best, pt = 1e9, (px, pz)
    for rd in PLAN["roads"]:
        for (x0,z0),(x1,z1) in zip(rd["pts"], rd["pts"][1:]):
            vx, vz = x1-x0, z1-z0
            L2 = vx*vx + vz*vz
            t = max(0.0, min(1.0, ((px-x0)*vx + (pz-z0)*vz) / L2))
            qx, qz = x0+vx*t, z0+vz*t
            dd = math.hypot(px-qx, pz-qz)
            if dd < best:
                best, pt = dd, (qx, qz)
    return pt

def push_off_roads(cx, cz, w, d, yaw, budget=8.0):
    """Отодвиг от полос дорог: тело пересекает полосу -> шаг по нормали ОТ
    ближайшей точки нарушающей дороги (двери мало, дорога бывает сбоку)."""
    moved = 0.0
    while moved < budget and any(rect_road_hit(cx, cz, w, d, yaw, rd)
                                 for rd in PLAN["roads"]):
        qx, qz = nearest_on_roads(cx, cz)
        nx, nz = cx - qx, cz - qz
        nl = math.hypot(nx, nz) or 1.0
        cx += nx / nl * 0.4
        cz += nz / nl * 0.4
        moved += 0.4
    return cx, cz

def half_proj(w, d, yaw_deg, ux, uz):
    """Половина тела в направлении (ux,uz) — опорная функция прямоугольника."""
    c = math.cos(math.radians(yaw_deg))
    sn = math.sin(math.radians(yaw_deg))
    return abs(c*ux - sn*uz) * w/2 + abs(sn*ux + c*uz) * d/2

# Доминанты задают сетку, рядовая застройка уступает (гайд §11: «Йоррваскр
# старше города — всё наросло вокруг него»). Их тела в разведении неподвижны.
FIXED_KINDS = ("keep", "longhall", "temple", "mill")
GAP_M = 0.35    # норматив щели фасадного ряда (гайд §10: 0.35-0.7)
MOVE_CAP = 6.0  # дальше схемного пятна тело не уходит — это чужой чертёж

def resolve_frames():
    """ТЕЛА ГОРОДА, РАЗВЕДЁННЫЕ МЕЖДУ СОБОЙ. Отодвиг от дорог считался
    каждым домом в одиночку и загонял его в соседа: лавка юга рынка уехала
    на 4 м в другую лавку, амбар — на 7 м в ферму (замер щелей: минимум
    -5.8 м, четыре наложения). Здесь тела расталкиваются совместно, до
    норматива щели, с потолком смещения от схемного пятна; после каждого
    круга — повторный отодвиг от полос. Один и тот же список используют и
    планировка площадок, и расстановка: считать раму дважды нельзя, второй
    счёт видел бы уже занятые места и давал другой ответ."""
    fr = []
    for hs in PLAN["houses"]:
        f = house_frame(hs)
        if f is None:
            continue
        rec, ccx, ccz, w, d, yaw, door = f
        # ДОМИНАНТА НЕ УСТУПАЕТ ДОРОГЕ. Улицы этого чертежа упираются в неё:
        # гравийная кончается на крыльце Йоррваскра, главная — у замка. Отодвиг
        # уносил Йоррваскр на 5 м, освобождая его же подъезд.
        px, pz = ((ccx, ccz) if hs["kind"] in FIXED_KINDS
                  else push_off_roads(ccx, ccz, w, d, yaw))
        fr.append({"hs": hs, "rec": rec, "x": px, "z": pz, "w": w, "d": d,
                   "yaw": yaw, "door": door, "x0": ccx, "z0": ccz})
    # Первые круги ещё уводят тела с полос, последние — только разводят тела:
    # отодвиг от дороги и разведение спорили друг с другом и оставляли лавки
    # юга рынка врезанными на 0.44 м. Наложение хуже, чем полметра на полосе.
    for it in range(64):
        road_pass = it < 40
        worst = 0.0
        for i in range(len(fr)):
            for j in range(i + 1, len(fr)):
                a, b = fr[i], fr[j]
                D = math.hypot(b["x"]-a["x"], b["z"]-a["z"]) or 1e-6
                ux, uz = (b["x"]-a["x"])/D, (b["z"]-a["z"])/D
                gap = (D - half_proj(a["w"], a["d"], a["yaw"], ux, uz)
                         - half_proj(b["w"], b["d"], b["yaw"], ux, uz))
                if gap >= GAP_M:
                    continue
                need = GAP_M - gap
                worst = max(worst, need)
                ma = a["hs"]["kind"] not in FIXED_KINDS
                mb = b["hs"]["kind"] not in FIXED_KINDS
                if not ma and not mb:
                    continue
                sa = need if (ma and not mb) else (need/2 if ma else 0.0)
                sb = need if (mb and not ma) else (need/2 if mb else 0.0)
                a["x"] -= ux*sa; a["z"] -= uz*sa
                b["x"] += ux*sb; b["z"] += uz*sb
        if it % 8 == 7:
            for f in fr:
                dx, dz = f["x"]-f["x0"], f["z"]-f["z0"]
                Lm = math.hypot(dx, dz)
                if Lm > MOVE_CAP:
                    f["x"], f["z"] = f["x0"]+dx/Lm*MOVE_CAP, f["z0"]+dz/Lm*MOVE_CAP
                if road_pass and f["hs"]["kind"] not in FIXED_KINDS:
                    f["x"], f["z"] = push_off_roads(f["x"], f["z"], f["w"],
                                                    f["d"], f["yaw"], budget=3.0)
        if worst < 0.02 and not road_pass:
            break
    return fr

def put_house(f):
    hs, rec = f["hs"], f["rec"]
    ccx, ccz, w, d, yaw = f["x"], f["z"], f["w"], f["d"], f["yaw"]
    fx, fz = f["door"]
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    ox = ccx - (w/2)*c - (d/2)*s
    oz = ccz + (w/2)*s - (d/2)*c
    y = sit_rect(ccx, ccz, w, d, yaw, 25.0)
    name = hs["name"] or hs["kind"] or "дом"
    house(rec, ox, y, oz, yaw, name)
    PLACED.append((ccx, ccz, w, d, yaw, name))
    FRAMES.append((rec, ox, y, oz, yaw, w, d, name))
    furnish(rec, ox, y, oz, yaw, name)
    # каменная дорожка с бордюрами: цепочка furn-walk2 от двери до улицы
    if hs["walk"]:
        door_x = fx
        door_z = fz
        wx, wz = hs["walk"]
        vx, vz = wx - door_x, wz - door_z
        L = math.hypot(vx, vz)
        if L > 0.8:
            ux, uz = vx/L, vz/L
            wyaw = math.degrees(math.atan2(-uz, ux)) % 360.0
            n = max(1, int(L / 1.9))
            for i in range(n):
                t = (i + 0.5) * L / n
                sx, sz = door_x + ux*t, door_z + uz*t
                gy = ground(sx, sz)
                house("furn-walk2.dfh", sx - ux, (25.0 if gy is None else gy) + 0.01,
                      sz - uz + 0.0, wyaw, "дорожка: " + (hs["name"] or hs["kind"] or "дом"))

PLACED = []  # (cx, cz, w, d, yaw_deg, name) реальных тел для проверок
FRAMES = []  # (rec, ox, oy, oz, yaw_deg, w, d, name) — рама дома для дворов

MRX, MRZ, MRW, MRD = PLAN["market"]["rect"]

def on_market(px, pz):
    """Точка на рыночной площади (с буфером): дороги, влившиеся в площадь,
    там не считаются — фасады стоят на самой площади, это норма."""
    return (MRX - 2 <= px <= MRX + MRW + 2) and (MRZ - 2 <= pz <= MRZ + MRD + 2)

ROAD_END_M = 6.0  # хвост дороги у её конца — это порог, а не конфликт

def rect_road_hit(cx, cz, w, d, yaw_deg, rd):
    """Сэмплы осевой дороги в локали дома против [±w/2]x[±d/2].
    КОНЦЫ ДОРОГИ НЕ СЧИТАЮТСЯ. Дорога в этом чертеже кончается НА цели —
    у ворот, у моста, у дуба, у крыльца Йоррваскра; последние метры осевой
    лежат в теле здания по замыслу («улицы упираются в доминанты», гайд §11).
    Без исключения отодвиг уносил Йоррваскр — доминанту, задающую сетку, —
    на 5.4 м от его места, лишь бы освободить свой же подъезд."""
    r = math.radians(yaw_deg)
    c, sn = math.cos(r), math.sin(r)
    ex0, ez0 = rd["pts"][0]
    ex1, ez1 = rd["pts"][-1]
    for (x0,z0),(x1,z1) in zip(rd["pts"], rd["pts"][1:]):
        L = math.hypot(x1-x0, z1-z0)
        for i in range(int(L*2) + 1):
            t = i / max(1, int(L*2))
            wx0, wz0 = x0+(x1-x0)*t, z0+(z1-z0)*t
            if on_market(wx0, wz0):
                continue
            if (math.hypot(wx0-ex0, wz0-ez0) < ROAD_END_M
                    or math.hypot(wx0-ex1, wz0-ez1) < ROAD_END_M):
                continue
            px, pz = wx0 - cx, wz0 - cz
            lx = px*c - pz*sn
            lz = px*sn + pz*c
            ddx = max(abs(lx) - w/2, 0.0)
            ddz = max(abs(lz) - d/2, 0.0)
            if math.hypot(ddx, ddz) < rd["w"]/2 - 0.3:
                return True
    return False

# --- ДВОРЫ (гайд §7/§10: «за каждым — дворик с поленницей/бочкой и межевым
# плетнём», обязательный набор — поленница, бочка дождевой воды, плетень
# 1.2-1.8 м) ------------------------------------------------------------------
# Двор лежит ЗА глухой стеной дома (локальное z<0: дверь на z=d), клаттер
# жмётся к стене и к линии плетня — «коридор прохода свободен». Ставится
# ВТОРЫМ проходом, когда все тела уже в PLACED: иначе поленница соседа,
# поставленная раньше, не видна проверке.
YARD_KINDS = ("", "old", "farm", "shop", "smithy", "tavern", "inn", "manor",
              "mill", "barn", "stable")

def dist_to_roads(px, pz):
    best = 1e9
    for rd in PLAN["roads"]:
        for (x0, z0), (x1, z1) in zip(rd["pts"], rd["pts"][1:]):
            vx, vz = x1-x0, z1-z0
            L2 = vx*vx + vz*vz
            t = max(0.0, min(1.0, ((px-x0)*vx + (pz-z0)*vz) / L2))
            best = min(best, math.hypot(px - (x0+vx*t), pz - (z0+vz*t)) - rd["w"]/2)
    return best

def spot_free(px, pz, r, skip=None):
    """Место под клаттер: не в теле дома, не на полосе дороги, не на площади."""
    if on_market(px, pz) or dist_to_roads(px, pz) < r:
        return False
    if not (1.0 <= px <= 255.0 and 1.0 <= pz <= 255.0):
        return False
    for (cx, cz, w, d, yaw, n) in PLACED:
        if n == skip:
            continue
        c = math.cos(math.radians(yaw))
        sn = math.sin(math.radians(yaw))
        dx, dz = px - cx, pz - cz
        lx = dx*c - dz*sn
        lz = dx*sn + dz*c
        if abs(lx) < w/2 + r and abs(lz) < d/2 + r:
            return False
    # река: клаттер не стоит в воде
    if river_dist(px, pz) < PLAN["river_half_w"] + 1.0:
        return False
    return True

def yards():
    n_items = 0
    for i, (rec, ox, oy, oz, yaw, w, d, name) in enumerate(FRAMES):
        if rec in ("city-keep-s.dfh", "city-donjon.dfh", "city-temple.dfh",
                   "city-longhall.dfh"):
            continue  # у доминант двор не бытовой
        # набор двора; локальное z отрицательное = за глухой стеной
        kit = [("furn-woodpile.dfh", 0.45, -1.30, 0, 0.9),
               ("furn-barrel.dfh", w - 1.05, -1.25, 0, 0.6)]
        if w >= 8.0:
            kit.append(("furn-barrel.dfh", w - 1.90, -1.30, 0, 0.6))
            kit.append(("furn-woodpile.dfh", 2.20, -1.30, 0, 0.9))
        # межевой плетень поперёк двора, звенья по 2 м с калиткой посередине
        fence_z = -3.6 - (i % 3) * 0.35
        span = max(2, int(round(w / 2.0)))
        for k in range(span):
            lx = (w / span) * k
            kind = "furn-fence-gate.dfh" if k == span // 2 else "furn-fence2.dfh"
            kit.append((kind, lx, fence_z, 0, 0.5))
        for ff, lx, lz, lyaw, rad in kit:
            mx, mz = loc_to_world(ox, oz, yaw, lx, lz)
            if not spot_free(mx, mz, rad, skip=name):
                continue
            gy = ground(mx, mz)
            house(ff, mx, (oy if gy is None else gy) - 0.03, mz,
                  (yaw + lyaw) % 360.0, "двор: " + name)
            n_items += 1
    return n_items

def check_layout():
    import itertools
    bad = []
    for (c1, z1, w1, d1, y1, n1), (c2, z2, w2, d2, y2, n2) in itertools.combinations(PLACED, 2):
        if math.hypot(c1-c2, z1-z2) < (max(w1,d1) + max(w2,d2)) / 2 * 0.55:
            bad.append(f"близко: {n1} и {n2}")
    for rd in PLAN["roads"]:
        for (cx, cz, w, d, y, n) in PLACED:
            if n.startswith("дорожка"):
                continue
            if rect_road_hit(cx, cz, w, d, y, rd):
                bad.append(f"на дороге ({rd['mat']}): {n}")
    if bad:
        print("ПРОВЕРКА РАССТАНОВКИ:")
        for b in sorted(set(bad)):
            print("  -", b)

def point_in_poly(x, z, pts):
    inside = False
    n = len(pts)
    j = n - 1
    for i in range(n):
        xi, zi = pts[i]
        xj, zj = pts[j]
        if (zi > z) != (zj > z) and x < (xj - xi) * (z - zi) / (zj - zi) + xi:
            inside = not inside
        j = i
    return inside

def build_plan_heights():
    """256x256 карта высот ИЗ ПОЛИГОНОВ чертежа: последняя содержащая зона
    побеждает, поверх микропятна, затем гауссово сглаживание — перепады
    1-3 м из схемы становятся склонами 20-30%, без паразитных горок
    эллипс-аппроксимации (бот v5-3 утыкался в такую у рынка)."""
    Hh = [[25.0] * 256 for _ in range(256)]
    for zn in PLAN["zones"]:
        xs = [q[0] for q in zn["pts"]]
        zs = [q[1] for q in zn["pts"]]
        x0, x1 = max(0, int(min(xs)) - 1), min(255, int(max(xs)) + 2)
        z0, z1 = max(0, int(min(zs)) - 1), min(255, int(max(zs)) + 2)
        for gz in range(z0, z1 + 1):
            for gx in range(x0, x1 + 1):
                if point_in_poly(gx, gz, zn["pts"]):
                    Hh[gz][gx] = float(zn["h"])
    for m in PLAN["micro"]:
        xs = [q[0] for q in m["pts"]]
        zs = [q[1] for q in m["pts"]]
        for gz in range(max(0, int(min(zs))), min(255, int(max(zs))) + 1):
            for gx in range(max(0, int(min(xs))), min(255, int(max(xs))) + 1):
                if point_in_poly(gx, gz, m["pts"]):
                    Hh[gz][gx] += m["dh"]
    # сепарабельный блюр (три прохода box ~ гаусс сигма ~2.2)
    for _ in range(3):
        for row in Hh:
            acc = row[:]
            for i in range(256):
                lo, hi = max(0, i - 2), min(255, i + 2)
                row[i] = sum(acc[lo:hi + 1]) / (hi - lo + 1)
        for i in range(256):
            col = [Hh[r][i] for r in range(256)]
            for r in range(256):
                lo, hi = max(0, r - 2), min(255, r + 2)
                Hh[r][i] = sum(col[lo:hi + 1]) / (hi - lo + 1)
    return Hh

NATURAL_PATH = "/tmp/whiterun_natural.txt"

# --- РЕКА: отметка воды снимается с ЗЕМЛИ, а не задаётся формулой -----------
# Порядок движка (Worldgen.cpp compose_passes): пады -> РЕКА -> ручной relief.
# Ручной слой говорит ПОСЛЕДНИМ, поэтому дельты чертежа зарисовывали русло
# обратно, а вода 23.6-i*0.08 оставалась на 3-4 м ниже земли — реки в кадре
# не было. Лечится двумя вещами:
#   1) вода = минимальная кромка ДВУХ берегов минус RIVER_FREEBOARD, с
#      монотонностью вниз по течению (река с горы-истока не течёт вверх);
#   2) дельты relief ГАСЯТСЯ в коридоре русла тем же smoothstep, каким движок
#      сводит дно с берегом (river_taper) — вырез движка остаётся вырезом.
# Дно = вода - RIVER_DEPTH берётся движком безусловно (apply_rivers), поэтому
# отдельные dh на дно не нужны: они бы его и засыпали.
RIVER_BANK_M = 3.0      # ширина сведения дна с берегом (1.4 м на 3 м ~ 25 гр.)
RIVER_DEPTH = 1.0       # дно ниже воды
RIVER_FREEBOARD = 0.40  # вода ниже кромки берега

def hh_at(Hh, x, z):
    xi = min(max(x, 0.0), 254.999)
    zi = min(max(z, 0.0), 254.999)
    x0, z0 = int(xi), int(zi)
    fx, fz = xi - x0, zi - z0
    r0, r1 = Hh[z0], Hh[z0 + 1]
    return ((r0[x0]*(1-fx) + r0[x0+1]*fx) * (1-fz)
            + (r1[x0]*(1-fx) + r1[x0+1]*fx) * fz)

def river_tangent(pts, i):
    a = pts[max(0, i-1)]
    b = pts[min(len(pts)-1, i+1)]
    vx, vz = b[0]-a[0], b[1]-a[1]
    L = math.hypot(vx, vz) or 1.0
    return vx/L, vz/L

def river_levels(Hh):
    """Отметка воды в каждой вершине осевой: ниже НИЖНЕГО из двух берегов на
    RIVER_FREEBOARD, монотонно НЕВОЗРАСТАЯ вниз по течению."""
    pts = PLAN["river"]
    off = PLAN["river_half_w"] + RIVER_BANK_M
    lv = []
    for i, (x, z) in enumerate(pts):
        ux, uz = river_tangent(pts, i)
        nx, nz = -uz, ux
        banks = []
        for s in (-1.0, 1.0):
            # берег меряется тремя отсчётами вдоль течения — одиночная точка
            # ловила случайную кочку блюра и роняла плёс на полметра
            banks.append(min(hh_at(Hh, x + nx*off*s + ux*t, z + nz*off*s + uz*t)
                             for t in (-2.0, 0.0, 2.0)))
        lv.append(min(banks) - RIVER_FREEBOARD)
    for i in range(1, len(lv)):
        lv[i] = min(lv[i], lv[i-1])
    return lv

def river_dist(x, z):
    best = 1e9
    pts = PLAN["river"]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        dx, dz = x1-x0, z1-z0
        L2 = dx*dx + dz*dz
        t = max(0.0, min(1.0, ((x-x0)*dx + (z-z0)*dz) / L2))
        best = min(best, math.hypot(x - (x0+dx*t), z - (z0+dz*t)))
    return best

def river_taper(x, z):
    """Доля ручной дельты, доживающая до этой точки: 0 в русле, 1 за берегом.
    Тот же smoothstep, что у apply_rivers, — берег сходится без ступеньки."""
    half = PLAN["river_half_w"]
    d = river_dist(x, z)
    if d <= half:
        return 0.0
    if d >= half + RIVER_BANK_M:
        return 1.0
    t = (d - half) / RIVER_BANK_M
    return t * t * (3.0 - 2.0 * t)

def grade_corridor(Hh, nodes, halfw, feather):
    """ПЛАНИРОВКА КОРИДОРА в поле высот чертежа: вдоль ломаной [(x,z,h)…] поле
    прижимается к заданной отметке (интерполяция по длине), с полной силой в
    полосе halfw и затуханием smoothstep до нуля на halfw+feather. Поле
    чертежа И ЕСТЬ земля (relief пишет дельту к ней), поэтому подходы,
    подъезды и срезы делаются здесь, а не отдельным слоем."""
    segs = list(zip(nodes, nodes[1:]))
    xs = [n[0] for n in nodes]
    zs = [n[1] for n in nodes]
    R = halfw + feather
    x0 = max(0, int(min(xs) - R) - 1)
    x1 = min(255, int(max(xs) + R) + 1)
    z0 = max(0, int(min(zs) - R) - 1)
    z1 = min(255, int(max(zs) + R) + 1)
    for gz in range(z0, z1 + 1):
        for gx in range(x0, x1 + 1):
            best, lvl = 1e9, None
            for (ax, az, ah), (bx, bz, bh) in segs:
                vx, vz = bx-ax, bz-az
                L2 = vx*vx + vz*vz
                t = max(0.0, min(1.0, ((gx-ax)*vx + (gz-az)*vz) / L2))
                d = math.hypot(gx - (ax+vx*t), gz - (az+vz*t))
                if d < best:
                    best, lvl = d, ah + (bh-ah)*t
            if best >= R:
                continue
            if best <= halfw:
                w = 1.0
            else:
                u = 1.0 - (best - halfw) / feather
                w = u * u * (3.0 - 2.0 * u)
            Hh[gz][gx] = Hh[gz][gx] * (1.0 - w) + lvl * w

def water_at(levels, x, z):
    """Отметка воды у ближайшей точки осевой (как river_nearest движка)."""
    pts = PLAN["river"]
    best, w = 1e9, levels[0]
    for i, ((x0, z0), (x1, z1)) in enumerate(zip(pts, pts[1:])):
        dx, dz = x1-x0, z1-z0
        L2 = dx*dx + dz*dz
        t = max(0.0, min(1.0, ((x-x0)*dx + (z-z0)*dz) / L2))
        d = math.hypot(x - (x0+dx*t), z - (z0+dz*t))
        if d < best:
            best, w = d, levels[i] + (levels[i+1] - levels[i]) * t
    return w

def bridge_frame():
    """Рама моста: (origin рецепта, середина торца, вектор вдоль пролёта).
    Рецепт city-bridge занимает локально x -2.4..10.4, z 0..4; кладётся
    origin-ом в (bx-4, bz-2) с yaw = -deg (локальный +X = (cos, -sin)).
    ПРОЛЁТ ЦЕНТРИРУЕТСЯ НА РУСЛЕ: точка [center] чертежа лежала в 0.66 м
    западнее осевой, и торцы уходили на 7.0 и 5.3 м от неё — восточная
    аппарель оставалась внутри берегового сведения движка и висела метром
    выше земли. Сдвиг вдоль пролёта делает оба выхода симметричными."""
    b = PLAN["bridge"]
    bx, bz = b["center"]
    yaw = math.radians(-b["deg"])
    ux, uz = math.cos(yaw), -math.sin(yaw)
    zx, zz = math.sin(yaw), math.cos(yaw)
    ox, oz = bx - 4.0, bz - 2.0
    mx, mz = ox + zx*2.0, oz + zz*2.0
    sh = min(((river_dist(mx + ux*(4.0+s), mz + uz*(4.0+s)), s)
              for s in (i*0.05 for i in range(-80, 81))))[1]
    return (ox + ux*sh, oz + uz*sh), (mx + ux*sh, mz + uz*sh), (ux, uz)

def main():
    PLAN_H = build_plan_heights()
    WLEV = river_levels(PLAN_H)
    # ПОДХОДЫ МОСТА. Западный берег — кромка городского шельфа (27.6 против
    # 25.4 на востоке): без планировки западная аппарель уходила в откос на
    # полтора метра, а мост читался «воткнутым в холм». Оба торца сажаются на
    # отметку аппарели, дальше отметка возвращается к чертежу за 9 м.
    (box, boz), (mx, mz), (ux, uz) = bridge_frame()
    bwat = water_at(WLEV, mx + ux*4.0, mz + uz*4.0)
    ramp_g = bwat + 0.50
    nodes = []
    for t, h in ((-11.0, None), (-2.4, ramp_g), (10.4, ramp_g), (19.0, None)):
        px, pz = mx + ux*t, mz + uz*t
        nodes.append((px, pz, hh_at(PLAN_H, px, pz) if h is None else h))
    grade_corridor(PLAN_H, nodes, 3.0, 3.0)
    # ПЛОЩАДКИ ПОД КРУПНЫЕ ЗДАНИЯ. Посадка по минимуму пятна честна только
    # там, где пятно помещается на ровное: у замка 26.6х14, Йоррваскра 16х8,
    # храма 12х10 и амбаров 12х9 пятно перелезает кромку террасы, и минимум
    # утягивал здание на нижнюю ступень. Дом такого размера в жизни встаёт на
    # СПЛАНИРОВАННУЮ площадку — её и режем в поле высот по медиане пятна.
    FR = resolve_frames()
    for f in FR:
        ccx, ccz, w, d, yaw = f["x"], f["z"], f["w"], f["d"], f["yaw"]
        if max(w, d) < 10.0:
            continue
        pts = rect_points(ccx, ccz, w, d, yaw, n=7)
        hs_lvl = sorted(hh_at(PLAN_H, px, pz) for px, pz in pts)[len(pts)//2]
        c = math.cos(math.radians(yaw))
        sn = math.sin(math.radians(yaw))
        # коридор по длинной оси пятна, halfw = половина короткой стороны
        ax, az = (c, -sn) if w >= d else (sn, c)
        half_long = max(w, d)/2
        halfw = min(w, d)/2
        grade_corridor(PLAN_H,
                       [(ccx - ax*half_long, ccz - az*half_long, hs_lvl),
                        (ccx + ax*half_long, ccz + az*half_long, hs_lvl)],
                       halfw + 0.8, 3.0)
    # --- постройки плана ---
    for f in FR:
        put_house(f)
    # рынок: плита, колодец, прилавки
    mx, mz, mw, md = PLAN["market"]["rect"]
    my = sit(mx + mw/2, mz + md/2, 27.0, half=max(mw, md)/2)
    house("city-plaza12.dfh", mx + 1, my, mz, 0, "рыночная площадь")
    wx, wz = PLAN["market"]["well"]
    house("city-well.dfh", wx, my + 0.02, wz, 0, "колодец рынка")
    # ПРИЛАВКИ С ТОВАРОМ. Лотки садились на отметку плиты рынка, а стоят они
    # НА ЗЕМЛЕ рядом с ней — ножки висели над травой ([10] критика). Каждый
    # садится по своему пятну, поворот свой (гайд §11: «ни одно здание не
    # параллельно соседнему»), товар — бочки: на прилавке и у ноги.
    for i, (sx, sz) in enumerate(PLAN["market"]["stalls"]):
        syaw = (180 + (i - 1) * 7) % 360.0
        sy = sit(sx, sz, my, half=1.4)
        sox, soz = sx - 1.2, sz - 0.5
        house("city-stall.dfh", sox, sy, soz, syaw, f"прилавок {i+1}")
        # товар: бочка на столешнице (верх 0.85) и две у ноги, в тень навеса
        for lx, lz, dy in ((0.30, 0.10, 0.85), (1.30, 0.12, 0.85),
                           (0.15, 1.35, 0.0), (1.55, 1.45, 0.0)):
            gx, gz = loc_to_world(sox, soz, syaw, lx, lz)
            house("furn-barrel.dfh", gx, sy + dy, gz, (syaw + i*37) % 360.0,
                  f"товар прилавка {i+1}")
        bx2, bz2 = loc_to_world(sox, soz, syaw, 0.2, 1.9)
        house("furn-shelf.dfh", bx2, sy, bz2, (syaw + 90) % 360.0,
              f"стеллаж прилавка {i+1}")
    # стена и башни
    wall = PLAN["wall"]
    for (ax, az), (bx, bz) in zip(wall, wall[1:] + [wall[0]]):
        L = math.hypot(bx-ax, bz-az)
        ux, uz = (bx-ax)/L, (bz-az)/L
        yaw = math.degrees(math.atan2(-uz, ux)) % 360.0
        t = 0.0
        while t < L - 0.5:
            last = t + 12.0 >= L
            st = max(L - 12.0, 0.0) if last else t
            x, z = ax + ux*st, az + uz*st
            mxp, mzp = ax + ux*(st+6), az + uz*(st+6)
            g0, g1, gm = ground(x, z), ground(ax+ux*min(st+12, L), az+uz*min(st+12, L)), ground(mxp, mzp)
            gs = [g for g in (g0, g1, gm) if g is not None]
            y = (min(gs) - 0.3) if gs else 25.0
            # разрывы под ворота
            skip = False
            for g in PLAN["gates"]:
                gx, gz = g["pos"]
                if min(math.hypot(gx-(x+ux*tt), gz-(z+uz*tt)) for tt in (0, 6, 12)) < 7.0:
                    skip = True
            if not skip:
                house("city-wall12.dfh", x, y, z, yaw, "стена кольца")
            if last:
                break
            t += 11.5
    ccx = sum(p[0] for p in wall)/len(wall)
    ccz = sum(p[1] for p in wall)/len(wall)
    for tx, tz in PLAN["towers"]:
        ang = math.degrees(math.atan2(tz-ccz, tx-ccx))
        # башня выдвинута наружу по нормали
        nx, nz = (tx-ccx), (tz-ccz)
        nl = math.hypot(nx, nz)
        px, pz = tx + nx/nl*1.5, tz + nz/nl*1.5
        g = ground(px, pz)
        house("city-tower.dfh", px - 1.8, (25.0 if g is None else g - 0.2), pz - 1.8,
              (ang + 90) % 360, "башня кольца (наружу)")
    for g in PLAN["gates"]:
        gx, gz = g["pos"]
        gy = ground(gx, gz)
        # ворота вдоль ближайшей грани стены
        best, byaw = 1e9, 0.0
        for (ax, az), (bx, bz) in zip(wall, wall[1:] + [wall[0]]):
            mxp, mzp = (ax+bx)/2, (az+bz)/2
            dd = (mxp-gx)**2 + (mzp-gz)**2
            if dd < best:
                best = dd
                byaw = math.degrees(math.atan2(-(bz-az), bx-ax)) % 360.0
        house("city-gate.dfh", gx - 6, (25.0 if gy is None else gy - 0.1), gz, byaw,
              f"ворота {g['name']}")
    # мост
    b = PLAN["bridge"]
    # МОСТ СТОИТ НА ВОДЕ, А НЕ НА БЕРЕГУ. Отметка бралась с земли в 7 м к
    # востоку — на дамбе берега, и настил уезжал от русла. Настил рецепта
    # лежит на +0.20..+0.35 от origin, аппарели спускаются к -0.50, быки
    # уходят на -2.20: origin на 1.05 над водой даёт проезжую часть в 1.25-1.40
    # м над плёсом, торцы аппарелей — на планированную кромку (вода+0.50),
    # быки — на 0.15 в дно.
    house("city-bridge.dfh", box, bwat + 1.05, boz,
          -b["deg"], "мост через реку")
    # Гилдергрин и дуб-поляна
    gg = PLAN["gildergreen"]
    ggy = sit(gg[0], gg[1], 30.0, half=3)
    place("great-forge-oak", gg[0], ggy, gg[1], 0, "Гилдергрин (розовая листва — зона флоры)")
    house("city-treering.dfh", gg[0] - 2.6, ggy, gg[1] - 2.6, 0, "кольцо Гилдергрина")
    og = PLAN["oak_glade"]
    oy = sit(og["oak"][0], og["oak"][1], 26.0, half=3)
    place("great-forge-oak", og["oak"][0], oy, og["oak"][1], 137, "дуб поляны")
    for i, (tx, tz) in enumerate(og["trees"]):
        ty = sit(tx, tz, 26.0, half=1.5)
        place(TREE["birch"][i % 2] , tx, ty, tz, (i * 61) % 360)
    # огороды
    for i, (gx, gz) in enumerate(PLAN["gardens"]):
        gy = sit(gx, gz, 33.0, half=1.5)
        for k in (0, 1):
            house("furn-bed-garden.dfh", gx - 0.5 + k * 1.6, gy, gz - 1.2, (i*23) % 20 - 10,
                  "огород")
    # деревья и кусты
    for i, t in enumerate(PLAN["trees"]):
        opts = TREE.get(t["kind"], TREE["bush"])
        ty = sit(t["x"], t["z"], 25.0, half=1.0)
        place(opts[i % len(opts)], t["x"], ty, t["z"], (i * 47) % 360)

    # дворы — вторым проходом, когда все тела уже известны
    n_yard = yards()

    # --- terrain: река; высоты плана пишутся в relief дельтами от natural ---
    # ЗАМЕР НАТУРАЛЬНОЙ ЗЕМЛИ ИДЁТ БЕЗ РЕКИ (WHITERUN_BARE=1): иначе в nat уже
    # сидит вырез старого русла, и дельта у берега считается от дна, а не от
    # земли. Дельты в коридоре гасятся river_taper, поэтому вырез там делает
    # только движок — от отметки воды, снятой с берегов.
    terrain = []
    if os.environ.get("WHITERUN_BARE") != "1":
        rpts = "\n".join(f"point = {x} {z} {WLEV[i]:.2f}"
                         for i, (x, z) in enumerate(PLAN["river"]))
        terrain.append(f"[river]\nwidth_m = {PLAN['river_half_w']*2:.1f}\n"
                       f"depth_m = {RIVER_DEPTH:.1f}\nbank_m = {RIVER_BANK_M:.1f}\n"
                       f"note = река плана: вода по земле, дно на {RIVER_DEPTH:.1f} м ниже\n"
                       f"{rpts}\n")

    # --- дороги: тропы рельефа ---
    relief_paths = []
    for rd in PLAN["roads"]:
        half = max(0.8, rd["w"] / 2)
        soft = 1.2 if rd["mat"] == "stone" else 1.0
        pts = "\n".join(f"pp {x} {z}" for x, z in rd["pts"])
        relief_paths.append(f"path {half:g} {soft:g}\n{pts}")

    out = ["# Daggerfall N scene — ВАЙТРАН v5 ПО ПЛАНУ (docs/WHITERUN_PLAN.json).",
           "# СГЕНЕРИРОВАНО tools/gen_whiterun.py; композицию править в чертеже",
           "# tools/gen_whiterun_plan.py и перегонять оба генератора.",
           "map = houses/whiterun",
           "world_span_m = 256",
           "relief = whiterun.relief",
           "spawn = 113 0 248",
           "spawn_yaw = 0", ""]
    out.append("\n".join(terrain))
    for obj, x, y, z, yaw, note in P:
        out += ["[place]", f"object = {obj}",
                f"pos = {x:.3f} {y:.3f} {z:.3f}", f"yaw = {yaw:.6f}", "scale = 1"]
        if note:
            out += [f"note = {note}"]
        out += [""]
    for file, x, y, z, yaw, note in H:
        out += ["[house]", f"file = {file}",
                f"pos = {x:.3f} {y:.3f} {z:.3f}", f"yaw = {yaw:.6f}",
                f"note = {note}", ""]
    # СВЕТ ОЧАГОВ. Рендер зажигает восемь ближайших ([light] — не объект),
    # тени просит только тот, кому они по сюжету (слотов два).
    for x, y, z, col, rad, shadow, note in LIGHTS:
        out += ["[light]", f"pos = {x:.3f} {y:.3f} {z:.3f}",
                f"color = {col[0]:g} {col[1]:g} {col[2]:g}",
                f"radius_m = {rad:g}",
                f"casts_shadow = {1 if shadow else 0}", f"note = {note}", ""]
    open(os.path.join(ROOT, "assets/scenes/whiterun.scene"), "w",
         encoding="utf-8").write("\n".join(out))

    # relief: высоты плана (дельты от натуральной земли) + тропы дорог
    rel = ["# Daggerfall N relief — Вайтран v5: высоты чертежа + тропы.",
           "step 1"]
    if os.environ.get("WHITERUN_BARE") != "1" and os.path.exists(NATURAL_PATH):
        nat = [[float(v) for v in line.split()]
               for line in open(NATURAL_PATH, encoding="utf-8")
               if not line.startswith("#")]
        Hh = PLAN_H
        for gz in range(256):
            for gx in range(256):
                dh = (Hh[gz][gx] - nat[gz][gx]) * river_taper(gx, gz)
                if abs(dh) > 0.05:
                    rel.append(f"dh {gx} {gz} {dh:.2f}")
    rel += relief_paths
    open(os.path.join(ROOT, "assets/scenes/whiterun.relief"), "w",
         encoding="utf-8").write("\n".join(rel) + "\n")

    open(os.path.join(ROOT, "assets/maps/houses/whiterun.map"), "w",
         encoding="utf-8").write(
        "name = Вайтран: город на холме\n"
        "zone = houses\nsize_chunks = 1\n"
        "# Вайтран v5 — построен по чертежу docs/WHITERUN_PLAN.html (см. JSON).\n"
        "source = stand:Gallery\n"
        "scene = assets/scenes/whiterun.scene\n"
        "objects = assets/objects/parts;assets/objects/signs;assets/objects/trees\n"
        "description = Город по плану: река от горного истока рвом вдоль стены, "
        "мост у Восточных ворот, рынок, Гилдергрин, замок; дуб-поляна на юго-западе.\n"
        "built_commit =\n")
    check_layout()
    print(f"whiterun v5: {len(H)} построек, {len(P)} расстановок, "
          f"{len(LIGHTS)} огней, {n_yard} дворовых предметов, "
          f"{len(terrain)} terrain-блоков, {len(relief_paths)} троп")

if __name__ == "__main__":
    main()
