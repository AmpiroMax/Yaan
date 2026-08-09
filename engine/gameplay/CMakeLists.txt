#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 22:21:30
# File: engine/gameplay/CMakeLists.txt
#
# Responsibility:
# - dfn_gameplay static library: player movement (ref core + World wrappers)
#   and the dice RNG. The rest of the gameplay contract (NpcAction executor,
#   stats, dialogue runtime) is headers-only in stage 2 by scope. Zone-owned
#   by sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers, dfn_core (World implementation for the wrappers).
# - Used by: engine/app, games/daggerfall_n, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Zero third-party here (Rule 1);
#   platform access through interfaces passed as parameters only (Rule 9).
#
# UPD:
# - 10:08:2026 - 01:53:17: StepFeel.cpp — the step-as-an-event math (в3).
# - 09:08:2026 - 00:45:08: Stage 2 — player movement + dice.
# - 09:08:2026 - 18:56:32: Interaction stage — four verbs, inventory, item
#                          definitions, prop spawning, save sections.
# - 09:08:2026 - 22:21:30: ViewModel.cpp — the visible first-person hand;
#                          PropCollision.cpp — buildings and boulders become
#                          solid, built from the triangles render draws (hence
#                          the new dfn_render link; those builders are pure
#                          geometry and pull in no third-party library).

add_library(dfn_gameplay STATIC
    sources/Dice.cpp
    sources/GameplaySave.cpp
    sources/HeldItem.cpp
    sources/InteractableSpawn.cpp
    sources/InteractionSystem.cpp
    sources/Inventory.cpp
    sources/InventoryScreen.cpp
    sources/PlayerActions.cpp
    sources/PlayerMovement.cpp
    sources/PlayerMovementWorld.cpp
    sources/PlaytestBot.cpp
    sources/PropCollision.cpp
    sources/StepAudio.cpp
    sources/StepFeel.cpp
    sources/ViewModel.cpp)

# dfn_world: gameplay registers its save sections with world::SaveDeltaCodec
# (the DAG allows gameplay -> world).
target_link_libraries(dfn_gameplay PUBLIC dfn_headers dfn_core dfn_world dfn_render)
