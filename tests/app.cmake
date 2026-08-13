#
# Created: 10:08:2026 - 19:24:11
# Last updated: 13:08:2026 - 23:06:40
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
# - 13:08:2026 - 23:06:40: app_menu и app_hud_screen в ctest (зона ui гоняла их руками). Тест ленты доказывает то, чего кадр не может: компас, едущий не в ту сторону, выглядит правильным на ЛЮБОМ снимке, поэтому знак проверяется поворотом в обе стороны.

if(TARGET dfn_render AND TARGET dfn_core)
    add_dfn_test(app_debug_overlay app/DebugOverlayTests.cpp dfn_render dfn_core)
    target_sources(app_debug_overlay PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    add_dfn_test(app_menu app/MenuTests.cpp dfn_render dfn_core)
    target_sources(app_menu PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Menu.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # The HUD's tests earn their place by proving what a FRAME CANNOT: a compass
    # ribbon running the wrong way looks right in any single screenshot, so the
    # sign is only provable by turning both ways and watching the marks move.
    add_dfn_test(app_hud_screen app/HudScreenTests.cpp dfn_render dfn_core)
    target_sources(app_hud_screen PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/HudScreen.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)
endif()
