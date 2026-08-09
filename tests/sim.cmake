#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 22:34:38
# File: tests/sim.cmake
#
# Responsibility:
# - Registers the sim zone's test executables (Rule 25 / Q34: zone-owned test
#   registration; the tests/ scaffold itself is lead-owned).
#
# Dependencies:
# - Uses: add_dfn_test() from tests/CMakeLists.txt; dfn_gameplay, dfn_physics,
#   dfn_platform_* targets.
# - Used by: tests/CMakeLists.txt (conditional include), ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Sim-owned; other zones register in
#   their own <zone>.cmake.
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — dice, player movement, null backends,
#                          jolt physics suites.
# - 09:08:2026 - 16:51:22: Added sim_tunnel_walk — the voxel terrain collision
#                          acceptance walk through the crag tunnel.
# - 09:08:2026 - 18:56:32: Added sim_interaction (four verbs, inventory, saves).
# - 09:08:2026 - 22:18:17: Added sim_movement_solid (jump apex, crouch under a
#                          ceiling, and the cliff-vs-jump invariant).
# - 09:08:2026 - 22:27:49: Added sim_prop_collision (buildings and boulders).
# - 09:08:2026 - 22:34:38: Added sim_view_model (hand anchor, inventory state).

add_dfn_test(sim_dice sim/DiceTests.cpp dfn_gameplay)

add_dfn_test(sim_player_movement sim/PlayerMovementTests.cpp
    dfn_gameplay dfn_platform_physics)

add_dfn_test(sim_null_backends sim/NullBackendTests.cpp
    dfn_platform_physics dfn_platform_anim dfn_platform_audio dfn_platform_llm)

# dfn_world is linked for the real-ChunkManager heightfield smoke test
# (cross-zone contract check suggested by core at the stage-2 sync).
add_dfn_test(sim_jolt_physics sim/JoltPhysicsTests.cpp
    dfn_physics dfn_platform_physics dfn_world dfn_gameplay)

# The four interaction verbs, inventory semantics and the save sections.
add_dfn_test(sim_interaction sim/InteractionTests.cpp
    dfn_gameplay dfn_platform_physics dfn_core)

# Jump/crouch/swim against REAL collision: apex height, the ceiling that refuses
# a stand-up, and the cliff invariant (jump must not repeal PLAYER_MAX_SLOPE).
add_dfn_test(sim_movement_solid sim/MovementSolidTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics)

# The visible hand and the inventory screen state.
add_dfn_test(sim_view_model sim/ViewModelTests.cpp dfn_gameplay dfn_core)

# Buildings and boulders are solid, built from the triangles render draws.
add_dfn_test(sim_prop_collision sim/PropCollisionTests.cpp
    dfn_gameplay dfn_world dfn_render dfn_platform_physics dfn_core)

# The voxel-terrain acceptance walk (crag tunnel): real generated world, real
# extracted collision mesh, real capsule.
add_dfn_test(sim_tunnel_walk sim/TunnelWalkTests.cpp
    dfn_physics dfn_platform_physics dfn_world dfn_core)
