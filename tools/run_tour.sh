#!/usr/bin/env bash
#
# File: tools/run_tour.sh
#
# Responsibility:
# - Run the screenshot tour ONCE at the default look (640x360, palette off).
#   Output: screenshots/. Pass "matrix" as $2 for the full resolution x palette
#   sweep — only when a look decision actually needs the comparison.
# - ON THE HOUSES DEMO MAP, not on the old generated world (see UPD 18.08).
#   Override with DFN_TOUR_MAP=<category>/<map> when a shot genuinely needs
#   another map.
#
# Dependencies:
# - Uses: dfn_app (build dir as $1, default build_lead), DFN_* env contract.
# - Used by: lead + render at visual verification (Rule 27); devlog screenshots.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - Do NOT re-add the 4-way default: near-identical frames are wasted work
#   (user instruction, 09.08.2026). One variant per verification run.

set -euo pipefail

build_dir="${1:-build_lead}"
mode="${2:-single}"
# КАРТА ТУРА. Умолчание — маленькая свежая карта домиков (решение пользователя
# 18.08: «там старая карта со старыми ассетами, её вообще запускать не надо;
# переделай скрипт демки так, чтобы она на карте с демкой домиков запускалась,
# она меньше и свежее»). Раньше карта не называлась ВОВСЕ, и тур поднимал
# сгенерированный мир по умолчанию — самый большой и самый старый из всего, что
# у нас есть, на каждый снимок.
tour_map="${DFN_TOUR_MAP:-houses/demo}"
app="$build_dir/engine/app/dfn_app"
[[ -x "$app" ]] || { echo "no dfn_app in $build_dir — build first" >&2; exit 1; }

shoot() {
    local res="$1" pal="$2" out="$3"
    mkdir -p "$out"
    echo "=== tour @ $res palette=$pal map=$tour_map -> $out"
    DFN_TOUR=1 DFN_TOUR_DIR="$out" DFN_INTERNAL_RES="$res" DFN_PALETTE="$pal" \
        DFN_OPEN_MAP="$tour_map" "$app"
}

if [[ "$mode" == "matrix" ]]; then
    for res in 640x360 320x180; do
        for pal in 0 1; do
            suffix=""
            [[ "$pal" == "1" ]] && suffix="_palette"
            shoot "$res" "$pal" "screenshots/${res}${suffix}"
        done
    done
else
    shoot 640x360 0 "screenshots"
    ls screenshots
fi
