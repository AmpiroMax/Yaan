/*
Created: 09:08:2026 - 18:56:32
Last updated: 13:08:2026 - 18:15:00
Module: engine/gameplay
File: engine/gameplay/sources/InteractableSpawn.h

Responsibility:
- Spawning interactable props into the world with their collision body, so the
  crosshair ray can find them: doors, chests, levers and loose items.

Key items:
- InteractableDesc: what to spawn (kind + placement + payload).
- spawn_interactable(): one prop, wired to physics and the ECS.

Dependencies:
- Uses: core ecs, core components (Transform), platform IPhysics,
  engine/physics collision layers, Interaction.h.
- Used by: engine/app (placing this stage's proving content), the content
  loader once core's JSON reader lands, tests.

Notes:
- A PROP MUST BE VISIBLE, and until 13.08 none of them was. The spawn attached
  Transform, Highlightable, the verb component and the ray box — and no
  RenderMesh, so a 1.8 x 2.0 m door stood 2.5 m in front of the spawn, dead
  ahead, drawn by nothing, while the ray hit its box and the app honestly
  printed "Open". The whole chain worked and there was nothing to see (ui's
  find). The three verb paths exist precisely so that two thirds of them are not
  left untested in the real game — and they could not be tested in the real game
  because they could not be SEEN.
- ONE COMPONENT WOULD NOT HAVE BEEN ENOUGH: render's ECS pass selects on
  Transform + PreviousTransform + RenderMesh, and there was no PreviousTransform
  either. Adding the mesh alone would have looked like a fix and changed
  nothing, which is the failure mode worth naming.
- THE DRAWN SHAPE AND THE SOLID BOX ARE ONE OBJECT, not two numbers that agree.
  The mesh is authored in the unit cube (InteractableMesh.h) and Transform.scale
  IS `half_extents`, so the box the ray hits is the cube the artist filled.
  For the door that is exact; the lever and the torch are inscribed in theirs
  and the residual is measured in the suite rather than described.
- Every prop gets a static box on LAYER_INTERACTABLE whose user_data is the
  entity's packed id — that is how a ray hit becomes an entity again. Props are
  NOT on LAYER_STATIC: the player walks through the ray, not into the prop, and
  making them block movement is a separate decision (a door you cannot walk
  through needs its collision to move with its open state, which is animation
  work this stage does not have).
- Content placement is data (Rule 5): this header is the mechanism, the
  positions and payloads come from the content file.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add a second spawn path; the loader and the app both call this.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial interactable spawning.
- 13:08:2026 - 17:20:00: PROPS BECOME VISIBLE (ui's find: all three demo props
  drew as nothing while the whole hover chain worked around them). RenderMesh +
  PreviousTransform + LocalBounds, and Transform.scale becomes `half_extents`
  so the drawn mesh and the ray box are the same cube. `mesh_asset` defaults to
  the verb's placeholder rather than to the "draw nothing" sentinel.
- 13:08:2026 - 18:15:00: InteractableBodies + reap_interactable_bodies. The ray
  box handle was DISCARDED at spawn, so no prop's box could ever be destroyed:
  taking an item left an invisible ray target standing where it had been, and
  every drop added another for the length of the session.
*/

#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <glm/vec3.hpp>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

enum class InteractableKind : uint8_t {
    Pickup,   // loose item -> TAKE
    Openable, // door, chest -> OPEN/CLOSE
    Usable,   // lever, campfire, bed -> USE
};

struct InteractableDesc {
    InteractableKind kind = InteractableKind::Pickup;
    glm::vec3 position{0.0f};      // world space, meters
    glm::vec3 half_extents{0.25f}; // collision box for the crosshair ray AND
                                   // the model-space scale of the drawn mesh
    std::string prompt_key;        // localization key (Rule 5)

    // What the player sees. 0 would be the documented "draw nothing", which is
    // how every prop in the world came to be invisible, so it is NOT the
    // default: an unset mesh takes the placeholder for the prop's verb
    // (interactable_mesh_for). Content overrides it with a real mesh id.
    // The failure mode is then "a generic door" instead of "no door", which is
    // the right way round for something a player is meant to walk up to.
    uint32_t mesh_asset = 0;

    // Pickup payload.
    ItemId item{};
    uint32_t count = 1;

    // Openable payload.
    bool starts_open = false;
    bool locked = false;

    // Usable payload.
    uint64_t action = 0;    // hashed content action id
    bool repeatable = true;
};

// World RESOURCE: the ray-target body of every prop, keyed by the packed id of
// the entity that owns it.
//
// IT EXISTS BECAUSE THE HANDLE WAS BEING THROWN AWAY. `spawn_interactable`
// created the box and discarded what came back, so nothing could ever destroy
// one. Taking an item despawns its ENTITY and leaves its box standing: an
// invisible ray target at the spot where the thing used to be, which stops the
// crosshair before whatever is actually behind it. Every drop spawns another,
// and they accumulate for the length of the session against a world sized for
// 16 384 bodies.
//
// Ordered, never hashed: bodies are destroyed in the same order on every run
// (Rule 13.2).
struct InteractableBodies {
    std::map<uint64_t, platform::PhysicsBodyHandle> bodies;
};

// Destroys the ray box of every prop whose entity is gone, and forgets it.
// Cheap when nothing died (a map walk). Called from player_actions_step, which
// is where an interaction can have killed one — no new call site for the app.
void reap_interactable_bodies(ecs::World& world, platform::IPhysics& physics);

// The placeholder mesh a verb gets when the content did not name one.
[[nodiscard]] uint32_t interactable_mesh_for(InteractableKind kind);

// Spawns one prop: entity + Transform/PreviousTransform + RenderMesh +
// LocalBounds + Highlightable + the kind's component, and a static collision
// box on LAYER_INTERACTABLE carrying its id.
ecs::EntityId spawn_interactable(ecs::World& world, platform::IPhysics& physics,
                                 const InteractableDesc& desc);

} // namespace dfn::gameplay
