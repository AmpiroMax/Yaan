#
# Created: 09:08:2026 - 00:42:03
# Last updated: 10:08:2026 - 10:40:28
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
#
# UPD:
# - 09:08:2026 - 00:42:03: Stage 2 — created dfn_world with Chunk/Worldgen/
#                          ChunkManager implementations (world file IO and
#                          SaveDelta deferred to stage 3).
# - 09:08:2026 - 11:05:22: Stage 3b — worldgen v2 pass sources (macro,
#                          hydrology, sites, scatter, validation).
# - 09:08:2026 - 13:28:27: P1 anisotropy touch: WorldgenWater.cpp added (query side split from WorldgenHydrology.cpp).
# - 09:08:2026 - 16:30:44: Representation swap: VoxelVolume.cpp and VoxelMesh.cpp.
# - 09:08:2026 - 16:47:51: P7: WorldgenCarve.cpp.
# - 09:08:2026 - 23:49:27: LOD streaming half: CoarseTerrain.cpp.
# - 10:08:2026 - 02:59:28: Forest stand (LANDSCAPE §8.1): WorldgenForest.cpp.
# - 10:08:2026 - 10:40:28: LF-8 erosion overlay: WorldgenErosion.cpp.

add_library(dfn_world STATIC
    sources/Chunk.cpp
    sources/ChunkManager.cpp
    sources/CoarseTerrain.cpp
    sources/VoxelMesh.cpp
    sources/VoxelVolume.cpp
    sources/Worldgen.cpp
    sources/WorldgenCarve.cpp
    sources/WorldgenCastle.cpp
    sources/WorldgenErosion.cpp
    sources/WorldgenForest.cpp
    sources/WorldgenHydrology.cpp
    sources/WorldgenMacro.cpp
    sources/WorldgenScatter.cpp
    sources/WorldgenSites.cpp
    sources/WorldgenValidation.cpp
    sources/WorldgenWater.cpp)

target_link_libraries(dfn_world PUBLIC dfn_core)
