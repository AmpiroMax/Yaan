#
# File: engine/core/CMakeLists.txt
#
# Responsibility:
# - dfn_core static library: ECS, math, time, events, types, serialization,
#   config. Zone-owned (core agent, Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers (root), C++ std + glm only (Rule 1).
# - Used by: dfn_world, engine layers above, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. No third-party libraries here beyond
#   glm via dfn_headers (Rule 2).

add_library(dfn_core STATIC
    ecs/sources/World.cpp
    materials/sources/MaterialRegistry.cpp
    events/sources/EventBus.cpp
    math/sources/Aabb.cpp
    math/sources/Frustum.cpp
    math/sources/Intersect.cpp
    math/sources/Ray.cpp
    serialization/sources/BinaryReader.cpp
    serialization/sources/BinaryWriter.cpp
    serialization/sources/ContentHash.cpp
    serialization/sources/Json.cpp
    serialization/sources/Sha256.cpp
    skeleton/sources/Skeleton.cpp
    time/sources/Clock.cpp
    time/sources/FixedTimestep.cpp
    types/sources/TypeId.cpp)

target_link_libraries(dfn_core PUBLIC dfn_headers)
