#
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
    sources/ViewModel.cpp
    sources/WorldAmbience.cpp)

# dfn_world: gameplay registers its save sections with world::SaveDeltaCodec
# (the DAG allows gameplay -> world).
target_link_libraries(dfn_gameplay PUBLIC dfn_headers dfn_core dfn_world dfn_render)
