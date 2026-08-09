#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 00:45:08
# File: engine/gameplay/CMakeLists.txt
#
# Responsibility:
# - dfn_gameplay static library: player movement (ref core + World wrappers)
#   and the dice RNG. The rest of the gameplay contract (NpcAction executor,
#   stats, dialogue runtime) is headers-only in stage 2 by scope. Zone-owned
#   by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers, dfn_core (World implementation for the wrappers).
# - Used by: engine/app, games/daggerfall_n, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Zero third-party here (Rule 1);
#   platform access through interfaces passed as parameters only (Rule 9).
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — player movement + dice.

add_library(dfn_gameplay STATIC
    sources/Dice.cpp
    sources/PlayerMovement.cpp
    sources/PlayerMovementWorld.cpp)

target_link_libraries(dfn_gameplay PUBLIC dfn_headers dfn_core)
