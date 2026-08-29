/*
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
- TREE TRUNKS ARE COVERED NOW, and by the same principle as the rest of this
  file rather than by the accessor this note used to wait for. The objection
  was right: `species_trunk_radius()` is the flare radius at the species'
  NOMINAL height while every instance is drawn at its own, so a cylinder built
  from it stands up to ~0.35 m proud of an oak's bark — an invisible wall. But
  the answer was never a better formula. A tree's bole is MEASURED out of the
  mesh flora draws for that instance (FloraCollision.h) and the drawn triangles
  below the crown base go into this same merged body. What stops you is bark
  you can see, at every maturity, with no second formula to drift.
- WHY NO PER-TRUNK BODY. A capsule per trunk is the obvious shape and it costs
  ~7 200 resident bodies at 44 trees/ha x 25 chunks, against a Jolt world sized
  for 16 384 — a budget with no room for logs, doors or NPCs, and one that a
  single increase of CHUNK_LOAD_RADIUS would blow. Folding the boles into the
  chunk mesh that already exists costs ZERO bodies and ~50 triangles a tree.
- BUSHES ARE NOT IN THE BODY AT ALL. They populate BrushField instead: a shrub
  you cannot pass is a fence and one you cannot feel is a painting, so a bush
  costs speed. See PlayerMovement's brush factor — the same speed factor wading
  already uses, not a second slowdown system.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic iteration only (Rule 13.2): the body map is ordered, never a
  hash map, so bodies are created in the same order on every run.
- Never hardcode a placement number here; the drawn placement is authoritative
  and its constants come from NUMBERS via dfn::config.
*/

#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/FloraCollision.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/world/sources/Chunk.h"

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
    // The measured-collider memo. It lives HERE rather than in a file-static so
    // the systems stay stateless (Rule 9) and a test can start from empty.
    FloraCollisionCache flora_cache;
    // Budget instrumentation, updated as bodies are built. A budget nobody
    // reads is a budget in different units from the thing it limits (Rule 42):
    // these are counted in the same triangles the body is made of.
    uint64_t last_chunk_triangles = 0;
    uint64_t resident_triangles = 0;
    // Plant instances that became SOLID geometry, and shrubs that became drag
    // discs. Counted rather than inferred from the body count: "one body per
    // chunk" says nothing about how many trees are in it, and the trees are
    // what the budget argument is about.
    uint64_t solid_plants = 0;
    uint64_t drag_plants = 0;
};

// One shrub, as physics sees it: a vertical disc you walk INTO, not around.
// No body, no shape, no broadphase entry — a bush's whole physical existence.
struct BrushDisc {
    glm::vec2 center{0.0f}; // world x/z
    float radius = 0.0f;    // meters, measured off the drawn mesh
    float top = 0.0f;       // world y of the foliage top
    float base = 0.0f;      // world y of the stem base
};

// World RESOURCE: the drag volumes of every resident chunk. Kept per chunk so
// it is created and destroyed by exactly the same residency reconcile as the
// bodies — brush cannot outlive its chunk or arrive before it.
struct BrushField {
    struct Chunk {
        world::ChunkCoord coord{};
        std::vector<BrushDisc> discs;
    };
    std::map<uint64_t, Chunk> chunks;
};

// How thoroughly a point stands inside brush: 0 = clear, 1 = the middle of the
// thickest shrub there. `feet_y` is the capsule bottom; a bush is only in the
// way if the walker's legs are actually inside it, so a player on a ledge above
// a shrub is not slowed by it.
//
// Returns the DENSITY, not a speed: what a density costs is a movement
// decision and lives with the other one, next to wading (Rule 35).
[[nodiscard]] float brush_density_at(const BrushField& field, const glm::vec3& feet,
                                     float body_radius);

// Convenience for the movement wrappers: the World's field, or 0 if the world
// has none (a test world, a dungeon) — which is "no brush", not a failure.
[[nodiscard]] float brush_density_at(const ecs::World& world, const glm::vec3& feet,
                                     float body_radius);

// Once per fixed tick: builds a merged static body for every resident chunk
// that has props and does not have one yet, and destroys the bodies of chunks
// that are no longer resident. Cheap when nothing changed (a map walk).
void update_prop_collision(ecs::World& world, platform::IPhysics& physics,
                           const world::ChunkManager& chunks);

} // namespace dfn::gameplay
