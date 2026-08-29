#!/bin/zsh
#
# File: tools/shoot_tiers.sh
#
# Responsibility:
# - РЕЦЕПТ ПРИЁМОЧНЫХ КАДРОВ волны ярусов: четыре камеры смотровой площадки
#   trees/forest-v2, одна метка плеча, один прогон.
#
# Usage:
#     tools/shoot_tiers.sh <плечо> [<сборка>]
#   Плечо — только имя папки для кадров. Само плечо выбирается ГЕНЕРАТОРОМ:
#     python3 tools/gen_trees_v2.py <судья>              — ярусы (сдаётся)
#     python3 tools/gen_trees_v2.py <судья> --uniform    — КОНТРОЛЬ: тот же
#         счёт подлеска равномерным рассевом
#     python3 tools/gen_trees_v2.py <судья> --no-tiers   — «до»: один полог
#
# ЗАЧЕМ ФАЙЛОМ, А НЕ СТРОКОЙ В ОТЧЁТЕ. Пара кадров «до/после» есть довод
# только если камера у обоих ОДНА. Камера, переписанная руками во второй раз,
# — это два разных кадра одной подписи.
#
# КАДР ОБРЕЗАЕТСЯ: свободная камера живёт в редакторе, а редактор рисует поверх
# кадра свои панели (двери, которая их гасит, нет). Обрезка — та же для всех
# плеч, поэтому сравнение честное.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
set -e
ROOT=${0:a:h:h}
cd $ROOT
ARM=${1:?плечо: after | uniform | before}
BUILD=${2:-build_tiers}
OUT=/tmp/tiers-shots/$ARM
mkdir -p $OUT
shoot () {
  local nm=$1 cam=$2
  DFN_EDITOR=1 DFN_OPEN_MAP=trees/forest-v2 DFN_EDITOR_CAM="$cam" \
  DFN_HUD=0 DFN_INTERNAL_RES=1920x1080 DFN_TIME_OF_DAY=0.42 \
  DFN_CAPTURE_DIR=$OUT/$nm DFN_CAPTURE_AFTER_FRAMES=90 DFN_NULL_AUDIO=1 \
  ./$BUILD/engine/app/dfn_app >/tmp/tiers-shots/$ARM-$nm.log 2>&1 || true
  # Обрезка редакторской рамки: 1617x1286 из 2560x1440, смещение 143,154.
  sips -c 1286 1617 --cropOffset 154 143 $OUT/$nm/capture_000.png \
       --out /tmp/tiers-shots/$ARM-$nm.png >/dev/null 2>&1 || true
}
# КУРТИНА подлеска в буковой рощице (пара к --uniform).
shoot clump   "132,41.6,140,1.5708,-0.16"
# КУРТИНЫ и ковёр сверху: закон посева виден только с высоты.
shoot aerial  "150,58,142,1.5708,-0.85"
# ПРИЗЕМНЫЙ ЯРУС в упор: ковёр, камни в покрове, высокие акценты.
shoot floor   "128,40.2,55,1.5708,-0.34"
# ОПУШКА и ПОДРОСТ у тропы.
shoot sapling "118,37.4,91,1.5708,-0.12"
ls /tmp/tiers-shots/$ARM-*.png
