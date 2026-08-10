#
# Created: 09:08:2026 - 00:35:09
# Last updated: 10:08:2026 - 21:05:04
# File: tests/CMakeLists.txt
#
# Responsibility:
# - Test scaffold: doctest main library + the add_dfn_test() helper. Each zone
#   registers its tests in its own tests/<zone>.cmake (included conditionally),
#   so parallel agents never edit this file.
#
# Dependencies:
# - Uses: doctest (root FetchContent), dfn_headers.
# - Used by: tests/core.cmake, tests/render.cmake, tests/sim.cmake (zone-owned).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned; register tests via your
#   zone's tests/<zone>.cmake using add_dfn_test(name source libs...).
#
# UPD:
# - 09:08:2026 - 00:35:09: Created scaffold for stage 2.
# - 09:08:2026 - 00:55:00: doctest include marked SYSTEM (silences __COUNTER__
#                          pedantic warnings in every zone's test TUs — core's
#                          report, note 2); enable_testing moved to root.
# - 10:08:2026 - 01:52:48: character zone added to the test loop (new Rule 25 zone).
# - 10:08:2026 - 19:26:40: Зона app в цикле тестов: у лида появился код, который можно проверять отдельно от окна.
# - 10:08:2026 - 21:05:04: Ворота DAG в ctest: архитектура утверждала, что слои держит CMake, а он не держал.

add_library(dfn_doctest_main STATIC DoctestMain.cpp)
target_include_directories(dfn_doctest_main SYSTEM PUBLIC
    $<TARGET_PROPERTY:doctest::doctest,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(dfn_doctest_main PUBLIC doctest::doctest dfn_headers)

# add_dfn_test(<name> <source> [extra libs...])
function(add_dfn_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE dfn_doctest_main ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# THE DAG GATE. ARCHITECTURE.md claimed CMake enforced the layering and it did
# not -- the repo root sits on every target's include path, so dfn_core could
# include RenderSystem.h and compile. The audit measured the graph clean first
# (777 engine includes, 0 violations), which is the only moment a gate like this
# is cheap: enforcement that lands GREEN can never be argued down later as
# disruptive. Its control is verified -- injecting a render include into
# core/ecs/World.h fails it.
add_test(NAME dag_contract
         COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/dag_check.py"
                 --root "${CMAKE_SOURCE_DIR}")

foreach(zone core render sim character app)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${zone}.cmake")
        include(${zone}.cmake)
    endif()
endforeach()
