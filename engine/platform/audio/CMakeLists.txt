#
# Created: 09:08:2026 - 00:45:08
# Last updated: 27:08:2026 - 20:00:24
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
# - 27:08:2026 - 20:00:24: OGG VORBIS (owner's order via the music session — the title
#                          theme loops in the main menu). stb_vorbis v1.22 is
#                          compiled in ONE extra TU, StbVorbis.cpp, and it is
#                          taken from the miniaudio checkout we ALREADY pin
#                          (extras/stb_vorbis.c) rather than copied into
#                          third_party/: one pin, one version, one provenance,
#                          and no snapshot to drift when either side moves
#                          (Rule 24). Licence: MIT or public domain, at our
#                          choice — recorded in StbVorbis.cpp's header.
#                          The extras/ directory joins the include path for the
#                          same two files that already see miniaudio itself; it
#                          stays PRIVATE, so nothing outside this library can
#                          reach a Vorbis symbol (Rule 1).
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

# THE VORBIS DECODER MUST BE WHERE WE THINK IT IS. A pin that moved the file
# would otherwise show up as "the menu is silent" long after the reconfigure.
if(NOT EXISTS "${miniaudio_SOURCE_DIR}/extras/stb_vorbis.c")
    message(FATAL_ERROR
        "miniaudio ${miniaudio_SOURCE_DIR} has no extras/stb_vorbis.c — "
        "Ogg Vorbis decoding would silently vanish. Check the pinned tag.")
endif()

add_library(dfn_platform_audio STATIC
    sources/miniaudio/MiniaudioAudio.cpp
    sources/miniaudio/StbVorbis.cpp
    sources/null/NullAudio.cpp)

# PRIVATE include: consumers see only the interface + factories (Rule 1).
# extras/ is on the same footing: it is how the two miniaudio TUs find
# stb_vorbis.c, and nobody else in the tree may include it.
target_include_directories(dfn_platform_audio SYSTEM PRIVATE
    ${miniaudio_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}/extras)

target_link_libraries(dfn_platform_audio PUBLIC dfn_headers)
