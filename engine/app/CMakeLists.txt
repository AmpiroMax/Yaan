#
# Created: 09:08:2026 - 00:50:00
# Last updated: 14:08:2026 - 17:36:02
# File: engine/app/CMakeLists.txt
#
# Responsibility:
# - dfn_app executable: the composition root linking every layer.
#
# Dependencies:
# - Uses: dfn_core, dfn_world, dfn_render, dfn_gameplay, platform backends.
# - Used by: root CMakeLists (conditional add_subdirectory).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned (Rule 25).
#
# UPD:
# - 09:08:2026 - 00:50:00: Created for stage 2 integration; guarded so partial
#                          trees (agents mid-stage) still configure and build.
# - 09:08:2026 - 15:07:13: Link dfn_physics (terrain collision helper) — the ferry now
# - 10:08:2026 - 02:42:46: Link dfn_anim (first-person body wiring).
#                          uses it instead of hand-filling TerrainDesc.
# - 10:08:2026 - 19:26:40: DebugOverlay.cpp в сборке; хэш коммита штампуется в бинарник (DFN_BUILD_COMMIT) — снимок состояния обязан называть сборку, иначе он невоспроизводим.
# - 10:08:2026 - 19:57:06: Хэш коммита штампуется во время СБОРКИ через tools/stamp_build_commit.cmake, с пометкой +dirty для незакоммиченного дерева.
# - 13:08:2026 - 20:41:07: HudScreen.cpp в сборке — прицел и будущий игровой интерфейс зоны ui.
# - 14:08:2026 - 16:11:00: EditorCamera.cpp в сборке — свободная камера режима AppMode::Editor.
# - 14:08:2026 - 16:50:36: MapCatalog.cpp в сборке — каталог карт браузера редактора (docs/MAP_LAYOUT.md).
# - 14:08:2026 - 17:36:02: ChatLog.cpp + TrajectoryRecord.cpp в сборке — ящик chat/<карта>.chat.jsonl (замечание человека + самодокументация демки, В28/O1), кольцевая телеметрия и запись/детерминированный повтор траектории (O3).

set(DFN_APP_DEPS
    dfn_core
    dfn_world
    dfn_render
    dfn_anim
    dfn_physics
    dfn_gameplay
    dfn_platform_window
    dfn_platform_input
    dfn_platform_render
    dfn_platform_physics
    dfn_platform_anim
    dfn_platform_audio
    dfn_platform_llm)

set(DFN_APP_READY TRUE)
foreach(dep IN LISTS DFN_APP_DEPS)
    if(NOT TARGET ${dep})
        set(DFN_APP_READY FALSE)
        message(STATUS "dfn_app: skipped (waiting for ${dep})")
        break()
    endif()
endforeach()

if(DFN_APP_READY)
    add_executable(dfn_app
        sources/Main.cpp
        sources/App.cpp
        sources/ChatLog.cpp
        sources/DebugOverlay.cpp
        sources/EditorCamera.cpp
        sources/HudScreen.cpp
        sources/Localization.cpp
        sources/MapCatalog.cpp
        sources/Menu.cpp
        sources/TrajectoryRecord.cpp)
    target_link_libraries(dfn_app PRIVATE dfn_headers ${DFN_APP_DEPS})

    # The commit the binary was built from, stamped into every state capture.
    # A bug report carries the build it was seen on or it is not reproducible.
    #
    # STAMPED AT BUILD TIME, NOT CONFIGURE TIME. The first version of this
    # resolved the hash during cmake configure, and sim's capture then named a
    # build TWO COMMITS STALE. That is worse than having no field at all: the
    # question it exists to answer is "was this taken before or after the fix",
    # and a wrong answer converts "I do not know which build this is" into a
    # confident false premise (Rule 34) that two agents can each reason
    # soundly from. The script also appends "+dirty" for uncommitted edits,
    # because a hash alone cannot describe the working tree that captures are
    # normally taken from.
    set(DFN_BUILDINFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/dfn_buildinfo")
    set(DFN_BUILDINFO_H "${DFN_BUILDINFO_DIR}/BuildInfo.h")
    file(MAKE_DIRECTORY "${DFN_BUILDINFO_DIR}")
    add_custom_target(dfn_buildinfo
        BYPRODUCTS "${DFN_BUILDINFO_H}"
        COMMAND ${CMAKE_COMMAND}
                -DOUT=${DFN_BUILDINFO_H}
                -DSRC=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_SOURCE_DIR}/tools/stamp_build_commit.cmake
        COMMENT "Stamping the build commit into BuildInfo.h"
        VERBATIM)
    add_dependencies(dfn_app dfn_buildinfo)
    target_include_directories(dfn_app PRIVATE "${DFN_BUILDINFO_DIR}")
endif()
