#
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
add_library(dfn_render STATIC
    sources/BitmapFont.cpp
    sources/CloudModel.cpp
    sources/DebugDraw.cpp
    sources/FirstPersonCamera.cpp
    sources/FloraBuild.cpp
    sources/FloraCards.cpp
    sources/FloraFireflies.cpp
    sources/FloraNeighbours.cpp
    sources/FloraSkeleton.cpp
    sources/FloraSpecies.cpp
    sources/FloraWeber.cpp
    sources/GroundTufts.cpp
    sources/LoadingScreen.cpp
    sources/LodTerrain.cpp
    sources/MapScreen.cpp
    sources/MorphBlend.cpp
    sources/PartsMaterial.cpp
    sources/ObjectRegistry.cpp
    sources/PartForge.cpp
    sources/PartForgeJoints.cpp
    sources/PartForgeRoofs.cpp
    sources/PartForgeCatalogue.cpp
    sources/PartForgeWalls.cpp
    sources/PartsAtlas.cpp
    sources/SignForge.cpp
    sources/PixelCanvas.cpp
    sources/ProcFlora.cpp
    sources/ProcMesh.cpp
    sources/ProcTexture.cpp
    sources/RenderSystem.cpp
    sources/RenderSystemResources.cpp
    sources/ScatterBatcher.cpp
    sources/TreeBark.cpp
    sources/TreeForge.cpp
    sources/TreeForgeV2.cpp
    sources/SkyModel.cpp
    sources/TerrainLod.cpp
    sources/TerrainMesher.cpp
    sources/WindModel.cpp
    sources/VoxelMesher.cpp
    sources/Tour.cpp
    sources/WaterMesher.cpp
    sources/PathMesher.cpp)
target_link_libraries(dfn_render PUBLIC dfn_headers dfn_core)
