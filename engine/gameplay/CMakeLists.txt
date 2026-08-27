#
# Created: 09:08:2026 - 00:45:08
# Last updated: 27:08:2026 - 11:33:53
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
# - 13:08:2026 - 17:20:00: InteractableMesh.cpp — placeholder geometry for the
#                          props, so a thing the crosshair can name is a thing
#                          the player can see (all three demo props drew as
#                          nothing; ui's find).
# - 13:08:2026 - 16:35:00: FloraCollision.cpp — a plant's collider MEASURED out
#                          of the mesh flora draws for it, so trunks stop you
#                          and brush slows you (user's complaint that trees and
#                          bushes are not physical objects at all).
# - 10:08:2026 - 21:13:08: GameplaySaveRegistration.cpp — the codec
#                          registration split out of GameplaySave.cpp so that
#                          using a save SECTION no longer drags in the
#                          not-yet-implemented world::SaveDeltaCodec symbol.
# - 27:08:2026 - 11:33:53: CameraBoom.cpp — стрела камеры третьего лица
#                          (коллизия по сферкасту; заказ владельца 27.08).

add_library(dfn_gameplay STATIC
    sources/CameraBoom.cpp
    sources/Dice.cpp
    sources/FloraCollision.cpp
    sources/GameplaySave.cpp
    sources/GameplaySaveRegistration.cpp
    sources/HeldItem.cpp
    sources/InteractableMesh.cpp
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
