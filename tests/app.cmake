#
# Created: 10:08:2026 - 19:24:11
# Last updated: 17:08:2026 - 19:25:28
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
# - 14:08:2026 - 18:57:03: app_editor_hud — раскладка редакторского блока. Он мерит то, чего не мерил никто: два оверлея делили верхний левый угол и печатались друг сквозь друга, а проверить это было нечем, потому что блок собирался прямо в App.cpp (окно, не тестируется). Файл порезан ведущим зоне editor ровно на эту регистрацию.

# - 14:08:2026 - 19:22:10: app_controls — таблица привязок клавиш; Controls.cpp добавлен и в app_menu (страница управления рисуется из той же таблицы). Файл порезан ведущим зоне editor на эту регистрацию, как и в прошлый раз.
# - 17:08:2026 - 16:53:31: app_build_tool — рука строителя: зелёное/красное решает СУДЬЯ, а не
# - 17:08:2026 - 19:18:24: app_editor_palette — модель меню объектов (фасеты, фильтр, избранное).
# - 17:08:2026 - 19:25:28: пути меню объектов — engine/editor (переезд под исключение про ImGui).
#   двойник правил в редакторе; тест держит именно это свойство.
if(TARGET dfn_render AND TARGET dfn_core)
    add_dfn_test(app_debug_overlay app/DebugOverlayTests.cpp dfn_render dfn_core)
    target_sources(app_debug_overlay PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    add_dfn_test(app_menu app/MenuTests.cpp dfn_render dfn_core)
    target_sources(app_menu PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Controls.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Menu.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # THE EDITOR'S OVERLAY BLOCK, and it is a layout suite rather than a text
    # one. The defect it was written for -- the readout and the editor banner
    # printing through each other in the top-left corner -- was invisible to
    # every test here and obvious to anyone who launched the game, because the
    # block was composed inline in App.cpp, which owns a window and cannot be
    # instantiated. Extracting it is what made the overlap measurable.
    add_dfn_test(app_editor_hud app/EditorHudTests.cpp dfn_render dfn_core)
    target_sources(app_editor_hud PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/EditorHud.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # МЕНЮ ОБЪЕКТОВ. Полка 2411 деталей: рукав держит то, чего кадр не покажет —
    # что имя РАЗОБРАНО, а не угадано, что счётчик на фишке значит «сколько
    # останется, если нажму», и что размер из имени сходится с меркой меша.
    add_dfn_test(app_editor_palette app/EditorPaletteTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_editor_palette PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPalette.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPaletteState.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp)

    # THE BUILD HAND. Worth its own suite for the same reason EditorHud is:
    # the decision lives in a module rather than in App.cpp, so an instrument
    # can see it. The property it holds — the ghost is coloured by ITS OWN
    # findings — is invisible in a screenshot and obvious in a test.
    add_dfn_test(app_build_tool app/BuildToolTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_build_tool PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp)

    # THE BINDING TABLE, and this suite is the reason the controls screen is
    # worth more than a paragraph of documentation: it holds the table TOTAL and
    # UNAMBIGUOUS, so a key added to App.cpp without a row cannot be dispatched
    # and a row without a description cannot be drawn.
    add_dfn_test(app_controls app/ControlsTests.cpp dfn_render dfn_core)
    target_sources(app_controls PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Controls.cpp
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
