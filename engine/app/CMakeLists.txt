#
# Created: 09:08:2026 - 00:50:00
# Last updated: 10:08:2026 - 19:26:40
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
        sources/DebugOverlay.cpp
        sources/Localization.cpp
        sources/Menu.cpp)
    target_link_libraries(dfn_app PRIVATE dfn_headers ${DFN_APP_DEPS})

    # The commit the binary was built from, stamped into every state capture.
    # A bug report carries the build it was seen on or it is not reproducible:
    # "it looks wrong" against an unknown revision costs more than it reports.
    # Resolved at CONFIGURE time, so a capture can name a commit that the
    # working tree has since moved past -- which is the normal case and exactly
    # why the field exists. "unknown" when this is not a git checkout.
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE DFN_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT DFN_GIT_COMMIT)
        set(DFN_GIT_COMMIT "unknown")
    endif()
    target_compile_definitions(dfn_app
        PRIVATE DFN_BUILD_COMMIT="${DFN_GIT_COMMIT}")
endif()
