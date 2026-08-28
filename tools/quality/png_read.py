"""
Created: 28:08:2026 - 12:00:00
Last updated: 28:08:2026 - 16:10:00
Module: tools/quality
File: tools/quality/png_read.py

Responsibility:
- ДЕКОДЕР PNG НА ОДНОМ zlib: IHDR/PLTE/IDAT, глубина 8, все пять фильтров
  строки, вывод рядами RGB. Нужен приборам приёмки, которые читают кадры
  владельца и наши, а ставить ради этого стороннюю библиотеку агентам нельзя
  (правило 24).

Key items:
- load(path) -> (ширина, высота, ряды [(r, g, b), ...]).

ЧТО НЕ УМЕЕТ, И ЭТО НАРОЧНО: чересстрочный Adam7 и глубину 16 — отвергает
утверждением, а не молча. Кадр, прочитанный НЕ ТАК, дал бы прибору числа,
которые он честно посчитает и честно ошибётся.

Dependencies:
- Uses: zlib, struct (стандартная библиотека).
- Used by: tools/quality/measure_surface.py, отчёты волн материалов.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ТОТ ЖЕ ДЕКОДЕР, ЧТО В ТЕСТАХ PngImage: числа приёмки обязаны совпадать с
  тем, что видит движок, а не с тем, что видит соседняя библиотека.
"""
# UPD:
# - 28:08:2026 - 12:00:00: Черновик ресёрчера (записка №3) — декодер для
#   таблицы двенадцати участков.
# - 28:08:2026 - 16:10:00: Переведён в штатный прибор волной фаски: шапка,
#   рукав через measure_surface.py.

import zlib, struct, sys

def load(path):
    d = open(path,'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    i, idat, pal, trns = 8, b'', None, None
    w=h=bd=ct=0
    while i < len(d):
        ln = struct.unpack('>I', d[i:i+4])[0]; typ = d[i+4:i+8]; body = d[i+8:i+8+ln]
        if typ==b'IHDR':
            w,h,bd,ct,comp,filt,inter = struct.unpack('>IIBBBBB', body)
            assert bd==8 and inter==0, (bd,inter,path)
        elif typ==b'PLTE': pal=body
        elif typ==b'IDAT': idat += body
        elif typ==b'IEND': break
        i += 12+ln
    raw = zlib.decompress(idat)
    nch = {0:1,2:3,3:1,4:2,6:4}[ct]
    stride = w*nch
    out = bytearray(h*stride)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        f = raw[pos]; pos+=1
        line = bytearray(raw[pos:pos+stride]); pos+=stride
        if f==1:
            for x in range(nch, stride): line[x]=(line[x]+line[x-nch])&255
        elif f==2:
            for x in range(stride): line[x]=(line[x]+prev[x])&255
        elif f==3:
            for x in range(stride):
                a = line[x-nch] if x>=nch else 0
                line[x]=(line[x]+((a+prev[x])>>1))&255
        elif f==4:
            for x in range(stride):
                a = line[x-nch] if x>=nch else 0
                b = prev[x]; c = prev[x-nch] if x>=nch else 0
                p = a+b-c; pa,pb,pc = abs(p-a),abs(p-b),abs(p-c)
                pr = a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[x]=(line[x]+pr)&255
        out[y*stride:(y+1)*stride]=line
        prev = line
    # -> RGB rows
    rgb=[]
    for y in range(h):
        row=[]
        base=y*stride
        for x in range(w):
            o=base+x*nch
            if ct==3:
                p=out[o]*3; row.append((pal[p],pal[p+1],pal[p+2]))
            elif ct in (0,4):
                v=out[o]; row.append((v,v,v))
            else:
                row.append((out[o],out[o+1],out[o+2]))
        rgb.append(row)
    return w,h,rgb
