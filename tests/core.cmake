#
# Created: 09:08:2026 - 00:42:03
# Last updated: 09:08:2026 - 23:49:27
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

add_dfn_test(test_ecs core/EcsTests.cpp dfn_core)
add_dfn_test(test_time core/TimeTests.cpp dfn_core)
add_dfn_test(test_events core/EventBusTests.cpp dfn_core)
add_dfn_test(test_math core/MathTests.cpp dfn_core)
add_dfn_test(test_worldgen_determinism core/WorldgenTests.cpp dfn_world)
add_dfn_test(test_worldgen_v2 core/WorldgenV2Tests.cpp dfn_world)
add_dfn_test(test_chunk_streaming core/ChunkManagerTests.cpp dfn_world)
add_dfn_test(test_voxel core/VoxelTests.cpp dfn_world)
add_dfn_test(test_coarse_lod core/CoarseLodTests.cpp dfn_world)
add_dfn_test(test_lod_seam core/LodSeamTests.cpp dfn_world dfn_render)
