#!/usr/bin/env bash
#
# Created: 09:08:2026 - 00:52:00
# Last updated: 09:08:2026 - 10:48:00
# File: tools/run_tour.sh
#
# Responsibility:
# - Run the screenshot tour at both candidate internal resolutions (Q9) so the
#   user can pick the pixel look side by side. Output: screenshots/<res>/.
#
# Dependencies:
# - Uses: dfn_app (build dir as $1, default build_lead), DFN_* env contract.
# - Used by: lead at stage-2 integration; devlog screenshots.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
#
# UPD:
# - 09:08:2026 - 00:52:00: Created for the stage-2 acceptance shoot.
# - 09:08:2026 - 10:48:00: Stage 3 — 4-way matrix: both resolutions x palette on/off.

set -euo pipefail

build_dir="${1:-build_lead}"
app="$build_dir/engine/app/dfn_app"
[[ -x "$app" ]] || { echo "no dfn_app in $build_dir — build first" >&2; exit 1; }

for res in 640x360 320x180; do
    for pal in 0 1; do
        suffix=""
        [[ "$pal" == "1" ]] && suffix="_palette"
        out="screenshots/${res}${suffix}"
        mkdir -p "$out"
        echo "=== tour @ $res palette=$pal -> $out"
        DFN_TOUR=1 DFN_TOUR_DIR="$out" DFN_INTERNAL_RES="$res" DFN_PALETTE="$pal" "$app"
        ls "$out"
    done
done
