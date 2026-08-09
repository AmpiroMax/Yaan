#
# Created: 09:08:2026 - 00:45:08
# Last updated: 10:08:2026 - 02:18:52
# File: engine/platform/anim/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_anim static library: IAnim backends. Stage 2 ships the null
#   backend only; the ozz backend (FetchContent, pinned 0.16.0) is a later
#   stage and will be added HERE. Zone-owned by character (Rule 25; carved
#   from sim 10:08:2026).
#
# Dependencies:
# - Uses: dfn_headers.
# - Used by: engine/app wiring, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Do not FetchContent ozz before the
#   stage-3 scope says so.
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — null backend only.
# - 10:08:2026 - 02:18:52: Zone handover: sim -> character (Rule 25).

add_library(dfn_platform_anim STATIC
    sources/null/NullAnim.cpp)

target_link_libraries(dfn_platform_anim PUBLIC dfn_headers)
