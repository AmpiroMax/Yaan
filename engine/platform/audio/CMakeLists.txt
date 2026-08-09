#
# Created: 09:08:2026 - 00:45:08
# Last updated: 10:08:2026 - 01:53:17
# File: engine/platform/audio/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_audio static library: IAudio backends (miniaudio + null).
#   The ONLY target that fetches miniaudio (Rule 1); the pinned tag lives here
#   (Rule 24). Zone-owned by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers, miniaudio 0.11.22 (FetchContent, pinned).
# - Used by: engine/app wiring, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. miniaudio types must never leak out
#   of sources/miniaudio/ (interface + factory headers stay miniaudio-free).
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — null backend only.
# - 10:08:2026 - 01:53:17: Stage 3 audio bring-up (landscape stage, в12):
#                          miniaudio backend, pinned 0.11.22 per the spec's
#                          FetchContent plan. Fetched as sources only (no
#                          add_subdirectory): the single header is the whole
#                          library, and building their subproject would add
#                          targets we do not use.
#

include(FetchContent)

# GIT_SHALLOW: the miniaudio history is ~250 MB while the pinned checkout is
# a few MB — a full clone stalled three zones' reconfigures for minutes.
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.22
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR does_not_exist_use_sources_only)
FetchContent_MakeAvailable(miniaudio)

add_library(dfn_platform_audio STATIC
    sources/miniaudio/MiniaudioAudio.cpp
    sources/null/NullAudio.cpp)

# PRIVATE include: consumers see only the interface + factories (Rule 1).
target_include_directories(dfn_platform_audio SYSTEM PRIVATE
    ${miniaudio_SOURCE_DIR})

target_link_libraries(dfn_platform_audio PUBLIC dfn_headers)
