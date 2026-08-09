#
# Created: 09:08:2026 - 00:42:03
# Last updated: 10:08:2026 - 01:58:00
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
#
# UPD:
# - 09:08:2026 - 00:42:03: Stage 2 — created dfn_core with ECS/time/events/math
#                          implementations (serialization IO deferred;
#                          ContentHash built for the determinism test).
# - 10:08:2026 - 01:58:00: JSON reader (serialization/Json) — tech-debt task 3.

add_library(dfn_core STATIC
    ecs/sources/World.cpp
    events/sources/EventBus.cpp
    math/sources/Aabb.cpp
    math/sources/Frustum.cpp
    math/sources/Intersect.cpp
    math/sources/Ray.cpp
    serialization/sources/ContentHash.cpp
    serialization/sources/Json.cpp
    time/sources/Clock.cpp
    time/sources/FixedTimestep.cpp
    types/sources/TypeId.cpp)

target_link_libraries(dfn_core PUBLIC dfn_headers)
