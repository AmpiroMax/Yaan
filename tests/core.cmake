#
# Created: 09:08:2026 - 00:42:03
# Last updated: 18:08:2026 - 12:31:04
# File: tests/core.cmake
#
# Responsibility:
# - Test registration for the core zone (engine/core + engine/world). Included
#   by tests/CMakeLists.txt (lead scaffold); uses add_dfn_test().
#
# Dependencies:
# - Uses: add_dfn_test (tests/CMakeLists.txt), dfn_core, dfn_world.
# - Used by: ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Zone-owned (core agent, Rule 25).
#
# UPD:
# - 09:08:2026 - 00:42:03: Stage 2 — ECS, time, events, math, worldgen
#                          determinism (Rule 13.1), chunk streaming suites.
# - 09:08:2026 - 11:05:22: Stage 3b — worldgen v2 design-contract suite
#                          (hydrology invariant, sites, corridors, C1).
# - 09:08:2026 - 16:30:44: Representation swap: test_voxel suite.
# - 09:08:2026 - 23:49:27: LOD streaming half: test_coarse_lod (node identity,
#                          the exact chunk<->node seam with its two counter-
#                          factual builders, async residency) and test_lod_seam,
#                          which links dfn_render as well because it checks the
#                          two zones AGREE (ladder equality + the inter-level
#                          disagreement table against render's skirt).
# - 10:08:2026 - 01:58:00: test_json (strict JSON reader, tech-debt task 3).
# - 10:08:2026 - 02:59:28: test_forest_stand (LANDSCAPE §8.1: stand selector
#                          byte-identity guard + LF-1/LF-2 acceptances with
#                          their Rule 30 controls).
# - 18:08:2026 - 16:51:17: test_house_graph — первый срез постройки-гиперграфа. Держит то, ради чего
#   модель затевалась: связи существуют и их видно, занятую вершину нельзя
#   удалить молча. Гиперребро проверяется ОТДЕЛЬНО от мультиребра — это разные
#   вещи, и путаница между ними однажды разложит пол на пары.

# - 10:08:2026 - 20:06:10: test_find_occlusion (BR-5's composed-scene
#                          ray-vs-disc instrument with its bare-terrain
#                          must-fail control; links dfn_render for geometry).
# - 10:08:2026 - 21:15:28: test_layout_load — the map-to-data migration guard
#                          (CODE_AUDIT §3.4). Runs from the repo ROOT: it opens
#                          the shipped asset by relative path on purpose, so a
#                          missing asset is a red test rather than a silent
#                          fallback to compiled content.
# - 11:08:2026 - 14:23:03: test_ground_relief (LANDSCAPE §10.1): the detrended
#                          bumpiness instrument on the SHIPPED world, read at
#                          the pinned standpoint of the archived lowland frames
#                          plus a trend-ranked (never sigma-ranked) sweep of
#                          the flattest legal ground. Runs from the repo ROOT
#                          for the same reason test_layout_load does.
# - 12:08:2026 - 22:53:00: test_great_oak (docs/GIANT_OAKS.md §2): the landmark
#                          tree's placement, measured IN THE GENERATOR with its
#                          zero-dose arm (DFN_NO_GREAT_OAK) in the same binary.
#                          Runs from the repo ROOT: it opens the shipped layout
#                          asset the way the app does.
# - 14:08:2026 - 18:43:43: test_world_format — the .dfw container's round trip,
#                          byte-identical rewrite (Rule 13.1) and its SOFT
#                          failures. Built on really generated chunks, because
#                          a hand-built chunk has empty voxel and scatter
#                          arrays and would pass while proving nothing about
#                          the fields the baker exists to carry.
# - 14:08:2026 - 20:18:30: test_mountain_horizon (the HORIZON PROFILE:
#                          skyline elevation angle per bearing from a fixed,
#                          unfiltered standpoint lattice -- the view-side read
#                          of "how much mountain does this world have").
# - 16:08:2026 - 20:16:09: test_scene — правила композиции (.scene): каждое правило проверено
#                          С ОБЕИХ СТОРОН — чистая сцена его проходит, подложенный
#                          дефект валит. Правило, которое не умеет покраснеть,
#                          ничего не охраняет.
# - 17:08:2026 - 12:49:26: core_scene_joint_rules (зона домов): правила соединителей
#   JointSeat/JointAngle с красными и зелёными руками из HOUSES.md §5.
# - 17:08:2026 - 16:58:13: core_scene_house_rules (зона домов): правила ПОСТРОЙКИ из
#   HOUSES.md §8 — WallTwoJoints, JointCapacity, DeckOnJoints, RoofSeat. Обе
#   руки каждой пары отличаются ОДНИМ числом (высота этажа, число стоек, метр
#   смещения), чтобы «правило сработало» и «не сработало» не оказались двумя
#   разными сценами с двумя разными ошибками.
# - 17:08:2026 - 17:16:17: core_scene_stair_rules (зона домов): лестница и ПРОЁМ над ней
#   (HOUSES.md §9) — пять красных рук, названных пользователем поимённо, и
#   сверка КАЛЬКУЛЯТОРА (выведенная длина проёма) с СУДЬЁЙ (капсула на каждой
#   ступени). Они пришли к 9u разными дорогами и не зовут друг друга.
# - 18:08:2026 - 12:31:04: test_height_lattice — решётка ХРАНЕНИЯ против
#                          решётки ГЕОМЕТРИИ. Рисуемая земля живёт на шаге
#                          VOXEL_SIZE, а хранится на HEIGHTMAP_STEP, и того,
#                          сколько первая ВЫДУМЫВАЕТ поверх второй, не мерил
#                          никто. Контроль — тот же прибор на нарочно
#                          огрублённой решётке, а не откаченная константа.

add_dfn_test(test_ecs core/EcsTests.cpp dfn_core)
add_dfn_test(test_json core/JsonTests.cpp dfn_core)
add_dfn_test(test_time core/TimeTests.cpp dfn_core)
add_dfn_test(test_events core/EventBusTests.cpp dfn_core)
add_dfn_test(test_math core/MathTests.cpp dfn_core)
add_dfn_test(test_worldgen_determinism core/WorldgenTests.cpp dfn_world)
add_dfn_test(test_worldgen_v2 core/WorldgenV2Tests.cpp dfn_world)
add_dfn_test(test_forest_stand core/ForestStandTests.cpp dfn_world)
add_dfn_test(test_chunk_streaming core/ChunkManagerTests.cpp dfn_world)
add_dfn_test(test_voxel core/VoxelTests.cpp dfn_world)
add_dfn_test(test_world_format core/WorldFormatTests.cpp dfn_world)
add_dfn_test(test_scene core/SceneTests.cpp dfn_world)

# ПОСТРОЙКА КАК ГИПЕРГРАФ, первый срез. Держит то, ради чего модель затевалась:
# связи существуют и их видно (выбрал вершину — вот её элементы), а занятую
# вершину нельзя удалить молча. Гиперребро проверяется отдельно от мультиребра:
# это разные вещи, и путать их — значит однажды разложить пол на пары и потерять
# «выбрал пол — подсветились его вершины».
add_dfn_test(test_house_graph core/HouseGraphTests.cpp dfn_world)
add_dfn_test(test_layout_load core/LayoutLoadTests.cpp dfn_world)
add_dfn_test(test_ground_relief core/GroundReliefTests.cpp dfn_world)
add_dfn_test(test_mountain_horizon core/MountainHorizonTests.cpp dfn_world)
add_dfn_test(test_great_oak core/GreatOakTests.cpp dfn_world)
add_dfn_test(test_coarse_lod core/CoarseLodTests.cpp dfn_world)
add_dfn_test(test_height_lattice core/HeightLatticeTests.cpp dfn_world)
add_dfn_test(test_lod_seam core/LodSeamTests.cpp dfn_world dfn_render)
# Links dfn_render for the SAME reason test_lod_seam does: it checks two zones
# agree about one geometry (the occluder discs are sized from render's own
# species table, never from literals copied into the test).
add_dfn_test(test_find_occlusion core/FindOcclusionTests.cpp dfn_world dfn_render)
add_dfn_test(core_scene_joint_rules core/SceneJointRuleTests.cpp dfn_world)
add_dfn_test(core_scene_house_rules core/SceneHouseRuleTests.cpp dfn_world)
add_dfn_test(core_scene_stair_rules core/SceneStairRuleTests.cpp dfn_world)
