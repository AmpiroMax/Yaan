#
# Created: 09:08:2026 - 00:42:03
# Last updated: 17:08:2026 - 16:30:25
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
# - 10:08:2026 - 10:44:13: §8.1 path network: WorldgenPaths.cpp.
# - 10:08:2026 - 10:55:03: BR-6 find layer: WorldgenFinds.cpp.
# - 10:08:2026 - 11:19:15: WorldgenVantages.cpp — the forest stand's acceptance
#                          standpoints, so render's Tour can shoot a stand it
#                          cannot see the coordinates of.
# - 10:08:2026 - 21:13:28: LayoutLoad.cpp — the map moves to data
#                          (CODE_AUDIT §3.4); world keeps the loader, the
#                          content lives under games/.
# - 10:08:2026 - 21:20:31: SaveDelta.cpp — the .dfs codec, written by sim
#                          under an explicit lead carve (this file and this
#                          line only). It was the last declaration-only piece
#                          between the engine and a save file the user can
#                          actually write.
# - 11:08:2026 - 15:15:55: WorldgenRelief.cpp + WorldgenOutcrop.cpp (§2.7 general relief, §10.5 B2 rock).
# - 13:08:2026 - 16:20:00: WorldgenForms.cpp (§10.1.3: the bench/riser
#                          operator — the ground gets FORMS, after three noise
#                          parameters in a row failed to make it hide itself).
# - 12:08:2026 - 22:51:00: WorldgenGreatOak.cpp (GIANT_OAKS §2 placement: the
# - 13:08:2026 - 00:40:00: WorldgenFlow.cpp -- the drainage pass that replaces the fixed-pitch draw comb.
#                          clearing, and the rarity derived from read distance).
# - 14:08:2026 - 18:38:33: WorldFormat.cpp -- the .dfw container's bodies, five
#                          days after its contract. The first half of the
#                          tooling pivot's baker: worldgen writes offline, the
#                          game only reads (в1).
# - 14:08:2026 - 20:47:52: WorldBake.cpp -- the generator's offline caller. A
#                          library function and not only a CLI, because the app
#                          bakes a map on its first run and a composition root
#                          shelling out to a binary would be a second way to do
#                          one thing.
# - 15:08:2026 - 16:24:04: Scene.cpp — файл композиции .scene и правила, которыми он
#                          проверяется (инструмент оформления мира).
# - 17:08:2026 - 16:30:25: SceneHouseRules.cpp — правила ПОСТРОЙКИ (зона домов,
#                          HOUSES.md §3-5 и §8). Вынесены из Scene.cpp, который
#                          перерос правило 21 (1052 строки) ещё до того, как
#                          заказ 17.08 удвоил число правил соединителей.

add_library(dfn_world STATIC
    sources/Chunk.cpp
    sources/ChunkManager.cpp
    sources/CoarseTerrain.cpp
    sources/LayoutLoad.cpp
    sources/SaveDelta.cpp
    sources/Scene.cpp
    sources/SceneHouseRules.cpp
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
