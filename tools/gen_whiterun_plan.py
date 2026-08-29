#!/usr/bin/env python3
# Module: tools
# File: tools/gen_whiterun_plan.py
#
# Responsibility:
# - СХЕМА ВАЙТРАНА v4.6 (заказ 21.08: «начни не с игры, а со схемы карты»).
#   Рисует чертёж города в docs/WHITERUN_PLAN.html и ЭКСПОРТИРУЕТ план в
#   docs/WHITERUN_PLAN.json — единственный источник для генератора игры
#   (tools/gen_whiterun.py читает JSON; правки города вносить СЮДА).
# - Автопроверки: постройки не в реке и не пересекаются.
import math

SC = 3.0
RIVER = [(224,40),(214,52),(202,66),(188,82),(172,102),(162,120),(158,138),(152,158),(143,178),(133,198),(126,220),(122,242),(121,256)]
RIVER_HALF = 3.6  # полуширина воды

def dist_to_polyline(pts, x, z):
    best = 1e9
    for (x0,z0),(x1,z1) in zip(pts, pts[1:]):
        dx, dz = x1-x0, z1-z0
        L2 = dx*dx+dz*dz
        t = max(0.0, min(1.0, ((x-x0)*dx+(z-z0)*dz)/L2))
        px, pz = x0+dx*t, z0+dz*t
        best = min(best, math.hypot(x-px, z-pz))
    return best

HOUSES = []  # (x, z, w, d) для проверок

def smooth_path(pts, close=True):
    P = pts; n = len(P)
    def mid(a, b): return ((a[0]+b[0])/2, (a[1]+b[1])/2)
    d = [f"M {((P[0][0]+P[1][0])/2)*SC:.1f} {((P[0][1]+P[1][1])/2)*SC:.1f}"]
    for i in range(1, n + (1 if close else -1)):
        p = P[i % n]; m = mid(p, P[(i+1) % n])
        d.append(f"Q {p[0]*SC:.1f} {p[1]*SC:.1f} {m[0]*SC:.1f} {m[1]*SC:.1f}")
    if close: d.append("Z")
    return " ".join(d)

svg = []
ZONES_X = []; MICRO_X = []; ROADS_X = []; TREES_X = []; HOUSES_FULL = []
def zone(color, pts, h=None):
    if h is not None:
        ZONES_X.append({"h": h, "pts": pts})
    svg.append(f'<path d="{smooth_path(pts)}" fill="{color}" stroke="none"/>')

zone("#b8c99a", [(0,0),(256,0),(256,256),(0,256)], h=25)
zone("#c9b784", [(196,10),(250,4),(256,30),(252,66),(224,84),(198,64),(188,34)], h=28)
zone("#d4ae71", [(206,14),(244,10),(250,34),(244,58),(222,72),(202,54),(196,32)], h=32)
zone("#dc9f5e", [(214,18),(240,16),(244,36),(238,52),(222,60),(206,46),(204,30)], h=36)
zone("#d98247", [(220,22),(236,20),(240,36),(232,46),(220,50),(212,38)], h=40)
zone("#adc493", [(58,88),(120,52),(160,58),(172,92),(168,128),(150,168),(128,196),(96,208),(70,186),(52,142)], h=26)
zone("#c2b98d", [(66,98),(122,62),(152,70),(162,100),(158,136),(140,168),(112,186),(82,174),(60,140)], h=27)
zone("#c9b784", [(72,100),(120,70),(146,78),(154,104),(148,134),(130,148),(104,158),(84,156),(66,136)], h=28)
zone("#cfb47b", [(76,100),(118,76),(142,84),(148,108),(142,130),(124,143),(103,150),(88,146),(72,130)], h=29)
zone("#d4ae71", [(80,98),(116,80),(142,86),(148,110),(138,130),(116,141),(96,145),(84,138),(74,120)], h=30)
zone("#d8a768", [(84,96),(114,84),(136,90),(140,112),(130,128),(112,138),(96,141),(86,132),(78,118)], h=31)
zone("#dc9f5e", [(86,94),(112,86),(130,96),(132,114),(124,126),(108,134),(96,136),(88,127),(82,114)], h=32)
zone("#e09755", [(84,86),(108,80),(126,90),(128,104),(116,116),(100,122),(88,114),(80,100)], h=33)
zone("#dd8d4e", [(84,58),(108,50),(122,62),(120,82),(104,94),(88,88),(78,72)], h=34)
zone("#d98247", [(88,56),(106,52),(116,62),(114,78),(100,86),(88,78),(84,66)], h=36)
zone("#c2b98d", [(24,28),(64,16),(88,34),(80,66),(44,76),(18,56)], h=27)
zone("#c9b784", [(32,34),(58,26),(74,42),(66,60),(42,64),(28,50)], h=28)
zone("#c2b98d", [(196,180),(228,170),(244,196),(232,228),(200,224),(186,202)], h=27)
zone("#adc493", [(60,206),(96,214),(96,240),(64,244),(48,226)], h=26)
zone("#b5cf9b", [(38,178),(62,172),(72,186),(68,202),(48,206),(34,194)], h=26)

micro = [
    ("+1", "#00000012", [(150,40),(162,36),(168,46),(158,54),(146,50)]),
    ("+1", "#00000012", [(180,140),(190,136),(194,146),(184,152)]),
    ("-1", "#ffffff30", [(96,178),(108,174),(112,184),(100,190)]),
    ("+1", "#00000012", [(120,120),(128,116),(132,124),(124,130)]),
    ("-1", "#ffffff30", [(90,124),(98,122),(100,130),(92,132)]),
    ("+1", "#00000012", [(134,146),(142,142),(146,150),(138,154)]),
    ("-1", "#ffffff30", [(70,110),(78,108),(80,116),(72,118)]),
    ("+1", "#00000012", [(102,64),(110,60),(114,68),(106,72)]),
    ("-1", "#ffffff30", [(160,200),(172,196),(176,206),(164,210)]),
]
for label, fill, pts in micro:
    MICRO_X.append({"dh": 1 if label == "+1" else -1, "pts": pts})
    svg.append(f'<path d="{smooth_path(pts)}" fill="{fill}"/>')
    cx = sum(p[0] for p in pts)/len(pts); cz = sum(p[1] for p in pts)/len(pts)
    svg.append(f'<text x="{cx*SC-6:.0f}" y="{cz*SC+4:.0f}" font-size="9" fill="#4a3c26" opacity="0.8">{label}</text>')

for h, x, z in [(25,170,40),(25,36,150),(26,150,182),(27,90,170),(27,44,40),(28,140,158),
                (30,128,138),(30,92,146),(33,116,108),(36,100,58),(27,214,206),(26,74,228),
                (28,196,70),(32,208,56),(36,226,58),(40,224,34),(26,50,190)]:
    svg.append(f'<text x="{x*SC}" y="{z*SC}" font-size="11" fill="#5a4a30" opacity="0.9">{h}</text>')

for gx, gz in [(92,74),(100,76),(120,72),(127,78),(112,68)]:
    svg.append(f'<g transform="translate({gx*SC},{gz*SC})">'
               f'<rect x="-7" y="-4.5" width="14" height="9" fill="#9aa86a" stroke="#6c7a44" stroke-width="1"/>'
               f'<line x1="-7" y1="-1.5" x2="7" y2="-1.5" stroke="#6c7a44" stroke-width="0.7"/>'
               f'<line x1="-7" y1="1.5" x2="7" y2="1.5" stroke="#6c7a44" stroke-width="0.7"/></g>')
svg.append(f'<text x="{104*SC}" y="{83*SC}" font-size="10" fill="#5c6a34">огороды</text>')

rp = smooth_path(RIVER, close=False)
svg.append(f'<path d="{rp}" fill="none" stroke="#7fb2d9" stroke-width="19" stroke-linecap="round" opacity="0.95"/>')
svg.append(f'<path d="{rp}" fill="none" stroke="#a5cbe8" stroke-width="12" stroke-linecap="round"/>')
svg.append(f'<ellipse cx="{224*SC}" cy="{40*SC}" rx="14" ry="10" fill="#8fc0e2" stroke="#4a7ba6" stroke-width="2"/>')
svg.append(f'<text x="{224*SC-24}" y="{40*SC-14}" font-size="10" font-weight="bold" fill="#2d5f86">ИСТОК</text>')
svg.append(f'<text x="{178*SC}" y="{96*SC}" font-size="11" fill="#2d5f86">река</text>')
for t, side in [(2,1),(3,-1),(5,1),(6,-1),(8,1),(9,-1),(11,-1)]:
    x0,z0 = RIVER[t]; x1,z1 = RIVER[t+1]
    bx, bz = (x0+x1)/2, (z0+z1)/2
    dx, dz = x1-x0, z1-z0
    L = math.hypot(dx,dz); nx, nz = -dz/L, dx/L
    svg.append(f'<circle cx="{(bx+nx*8*side)*SC:.0f}" cy="{(bz+nz*8*side)*SC:.0f}" r="4" fill="#6f9440" opacity="0.85"/>')

wall = [(80,168),(102,177),(122,174),(138,160),(148,142),(147,118),(140,94),(128,74),(118,59),(86,57),(74,84),(66,110),(65,138),(71,156)]
wp = " ".join(f"{x*SC:.0f},{z*SC:.0f}" for x, z in wall)
svg.append(f'<polygon points="{wp}" fill="none" stroke="#6d6a63" stroke-width="6" stroke-linejoin="round" opacity="0.92"/>')
ccx = sum(p[0] for p in wall)/len(wall); ccz = sum(p[1] for p in wall)/len(wall)
TOWERS = [(102,177),(138,160),(148,142),(147,118),(128,74),(86,57),(66,110),(71,156)]
for x, z in TOWERS:
    ang = math.degrees(math.atan2(z-ccz, x-ccx))
    svg.append(f'<g transform="translate({x*SC:.0f},{z*SC:.0f}) rotate({ang+90:.0f})">'
               f'<rect x="-6" y="-10" width="12" height="15" fill="#57544e" stroke="#2e2c28"/>'
               f'<rect x="-3.5" y="-13" width="7" height="4" fill="#57544e" stroke="#2e2c28"/></g>')
for (x, z, label, dx) in [(119,175.6,"ЮЖНЫЕ ВОРОТА",10), (147.6,130,"ВОСТОЧНЫЕ ВОРОТА",10)]:
    svg.append(f'<circle cx="{x*SC}" cy="{z*SC}" r="7" fill="#f3e8c8" stroke="#8a6b2f" stroke-width="2"/>')
    svg.append(f'<text x="{x*SC+dx}" y="{z*SC+4}" font-size="11" font-weight="bold" fill="#6b5320">{label}</text>')

def stone(pts_m, w=8, kind="stone", width_m=6.0):
    ROADS_X.append({"mat": kind, "w": width_m, "pts": pts_m})
    p = smooth_path(pts_m, close=False)
    svg.append(f'<path d="{p}" fill="none" stroke="#8f8c85" stroke-width="{w}" stroke-linecap="round"/>')
    svg.append(f'<path d="{p}" fill="none" stroke="#b5b1a8" stroke-width="{w-3}" stroke-linecap="round"/>')
def dirt(pts_m, w=5, width_m=2.5):
    ROADS_X.append({"mat": "dirt", "w": width_m, "pts": pts_m})
    p = smooth_path(pts_m, close=False)
    svg.append(f'<path d="{p}" fill="none" stroke="#a08052" stroke-width="{w}" stroke-dasharray="10,6" stroke-linecap="round"/>')

MAIN = [(119,173),(118,162),(116,152),(112,140),(108,128),(107,118),(103,108),(98,98),(95,88),(96,76),(98,68)]
stone([(166,127),(159.5,130),(152,131),(147,131)])
stone([(147,131),(143,138),(136,142),(127,145),(120,148)])
stone(MAIN)
# УЛИЦА ВЕТРОВ ОБХОДИТ ПРИСТВОЛЬНЫЙ КРУГ ГИЛДЕРГРИНА (заказ владельца). Прямой отрезок (107,118)->(116,113) проходил в 4.23 м от ствола,
# а полотно шириной 4 м рисует камень до 2.23 м от него — то есть улица и
# мостила подножие дерева. Излом разнесён на два узла, чтобы осевая у
# фасадного ряда (x=117) осталась на прежней отметке z=112.5: тамошний
# коридор между улицей и просёлком ГРЕБНЯ шириной ровно в один дом, и
# сдвиг улицы на метр к северу выдавливал дом на просёлок.
GRAVEL_Y = [(107,118),(111,113),(117,112.5),(124,110),(130,107)]
ROADS_X.append({"mat": "gravel", "w": 4.0, "pts": GRAVEL_Y})
p = smooth_path(GRAVEL_Y, close=False)
svg.append(f'<path d="{p}" fill="none" stroke="#b9a06a" stroke-width="5" stroke-linecap="round"/>')
dirt([(107,118),(98,114),(90,111)], 4); dirt([(103,108),(112,102),(120,98)], 4)
dirt([(118,162),(106,158),(97,152),(90,149),(85,145)], 4)
dirt([(119,177),(120,196),(117,218),(114,242),(113,256)])
svg.append(f'<g transform="translate({159.5*SC:.0f},{130*SC:.0f}) rotate(14)">'
           f'<rect x="-20" y="-6" width="40" height="12" fill="#8f8c85" stroke="#4a4741" stroke-width="1.5"/>'
           f'<line x1="-20" y1="-6" x2="20" y2="-6" stroke="#4a4741"/><line x1="-20" y1="6" x2="20" y2="6" stroke="#4a4741"/></g>')
svg.append(f'<text x="{160*SC-10}" y="{125*SC-6}" font-size="10" font-weight="bold" fill="#4a4741">МОСТ</text>')
stone([(256,160),(236,152),(214,142),(196,134),(180,129),(166,127)], 7)

oak = (52, 190)
stone([(113,214),(96,206),(78,200),(66,196),(oak[0]+3,oak[1]+1)], 6, width_m=2.0)
svg.append(f'<circle cx="{oak[0]*SC}" cy="{oak[1]*SC}" r="{5.5*SC}" fill="#6f8f3f" stroke="#44601f" stroke-width="2.5"/>')
svg.append(f'<circle cx="{oak[0]*SC}" cy="{oak[1]*SC}" r="4" fill="#4b3a1e"/>')
svg.append(f'<text x="{oak[0]*SC-24}" y="{(oak[1]+9)*SC}" font-size="11" font-weight="bold" fill="#3f5a1c">ДУБ-ПОЛЯНА</text>')
for x, z in [(42,180),(60,176),(68,184),(66,199),(44,202),(36,188),(58,204)]:
    svg.append(f'<circle cx="{x*SC:.0f}" cy="{z*SC:.0f}" r="6" fill="#a8c86a" stroke="#6a8a3a" stroke-width="1.2" opacity="0.9"/>')

svg.append('<defs><marker id="door" markerWidth="7" markerHeight="7" refX="5" refY="3.5" orient="auto"><path d="M0,0 L7,3.5 L0,7 z" fill="#c23b22"/></marker></defs>')
def seg_dir(pts, x, z):
    best, ang = 1e9, 0.0
    for (x0,z0),(x1,z1) in zip(pts, pts[1:]):
        mx, mz = (x0+x1)/2, (z0+z1)/2
        d = (mx-x)**2 + (mz-z)**2
        if d < best: best, ang = d, math.degrees(math.atan2(z1-z0, x1-x0))
    return ang

def curb(a, b):
    x0, z0 = a; x1, z1 = b
    svg.append(f'<line x1="{x0*SC:.0f}" y1="{z0*SC:.0f}" x2="{x1*SC:.0f}" y2="{z1*SC:.0f}" stroke="#5f5c55" stroke-width="5" stroke-linecap="round"/>')
    svg.append(f'<line x1="{x0*SC:.0f}" y1="{z0*SC:.0f}" x2="{x1*SC:.0f}" y2="{z1*SC:.0f}" stroke="#c4c0b6" stroke-width="2.4" stroke-linecap="round"/>')

def house(x, z, w, d, deg, color, name=None, door="S", walk=None, stroke="#4b3a26", ns=None, kind=None):
    HOUSES.append((x, z, w, d, name or ""))
    HOUSES_FULL.append({"x": x, "z": z, "w": w, "d": d, "deg": deg, "door": door,
                        "walk": list(walk) if walk else None, "name": name or "", "kind": kind or ""})
    cx, cz = x*SC, z*SC
    rad = math.radians(deg)
    dirs = {"S": (0, d/2), "N": (0, -d/2), "E": (w/2, 0), "W": (-w/2, 0)}
    dx, dz = dirs[door]
    wx = x + (dx*math.cos(rad) - dz*math.sin(rad))
    wz = z + (dx*math.sin(rad) + dz*math.cos(rad))
    if walk: curb((wx, wz), walk)
    svg.append(f'<g transform="translate({cx:.0f},{cz:.0f}) rotate({deg:.0f})">')
    svg.append(f'<rect x="{-w*SC/2:.0f}" y="{-d*SC/2:.0f}" width="{w*SC:.0f}" height="{d*SC:.0f}" fill="{color}" stroke="{stroke}" stroke-width="1.6"/>')
    svg.append(f'<line x1="0" y1="0" x2="{dx*1.45*SC:.0f}" y2="{dz*1.45*SC:.0f}" stroke="#c23b22" stroke-width="2" marker-end="url(#door)"/>')
    svg.append('</g>')
    if name:
        nx, nz = ns if ns else (cx+4, cz-6)
        svg.append(f'<text x="{nx:.0f}" y="{nz:.0f}" font-size="10" fill="#3d2f1c">{name}</text>')

def along(street, x, z): return seg_dir(street, x, z)

house(100, 64, 15, 8, 0, "#9c8f7d", "ЗАМОК", "S", walk=(100,70), stroke="#3b332a", ns=(100*SC-16, 57*SC), kind="keep")
house(91, 69, 5, 6, 0, "#8f8375", None, "E", walk=(97,71), kind="wing")
house(109, 69, 5, 6, 0, "#8f8375", None, "W", walk=(103,71), kind="wing")
house(115, 70, 4, 4, 0, "#7a7065", "донжон", "W", walk=(107,71), kind="donjon")
svg.append(f'<circle cx="{97*SC}" cy="{73*SC}" r="3.5" fill="#4d698a"/><text x="{97*SC+5}" y="{73*SC+3}" font-size="9" fill="#33435a">колодец</text>')
house(90, 90, 9, 7, along(MAIN,95,90)+90, "#c8a06a", "усадьба", "E", walk=(95,90), kind="manor")
GREB = [(103,108),(112,102),(120,98)]
house(103, 86, 8, 6, along(MAIN,98,90)+90, "#c8a06a", None, "S", walk=(99,92))
house(115, 95, 8, 6, along(GREB,115,98), "#c8a06a", None, "S", walk=(115,99))
# ГИЛДЕРГРИН СТОИТ У УЛИЦЫ, А НЕ НА НЕЙ (претензия [N25]). Дерево стояло в
# (108,118) — В МЕТРЕ от осевой главной улицы, и это не рисунок, а тело:
# ствол-коллайдер занимает 107.1..108.9 по x при осевой x=107.0 (перепись тел
# Jolt в прогоне бота), а кольцо city-treering — восьмигранник радиусом 2.6 со
# стенкой 0.5 м — накрывает осевую целиком. Ходок обязан свернуть с осевой у
# самого дерева и сворачивал: критик застал бота в 0.15 м от северной стены
# дома (98.5,126.6), в пяти метрах западнее улицы, и назвал это «террасным
# уступом». Уступа там нет — земля идёт 0.12 м/м (замер профиля).
# Место выбрано ПРОБОЙ, а не на глаз: перебор полуметровой сеткой по кварталу
# с двумя требованиями — центр не ближе 4.2 м ни к одной осевой (кольцо 2.6 +
# капсула 0.35 + запас) и не ближе 3.3 м к телу дома. Ближайшая точка,
# отвечающая обоим, — (115.5,118), в 7.5 м на восток: дерево остаётся на той
# же широте, у начала улицы Ветров (гайд §11: улица упирается в Гилдергрин),
# и уходит с проезжей части главной. Замер после переноса — в check_landmarks.
GILDER = (115.5, 118)
svg.append(f'<circle cx="{GILDER[0]*SC}" cy="{GILDER[1]*SC}" r="{7*SC}" fill="none" stroke="#b58f52" stroke-width="2" stroke-dasharray="4,4"/>')
svg.append(f'<circle cx="{GILDER[0]*SC}" cy="{GILDER[1]*SC}" r="{4.4*SC}" fill="#e8a7c4" stroke="#b06a8c" stroke-width="2"/>')
svg.append(f'<text x="{GILDER[0]*SC-32}" y="{GILDER[1]*SC+36}" font-size="11" font-weight="bold" fill="#8c4a68">ГИЛДЕРГРИН</text>')
house(89, 108, 10, 7, 90, "#d9d2c2", "ХРАМ", "E", walk=(94,111), ns=(84*SC, 102*SC), kind="temple")
house(129, 106, 12, 7, 45, "#caa26a", "ЙОРРВАСКР 45°", "W", walk=(128,108), ns=(125*SC, 97*SC), kind="longhall")
house(117, 109, 7, 6, along(GRAVEL_Y,117,112), "#c8a06a", None, "S", walk=(118,112.5))
house(123, 115, 7, 6, along(GRAVEL_Y,123,111), "#c8a06a", None, "N", walk=(124,110.5))
house(102, 124, 7, 6, along(MAIN,106,124)+90, "#c8a06a", None, "E", walk=(106.7,124))
# ДОМ ОТОДВИНУТ НА ЮГ РАДИ СКВЕРА (заказ владельца 22.08: «окультурить
# пространство вокруг дерева, оно должно быть точкой притяжения»).
# Стоял в (113.5,124.5) — в 3.5 м к югу от ствола Гилдергрина, и это не «тесно
# на глаз», а ИЗМЕРЕНО: кольцевая скамья кузни-3 (радиус сиденья 3.52..4.18)
# садилась одним сегментом из восьми, остальные семь отказывала проба — три
# упирались в тело ЭТОГО дома, четыре в полосы двух улиц. Дерево между двумя
# улицами и домом стоит в проходе, а не на площадке; точки притяжения из такого
# места не бывает никакой расстановкой.
# ПЕРЕНОСИТСЯ ДОМ, А НЕ ДЕРЕВО, и это важно: место Гилдергрина выбрано прошлой
# волной ПРОБОЙ по двум требованиям ([N25], бот застревал) и держит гайд §11 —
# улица Ветров упирается в дерево. Сдвинув дерево, я отменил бы и то и другое
# ради своей же клумбы. Дом же ничем не связан: он рядовой, дверь на ту же
# главную улицу, южнее его подпирает только пустой квартал.
house(112, 131, 7, 6, along(MAIN,110,131)+90, "#c8a06a", None, "W", walk=(108.6,131))
svg.append(f'<rect x="{109*SC}" y="{146*SC}" width="{14*SC}" height="{11*SC}" fill="#cfc5a8" stroke="#8f855f"/>')
# КОЛОДЕЦ — ОРИЕНТИР ПЛОЩАДИ, И ЕГО МЕСТО ВЫБИРАЕТ ПРОБА, А НЕ РИСУНОК.
# Число тут ДВАЖДЫ врало: чертёж рисовал колодец в (116,151.5) и после того,
# как экспорт увёл его в (110.5,153) — рисунок и город разошлись. Теперь одна
# константа на оба. И само (110.5,153) оказалось не лучше первого: тело лавки
# (107,156.5), отодвинутое от дороги в площадь, накрывает 5.9х6.8 м западной
# половины плиты и садится ПОВЕРХ колодца — «зажат домом с трёх сторон» [24].
# Перебор полуметровой сеткой по плите (тело 2.80х2.74 от угла (-0.55,-0.52))
# с двумя требованиями — целиком на плите и не ближе полуширины ни к одной
# полосе — оставляет на всей площади ОДНО место: угол (120,151.5), центр
# (120.85,152.35), 4.69 м от осевой главной улицы и 0.72 м от таверны.
# Восточная половина площади задумывалась свободной под тракт Восточных ворот,
# но тракт кончается узлом (120,148) севернее колодца, а западной половины у
# площади фактически нет — её занял чужой дом.
WELL = (120, 151.5)
svg.append(f'<circle cx="{(WELL[0]+0.85)*SC}" cy="{(WELL[1]+0.85)*SC}" r="4" fill="#4d698a"/><text x="{(WELL[0]+0.85)*SC+6}" y="{(WELL[1]+0.85)*SC+3}" font-size="9" fill="#33435a">колодец</text>')
svg.append(f'<text x="{103*SC}" y="{161*SC+8}" font-size="11" font-weight="bold" fill="#6d6142">РЫНОК</text>')
for sx in (112, 116.5, 121):
    svg.append(f'<rect x="{sx*SC-4}" y="{148.2*SC}" width="8" height="5" fill="#c46a4f" stroke="#8a4433"/>')
# СЕВЕРНАЯ ЛАВКА ТОЖЕ СХОДИТ С ПЛИТЫ. Она стояла в (107,147.5) и телом 6.8x9.0
# заходила на прямоугольник рынка на 1.5 м по x. Пад под домом ВНУТРИ рынка не
# режется (площадь говорит последней — иначе под ней вырастала ступень 1.18 м),
# и ровно эта заходящая полоса оставалась на отметке площади: земля протыкала
# пол лавки на 0.23 м. Тело целиком снаружи прямоугольника — и пад работает.
house(103.5, 146.5, 6, 7, 0, "#c8a06a", "лавка", "E", walk=(107.5,147), kind="shop")
# ЛАВКА УБРАНА С ПЛИТЫ РЫНКА (заказ владельца 22.08: «оживить рынок»).
# Стояла в (107,156.5) и ОТОДВИГОМ ОТ ГРУНТОВОЙ ДОРОГИ уезжала на восток,
# телом 6.8x9.0 в центр (110.4,153.4) — то есть накрывала x 105.9..114.9 на
# всю глубину площади. Карта свободы прямоугольника рынка (прибор этой волны)
# показывала после этого ровно две свободные полосы: северо-западный угол и
# восточная кромка у колодца, всё остальное — тело лавки и полоса главной
# улицы, которая рынок и пересекает. Отсюда «рынок: 0 предметов оживления» на
# первом прогоне: оживлять было НЕГДЕ, а не нечем.
# Новое место — южнее переулка, дверью на ту же главную улицу; отодвиг теперь
# толкает её ОТ площади, а не на неё.
house(104, 161, 6, 7, 0, "#c8a06a", "лавка", "E", walk=(108,161), kind="shop")
house(127.5, 152, 8, 7, 0, "#b8894f", "таверна", "W", walk=(123.5,152), kind="tavern")
house(124, 168.5, 7, 6, 0, "#8f8375", "кузница", "N", walk=(121,164), ns=(125*SC, 174*SC), kind="smithy")
GRAV_E = [(147,131),(143,138),(136,142),(127,145),(120,148)]
house(130, 138, 7, 6, along(GRAV_E,132,141), "#c8a06a", None, "S", walk=(132,141.5))
house(138, 147, 7, 6, along(GRAV_E,139,141), "#c8a06a", None, "N", walk=(138,142.5))
house(112, 168.5, 6, 6, along(MAIN,118,168)+90, "#c8a06a", None, "E", walk=(117,168))
house(111, 161, 6, 6, along(MAIN,118,163)+90, "#c8a06a", None, "E", walk=(117,161))
WLANE = [(97,152),(90,149),(85,145)]
house(91, 153.5, 7, 6, along(WLANE,91,150), "#b09a70", "ветхий", "N", walk=(91,150.4), kind="old")
house(84, 149, 6, 6, along(WLANE,85,146), "#b09a70", "ветхий", "N", walk=(85.5,146.4), kind="old")
SOUTH = [(119,177),(120,196),(117,218)]
house(102, 196, 8, 6, along(SOUTH,119,196)+90, "#c8a06a", "ферма", "E", walk=(107,196), kind="farm")
house(92, 204, 9, 6, 0, "#a98f60", "амбар", "E", walk=(98,204.5), kind="barn")
# МЕЛЬНИЦА: на западном берегу, корпус на суше, колесо вылетом над водой
RIV_D = [(143,178),(133,198)]
house(132.5, 182, 7, 7, along(RIV_D,136,182), "#8a6a40", "МЕЛЬНИЦА", "W", walk=(128,183), ns=(120*SC, 177*SC), kind="mill")
svg.append(f'<g transform="translate({137.3*SC:.0f},{183.6*SC:.0f}) rotate(64)"><rect x="-1.5" y="-7" width="3" height="14" fill="#5b4326"/><circle cx="0" cy="0" r="6.5" fill="none" stroke="#5b4326" stroke-width="2"/></g>')
svg.append(f'<text x="{139*SC}" y="{189*SC}" font-size="9" fill="#4a3a20">колесо</text>')
dirt([(128,183),(122,188),(120,193)], 4)  # тропка мельницы к южной дороге
# КОНЮШНИ: к западу от южной дороги, на сухом
house(112, 191, 8, 5, along(SOUTH,119,191)+90, "#a98f60", "конюшни", "E", walk=(117.6,191), kind="stable")
EAST_RD = [(166,127),(180,129),(196,134)]
house(178, 122, 9, 7, along(EAST_RD,178,128), "#b8894f", "постоялый двор", "S", walk=(178,127), kind="inn")

GARDENS = [(92,74),(100,76),(120,72),(127,78),(112,68)]
GARDEN_R = 8.0   # куст ближе этого к грядке — культура огорода, а не самосев

def in_walls(x, z):
    inside = False
    n = len(wall); j = n - 1
    for i in range(n):
        xi, zi = wall[i]; xj, zj = wall[j]
        if (zi > z) != (zj > z) and x < (xj - xi) * (z - zi) / (zj - zi) + xi:
            inside = not inside
        j = i
    return inside

FELLED = []

def tree(x, z, r, color, stroke, kind="bush"):
    # ВЫРУБКА ВНУТРИ СТЕН (решение владельца): в городе остаётся ОДНО дерево —
    # Гилдергрин. Берёзы внутри кольца снимаются все; кусты остаются только на
    # грядках огородов — это культура, а не самосев. Ели и дубы стоят снаружи
    # и не трогаются. Правится ЗДЕСЬ: чертёж — единственный источник состава.
    if in_walls(x, z) and kind != "oak":
        if kind == "bush" and min(math.hypot(x-gx, z-gz) for gx, gz in GARDENS) <= GARDEN_R:
            pass
        else:
            FELLED.append((kind, x, z))
            return
    TREES_X.append({"kind": kind, "x": x, "z": z})
    svg.append(f'<circle cx="{x*SC:.0f}" cy="{z*SC:.0f}" r="{r*SC:.1f}" fill="{color}" stroke="{stroke}" stroke-width="1.2" opacity="0.9"/>')
for x, z in [(97,133),(100,131),(95,130), (137,116),(140,113), (85,97),(83,101), (119,132),(122,134)]:
    tree(x, z, 1.8, "#a8c86a", "#6a8a3a", kind="birch")
for x, z in [(36,50),(41,46),(33,42),(46,53),(40,58), (56,30),(61,34),(52,26),
             (238,74),(244,80),(234,84), (196,176),(202,182),(208,174),
             (28,118),(24,126),(32,128), (204,94),(210,100)]:
    tree(x, z, 2.2, "#4f7a46", "#2f5230", kind="spruce")
for x, z in [(84,222),(89,226),(152,226),(150,60)]:
    tree(x, z, 3.0, "#7da052", "#4c6b32", kind="oak")
for x, z in [(94,104),(110,88),(121,90),(98,142),(107,162),(131,155),(84,120),(93,122),
             (144,150),(78,160),(73,146),(86,72),(106,74),(134,92),(143,116)]:
    tree(x, z, 1.0, "#8fae5c", "#5c7a38")

# ---------- АВТОПРОВЕРКИ ----------
problems = []
for x, z, w, d, name in HOUSES:
    r_half = math.hypot(w, d) / 2
    dist = dist_to_polyline(RIVER, x, z)
    if dist < RIVER_HALF + r_half * 0.75:
        problems.append(f"РЕКА: {name or '(дом)'} ({x},{z}) — {dist:.1f} м до осевой")
for i in range(len(HOUSES)):
    for j in range(i+1, len(HOUSES)):
        x1,z1,w1,d1,n1 = HOUSES[i]; x2,z2,w2,d2,n2 = HOUSES[j]
        if math.hypot(x1-x2, z1-z2) < (max(w1,d1)+max(w2,d2))/2 * 0.85:
            problems.append(f"ПЕРЕСЕЧЕНИЕ: {n1 or i}({x1},{z1}) и {n2 or j}({x2},{z2})")
if problems:
    print("ПРОБЛЕМЫ:"); [print(" -", p) for p in problems]
else:
    print("проверки чисты: река и пересечения")
_inside = [t for t in TREES_X if in_walls(t["x"], t["z"])]
print(f"вырубка внутри стен: снято {len(FELLED)} "
      f"({sum(1 for k,_,_ in FELLED if k=='birch')} берёз, "
      f"{sum(1 for k,_,_ in FELLED if k=='bush')} кустов); "
      f"осталось внутри {len(_inside)} — {[t['kind'] for t in _inside]}")

grid = ['<g stroke="#00000018" stroke-width="1">']
for i in range(0, 257, 32):
    grid.append(f'<line x1="{i*SC}" y1="0" x2="{i*SC}" y2="{256*SC}"/>')
    grid.append(f'<line x1="0" y1="{i*SC}" x2="{256*SC}" y2="{i*SC}"/>')
grid.append('</g>')
for i in range(0, 257, 64):
    grid.append(f'<text x="{i*SC+3}" y="12" font-size="10" fill="#00000055">{i}</text>')
    grid.append(f'<text x="3" y="{i*SC+12}" font-size="10" fill="#00000055">{i}</text>')

svg_body = "\n".join(['<svg viewBox="0 0 768 768" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:900px;background:#dfe8d0;border:1px solid #999">'] + svg + grid + ['</svg>'])

html = """<!doctype html>
<!--
Module: docs
File: docs/WHITERUN_PLAN.html
Responsibility: чертёж Вайтрана (генерируется tools/gen_whiterun_plan.py;
правки вносить в генератор чертежа, не сюда).
-->
<meta charset="utf-8"><title>Схема Вайтрана v4.6</title>
<body style="max-width:960px;margin:1.5em auto;font:15px/1.55 -apple-system,sans-serif;color:#222;background:#faf8f4;padding:0 1em">
<h1 style="margin:.2em 0">Вайтран v4.6 — чертёж (по гайду 8/10 → 10/10)</h1>
<p><b>Правка «дома в речке»:</b> мельница пересажена на западный берег (корпус на суше,
колесо вылетом над водой, своя тропка к южной дороге), конюшни — к западу от южной дороги
на сухое. В генератор схемы добавлены АВТОПРОВЕРКИ: ни одна постройка не ближе к осевой
реки, чем полширины воды + свой радиус, и ни одна пара построек не пересекается — прогон
чист.</p>
""" + svg_body + """
<h2>Легенда</h2>
<ul style="columns:2">
<li>Заливки — высоты (24…40), «+1/−1» — микрорельеф; гора-исток на востоке.</li>
<li>Река — один рукав от родника, рвом вдоль восточной стены; мост НА воде.</li>
<li>Двойные серые — булыжник; песочная — гравий; пунктир — земля. НОРМАТИВ ШИРИН для
переноса в игру: главная улица и тракты — 6 м, гравийные — 4 м, переулки-земля — 2.5 м,
дорожки с бордюрами — 1.2 м; река 7 м; зазор фасадов в ряду ≤0.5 м, разрыв больше — это
проход во двор.</li>
<li>Тонкие с окантовкой — каменные дорожки с бордюрами от каждой двери.</li>
<li>Красная стрелка — вход; башни наружу; прилавки на рынке; борозды — огороды.</li>
<li>Розовый — Гилдергрин; дуб-поляна на ЮЗ; ели вне стен; мельничное колесо — над водой.</li>
</ul>
<p style="color:#666">Жду «ок» — дальше рельеф и генератор по этой схеме.</p>
</body>"""

import json
def hexport():
    return HOUSES_FULL
open("docs/WHITERUN_PLAN.json", "w", encoding="utf-8").write(json.dumps({
    "river": RIVER, "river_half_w": RIVER_HALF,
    "wall": wall, "towers": TOWERS,
    "gates": [{"pos": [119,175.6], "name": "south"}, {"pos": [147.6,130], "name": "east"}],
    "bridge": {"center": [159.5,130], "deg": 14, "len": 13},
    "roads": ROADS_X,
    "houses": HOUSES_FULL,
    "market": {"rect": [109,146,14,11], "well": list(WELL), "stalls": [[111.3,149.2],[118,149.2],[121.3,149.2]]},
    "gildergreen": list(GILDER), "oak_glade": {"oak": [52,190], "trees": [[42,180],[60,176],[68,184],[66,199],[44,202],[36,188],[58,204]]},
    "gardens": [[92,74],[100,76],[120,72],[127,78],[112,68]],
    "trees": TREES_X,
    "zones": ZONES_X, "micro": MICRO_X,
    "spring": [224,40],
}, ensure_ascii=False, indent=1))
open("docs/WHITERUN_PLAN.html", "w", encoding="utf-8").write(html)
print("v4.6 written")
