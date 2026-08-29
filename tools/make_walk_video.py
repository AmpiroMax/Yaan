#!/usr/bin/env python3
# Module: tools
# File: tools/make_walk_video.py
#
# Responsibility:
# - СБОРКА ВИДЕО ПРОХОДА (заказ 20.08: «запись экрана прохода... к видео
#   сохраняй и субтитры — позиции игрока, направление взгляда, дебаг»).
#   Берёт ленту rec_%05d.png + rec.log, снятые дверью DFN_RECORD_EVERY,
#   и собирает walkthrough.mp4 + walkthrough.srt (кадр = строка состояния).
#
# Usage: python3 tools/make_walk_video.py <dir_with_rec_pngs> <out_stem> [fps]

import os
import subprocess
import sys

def srt_time(t):
    ms = int(t * 1000)
    return f"{ms // 3600000:02d}:{ms % 3600000 // 60000:02d}:{ms % 60000 // 1000:02d},{ms % 1000:03d}"

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    src = sys.argv[1]
    out = sys.argv[2]
    fps = float(sys.argv[3]) if len(sys.argv) > 3 else 10.0

    log = os.path.join(src, "rec.log")
    lines = []
    if os.path.exists(log):
        with open(log, encoding="utf-8") as f:
            lines = [l.strip() for l in f if l.strip()]

    with open(out + ".srt", "w", encoding="utf-8") as f:
        for i, line in enumerate(lines):
            f.write(f"{i + 1}\n{srt_time(i / fps)} --> {srt_time((i + 1) / fps)}\n")
            # Строка лога как есть: позиция, взгляд, скорость, аллюр, опора.
            f.write(line + "\n\n")

    subprocess.check_call([
        "ffmpeg", "-y", "-framerate", str(fps),
        "-i", os.path.join(src, "rec_%05d.png"),
        "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "23",
        out + ".mp4",
    ])
    print(f"written {out}.mp4 ({len(lines)} строк субтитров в {out}.srt)")

if __name__ == "__main__":
    main()
