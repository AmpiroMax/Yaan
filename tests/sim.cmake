#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 00:45:08
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

add_dfn_test(sim_dice sim/DiceTests.cpp dfn_gameplay)

add_dfn_test(sim_player_movement sim/PlayerMovementTests.cpp
    dfn_gameplay dfn_platform_physics)

add_dfn_test(sim_null_backends sim/NullBackendTests.cpp
    dfn_platform_physics dfn_platform_anim dfn_platform_audio dfn_platform_llm)

# dfn_world is linked for the real-ChunkManager heightfield smoke test
# (cross-zone contract check suggested by core at the stage-2 sync).
add_dfn_test(sim_jolt_physics sim/JoltPhysicsTests.cpp
    dfn_physics dfn_platform_physics dfn_world)
