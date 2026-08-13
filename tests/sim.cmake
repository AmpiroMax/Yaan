#
# Created: 09:08:2026 - 00:45:08
# Last updated: 13:08:2026 - 18:10:00
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
# - 10:08:2026 - 01:53:17: Added sim_step_feel (the step as an event: spacing,
#                          stride arithmetic, zero-when-still, dip/settle,
#                          FOV clamp) and sim_audio (miniaudio backend +
#                          placeholder bank; DFN_REPO_ROOT for asset paths).
# - 09:08:2026 - 00:45:08: Stage 2 — dice, player movement, null backends,
#                          jolt physics suites.
# - 09:08:2026 - 16:51:22: Added sim_tunnel_walk — the voxel terrain collision
#                          acceptance walk through the crag tunnel.
# - 09:08:2026 - 18:56:32: Added sim_interaction (four verbs, inventory, saves).
# - 09:08:2026 - 22:18:17: Added sim_movement_solid (jump apex, crouch under a
#                          ceiling, and the cliff-vs-jump invariant).
# - 09:08:2026 - 22:27:49: Added sim_prop_collision (buildings and boulders).
# - 09:08:2026 - 22:34:38: Added sim_view_model (hand anchor, inventory state).
# - 10:08:2026 - 21:13:08: Added sim_save_format — the save CONTAINER's own
#                          suite (byte-exact grammar + endianness, the
#                          committed fixture that proves an older build's file
#                          still loads, skip-unknown, fail-soft truncation, and
#                          the two misuse latches). Registered here rather than
#                          in core.cmake because sim implemented the IO under a
#                          lead carve; it moves to tests/core/ when core takes
#                          serialization back.
# - 10:08:2026 - 21:21:55: Added sim_save_delta — the .dfs codec end to
#                          end on a real file: delta round trip with a second
#                          payload as its control, the gameplay sections
#                          travelling in the same file, the seed guard, the
#                          missing-META refusal, and the forward-compatibility
#                          case (a section this build does not understand must
#                          survive a load AND a re-save).
# - 10:08:2026 - 21:24:32: sim_jolt_physics links dfn_render, for the same
#                          reason test_lod_seam does: the new diagonal case
#                          checks that two zones agree about one geometry, and
#                          it reads render's actual mesh rather than a literal
#                          copy of its triangulation.
# - 10:08:2026 - 21:33:52: sim_tunnel_walk links dfn_gameplay. The castle
#                          curtain-wall tunnelling case had no curtain wall in
#                          its physics world — the rig built terrain collision
#                          only, and the wall is a prop — so eight charges were
#                          running through open ground and reporting success.
# - 13:08:2026 - 16:20:00: Added sim_flora_collision — the world you cannot walk
#                          through (solid boles measured from their own drawn
#                          triangles, brush as drag, the log/step-height
#                          watershed), with the control arms for each claim.
# - 13:08:2026 - 17:30:00: Added sim_interactable_visible.
# - 13:08:2026 - 17:45:00: Added sim_great_oak_stair (RED on purpose; label known-defect).
# - 13:08:2026 - 18:10:00: sim_interactable_visible links dfn_physics (the
#                          entity-{0,0} targeting case needs the Jolt backend).

add_dfn_test(sim_dice sim/DiceTests.cpp dfn_gameplay)

add_dfn_test(sim_player_movement sim/PlayerMovementTests.cpp
    dfn_gameplay dfn_platform_physics)

add_dfn_test(sim_null_backends sim/NullBackendTests.cpp
    dfn_platform_physics dfn_platform_anim dfn_platform_audio dfn_platform_llm)

# dfn_world is linked for the real-ChunkManager heightfield smoke test
# (cross-zone contract check suggested by core at the stage-2 sync).
add_dfn_test(sim_jolt_physics sim/JoltPhysicsTests.cpp
    dfn_physics dfn_platform_physics dfn_world dfn_gameplay dfn_render)

# The four interaction verbs, inventory semantics and the save sections.
add_dfn_test(sim_interaction sim/InteractionTests.cpp
    dfn_gameplay dfn_platform_physics dfn_core)

# Jump/crouch/swim against REAL collision: apex height, the ceiling that refuses
# a stand-up, and the cliff invariant (jump must not repeal PLAYER_MAX_SLOPE).
add_dfn_test(sim_movement_solid sim/MovementSolidTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics)

# The visible hand and the inventory screen state.
add_dfn_test(sim_view_model sim/ViewModelTests.cpp
    dfn_gameplay dfn_core dfn_platform_physics)

# Buildings and boulders are solid, built from the triangles render draws.
add_dfn_test(sim_prop_collision sim/PropCollisionTests.cpp
    dfn_gameplay dfn_world dfn_render dfn_platform_physics dfn_core)

# The voxel-terrain acceptance walk (crag tunnel): real generated world, real
# extracted collision mesh, real capsule.
add_dfn_test(sim_tunnel_walk sim/TunnelWalkTests.cpp
    dfn_physics dfn_platform_physics dfn_world dfn_core dfn_gameplay)

# THE STEP IS AN EVENT (в3): footfall spacing/arithmetic, zero-when-still (the
# rejected floating is the control), landing dip, stop settle, FOV clamp.
add_dfn_test(sim_step_feel sim/StepFeelTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics dfn_core)

# miniaudio backend contract + the placeholder step-sound bank. Needs the repo
# root to find the generated wav assets from the build dir.
add_dfn_test(sim_audio sim/AudioTests.cpp
    dfn_gameplay dfn_platform_audio dfn_core)
target_compile_definitions(sim_audio PRIVATE
    DFN_REPO_ROOT="${CMAKE_SOURCE_DIR}")

# The playtest checker's own controls (Rule 30): broken runs MUST fire, the
# clean patrol must not, and the bot must actually cover ground.
add_dfn_test(sim_playtest sim/PlaytestTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics dfn_core)

# COLLISION MESH COST. After the streaming fix, sim's Jolt MeshShape build is
# the dominant remaining term in a chunk admission (~68 of ~83 ms). This sizes
# what a coarser collision mesh would buy, so the decision is taken against a
# curve. A measurement, not a gate — no wall-clock threshold (Rule 38).
add_dfn_test(sim_collision_cost sim/CollisionCostTests.cpp
    dfn_physics dfn_platform_physics dfn_world dfn_core)

# The save container itself (engine/core/serialization), implemented by sim
# under a lead carve. DFN_REPO_ROOT locates the committed byte-exact fixture.
add_dfn_test(sim_save_format sim/SaveFormatTests.cpp
    dfn_core dfn_gameplay)
target_compile_definitions(sim_save_format PRIVATE
    DFN_REPO_ROOT="${CMAKE_SOURCE_DIR}")

# The .dfs codec (engine/world), implemented by sim under a lead carve.
add_dfn_test(sim_save_delta sim/SaveDeltaTests.cpp
    dfn_world dfn_gameplay dfn_core)

# THE WORLD YOU CANNOT WALK THROUGH (user: «деревья — не объекты физики… кусты,
# поваленные деревья пропускают героя»). Trunks solid from their own drawn
# triangles, brush as drag, the log/step-height watershed — with the control
# arms that make each claim refutable.
add_dfn_test(sim_flora_collision sim/FloraCollisionTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics dfn_world dfn_render dfn_core)

# THE PROPS MUST BE VISIBLE (ui's find: all three demo props drew as nothing
# while the crosshair, the hover target and the prompt worked around them).
# Asserts against render's ACTUAL ECS selector, and measures how much of each
# prop's target box has the prop behind it.
add_dfn_test(sim_interactable_visible sim/InteractableVisibleTests.cpp
    dfn_gameplay dfn_render dfn_physics dfn_platform_physics dfn_core)

# THE GREAT OAK'S STAIR -- THE GATE, AND IT IS RED ON PURPOSE TODAY.
# Climbs the tread pairs with the real character controller against the real
# collider and reports the fraction a walker can take; NUMBERS.md
# (GREAT_OAK_STEP_RISE) says that fraction must be 1.0. The defect is in flora's
# geometry, but the measurement needs a controller, so the instrument lives
# here. Labelled so a runner can ask for the tree's state WITHOUT it
# (`ctest -LE known-defect`) -- what must never happen is the assertion being
# relaxed to make the suite green, because the suite going green is the event
# this test exists to announce.
add_dfn_test(sim_great_oak_stair sim/GreatOakStairTests.cpp
    dfn_gameplay dfn_physics dfn_platform_physics dfn_render dfn_core)
set_tests_properties(sim_great_oak_stair PROPERTIES LABELS "known-defect")
