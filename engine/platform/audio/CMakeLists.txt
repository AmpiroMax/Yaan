#
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
