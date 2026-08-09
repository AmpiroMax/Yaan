#
# Created: 09:08:2026 - 00:50:00
# Last updated: 09:08:2026 - 00:50:00
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

set(DFN_APP_DEPS
    dfn_core
    dfn_world
    dfn_render
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
        sources/App.cpp)
    target_link_libraries(dfn_app PRIVATE dfn_headers ${DFN_APP_DEPS})
endif()
