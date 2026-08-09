#!/usr/bin/env bash
#
# Created: 09:08:2026 - 00:52:00
# Last updated: 09:08:2026 - 14:32:00
# File: tools/run_tour.sh
#
# Responsibility:
# - Run the screenshot tour ONCE at the default look (640x360, palette off).
#   Output: screenshots/. Pass "matrix" as $2 for the full resolution x palette
#   sweep — only when a look decision actually needs the comparison.
#
# Dependencies:
# - Uses: dfn_app (build dir as $1, default build_lead), DFN_* env contract.
# - Used by: lead + render at visual verification (Rule 27); devlog screenshots.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - Do NOT re-add the 4-way default: near-identical frames are wasted work
#   (user instruction, 09.08.2026). One variant per verification run.
#
# UPD:
# - 09:08:2026 - 00:52:00: Created for the stage-2 acceptance shoot.
# - 09:08:2026 - 10:48:00: Stage 3 — 4-way matrix: both resolutions x palette on/off.
# - 09:08:2026 - 14:32:00: Single-variant default (user: stop shooting near-identical
#                          frames); the 4-way sweep moved behind the "matrix" argument.

set -euo pipefail

build_dir="${1:-build_lead}"
mode="${2:-single}"
app="$build_dir/engine/app/dfn_app"
[[ -x "$app" ]] || { echo "no dfn_app in $build_dir — build first" >&2; exit 1; }

shoot() {
    local res="$1" pal="$2" out="$3"
    mkdir -p "$out"
    echo "=== tour @ $res palette=$pal -> $out"
    DFN_TOUR=1 DFN_TOUR_DIR="$out" DFN_INTERNAL_RES="$res" DFN_PALETTE="$pal" "$app"
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
