/*
Created: 09:08:2026 - 22:21:30
Last updated: 09:08:2026 - 22:21:30
Module: engine/gameplay
File: engine/gameplay/sources/PropCollision.h

Responsibility:
- Static collision for the things standing ON the terrain: buildings and
  boulders. Their collision is built from THE SAME TRIANGLES RENDER DRAWS, so
  what you can bump into is what you can see.

Key items:
- PropCollisionState: the World resource owning one body per resident chunk.
- update_prop_collision(): reconciles bodies against the resident chunk set.

Dependencies:
- Uses: core ecs, core components (Transform, RenderMesh), engine/world
  (ChunkManager, SiteMarker, chunk_group), engine/render (build_site_mesh,
  build_scatter_mesh, append_transformed — pure geometry, no GPU), platform
  IPhysics, engine/physics collision layers.
- Used by: engine/app once per fixed tick; tests.

Notes:
- WHY THE DRAWN TRIANGLES AND NOT A BOX. The user asked for two things that are
  the same request: boulders you can jump onto, and buildings whose collision
  follows the geometry rather than a bounding box. A box around a house is a
  house with no doorway — and the moment render cuts a doorway into the mesh,
  a box would still refuse to let anyone through. Building the body from
  `build_site_mesh` means the door becomes walkable the day it is modelled,
  with no change here. (The DAG permits gameplay -> render; those builders are
  pure functions and `dfn_render` pulls in no third-party library.)
- ONE BODY PER CHUNK, not one per prop. Props never move, so a merged mesh is
  strictly cheaper than N bodies, and it matches how terrain collision is
  already streamed — created on residency, destroyed on eviction.
- LAYER_STATIC, not LAYER_INTERACTABLE: these bodies BLOCK you. Interactable
  props keep their own thin boxes on their own layer so the crosshair ray finds
  them without ordinary walls swallowing it.
- NOT COVERED YET: tree trunks. `species_trunk_radius()` reports the flared
  base radius against the species' NOMINAL height, while every instance is
  drawn at a per-variant height, so a trunk cylinder built from it would stand
  up to ~0.35 m proud of an oak's actual bark — an invisible wall, which is a
  worse bug than walking through a tree because you cannot see what stopped
  you. Waiting on a per-instance accessor from render; requested, not forgotten.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic iteration only (Rule 13.2): the body map is ordered, never a
  hash map, so bodies are created in the same order on every run.
- Never hardcode a placement number here; the drawn placement is authoritative
  and its constants come from NUMBERS via dfn::config.
*/
/*
UPD:
- 09:08:2026 - 22:21:30: Created — buildings and boulders become solid
                         (user request: boulders physical and jumpable,
                         buildings collided by geometry rather than a box).
*/

#pragma once

#include <cstdint>
#include <map>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::ecs {
class World;
}
namespace dfn::world {
class ChunkManager;
}

namespace dfn::gameplay {

// World RESOURCE: the prop collision body of each resident chunk, keyed by the
// chunk's packed coordinate. Ordered (not hashed) so creation order — and with
// it the backend's body ids — is identical on every run (Rule 13.2).
struct PropCollisionState {
    std::map<uint64_t, platform::PhysicsBodyHandle> bodies;
};

// Once per fixed tick: builds a merged static body for every resident chunk
// that has props and does not have one yet, and destroys the bodies of chunks
// that are no longer resident. Cheap when nothing changed (a map walk).
void update_prop_collision(ecs::World& world, platform::IPhysics& physics,
                           const world::ChunkManager& chunks);

} // namespace dfn::gameplay
