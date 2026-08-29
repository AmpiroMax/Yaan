#!/usr/bin/env python3
#
# Module: tools
# File: tools/heraldry/png_io.py
#
# Responsibility:
# - ЧТЕНИЕ И ЗАПИСЬ PNG НА ОДНОЙ СТАНДАРТНОЙ БИБЛИОТЕКЕ (zlib + struct), без
#   Pillow. Нужно генератору герба (tools/gen_heraldry.py): он читает силуэт
#   дуба и пишет кадры превью, а Pillow в этой системе не установлен ни в один
#   из семи питонов (проверено 27.08).
#
# ПОЧЕМУ СВОЙ ЧИТАТЕЛЬ, А НЕ «поставьте Pillow». Офлайн-инструмент, который
# нельзя запустить на машине владельца без установки пакета, — это инструмент,
# который не запустят. PNG без чересстрочности — это zlib-поток и пять фильтров
# строки; всё вместе это сотня строк, и она не устаревает. Ровно тем же доводом
# в рантайме живёт свой engine/app/sources/PngImage.h.
#
# ЧТО НЕ ПОДДЕРЖИВАЕТСЯ НАРОЧНО: чересстрочность Adam7, битовая глубина 16 и
# палитра с tRNS. Наши активы — 8-битные RGBA/RGB/серые без чересстрочности
# (проверено на assets/branding), а читатель, который делает вид, что понимает
# формат, и молча отдаёт мусор, хуже читателя, который отказывается.
#
# Dependencies:
# - Uses: стандартная библиотека (zlib, struct), numpy.
# - Used by: tools/gen_heraldry.py.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - Это ОФЛАЙН-инструмент. Рантайм читает PNG своим PngImage.h; второй читатель
#   в engine/ заводить нельзя.
"""PNG 8-bit non-interlaced reader/writer on the standard library alone."""

import struct
import zlib

import numpy as np

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"

# Каналов на пиксель по типу цвета PNG (IHDR colour type).
_CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}

def _unfilter(raw: bytes, width: int, height: int, channels: int) -> np.ndarray:
    """Снимает пять фильтров строки PNG. Возвращает (h, w, channels) uint8.

    Фильтры 1/3/4 (Sub, Average, Paeth) ссылаются на ЛЕВОГО соседа В УЖЕ
    ВОССТАНОВЛЕННОЙ строке, поэтому векторизовать их по строке нельзя — это
    последовательная рекуррента. Строки при этом независимы, и обход идёт по
    байтам только внутри строки: для наших 652x718 это доли секунды.
    """
    stride = width * channels
    out = np.zeros((height, stride), dtype=np.uint8)
    prev = np.zeros(stride, dtype=np.uint8)
    pos = 0
    bpp = channels
    for y in range(height):
        ftype = raw[pos]
        pos += 1
        cur = bytearray(raw[pos:pos + stride])
        pos += stride
        if ftype == 0:
            pass
        elif ftype == 1:
            for i in range(bpp, stride):
                cur[i] = (cur[i] + cur[i - bpp]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                cur[i] = (cur[i] + int(prev[i])) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = cur[i - bpp] if i >= bpp else 0
                cur[i] = (cur[i] + ((left + int(prev[i])) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = cur[i - bpp] if i >= bpp else 0
                c = int(prev[i - bpp]) if i >= bpp else 0
                b = int(prev[i])
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                cur[i] = (cur[i] + pred) & 0xFF
        else:
            raise ValueError("PNG: неизвестный фильтр строки %d" % ftype)
        row = np.frombuffer(bytes(cur), dtype=np.uint8)
        out[y] = row
        prev = row
    return out.reshape(height, width, channels)

def read_png(path: str) -> np.ndarray:
    """Читает PNG и отдаёт RGBA uint8 (h, w, 4). Расширяет серый/RGB до RGBA."""
    data = open(path, "rb").read()
    if data[:8] != PNG_MAGIC:
        raise ValueError("%s: не PNG" % path)
    off = 8
    idat = []
    width = height = depth = ctype = 0
    palette = None
    while off + 8 <= len(data):
        length = struct.unpack(">I", data[off:off + 4])[0]
        ctag = data[off + 4:off + 8]
        payload = data[off + 8:off + 8 + length]
        if ctag == b"IHDR":
            width, height, depth, ctype, _comp, _filt, interlace = struct.unpack(
                ">IIBBBBB", payload)
            if interlace != 0:
                raise ValueError("%s: чересстрочный PNG не поддержан" % path)
            if depth != 8:
                raise ValueError("%s: битовая глубина %d, поддержана только 8"
                                 % (path, depth))
        elif ctag == b"PLTE":
            palette = np.frombuffer(payload, dtype=np.uint8).reshape(-1, 3)
        elif ctag == b"IDAT":
            idat.append(payload)
        elif ctag == b"IEND":
            break
        off += 12 + length
    channels = _CHANNELS[ctype]
    pixels = _unfilter(zlib.decompress(b"".join(idat)), width, height, channels)
    rgba = np.empty((height, width, 4), dtype=np.uint8)
    if ctype == 0:      # grey
        rgba[..., :3] = pixels[..., :1]
        rgba[..., 3] = 255
    elif ctype == 2:    # RGB
        rgba[..., :3] = pixels
        rgba[..., 3] = 255
    elif ctype == 3:    # palette
        if palette is None:
            raise ValueError("%s: палитровый PNG без PLTE" % path)
        rgba[..., :3] = palette[pixels[..., 0]]
        rgba[..., 3] = 255
    elif ctype == 4:    # grey + alpha
        rgba[..., :3] = pixels[..., :1]
        rgba[..., 3] = pixels[..., 1]
    else:               # RGBA
        rgba[...] = pixels
    return rgba

def write_png(path: str, rgba: np.ndarray) -> None:
    """Пишет RGBA8 (h, w, 4) как PNG. Фильтр строки 0 — размер тут не главное."""
    rgba = np.ascontiguousarray(rgba.astype(np.uint8))
    height, width = rgba.shape[:2]
    stride = width * 4
    # Байт фильтра (0) перед каждой строкой — одним numpy-склеиванием.
    body = np.zeros((height, stride + 1), dtype=np.uint8)
    body[:, 1:] = rgba.reshape(height, stride)
    raw = body.tobytes()

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    out = (PNG_MAGIC + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    with open(path, "wb") as handle:
        handle.write(out)
