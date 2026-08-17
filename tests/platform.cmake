#
# Created: 18:08:2026 - 00:25:00
# Last updated: 18:08:2026 - 00:25:00
# File: tests/platform.cmake
#
# Responsibility:
# - Рукава слоя platform. Их здесь ровно один, и он единственный во всём дереве,
#   кто поднимает НАСТОЯЩЕЕ окно GLFW.
#
# Key items:
# - platform_cursor_capture: захват курсора не смеет съедать смещение мыши.
#
# Dependencies:
# - Uses: add_dfn_test() из tests/CMakeLists.txt; dfn_platform_input,
#   dfn_platform_window.
# - Used by: ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
#
# UPD:
# - 18:08:2026 - 00:25:00: Создан ради platform_cursor_capture.
#   ПОЧЕМУ ЗДЕСЬ ЖИВОЕ ОКНО, хотя правило 3 держит всё остальное на нулевом
#   бэкенде: проверяется САМ бэкенд GLFW, и подменять его нечем. Отказ,
#   который рукав держит, невидим ни для одного другого прибора в дереве —
#   App просит захват курсора КАЖДЫМ кадром, платформа считала каждый такой
#   вызов событием и сбрасывала «предыдущее положение известно», смещение
#   выходило нулевым всегда. Камера редактора не поворачивалась вовсе, и
#   пользователь три захода подряд писал об этом сам, потому что прибора не
#   было. Рукаву нужен дисплей; окна нет — он говорит вслух и выходит зелёным,
#   а не притворяется, будто проверил.
#

if(TARGET dfn_platform_input AND TARGET dfn_platform_window)
    add_dfn_test(platform_cursor_capture platform/CursorCaptureTests.cpp
                 dfn_platform_input dfn_platform_window dfn_core)
endif()
