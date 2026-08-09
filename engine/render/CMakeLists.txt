#
# Created: 09:08:2026 - 00:45:00
# Last updated: 09:08:2026 - 22:12:57
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
# - 09:08:2026 - 19:42:00: VoxelMesher.cpp (draw the carved voxel surface).
# - 09:08:2026 - 19:46:00: FloraSpecies.cpp + ProcFlora.cpp (flora agent's zone,
#   same directory, registered here per Q34).
# - 09:08:2026 - 20:10:11: WindModel.cpp (one wind for the world).
# - 09:08:2026 - 20:21:13: FloraCards.cpp (flora agent's alpha-cutout leaf mask
# - 09:08:2026 - 20:58:00: TerrainLod.cpp (LOD node ladder, selection and the
#   two-level fade window).
# - 09:08:2026 - 22:12:57: LodTerrain.cpp (the LOD DRAWING half: node meshes,
#   skirts, cross-faded submission) + RenderSystemResources.cpp (RenderSystem
#   split at the 800-line hard limit, Rule 21).
#   atlas + card emitter). EDITED BY THE FLORA AGENT under an explicit
#   lead-granted Rule 25 exception (render's session had ended and the leaf-card
#   stage could not wait); bounded to registration only — no shader, material,
#   wind or backend change. Not a precedent.

add_library(dfn_render STATIC
    sources/DebugDraw.cpp
    sources/FirstPersonCamera.cpp
    sources/FloraCards.cpp
    sources/FloraSpecies.cpp
    sources/LodTerrain.cpp
    sources/MapScreen.cpp
    sources/PixelCanvas.cpp
    sources/ProcFlora.cpp
    sources/ProcMesh.cpp
    sources/ProcTexture.cpp
    sources/RenderSystem.cpp
    sources/RenderSystemResources.cpp
    sources/ScatterBatcher.cpp
    sources/SkyModel.cpp
    sources/TerrainLod.cpp
    sources/TerrainMesher.cpp
    sources/WindModel.cpp
    sources/VoxelMesher.cpp
    sources/Tour.cpp
    sources/WaterMesher.cpp)
target_link_libraries(dfn_render PUBLIC dfn_headers)
