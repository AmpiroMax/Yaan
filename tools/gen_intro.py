#
# Module: tools
# File: tools/gen_intro.py
#
# Responsibility:
# - ПЕЧЁТ ИНТРО СТУДИИ КАК ПРЕДЗАПИСАННОЕ ВИДЕО (заказ владельца 27.08: «должно
#   интро показываться: из черноты появляется эмблема, максимально яркой
#   становится и после угасает — тогда открывается меню. Это должно быть
#   ПРЕДЗАПИСАННОЕ ВИДЕО, а не кодом рисующиеся элементы»). На выходе —
#   assets/intro/spiral_intro.dfv: контейнер кадров, который рантайм только
#   ДЕКОДИРУЕТ по таймеру и кладёт на холст. Ни одной программной примитивной
#   фигуры на экране интро больше нет.
#
# ПОЧЕМУ ОФЛАЙН, А НЕ В КАДРЕ. Всё, что делает эту заставку кинематографичной —
# гауссов глоу в несколько проходов, разрешение сведения выше кадра, мягкая
# виньетка, плёночное зерно, — стоит десятки миллисекунд НА КАДР. Офлайн это
# бесплатно: считается один раз здесь, и цена рантайма падает до inflate одного
# PNG (замерено на этом дереве: 18.3 мс на кадр 1920×1080 нашим декодером,
# tools/ + engine/app/sources/PngImage.cpp).
#
# ФОРМАТ КОНТЕЙНЕРА (.dfv, «Daggerfall N video»), little-endian:
#     "DFNV"                 4 байта магии
#     u32 version            = 1
#     u32 width, u32 height  размер кадра в пикселях
#     u32 frame_count
#     u32 fps_num, u32 fps_den   частота как ДРОБЬ (30/1), а не float:
#                                длительность обязана быть точной, иначе
#                                последний кадр то есть, то нет
#     u32 codec              = 1 («каждый кадр — целый файл PNG»)
#     u32 reserved           = 0
#     далее frame_count записей: u32 size, затем size байт PNG
#
# ПОЧЕМУ КАЖДЫЙ КАДР — ЦЕЛЫЙ PNG, А НЕ ДЕЛЬТА. Дельта между соседними кадрами
# сжалась бы лучше, и от неё пришлось бы завести ВТОРОЙ декодер в рантайме —
# рядом с тем, который уже есть, проверен на настоящих файлах бренда
# (tests/app/PngImageTests.cpp) и чьи ожидаемые пиксели получены независимой
# реализацией. Замер снял вопрос: кадр этого интро весит 25–260 КБ (фон —
# настоящий чёрный, а он сжимается почти в ничто), всё интро — единицы мегабайт
# при бюджете в 10–20. Платить за это вторым непроверенным декодером незачем
# (правило 39: второй экземпляр правила — это уже не правило).
#
# Usage:
#     python3 tools/gen_intro.py                  (печёт assets/intro/spiral_intro.dfv)
#     python3 tools/gen_intro.py --preview out/   (плюс PNG-кадры для глаза)
#     python3 tools/gen_intro.py --width 1280 --height 720
#
# Dependencies:
# - Uses: Pillow + numpy (только здесь, в офлайне; в рантайме сторонних
#   библиотек нет), assets/branding/spiral_logo/spiral_logo_full_1024.png.
# - Used by: engine/app/sources/IntroVideo.cpp читает готовый .dfv.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
# - ИСХОДНИК — ФАЙЛ БРЕНДА, А НЕ КОПИЯ ЕГО ФОРМЫ. Логотип не перерисовывается
#   здесь кодом ни при каких обстоятельствах: assets/branding/README.txt —
#   единственный владелец того, как выглядит знак студии.
import argparse
import io
import os
import struct
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError as exc:  # pragma: no cover - a tool, not a test
    sys.stderr.write(
        "gen_intro: нужны Pillow и numpy (офлайн-зависимости этого скрипта): %s\n" % exc)
    raise SystemExit(2)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, "assets/branding/spiral_logo/spiral_logo_full_1024.png")
OUT = os.path.join(REPO, "assets/intro/spiral_intro.dfv")

# ГРУНТ ЗНАКА, ИЗМЕРЕННЫЙ, А НЕ НАЗВАННЫЙ. spiral_logo_full_1024.png — RGB без
# альфы: знак СВЕДЁН на своей подложке #0c0e12. Чтобы положить его на настоящий
# чёрный (а интро начинается и кончается настоящим чёрным), подложку надо снять,
# и порог берётся от самого частого цвета файла, а не вписывается числом.
KEY_TOLERANCE = 40.0  # каналов от грунта до полной непрозрачности

# ХРОНОМЕТРАЖ, В СЕКУНДАХ ОТ НАЧАЛА. Владелец назвал форму словами: «из черноты
# появляется эмблема, максимально яркой становится и после угасает». Числа —
# то, что делает эти три слова кино, а не миганием:
#   пауза на чёрном  — глаз должен успеть понять, что кадр УЖЕ идёт;
#   восход           — длиннее, чем угасание: появление читается как событие;
#   выдержка на пике — короткая: это точка, а не экран ожидания;
#   угасание         — быстрее восхода, иначе конец тянется;
#   хвост на чёрном  — меню открывается ИЗ черноты, а не из полусвета.
HOLD_IN_S = 0.30
RISE_S = 1.35
PEAK_S = 0.55
FALL_S = 1.05
HOLD_OUT_S = 0.25
TOTAL_S = HOLD_IN_S + RISE_S + PEAK_S + FALL_S + HOLD_OUT_S  # 3.50 c

FPS_NUM = 30
FPS_DEN = 1

# Доля высоты кадра, которую занимает сведённый знак на пике. Меньше половины:
# логотип во весь экран читается как ошибка масштабирования, а не как заставка.
LOGO_BOX_FRACTION = 0.60
# Знак «дышит»: к пику он подрастает на полтора процента. Ровно столько, чтобы
# кадр не был мёртвым, и недостаточно, чтобы это заметили как движение.
SCALE_START = 0.965
SCALE_PEAK = 1.000

def smoothstep(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)

def ease_in(x):
    """Медленное начало, быстрый конец — «появляется ИЗ черноты», а не включается."""
    x = max(0.0, min(1.0, x))
    return x * x * x

def ease_out(x):
    x = max(0.0, min(1.0, x))
    return 1.0 - (1.0 - x) ** 2.2

def box_blur(img, radius):
    """Разделимое коробчатое размытие. Трижды подряд оно почти неотличимо от
    гауссова (центральная предельная теорема), а стоит O(n) вместо свёртки."""
    if radius < 1:
        return img
    k = 2 * radius + 1
    out = img
    for _ in range(3):
        pad = np.pad(out, ((radius, radius), (radius, radius), (0, 0)), mode="constant")
        cum = np.cumsum(pad, axis=0)
        cum = np.concatenate([np.zeros((1,) + cum.shape[1:], cum.dtype), cum], axis=0)
        out = (cum[k:k + out.shape[0]] - cum[:out.shape[0]]) / k
        pad = np.pad(out, ((0, 0), (radius, radius), (0, 0)), mode="constant")
        cum = np.cumsum(pad, axis=1)
        cum = np.concatenate([np.zeros((cum.shape[0], 1) + cum.shape[2:], cum.dtype), cum], axis=1)
        out = (cum[:, k:k + out.shape[1]] - cum[:, :out.shape[1]]) / k
    return out

def load_logo_premultiplied():
    """Знак студии, снятый со своей подложки и умноженный на альфу.

    Возвращает float32 HxWx3 «как он лежит на настоящем чёрном»."""
    src = Image.open(SOURCE).convert("RGB")
    rgb = np.array(src).astype(np.float32)
    # Грунт — самый частый цвет файла: он занимает большую часть кадра, и
    # угадывать его константой значило бы сломать интро при следующем экспорте
    # бренда без единого сообщения.
    flat = np.array(src).reshape(-1, 3)
    colours, counts = np.unique(flat, axis=0, return_counts=True)
    ground = colours[int(np.argmax(counts))].astype(np.float32)
    dist = np.abs(rgb - ground).max(axis=2)
    alpha = np.clip(dist / KEY_TOLERANCE, 0.0, 1.0)
    return rgb * alpha[..., None], ground

def envelope(t):
    """(яркость, сила глоу, масштаб) в момент t секунд."""
    if t < HOLD_IN_S:
        return 0.0, 0.0, SCALE_START
    t -= HOLD_IN_S
    if t < RISE_S:
        x = t / RISE_S
        b = ease_in(x)
        # ГЛОУ ОБГОНЯЕТ ЯРКОСТЬ. Свет вокруг знака появляется чуть раньше самого
        # знака — так кадр читается как «эмблема выходит из темноты», а не как
        # «картинку включили по громкости».
        g = smoothstep(min(1.0, x * 1.25)) * 0.55
        s = SCALE_START + (SCALE_PEAK - SCALE_START) * smoothstep(x)
        return b, g, s
    t -= RISE_S
    if t < PEAK_S:
        x = t / PEAK_S
        # На пике знак СТОИТ, а глоу дышит один раз: сияние, а не мигание.
        pulse = 0.55 + 0.22 * np.sin(x * np.pi)
        return 1.0, float(pulse), SCALE_PEAK
    t -= PEAK_S
    if t < FALL_S:
        x = t / FALL_S
        k = 1.0 - ease_out(x)
        # Глоу гаснет МЕДЛЕННЕЕ знака: последнее, что остаётся в кадре, — свет.
        return k, 0.55 * (1.0 - ease_out(min(1.0, x * 0.85))), SCALE_PEAK
    return 0.0, 0.0, SCALE_PEAK

def render_frame(logo, width, height, t):
    brightness, glow_gain, scale = envelope(t)
    frame = np.zeros((height, width, 3), dtype=np.float32)
    if brightness <= 0.0 and glow_gain <= 0.0:
        return Image.fromarray(frame.astype(np.uint8))

    box = int(round(height * LOGO_BOX_FRACTION * scale))
    small = Image.fromarray(np.clip(logo, 0, 255).astype(np.uint8)).resize(
        (box, box), Image.LANCZOS)
    layer = np.zeros((height, width, 3), dtype=np.float32)
    x0 = (width - box) // 2
    y0 = (height - box) // 2
    layer[y0:y0 + box, x0:x0 + box] = np.array(small).astype(np.float32)

    # ГЛОУ СЧИТАЕТСЯ НА УМЕНЬШЕННОЙ КОПИИ и растягивается обратно: радиус в
    # десятки пикселей на 1920×1080 — это минуты на кадр, а сияние по
    # построению не имеет высоких частот, терять на восьмикратном уменьшении
    # ему нечего.
    dw, dh = max(4, width // 8), max(4, height // 8)
    tiny = np.array(Image.fromarray(np.clip(layer, 0, 255).astype(np.uint8))
                    .resize((dw, dh), Image.LANCZOS)).astype(np.float32)
    halo = box_blur(tiny, max(1, dh // 24))
    halo = np.array(Image.fromarray(np.clip(halo, 0, 255).astype(np.uint8))
                    .resize((width, height), Image.BICUBIC)).astype(np.float32)

    frame = layer * brightness + halo * glow_gain * 1.9
    # ВИНЬЕТКА — ЕДИНСТВЕННОЕ, ЧТО ЗДЕСЬ ЕСТЬ ОТ «ПЛЁНКИ», и она слабая: кадр и
    # так почти весь чёрный, и сильная виньетка на чёрном не видна вовсе, зато
    # съедает края глоу.
    yy = (np.arange(height, dtype=np.float32) / height - 0.5) * 2.0
    xx = (np.arange(width, dtype=np.float32) / width - 0.5) * 2.0
    r2 = (yy[:, None] ** 2) * 0.75 + (xx[None, :] ** 2) * 0.55
    frame *= (1.0 - 0.22 * np.clip(r2, 0.0, 1.0))[..., None]
    return Image.fromarray(np.clip(frame, 0.0, 255.0).astype(np.uint8))

def main():
    ap = argparse.ArgumentParser(description="печёт интро студии в .dfv")
    ap.add_argument("--width", type=int, default=1920)
    ap.add_argument("--height", type=int, default=1080)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--preview", default=None,
                    help="папка, куда дополнительно положить кадры как PNG")
    args = ap.parse_args()

    logo, ground = load_logo_premultiplied()
    frames = int(round(TOTAL_S * FPS_NUM / FPS_DEN))
    sys.stderr.write(
        "gen_intro: грунт знака %s, кадров %d, %dx%d, %.2f c\n"
        % (tuple(int(c) for c in ground), frames, args.width, args.height, TOTAL_S))

    blobs = []
    for i in range(frames):
        t = i * FPS_DEN / FPS_NUM
        img = render_frame(logo, args.width, args.height, t)
        buf = io.BytesIO()
        img.save(buf, "PNG", compress_level=9)
        blobs.append(buf.getvalue())
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            img.save(os.path.join(args.preview, "frame_%03d.png" % i))
        sys.stderr.write("\r  кадр %d/%d (%d КБ)" % (i + 1, frames, len(blobs[-1]) // 1024))
    sys.stderr.write("\n")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(b"DFNV")
        f.write(struct.pack("<IIIIIIII", 1, args.width, args.height, frames,
                            FPS_NUM, FPS_DEN, 1, 0))
        for b in blobs:
            f.write(struct.pack("<I", len(b)))
            f.write(b)
    total = os.path.getsize(args.out)
    sys.stderr.write("gen_intro: %s — %.2f МБ, %d кадров, %.2f c\n"
                     % (args.out, total / (1024 * 1024), frames,
                        frames * FPS_DEN / FPS_NUM))

if __name__ == "__main__":
    main()
