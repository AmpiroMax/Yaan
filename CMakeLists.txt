#
# Created: 09:08:2026 - 00:35:09
# Last updated: 09:08:2026 - 00:35:09
# File: CMakeLists.txt
#
# Responsibility:
# - Root build: project setup, shared dependencies (glm, doctest), the
#   NUMBERS.md constants generation step, and conditional inclusion of every
#   layer's own CMakeLists (Q34: zones own their layer files; the root only
#   aggregates, so parallel agents never edit this file).
#
# Key items:
# - dfn_headers INTERFACE target: repo-root + generated include dirs, glm,
#   project warnings. Every dfn_* target links it.
# - Target naming convention: dfn_core, dfn_world, dfn_platform_<module>,
#   dfn_render, dfn_gameplay, dfn_app (documented here, used by all zones).
#
# Dependencies:
# - Uses: tools/gen_constants.py, docs/NUMBERS.md, FetchContent (pinned tags).
# - Used by: every layer CMakeLists via add_subdirectory.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. This file is LEAD-owned (Rule 25):
#   add targets in your OWN layer's CMakeLists.txt, never here.
#
# UPD:
# - 09:08:2026 - 00:35:09: Created for stage 2 (skeleton walk).

cmake_minimum_required(VERSION 3.28)
project(daggerfall_n CXX)

set(CMAKE_CXX_STANDARD 23)              # Rule 19: clang 22 primary; AppleClang 15
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # builds it as c++2b — guard 23-only libs.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clangd
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Generated constants (Q54): NUMBERS.md is the single source of truth -----
set(DFN_GENERATED_DIR "${CMAKE_BINARY_DIR}/dfn_generated")
execute_process(
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_constants.py"
            --numbers "${CMAKE_CURRENT_SOURCE_DIR}/docs/NUMBERS.md"
            --out "${DFN_GENERATED_DIR}/dfn_generated/Constants.h"
    RESULT_VARIABLE DFN_GEN_RESULT)
if(NOT DFN_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "gen_constants.py failed — fix docs/NUMBERS.md")
endif()
# Re-run configure when NUMBERS.md changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS docs/NUMBERS.md)

# --- Shared third-party (Rule 24: pinned, FetchContent only) -----------------
include(FetchContent)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11)
FetchContent_MakeAvailable(glm doctest)

# --- Common interface target -------------------------------------------------
add_library(dfn_headers INTERFACE)
target_include_directories(dfn_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"        # absolute-from-root includes (rules/structure.md)
    "${DFN_GENERATED_DIR}")
target_link_libraries(dfn_headers INTERFACE glm::glm)
target_compile_options(dfn_headers INTERFACE -Wall -Wextra -Wpedantic)

# --- Layers (zone-owned; included once their CMakeLists lands) ---------------
set(DFN_LAYERS
    engine/core
    engine/world
    engine/platform/window
    engine/platform/input
    engine/platform/render
    engine/platform/physics
    engine/platform/anim
    engine/platform/audio
    engine/platform/llm
    engine/render
    engine/physics
    engine/anim
    engine/gameplay
    engine/editor
    engine/app
    tests)
foreach(layer IN LISTS DFN_LAYERS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${layer}/CMakeLists.txt")
        add_subdirectory(${layer})
    endif()
endforeach()
