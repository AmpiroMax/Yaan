#
# File: engine/physics/CMakeLists.txt
#
# Responsibility:
# - dfn_physics static library: engine-level physics on top of the IPhysics
#   interface — collision layer semantics, heightfield -> terrain conversion.
#   No third-party dependencies (Rule 1). Zone-owned by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers (interfaces + core headers).
# - Used by: engine/gameplay, engine/app, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. This layer never includes backend
#   headers — IPhysics only.

add_library(dfn_physics STATIC
    sources/TerrainCollision.cpp)

target_link_libraries(dfn_physics PUBLIC dfn_headers)
