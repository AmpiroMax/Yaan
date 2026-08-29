#!/usr/bin/env python3
#
# File: tools/dfo_read.py
#
# Responsibility:
# - ЧИТАЛКА .dfo НА ПИТОНЕ — ровно столько формата, сколько нужно ГЕНЕРАТОРУ
#   и СЧЁТУ БЮДЖЕТА: имя, вид, потоки (сколько вершин и треугольников в
#   каждом), габарит и признак «сплошной» по мерке движка.
#
# ЗАЧЕМ НЕ ЕЩЁ ОДИН БИНАРНИК. Генератор сцены — питон, и ему нужно знать
# радиус куста, высоту деревца и число треугольников объекта, чтобы посеять по
# закону и посчитать бюджет. Заводить ради трёх чисел четвёртый инструмент на
# C++ (и цель в общем CMakeLists, который правят три параллельные волны) —
# дороже, чем прочитать формат: он документирован в ObjectRegistry.cpp и
# состоит из заголовка и секций фиксированной раскладки.
#
# ЧЕГО ОН НЕ ДЕЛАЕТ, ЧЕСТНО: не проверяет content_hash и не пишет файлы.
# Личность объекта сторожит движок при чтении; здесь — только МЕРКА.
# Признак solid повторяет measure_object() (ObjectRegistry.cpp): верх потоков
# wood/bark/house выше PLAYER_STEP_HEIGHT. Значение шага берётся из
# Constants.h чтением, а не переписыванием (Rule 14).
#
# Dependencies:
# - Uses: Python stdlib.
# - Used by: tools/flora_sow.py, tools/measure_scene_budget.py, gen_trees_v2.py.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - ФОРМАТ ЗДЕСЬ — КОПИЯ, И ЭТО ЕЁ ЕДИНСТВЕННЫЙ РИСК. Если .dfo сменит
#   раскладку, эта читалка обязана отказать ВСЛУХ, а не прочитать мусор:
#   поэтому проверяется магия, версия контейнера и длина каждой секции.

import os
import re
import struct

MAGIC = b"DFNO"
MAX_VERSION = 3
_VERTEX = 36            # 3 pos + 3 nrm + 2 uv float32 + rgba u32
_STREAMS = (b"WOOD", b"CARD", b"GRND", b"BARK")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def _constant(name, default):
    """Читает число из engine/core/config/sources/Constants.h (Rule 14)."""
    path = os.path.join(ROOT, "engine", "core", "config", "sources", "Constants.h")
    try:
        with open(path, encoding="utf-8") as f:
            m = re.search(r"\b%s\s*=\s*([0-9.]+)" % re.escape(name), f.read())
    except OSError:
        return default
    return float(m.group(1)) if m else default

PLAYER_STEP_HEIGHT = _constant("PLAYER_STEP_HEIGHT", 0.35)

class Stream:
    __slots__ = ("verts", "tris", "lo", "hi")

    def __init__(self, verts, tris, lo, hi):
        self.verts = verts
        self.tris = tris
        self.lo = lo      # (x, y, z) минимум
        self.hi = hi      # (x, y, z) максимум

class Object:
    __slots__ = ("name", "kind", "source", "streams", "house_tris", "house_verts",
                 "house_lo", "house_hi")

    def __init__(self):
        self.name = ""
        self.kind = ""
        self.source = ""
        self.streams = {}
        self.house_tris = 0
        self.house_verts = 0
        self.house_lo = None
        self.house_hi = None

    # --- МЕРКА, повторяющая render::measure_object -------------------------
    def _boxes(self, names):
        lo = hi = None
        for n in names:
            s = self.streams.get(n)
            if s is None or s.verts == 0:
                continue
            lo = s.lo if lo is None else tuple(min(a, b) for a, b in zip(lo, s.lo))
            hi = s.hi if hi is None else tuple(max(a, b) for a, b in zip(hi, s.hi))
        return lo, hi

    @property
    def tris(self):
        return sum(s.tris for s in self.streams.values()) + self.house_tris

    @property
    def verts(self):
        return sum(s.verts for s in self.streams.values()) + self.house_verts

    @property
    def height(self):
        lo, hi = self._boxes(_STREAM_NAMES)
        if lo is None:
            return 0.0
        return hi[1] - lo[1]

    @property
    def radius(self):
        """Максимум sqrt(x^2+z^2) по вершинам — но по КОРОБКЕ, что достаточно
        для посева и НЕ требует держать вершины в памяти."""
        lo, hi = self._boxes(_STREAM_NAMES)
        if lo is None:
            return 0.0
        return max(abs(lo[0]), abs(hi[0]), abs(lo[2]), abs(hi[2]))

    @property
    def solid(self):
        lo, hi = self._boxes(("WOOD", "BARK"))
        top = hi[1] if hi is not None else 0.0
        if self.house_hi is not None:
            top = max(top, self.house_hi[1])
        return top > PLAYER_STEP_HEIGHT

    @property
    def solid_radius(self):
        lo, hi = self._boxes(("WOOD", "BARK"))
        if lo is None:
            return 0.0
        return max(abs(lo[0]), abs(hi[0]), abs(lo[2]), abs(hi[2]))

_STREAM_NAMES = ("WOOD", "CARD", "GRND", "BARK")

def _read_stream_body(buf, off):
    (vcount,) = struct.unpack_from("<I", buf, off)
    off += 4
    lo = [1e30, 1e30, 1e30]
    hi = [-1e30, -1e30, -1e30]
    if vcount:
        end = off + vcount * _VERTEX
        if end > len(buf):
            raise ValueError("поток вершин обрезан")
        for k in range(off, end, _VERTEX):
            x, y, z = struct.unpack_from("<fff", buf, k)
            if x < lo[0]: lo[0] = x
            if y < lo[1]: lo[1] = y
            if z < lo[2]: lo[2] = z
            if x > hi[0]: hi[0] = x
            if y > hi[1]: hi[1] = y
            if z > hi[2]: hi[2] = z
        off = end
    else:
        lo = hi = [0.0, 0.0, 0.0]
    (icount,) = struct.unpack_from("<I", buf, off)
    off += 4 + icount * 4
    return Stream(vcount, icount // 3, tuple(lo), tuple(hi)), off

def _read_string(buf, off):
    (n,) = struct.unpack_from("<I", buf, off)
    off += 4
    return buf[off:off + n].decode("utf-8"), off + n

def read_dfo(path):
    with open(path, "rb") as f:
        buf = f.read()
    if len(buf) < 8 or buf[:4] != MAGIC:
        raise ValueError("%s: не .dfo (магия)" % path)
    (version,) = struct.unpack_from("<I", buf, 4)
    if version > MAX_VERSION:
        raise ValueError("%s: контейнер версии %d новее известной %d"
                         % (path, version, MAX_VERSION))
    obj = Object()
    off = 8
    while off + 14 <= len(buf):
        tag = buf[off:off + 4]
        (_sv,) = struct.unpack_from("<H", buf, off + 4)
        (blen,) = struct.unpack_from("<Q", buf, off + 6)
        body = off + 14
        if body + blen > len(buf):
            raise ValueError("%s: секция %s обрезана" % (path, tag))
        if tag == b"INFO":
            p = body
            obj.name, p = _read_string(buf, p)
            obj.kind, p = _read_string(buf, p)
            obj.source, p = _read_string(buf, p)
        elif tag in _STREAMS:
            s, _ = _read_stream_body(buf, body)
            obj.streams[tag.decode()] = s
        elif tag == b"HOUS":
            p = body
            (count,) = struct.unpack_from("<I", buf, p)
            p += 4
            for _ in range(count):
                p += 12          # surface, tone, emissive
                s, p = _read_stream_body(buf, p)
                obj.house_tris += s.tris
                obj.house_verts += s.verts
                if s.verts:
                    obj.house_lo = s.lo if obj.house_lo is None else tuple(
                        min(a, b) for a, b in zip(obj.house_lo, s.lo))
                    obj.house_hi = s.hi if obj.house_hi is None else tuple(
                        max(a, b) for a, b in zip(obj.house_hi, s.hi))
        off = body + blen
    return obj

def shelf(dirs):
    """Читает полки в порядке следования: первое попадание побеждает."""
    out = {}
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".dfo"):
                continue
            name = fn[:-4]
            if name in out:
                continue
            out[name] = read_dfo(os.path.join(d, fn))
    return out

if __name__ == "__main__":
    import sys
    for d in sys.argv[1:]:
        for name, o in sorted(shelf([d]).items()):
            lo, hi = o._boxes(_STREAM_NAMES)
            print("%-28s %-9s h=%6.2f r=%5.2f tris=%7d solid=%s"
                  % (name, o.kind, o.height, o.radius, o.tris, o.solid))
