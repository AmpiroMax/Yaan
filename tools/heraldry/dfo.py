#!/usr/bin/env python3
#
# Module: tools
# File: tools/heraldry/dfo.py
#
# Responsibility:
# - ЗАПИСЬ И ЧТЕНИЕ .dfo НА PYTHON: тот же контейнер, что пишет
#   engine/render/sources/ObjectRegistry.cpp, включая fnv1a64-личность объекта.
#   Нужно генератору герба, который печёт меш офлайн и обязан положить его на
#   полку реестра, а не рядом с ней.
#
# ПОЧЕМУ НЕ КУЗНИЦА НА C++, КАК forge_trees. Кузницы на C++ линкуются с
# dfn_render и требуют полной сборки; герб — разовая офлайн-запечка из
# картинки, и питон здесь читает PNG и считает преобразование расстояний.
# Формат при этом ОДИН, и цена этого — вот этот файл: вторая реализация
# контейнера всегда рискует разойтись с первой.
#
# КАК ЭТОТ РИСК СНЯТ — ЭТО ГЛАВНОЕ, ЧТО НАДО ЗНАТЬ ПРО ФАЙЛ. Проверка не
# «прочитали свой же файл своим же читателем» (так сходится любая ошибка,
# сделанная дважды), а ДВУСТОРОННЯЯ СВЕРКА С ЧУЖИМИ БАЙТАМИ:
#   1. читаем .dfo, ИСПЕЧЁННЫЙ ДВИЖКОМ, и заново считаем его хэш — совпал,
#      значит наша fnv1a64 и наш порядок полей вершины те же (иначе хэш
#      разъедется на первой же вершине);
#   2. пишем прочитанное НАШИМ писателем и сверяем БАЙТ В БАЙТ с исходником —
#      совпал, значит те же секции, тот же порядок, те же длины.
# Прогон: python3 tools/heraldry/dfo.py --verify (см. main внизу).
#
# Формат (BinaryWriter.h, правило 7):
#   file    := magic:u32 container_version:u32 section*
#   section := tag:u32 section_version:u16 byte_length:u64 payload
#   stream  := vertex_count:u32 (pos3f normal3f uv2f colour:u32)* index_count:u32 u32*
# Всё — little-endian. Вершина пишется ПОЛЕ ЗА ПОЛЕМ: раскладка структуры в
# памяти — это выбор компилятора, а не формат.
#
# Dependencies:
# - Uses: стандартная библиотека (struct), numpy.
# - Used by: tools/gen_heraldry.py.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ХЭШ — ЭТО ЛИЧНОСТЬ ОБЪЕКТА, и fnv1a64 заморожена навсегда. Порядок потоков
#   (wood, cards, ground, bark [, house]) менять нельзя: движок сверяет хэш при
#   КАЖДОМ чтении и отказывает файлу целиком при расхождении.
# - Секция HOUS входит в хэш, ТОЛЬКО если непуста (иначе переверсировались бы
#   все 2500+ файлов полок).
"""The .dfo object container in Python: same bytes as ObjectRegistry.cpp."""

import struct
import sys

import numpy as np

FNV1A64_OFFSET_BASIS = 14695981039346656037
FNV1A64_PRIME = 1099511628211
MASK64 = (1 << 64) - 1

def _tag(text: str) -> int:
    """make_tag('D','F','N','O') — четыре ASCII в u32, младший байт первый."""
    a, b, c, d = text.encode("ascii")
    return a | (b << 8) | (c << 16) | (d << 24)

OBJECT_MAGIC = _tag("DFNO")
OBJECT_FORMAT_VERSION = 3
SECTION_VERSION = 1
SEC_INFO, SEC_WOOD = _tag("INFO"), _tag("WOOD")
SEC_CARD, SEC_GRND = _tag("CARD"), _tag("GRND")
SEC_BARK, SEC_HOUS = _tag("BARK"), _tag("HOUS")

# Порядок потоков в файле И в хэше. Один список на оба, чтобы они не разошлись.
STREAM_ORDER = (("wood", SEC_WOOD), ("cards", SEC_CARD),
                ("ground", SEC_GRND), ("bark", SEC_BARK))

class Fnv1a64:
    """Потоковая FNV-1a 64. update_u64 скармливает 8 байт little-endian."""

    def __init__(self) -> None:
        self.state = FNV1A64_OFFSET_BASIS

    def update(self, data: bytes) -> None:
        state = self.state
        for byte in data:
            state = ((state ^ byte) * FNV1A64_PRIME) & MASK64
        self.state = state

    def update_u64(self, value: int) -> None:
        self.update(struct.pack("<Q", value & MASK64))

    def digest(self) -> int:
        return self.state

class Mesh:
    """Поток .dfo: вершины (pos, normal, uv, colour) и индексы треугольников.

    Вершины держим массивами numpy, а не списком объектов: герб — это десятки
    тысяч вершин, и список питоньих объектов на них стоил бы секунд на ровном
    месте.
    """

    def __init__(self) -> None:
        self.positions = np.zeros((0, 3), dtype=np.float32)
        self.normals = np.zeros((0, 3), dtype=np.float32)
        self.uvs = np.zeros((0, 2), dtype=np.float32)
        self.colors = np.zeros((0,), dtype=np.uint32)
        self.indices = np.zeros((0,), dtype=np.uint32)

    def __len__(self) -> int:
        return int(self.positions.shape[0])

    @property
    def triangle_count(self) -> int:
        return int(self.indices.size // 3)

    def vertex_bytes(self) -> bytes:
        """36 байт на вершину в порядке полей формата, одним numpy-склеиванием."""
        count = len(self)
        buf = np.empty((count, 9), dtype=np.uint32)
        buf[:, 0:3] = self.positions.astype("<f4").view(np.uint32)
        buf[:, 3:6] = self.normals.astype("<f4").view(np.uint32)
        buf[:, 6:8] = self.uvs.astype("<f4").view(np.uint32)
        buf[:, 8] = self.colors
        return buf.astype("<u4").tobytes()

def pack_color(rgb) -> int:
    """Линейный 0..1 -> 0xAABBGGRR, как render::pack() (альфа всегда 255)."""
    r, g, b = (int(round(max(0.0, min(1.0, c)) * 255.0)) for c in rgb)
    return (0xFF << 24) | (b << 16) | (g << 8) | r

class RegistryObject:
    def __init__(self, name: str = "", kind: str = "", source: str = "") -> None:
        self.name, self.kind, self.source = name, kind, source
        self.wood = Mesh()
        self.cards = Mesh()
        self.ground = Mesh()
        self.bark = Mesh()
        self.house = []  # (surface, tone, emissive, Mesh)

def _hash_stream(hasher: Fnv1a64, mesh: Mesh) -> None:
    """Как hash_stream в ObjectRegistry.cpp: БИТЫ float, не значения."""
    hasher.update_u64(len(mesh))
    if len(mesh):
        # Те же 9 слов на вершину, что уходят в файл, — по одному update_u64.
        words = np.frombuffer(mesh.vertex_bytes(), dtype="<u4")
        for word in words.tolist():
            hasher.update_u64(word)
    hasher.update_u64(int(mesh.indices.size))
    for index in mesh.indices.tolist():
        hasher.update_u64(index)

def content_hash(obj: RegistryObject) -> int:
    hasher = Fnv1a64()
    for attr, _tagv in STREAM_ORDER:
        _hash_stream(hasher, getattr(obj, attr))
    if obj.house:
        hasher.update_u64(len(obj.house))
        for surface, tone, emissive, mesh in obj.house:
            hasher.update_u64(surface)
            hasher.update_u64(tone)
            hasher.update_u64(1 if emissive else 0)
            _hash_stream(hasher, mesh)
    return hasher.digest()

def content_hash_v1(obj: RegistryObject) -> int:
    """Личность формата v1: БЕЗ потока bark. Версия — обещание о том, как читать."""
    hasher = Fnv1a64()
    for attr in ("wood", "cards", "ground"):
        _hash_stream(hasher, getattr(obj, attr))
    return hasher.digest()

class _Writer:
    def __init__(self) -> None:
        self.buf = bytearray()
        self.section_length_offset = 0

    def begin_file(self, magic: int, version: int) -> None:
        self.buf += struct.pack("<II", magic, version)

    def begin_section(self, tag: int, version: int) -> None:
        self.buf += struct.pack("<IH", tag, version)
        self.section_length_offset = len(self.buf)
        self.buf += struct.pack("<Q", 0)

    def end_section(self) -> None:
        start = self.section_length_offset + 8
        length = len(self.buf) - start
        self.buf[self.section_length_offset:start] = struct.pack("<Q", length)

    def u32(self, value: int) -> None:
        self.buf += struct.pack("<I", value)

    def u64(self, value: int) -> None:
        self.buf += struct.pack("<Q", value)

    def string(self, text: str) -> None:
        raw = text.encode("utf-8")
        self.buf += struct.pack("<I", len(raw)) + raw

def _write_stream_body(writer: _Writer, mesh: Mesh) -> None:
    writer.u32(len(mesh))
    writer.buf += mesh.vertex_bytes()
    writer.u32(int(mesh.indices.size))
    writer.buf += mesh.indices.astype("<u4").tobytes()

def serialize(obj: RegistryObject, container_version: int = OBJECT_FORMAT_VERSION) -> bytes:
    """container_version переопределяется ТОЛЬКО сверкой (--verify): чтобы
    сравнить наши байты с файлом, испечённым движком версии v2, надо писать ту
    же версию. Запечка герба всегда пишет текущую."""
    writer = _Writer()
    writer.begin_file(OBJECT_MAGIC, container_version)
    writer.begin_section(SEC_INFO, SECTION_VERSION)
    writer.string(obj.name)
    writer.string(obj.kind)
    writer.string(obj.source)
    writer.u64(content_hash(obj))
    writer.end_section()
    for attr, tagv in STREAM_ORDER:
        writer.begin_section(tagv, SECTION_VERSION)
        _write_stream_body(writer, getattr(obj, attr))
        writer.end_section()
    if obj.house:
        writer.begin_section(SEC_HOUS, SECTION_VERSION)
        writer.u32(len(obj.house))
        for surface, tone, emissive, mesh in obj.house:
            writer.u32(surface)
            writer.u32(tone)
            writer.u32(1 if emissive else 0)
            _write_stream_body(writer, mesh)
        writer.end_section()
    return bytes(writer.buf)

def write_object(obj: RegistryObject, path: str) -> None:
    """Пишет .dfo атомарно (временный файл рядом + переименование)."""
    import os
    if not any(len(getattr(obj, a)) for a, _t in STREAM_ORDER) and not obj.house:
        raise ValueError("отказ писать объект без потоков — имя, указывающее в пустоту")
    tmp = path + ".tmp"
    with open(tmp, "wb") as handle:
        handle.write(serialize(obj))
    os.replace(tmp, path)

class _Reader:
    def __init__(self, data: bytes) -> None:
        self.data, self.pos = data, 0

    def u16(self) -> int:
        value = struct.unpack_from("<H", self.data, self.pos)[0]
        self.pos += 2
        return value

    def u32(self) -> int:
        value = struct.unpack_from("<I", self.data, self.pos)[0]
        self.pos += 4
        return value

    def u64(self) -> int:
        value = struct.unpack_from("<Q", self.data, self.pos)[0]
        self.pos += 8
        return value

    def string(self) -> str:
        length = self.u32()
        text = self.data[self.pos:self.pos + length].decode("utf-8")
        self.pos += length
        return text

def _read_stream(reader: _Reader) -> Mesh:
    mesh = Mesh()
    count = reader.u32()
    words = np.frombuffer(reader.data, dtype="<u4", count=count * 9,
                          offset=reader.pos).reshape(count, 9)
    reader.pos += count * 36
    mesh.positions = words[:, 0:3].copy().view(np.float32)
    mesh.normals = words[:, 3:6].copy().view(np.float32)
    mesh.uvs = words[:, 6:8].copy().view(np.float32)
    mesh.colors = words[:, 8].copy()
    index_count = reader.u32()
    mesh.indices = np.frombuffer(reader.data, dtype="<u4", count=index_count,
                                 offset=reader.pos).copy()
    reader.pos += index_count * 4
    return mesh

def read_object(path: str):
    """Читает .dfo и СВЕРЯЕТ хэш, как это делает движок. None при отказе."""
    data = open(path, "rb").read()
    reader = _Reader(data)
    if reader.u32() != OBJECT_MAGIC:
        return None
    version = reader.u32()
    if version > OBJECT_FORMAT_VERSION:
        return None
    obj = RegistryObject()
    stored = 0
    by_tag = {tagv: attr for attr, tagv in STREAM_ORDER}
    while reader.pos + 14 <= len(data):
        tagv = reader.u32()
        reader.u16()
        length = reader.u64()
        end = reader.pos + length
        if tagv == SEC_INFO:
            obj.name, obj.kind, obj.source = (reader.string(), reader.string(),
                                              reader.string())
            stored = reader.u64()
        elif tagv in by_tag:
            setattr(obj, by_tag[tagv], _read_stream(reader))
        elif tagv == SEC_HOUS:
            for _ in range(reader.u32()):
                surface, tone, emissive = reader.u32(), reader.u32(), reader.u32()
                obj.house.append((surface, tone, emissive != 0, _read_stream(reader)))
        reader.pos = end  # неизвестные разделы перешагиваются (правило 7)
    obj.container_version = version
    computed = content_hash(obj) if version >= 2 else content_hash_v1(obj)
    if computed != stored:
        sys.stderr.write("[dfo] %s: хэш не сошёлся (в файле %x, посчитан %x)\n"
                         % (path, stored, computed))
        return None
    obj.content_hash = computed
    return obj

def _verify(paths) -> int:
    """ДВУСТОРОННЯЯ СВЕРКА С ЧУЖИМИ БАЙТАМИ (см. шапку). Возврат — код выхода."""
    bad = 0
    for path in paths:
        obj = read_object(path)
        if obj is None:
            print("ОТКАЗ (хэш/формат): %s" % path)
            bad += 1
            continue
        # Пишем В ТОЙ ЖЕ версии контейнера, что стоит в файле: полки испечены
        # движком v2, и наш писатель, ставящий текущую v3, разошёлся бы с ними
        # ровно одним байтом на смещении 4 — что о верности полей не говорит
        # ничего. Сверяем то, что сверять осмысленно.
        again = serialize(obj, obj.container_version)
        original = open(path, "rb").read()
        if again != original:
            print("РАЗОШЛИСЬ БАЙТЫ (%d против %d): %s"
                  % (len(again), len(original), path))
            bad += 1
        else:
            print("ok  хэш %016x, %6d байт, %6d тр.  %s"
                  % (obj.content_hash, len(original),
                     sum(getattr(obj, a).triangle_count for a, _t in STREAM_ORDER),
                     path))
    return 1 if bad else 0

if __name__ == "__main__":
    import glob
    import os
    args = sys.argv[1:]
    if args and args[0] == "--verify":
        rest = args[1:]
        if not rest:
            repo = os.path.dirname(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__))))
            rest = sorted(glob.glob(os.path.join(repo, "assets/objects/*/*.dfo")))[:12]
        raise SystemExit(_verify(rest))
    print(__doc__)
