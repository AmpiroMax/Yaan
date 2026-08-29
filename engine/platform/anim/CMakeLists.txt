#
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

add_library(dfn_platform_anim STATIC
    sources/null/NullAnim.cpp)

target_link_libraries(dfn_platform_anim PUBLIC dfn_headers)
