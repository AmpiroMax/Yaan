#
# Created: 09:08:2026 - 00:35:09
# Last updated: 10:08:2026 - 19:26:40
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

foreach(zone core render sim character app)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${zone}.cmake")
        include(${zone}.cmake)
    endif()
endforeach()
