#!/usr/bin/env python3
# Created: 21:08:2026 - 04:10:00
# Last updated: 22:08:2026 - 14:20:00
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

import json
import math
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAN = json.load(open(os.path.join(ROOT, "docs/WHITERUN_PLAN.json"), encoding="utf-8"))

H = []   # [house]
P = []   # [place]

def house(file, x, y, z, yaw_deg, note):
    H.append((f"assets/houses/{file}", x, y, z, math.radians(yaw_deg), note))

def place(obj, x, y, z, yaw_deg=0.0, note=None):
    P.append((obj, x, y, z, math.radians(yaw_deg), note))

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

# --- kind -> рецепт и реальные габариты (фронт w вдоль двери, глубина d) ----
KIND = {
    "keep":    ("city-keep-s.dfh", 25.0, 9.0),
    "wing":    (None, 0, 0),  # крылья входят в рецепт keep-s
    "donjon":  ("city-donjon.dfh", 3.6, 3.6),
    # city-manor реально 14х15 с крылом — на схемном пятне 9х7 крыло легло
    # на главную улицу (прогон v5-1). Усадьба плана = крупный дом.
    "manor":   ("city-house-l.dfh", 10.2, 8.2),
    "temple":  ("city-temple.dfh", 10.0, 8.0),
    "longhall":("city-longhall.dfh", 12.0, 8.0),
    "shop":    ("city-shop.dfh", 6.4, 8.6),
    "tavern":  ("city-house-l.dfh", 10.2, 8.2),
    "smithy":  ("city-shop-old.dfh", 6.4, 8.6),
    "old":     ("city-house-s-old.dfh", 7.0, 7.0),
    "farm":    ("city-house-s.dfh", 7.0, 7.0),
    "barn":    ("city-barn.dfh", 12.0, 9.0),
    "mill":    ("city-mill.dfh", 7.0, 7.0),
    "stable":  ("city-barn-old.dfh", 12.0, 9.0),
    "inn":     ("city-house-l.dfh", 10.2, 8.2),
    "":        ("city-house-s.dfh", 7.0, 7.0),
}
TREE = {"birch": ["birch-forge-a", "birch-forge-b"],
        "spruce": ["spruce-forge-a", "pine-forge-a", "spruce-forge-b"],
        "oak": ["oak-forge-a"], "bush": ["juniper-forge-a"]}

def door_dir(deg, door):
    """Мировое направление наружу от дверной стороны прямоугольника схемы."""
    base = {"S": (0, 1), "N": (0, -1), "E": (1, 0), "W": (-1, 0)}[door]
    r = math.radians(deg)
    return (base[0]*math.cos(r) - base[1]*math.sin(r),
            base[0]*math.sin(r) + base[1]*math.cos(r))

def put_house(hs):
    rec, w, d = KIND.get(hs["kind"], KIND[""])
    if rec is None:
        return
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
    # Отодвиг от дорог: тело пересекает полосу -> шаг по нормали ОТ ближайшей
    # точки нарушающей дороги (двери может быть недостаточно, дорога бывает
    # сбоку). Шаг 0.4, не больше 5 м.
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
    moved = 0.0
    yaw_now = math.degrees(math.atan2(dx, dz)) % 360.0
    while moved < 8.0 and any(rect_road_hit(ccx, ccz, w, d, yaw_now, rd)
                              for rd in PLAN["roads"]):
        qx, qz = nearest_on_roads(ccx, ccz)
        nx, nz = ccx - qx, ccz - qz
        nl = math.hypot(nx, nz) or 1.0
        ccx += nx / nl * 0.4
        ccz += nz / nl * 0.4
        moved += 0.4
    ox = ccx - (w/2)*c - (d/2)*s
    oz = ccz + (w/2)*s - (d/2)*c
    y = sit(ccx, ccz, 25.0, half=max(w, d)/2)
    house(rec, ox, y, oz, yaw, hs["name"] or hs["kind"] or "дом")
    PLACED.append((ccx, ccz, w, d, yaw, hs["name"] or hs["kind"] or "дом"))
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

MRX, MRZ, MRW, MRD = PLAN["market"]["rect"]

def on_market(px, pz):
    """Точка на рыночной площади (с буфером): дороги, влившиеся в площадь,
    там не считаются — фасады стоят на самой площади, это норма."""
    return (MRX - 2 <= px <= MRX + MRW + 2) and (MRZ - 2 <= pz <= MRZ + MRD + 2)

def rect_road_hit(cx, cz, w, d, yaw_deg, rd):
    """Сэмплы осевой дороги в локали дома против [±w/2]x[±d/2]."""
    r = math.radians(yaw_deg)
    c, sn = math.cos(r), math.sin(r)
    for (x0,z0),(x1,z1) in zip(rd["pts"], rd["pts"][1:]):
        L = math.hypot(x1-x0, z1-z0)
        for i in range(int(L*2) + 1):
            t = i / max(1, int(L*2))
            wx0, wz0 = x0+(x1-x0)*t, z0+(z1-z0)*t
            if on_market(wx0, wz0):
                continue
            px, pz = wx0 - cx, wz0 - cz
            lx = px*c - pz*sn
            lz = px*sn + pz*c
            ddx = max(abs(lx) - w/2, 0.0)
            ddz = max(abs(lz) - d/2, 0.0)
            if math.hypot(ddx, ddz) < rd["w"]/2 - 0.3:
                return True
    return False

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
    # --- постройки плана ---
    for hs in PLAN["houses"]:
        put_house(hs)
    # рынок: плита, колодец, прилавки
    mx, mz, mw, md = PLAN["market"]["rect"]
    my = sit(mx + mw/2, mz + md/2, 27.0, half=max(mw, md)/2)
    house("city-plaza12.dfh", mx + 1, my, mz, 0, "рыночная площадь")
    wx, wz = PLAN["market"]["well"]
    house("city-well.dfh", wx, my + 0.02, wz, 0, "колодец рынка")
    for i, (sx, sz) in enumerate(PLAN["market"]["stalls"]):
        house("city-stall.dfh", sx - 1.2, my + 0.02, sz - 0.5, 180, f"прилавок {i+1}")
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
          f"{len(terrain)} terrain-блоков, {len(relief_paths)} троп")

if __name__ == "__main__":
    main()
