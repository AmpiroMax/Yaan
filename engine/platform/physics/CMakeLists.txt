#
# File: engine/platform/physics/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_physics static library: the IPhysics backends (jolt + null).
#   The ONLY target that fetches/links Jolt (Rule 1); the pinned tag lives here
#   (Rule 24). Zone-owned by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers, JoltPhysics v5.2.0 (FetchContent, pinned).
# - Used by: engine/app wiring, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Jolt types must never leak out of
#   sources/jolt/ (interface + factory headers stay Jolt-free).

include(FetchContent)

# Build only the Jolt library — no samples/tests/viewer (Rule 24: lean, pinned).
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
# Avoid LTO mismatches with our toolchain matrix (Rule 19).
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build)
FetchContent_MakeAvailable(JoltPhysics)

add_library(dfn_platform_physics STATIC
    sources/jolt/JoltPhysics.cpp
    sources/null/NullPhysics.cpp)

# PRIVATE Jolt: consumers see only the interface + factories (Rule 1). Jolt's
# PUBLIC compile definitions still reach our backend TU, keeping ABI flags in
# sync with the fetched library.
target_link_libraries(dfn_platform_physics
    PUBLIC dfn_headers
    PRIVATE Jolt)
