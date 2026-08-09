#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 00:45:08
# File: engine/platform/audio/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_audio static library: IAudio backends. Stage 2 ships the null
#   backend only; the miniaudio backend (FetchContent, pinned 0.11.22) is
#   stage 3 and will be added HERE. Zone-owned by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers.
# - Used by: engine/app wiring, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Do not FetchContent miniaudio before
#   the stage-3 scope says so.
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — null backend only.

add_library(dfn_platform_audio STATIC
    sources/null/NullAudio.cpp)

target_link_libraries(dfn_platform_audio PUBLIC dfn_headers)
