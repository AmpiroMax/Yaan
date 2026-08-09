#
# Created: 09:08:2026 - 00:45:00
# Last updated: 09:08:2026 - 00:45:00
# File: engine/render/CMakeLists.txt
#
# Responsibility:
# - dfn_render target: the engine render layer (camera, terrain mesher, render
#   system, tour, debug draw). Owned by the render agent (Rule 25, Q34).
#
# Dependencies:
# - Uses: dfn_headers (core + platform interfaces are header-only includes).
# - Used by: root CMakeLists (auto-included), engine/app link.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. This layer never links bgfx (Rule 1);
#   backends are linked by engine/app.
#
# UPD:
# - 09:08:2026 - 00:45:00: Stage 2 — implementation sources.

add_library(dfn_render STATIC
    sources/DebugDraw.cpp
    sources/FirstPersonCamera.cpp
    sources/RenderSystem.cpp
    sources/TerrainMesher.cpp
    sources/Tour.cpp)
target_link_libraries(dfn_render PUBLIC dfn_headers)
