#
# File: engine/world/CMakeLists.txt
#
# Responsibility:
# - dfn_world static library: chunk data, deterministic worldgen, chunk
#   streaming. Zone-owned (core agent, Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_core, dfn_headers. Core only (Rule 1: no physics, no rendering).
# - Used by: engine/app, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. World depends on core only.

add_library(dfn_world STATIC
    sources/Chunk.cpp
    sources/ChunkManager.cpp
    sources/CoarseTerrain.cpp
    sources/LayoutLoad.cpp
    sources/ReliefLayer.cpp
    sources/SaveDelta.cpp
    sources/HouseConnect.cpp
    sources/HouseFile.cpp
    sources/HouseBodies.cpp
    sources/HouseEdges.cpp
    sources/HouseGeom.cpp
    sources/HouseLod.cpp
    sources/HouseMesh.cpp
    sources/HouseParams.cpp
    sources/HouseParquet.cpp
    sources/HousePlate.cpp
    sources/HouseRoof.cpp
    sources/HouseRules.cpp
    sources/HouseStairs.cpp
    sources/HouseWalls.cpp
    sources/HouseStyle.cpp
    sources/HouseGraph.cpp
    sources/HouseRegister.cpp
    sources/Scene.cpp
    sources/SceneHouseRules.cpp
    sources/SceneStairRules.cpp
    sources/VoxelMesh.cpp
    sources/VoxelVolume.cpp
    sources/WorldBake.cpp
    sources/WorldFormat.cpp
    sources/Worldgen.cpp
    sources/WorldgenCarve.cpp
    sources/WorldgenCastle.cpp
    sources/WorldgenErosion.cpp
    sources/WorldgenFlow.cpp
    sources/WorldgenFinds.cpp
    sources/WorldgenForest.cpp
    sources/WorldgenForms.cpp
    sources/WorldgenGreatOak.cpp
    sources/WorldgenHydrology.cpp
    sources/WorldgenMacro.cpp
    sources/WorldgenPaths.cpp
    sources/WorldgenScatter.cpp
    sources/WorldgenSites.cpp
    sources/WorldgenOutcrop.cpp
    sources/WorldgenRelief.cpp
    sources/WorldgenValidation.cpp
    sources/WorldgenVantages.cpp
    sources/WorldgenWater.cpp)

target_link_libraries(dfn_world PUBLIC dfn_core)
