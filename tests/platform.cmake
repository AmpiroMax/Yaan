#
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

if(TARGET dfn_platform_input AND TARGET dfn_platform_window)
    add_dfn_test(platform_cursor_capture platform/CursorCaptureTests.cpp
                 dfn_platform_input dfn_platform_window dfn_core)
endif()
