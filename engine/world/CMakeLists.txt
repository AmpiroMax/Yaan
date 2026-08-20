#
# Created: 09:08:2026 - 00:42:03
# Last updated: 21:08:2026 - 00:40:00
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
# - 17:08:2026 - 17:16:17: SceneStairRules.cpp — лестница и ПРОЁМ над ней (HOUSES.md §9).
#                          Свой файл, потому что SceneHouseRules.cpp уже упёрся
#                          в правило 21, и потому что у лестницы свой прибор —
#                          капсула игрока на каждой ступени, а не ещё одна
#                          проверка посадки панели.

# - 17:08:2026 - 19:05:00: ReliefLayer.cpp — слой ручной правки земли (кисти рельефа,
#                          заказ 17.08). Отдельный файл, потому что у него свой
#                          формат-сиделка рядом со .scene, а Worldgen.cpp и без
#                          того держит стек проходов целиком.

# - 18:08:2026 - 16:48:47: HouseGraph.cpp — постройка как ГИПЕРГРАФ (docs/DESIGN_HOUSE_GRAPH.md).
#   Живёт в world, а не в editor, по той же причине, по которой здесь живёт
#   Scene: это ДАННЫЕ МИРА, которые редактор правит, а судья проверяет. Модель в
#   зоне редактора была бы моделью, недоступной судье.

# - 18:08:2026 - 17:07:34: HouseFile.cpp — чтение и запись .dfh. Текстом, а не двоично: агент
#   строит город рецептами, и рецепт обязан читаться глазами и различаться в
#   diff. Двоичный формат сделал бы историю правок непроверяемой.
# - 18:08:2026 - 17:33:36: HouseMesh.cpp — геометрия постройки из графа. Отдельным
#   файлом от модели, потому что это РАЗНЫЕ вопросы: HouseGraph отвечает «кто с
#   кем связан», HouseMesh — «как это выглядит и во что упирается игрок». Живёт
#   в world, а не в render, по правилу 3: меш считается и проверяется БЕЗ
#   рендерера, а зона отрисовки только заливает готовые буферы. Тот же меш идёт
#   в физику, и это не совпадение — коллайдер обязан описывать ровно то, что
#   видно.

# - 18:08:2026 - 17:54:03: HouseRegister.cpp — регистрация типа постройки. Проверки СТРОЖЕ, чем при
#   правке: недостроенное состояние нормально, пока человек строит, но в
#   библиотеку типов попадать не должно — тип ставится в десяти местах, и его
#   дефект размножится прежде, чем кто-нибудь заметит.

# - 18:08:2026 - 18:05:07: HouseStyle.cpp — СТИЛЬ СТЕНЫ КАК ПРАВИЛО ПОРОЖДЕНИЯ и укладчик по
#   нему (docs/DESIGN_HOUSE_GRAPH.md §2.4/§5.7). Отдельным файлом от HouseMesh,
#   потому что это РАЗНЫЕ вопросы: укладчик отвечает «что где стоит на стене»
#   (доска от и до, раскос под углом, проём в такой-то точке), построитель — «из
#   каких треугольников это сделано». Раскладка проверяется числами БЕЗ окна, и
#   она не знает ни про вершины, ни про нормали.
# - 21:08:2026 - 00:40:00: HouseMesh.cpp разрезан на модули по файлу на алгоритм (решение пользователя).

add_library(dfn_world STATIC
    sources/Chunk.cpp
    sources/ChunkManager.cpp
    sources/CoarseTerrain.cpp
    sources/LayoutLoad.cpp
    sources/ReliefLayer.cpp
    sources/SaveDelta.cpp
    sources/HouseFile.cpp
    sources/HouseBodies.cpp
    sources/HouseGeom.cpp
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
