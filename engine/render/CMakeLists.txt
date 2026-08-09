#
# Created: 09:08:2026 - 00:45:00
# Last updated: 09:08:2026 - 21:00:00
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
# - 09:08:2026 - 11:05:00: Stage 3 — ProcTexture.cpp (procedural textures).
# - 09:08:2026 - 11:57:20: Stage 3b — ProcMesh, ScatterBatcher, WaterMesher.
# - 09:08:2026 - 17:32:10: Map screen — PixelCanvas (first UI primitive) and
#   MapScreen (explored top-down map).
# - 09:08:2026 - 19:26:00: SkyModel.cpp (day/night sun+moon model).
# - 09:08:2026 - 21:00:00: VoxelMesher.cpp (draw the carved voxel surface).

add_library(dfn_render STATIC
    sources/DebugDraw.cpp
    sources/FirstPersonCamera.cpp
    sources/MapScreen.cpp
    sources/PixelCanvas.cpp
    sources/ProcMesh.cpp
    sources/ProcTexture.cpp
    sources/RenderSystem.cpp
    sources/ScatterBatcher.cpp
    sources/SkyModel.cpp
    sources/TerrainMesher.cpp
    sources/VoxelMesher.cpp
    sources/Tour.cpp
    sources/WaterMesher.cpp)
target_link_libraries(dfn_render PUBLIC dfn_headers)
