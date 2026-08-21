#!/usr/bin/env python3
# Created: 21:08:2026 - 04:10:00
# Last updated: 22:08:2026 - 01:30:00
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
    "manor":   ("city-manor.dfh", 14.0, 7.0),
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
    # origin (угол 0,0) из центра схемы: центр = origin + X*w/2 + Z*d/2
    ox = hs["x"] - (w/2)*c - (d/2)*s
    oz = hs["z"] + (w/2)*s - (d/2)*c
    y = sit(hs["x"], hs["z"], 25.0, half=max(w, d)/2)
    house(rec, ox, y, oz, yaw, hs["name"] or hs["kind"] or "дом")
    # каменная дорожка с бордюрами: цепочка furn-walk2 от двери до улицы
    if hs["walk"]:
        door_x = hs["x"] + dx*(d/2)
        door_z = hs["z"] + dz*(d/2)
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

def main():
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
    bx, bz = b["center"]
    gb = ground(bx + 7, bz)
    house("city-bridge.dfh", bx - 4, (25.0 if gb is None else gb + 0.45), bz - 2,
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

    # --- terrain: пады из зон, микрорельеф, река ---
    terrain = []
    for zn in PLAN["zones"]:
        xs = [p[0] for p in zn["pts"]]
        zs = [p[1] for p in zn["pts"]]
        cx, cz = sum(xs)/len(xs), sum(zs)/len(zs)
        hx = max(2.0, (max(xs)-min(xs))/2 - 2)
        hz = max(2.0, (max(zs)-min(zs))/2 - 2)
        if zn["h"] == 25:
            continue  # базовая равнина = натуральная земля
        terrain.append(f"[pad]\ncenter = {cx:.1f} {cz:.1f}\n"
                       f"half_extents = {hx:.1f} {hz:.1f}\nblend = 6\n"
                       f"height = {zn['h']}\nnote = зона плана h={zn['h']}\n")
    for m in PLAN["micro"]:
        xs = [p[0] for p in m["pts"]]
        zs = [p[1] for p in m["pts"]]
        cx, cz = sum(xs)/len(xs), sum(zs)/len(zs)
        # микропятно: высота относительно окружения решится вторым проходом
        base = ground(cx, cz)
        if base is None:
            continue
        terrain.append(f"[pad]\ncenter = {cx:.1f} {cz:.1f}\n"
                       f"half_extents = {max(2.0,(max(xs)-min(xs))/2):.1f} "
                       f"{max(2.0,(max(zs)-min(zs))/2):.1f}\nblend = 4\n"
                       f"height = {base + m['dh']}\nnote = микрорельеф {m['dh']:+d}\n")
    rpts = "\n".join(f"point = {x} {z} {23.6 - i*0.08:.2f}"
                     for i, (x, z) in enumerate(PLAN["river"]))
    terrain.append(f"[river]\nwidth_m = {PLAN['river_half_w']*2:.0f}\ndepth_m = 1.0\n"
                   f"bank_m = 5\nnote = река плана: один рукав от истока\n{rpts}\n")

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

    # relief: тропы дорог (свой файл, без копии town-land — рельеф теперь падовый)
    rel = ["# Daggerfall N relief — Вайтран v5: дороги планом, рельеф падами.",
           "step 1"]
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
    print(f"whiterun v5: {len(H)} построек, {len(P)} расстановок, "
          f"{len(terrain)} terrain-блоков, {len(relief_paths)} троп")

if __name__ == "__main__":
    main()
