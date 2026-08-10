#
# Created: 10:08:2026 - 19:24:11
# Last updated: 10:08:2026 - 19:24:11
# File: tests/app.cmake
#
# Responsibility:
# - Registers the app (lead) zone's test executables. The app is an EXECUTABLE,
#   not a library, so its testable pieces are compiled into the test binary
#   directly rather than linked -- App.cpp itself owns a window and is not
#   testable, but DebugOverlay.cpp is pure data in / data out and is.
#
# Dependencies:
# - Uses: add_dfn_test() from tests/CMakeLists.txt; dfn_render, dfn_core.
# - Used by: tests/CMakeLists.txt (conditional include), ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned.
#
# UPD:
# - 10:08:2026 - 19:24:11: Created -- state capture round-trip and the compass,
#                          each with its control.

if(TARGET dfn_render AND TARGET dfn_core)
    add_dfn_test(app_debug_overlay app/DebugOverlayTests.cpp dfn_render dfn_core)
    target_sources(app_debug_overlay PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)
endif()
