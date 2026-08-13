#
# Created: 09:08:2026 - 00:45:00
# Last updated: 13:08:2026 - 21:00:00
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
# - 09:08:2026 - 23:27:10: Registered the flora agent's three new translation
#   units (FloraSkeleton / FloraBuild / FloraNeighbours) at its request. The
#   last two are Rule 21 splits of ProcFlora.cpp (931 lines, over the hard
#   limit) rather than new features, and ProcFlora.h is unchanged, so nothing
#   crosses the render/flora seam. Registered by render, not by flora — the
#   file is render's under Rule 25 and the earlier exception said so itself.
# - 09:08:2026 - 23:32:07: BitmapFont.cpp — the project's first font.
# - 10:08:2026 - 03:14:00: CloudModel.cpp (W4 cloud drift off the shared wind).
# - 12:08:2026 - 00:52:40: sources/GroundTufts.cpp — the sparse near-field grass layer.
# - 13:08:2026 - 21:00:00: sources/FloraWeber.cpp — the Weber & Penn generator, at
#   flora's request and with the lead's leave (Rule 21: it does not fit in
#   FloraSkeleton.cpp, already over the 800-line limit).

add_library(dfn_render STATIC
    sources/BitmapFont.cpp
    sources/CloudModel.cpp
    sources/DebugDraw.cpp
    sources/FirstPersonCamera.cpp
    sources/FloraBuild.cpp
    sources/FloraCards.cpp
    sources/FloraNeighbours.cpp
    sources/FloraSkeleton.cpp
    sources/FloraSpecies.cpp
    sources/FloraWeber.cpp
    sources/GroundTufts.cpp
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
    sources/WaterMesher.cpp
    sources/PathMesher.cpp)
target_link_libraries(dfn_render PUBLIC dfn_headers)
