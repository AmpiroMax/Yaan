#
# Created: 09:08:2026 - 00:42:03
# Last updated: 12:08:2026 - 22:53:00
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
add_dfn_test(test_layout_load core/LayoutLoadTests.cpp dfn_world)
add_dfn_test(test_ground_relief core/GroundReliefTests.cpp dfn_world)
add_dfn_test(test_great_oak core/GreatOakTests.cpp dfn_world)
add_dfn_test(test_coarse_lod core/CoarseLodTests.cpp dfn_world)
add_dfn_test(test_lod_seam core/LodSeamTests.cpp dfn_world dfn_render)
# Links dfn_render for the SAME reason test_lod_seam does: it checks two zones
# agree about one geometry (the occluder discs are sized from render's own
# species table, never from literals copied into the test).
add_dfn_test(test_find_occlusion core/FindOcclusionTests.cpp dfn_world dfn_render)