/*
Module: engine/gameplay
File: engine/gameplay/sources/PropCollision.cpp

Responsibility:
- Builds the merged per-chunk static body for buildings and boulders out of the
  triangles render draws for them.

Key items:
- update_prop_collision(): the reconcile pass.
- append_site / append_boulders: the two geometry sources.

Dependencies:
- Uses: PropCollision.h, core ecs/components, world ChunkManager + SiteMarker,
  render ProcMesh builders, engine/physics collision layers, generated constants.
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The DRAWN placement is authoritative; never invent a second one here.
*/

#include "engine/gameplay/sources/PropCollision.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/FloraCollision.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/render/sources/ProcFlora.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/SiteComponents.h"

namespace dfn::gameplay {

namespace {

// The drawn sink: a scattered prop is bedded into the ground by this fraction
// of its scale, so a boulder sits IN the slope rather than on it. Collision
// must use the same value or the solid rock floats above the visible one.
constexpr float SCATTER_SINK = static_cast<float>(config::SCATTER_GROUND_SINK_FRAC);

// How far outside a chunk's footprint a shrub rooted inside it can still reach.
// An engine internal, not a tuning row: the widest drag disc measured off any
// drawn shrub is 1.80 m (BigBush), so 4 m is that with the slack a measurement
// deserves. Too large only costs a few extra distance checks; too small would
// silently drop the brush on a chunk seam, which is the failure worth avoiding.
constexpr float BRUSH_QUERY_MARGIN = 4.0f;

// Appends `src` transformed by a full Transform (translation, rotation, uniform
// scale). Used for site entities, whose drawn orientation is the QUATERNION on
// their Transform — not a yaw scalar — so this is what render's ECS pass does.
void append_by_transform(render::MeshData& dst, const render::MeshData& src,
                         const components::Transform& xf) {
    const uint32_t base = static_cast<uint32_t>(dst.vertices.size());
    dst.vertices.reserve(dst.vertices.size() + src.vertices.size());
    for (const platform::Vertex& v : src.vertices) {
        platform::Vertex out = v;
        const glm::vec3 scaled{v.position.x * xf.scale.x, v.position.y * xf.scale.y,
                               v.position.z * xf.scale.z};
        out.position = xf.position + (xf.rotation * scaled);
        out.normal = xf.rotation * v.normal;
        dst.vertices.push_back(out);
    }
    dst.indices.reserve(dst.indices.size() + src.indices.size());
    for (const uint32_t i : src.indices) {
        dst.indices.push_back(base + i);
    }
}

// Buildings and other site props resident in this chunk. Their meshes are the
// placeholder structure meshes; when one of them gains a doorway, the doorway
// becomes walkable here for free.
void append_sites(render::MeshData& out, const ecs::World& world, world::ChunkCoord coord) {
    for (const ecs::EntityId id : world.entities_in_group(world::chunk_group(coord))) {
        const auto* marker = world.get<world::SiteMarker>(id);
        const auto* mesh = world.get<components::RenderMesh>(id);
        const auto* xf = world.get<components::Transform>(id);
        if (marker == nullptr || mesh == nullptr || xf == nullptr) {
            continue;
        }
        const render::MeshData src = render::build_site_mesh(mesh->mesh_asset);
        if (src.indices.empty()) {
            continue; // an id render has no mesh for: nothing drawn, nothing solid
        }
        append_by_transform(out, src, *xf);
    }
}

// PLANTS: the ones that stop you go into the merged mesh, the ones that only
// slow you down go into the drag field. One walk over the chunk's scatter feeds
// both, because they are the same question asked of every instance — "what is
// this thing, physically" — and asking it twice is how the two lists drift.
//
// NO GROUND SINK ON THIS PATH, and that is not an omission: flora's contract is
// that a plant mesh already stands on its own root flare and buries its own
// lower half, so the batcher draws it at `inst.position` untouched. Sinking it
// here would put solid bark 12 cm below visible bark on every tree in the
// world — the exact mirror of the boulder bug SCATTER_GROUND_SINK_FRAC exists
// to prevent.
void append_plants(render::MeshData& out, BrushField::Chunk& brush,
                   PropCollisionState& state, const world::ChunkManager& chunks,
                   world::ChunkCoord coord) {
    FloraCollisionCache& cache = state.flora_cache;
    for (const math::ScatterInstance& inst : chunks.scatter(coord)) {
        // The cheap question first. Three quarters of a chunk's scatter is
        // boulders and ground cover, and every one of them used to pay for a
        // variant lookup and a maturity draw before being thrown away.
        if (flora_solid_kind(inst.species) == FloraSolidKind::None) {
            continue;
        }
        const glm::vec2 xz{inst.position.x, inst.position.z};
        // Variant and maturity are asked of the SAME two functions the batcher
        // asks, so the collider is built for the tree that is drawn and not for
        // a sibling of it (Rule 35).
        const uint32_t variant = render::flora_variant_for(xz);
        const float maturity =
            flora_collision_maturity(inst.species, xz, inst.scale);
        const FloraSolid& solid = flora_solid(cache, inst.species, variant, maturity);
        switch (solid.kind) {
        case FloraSolidKind::None:
            break;
        case FloraSolidKind::Solid:
            render::append_transformed(out, solid.mesh, inst.position, inst.yaw, 1.0f);
            ++state.solid_plants;
            break;
        case FloraSolidKind::Drag:
            brush.discs.push_back(BrushDisc{.center = xz,
                                            .radius = solid.drag_radius,
                                            .top = inst.position.y + solid.drag_top,
                                            .base = inst.position.y});
            ++state.drag_plants;
            break;
        }
    }
}

// Boulders, which are not flora and keep their own drawn mesh + ground sink.
void append_boulders(render::MeshData& out, const world::ChunkManager& chunks,
                     world::ChunkCoord coord) {
    const render::MeshData stone = render::build_scatter_mesh(math::ScatterSpecies::Stone);
    if (stone.indices.empty()) {
        return;
    }
    for (const math::ScatterInstance& inst : chunks.scatter(coord)) {
        if (inst.species != math::ScatterSpecies::Stone) {
            continue;
        }
        const glm::vec3 position{inst.position.x,
                                 inst.position.y - SCATTER_SINK * inst.scale,
                                 inst.position.z};
        render::append_transformed(out, stone, position, inst.yaw, inst.scale);
    }
}

} // namespace

void update_prop_collision(ecs::World& world, platform::IPhysics& physics,
                           const world::ChunkManager& chunks) {
    if (!world.has_resource<PropCollisionState>()) {
        world.add_resource(PropCollisionState{});
    }
    if (!world.has_resource<BrushField>()) {
        world.add_resource(BrushField{});
    }
    auto& state = world.resource<PropCollisionState>();
    auto& brush_field = world.resource<BrushField>();

    // 1. Create what is resident and missing. Iterating the resident list (not
    //    the body map) keeps creation order tied to the streaming order.
    std::vector<uint64_t> resident;
    for (const world::ChunkCoord coord : chunks.loaded_chunks()) {
        const uint64_t key = world::chunk_group(coord);
        resident.push_back(key);
        // THE BRUSH FIELD IS THE "ALREADY BUILT" MARKER, and the body map is
        // not, which is a correctness point and not a style one. Every resident
        // chunk gets a brush entry, even an empty one; a chunk gets a BODY only
        // if it has something solid in it. Gating on the body map therefore
        // rebuilds every propless chunk's geometry on EVERY TICK, forever,
        // because the thing it is waiting for is never going to appear. (The
        // same read also loses a meadow of shrubs on bare ground: brush and no
        // body is a legitimate chunk, and reading residency off `bodies` would
        // silently skip it — absence presenting as a neutral state, the failure
        // that hid the missing site meshes for a whole stage.)
        if (brush_field.chunks.contains(key)) {
            continue;
        }
        BrushField::Chunk brush;
        brush.coord = coord;
        render::MeshData mesh;
        append_sites(mesh, world, coord);
        append_plants(mesh, brush, state, chunks, coord);
        append_boulders(mesh, chunks, coord);
        brush_field.chunks.emplace(key, std::move(brush));
        state.last_chunk_triangles = mesh.indices.size() / 3;
        if (mesh.indices.size() < 3) {
            continue; // a chunk with no props needs no body; not an error
        }

        std::vector<glm::vec3> positions;
        positions.reserve(mesh.vertices.size());
        for (const platform::Vertex& v : mesh.vertices) {
            positions.push_back(v.position);
        }

        platform::TerrainMeshDesc desc;
        desc.positions = positions;
        desc.indices = mesh.indices;
        desc.layer = physics::LAYER_STATIC;
        desc.user_data = 0; // merged: a hit resolves to the world, not to a prop
        const platform::PhysicsBodyHandle body = physics.create_terrain_mesh(desc);
        if (body.valid()) {
            state.bodies.emplace(key, body);
            state.resident_triangles += mesh.indices.size() / 3;
        }
    }

    // 2. Destroy what is no longer resident. Bodies and brush go together: they
    //    were built from the same chunk in the same pass, and a drag disc that
    //    survived its chunk would slow a player standing in an empty field.
    for (auto it = state.bodies.begin(); it != state.bodies.end();) {
        const bool still_resident =
            std::find(resident.begin(), resident.end(), it->first) != resident.end();
        if (still_resident) {
            ++it;
            continue;
        }
        physics.destroy_body(it->second);
        it = state.bodies.erase(it);
    }
    for (auto it = brush_field.chunks.begin(); it != brush_field.chunks.end();) {
        const bool still_resident =
            std::find(resident.begin(), resident.end(), it->first) != resident.end();
        it = still_resident ? std::next(it) : brush_field.chunks.erase(it);
    }
}

float brush_density_at(const BrushField& field, const glm::vec3& feet, float body_radius) {
    // THE THICKEST SHRUB DECIDES, rather than the sum of them. Brush overlaps
    // constantly — that is what a thicket is — and adding densities would make
    // an ordinary hedge row impassable while each bush in it stayed gentle. The
    // player's experience of "how deep in this am I" is the deepest single
    // thing they are in, so max() is the honest reduction.
    //
    // The tallest brush in the world is a few metres wide, so a chunk whose
    // footprint the walker is not standing within (plus that margin) cannot
    // hold a shrub they are inside. Measured at 0.0014 ms/query without this
    // filter over 2 725 discs, which is already nothing -- but the cost is
    // LINEAR in the resident set, and CHUNK_LOAD_RADIUS is a number somebody
    // will raise. This is what `BrushField::Chunk::coord` is for.
    constexpr float CHUNK = static_cast<float>(config::CHUNK_SIZE);
    const float margin = BRUSH_QUERY_MARGIN + body_radius;
    float density = 0.0f;
    for (const auto& [key, chunk] : field.chunks) {
        (void)key;
        const float x0 = static_cast<float>(chunk.coord.x) * CHUNK - margin;
        const float z0 = static_cast<float>(chunk.coord.z) * CHUNK - margin;
        if (feet.x < x0 || feet.x > x0 + CHUNK + 2.0f * margin || feet.z < z0 ||
            feet.z > z0 + CHUNK + 2.0f * margin) {
            continue;
        }
        for (const BrushDisc& disc : chunk.discs) {
            // Vertically: the legs must be in it. Above the foliage top there
            // is nothing to push through; below the base the walker is under
            // the shrub, in a hole or on the far side of a ledge.
            if (feet.y >= disc.top || feet.y + body_radius < disc.base) {
                continue;
            }
            const float dx = feet.x - disc.center.x;
            const float dz = feet.z - disc.center.y;
            const float dist = std::sqrt(dx * dx + dz * dz);
            const float reach = disc.radius + body_radius;
            if (reach <= 0.0f || dist >= reach) {
                continue;
            }
            // Linear from the rim to the middle: entering a bush costs nothing
            // at the first leaf and everything at its heart, so the player feels
            // themselves push IN rather than hit a speed wall at the edge.
            density = std::max(density, 1.0f - dist / reach);
        }
    }
    return std::clamp(density, 0.0f, 1.0f);
}

float brush_density_at(const ecs::World& world, const glm::vec3& feet, float body_radius) {
    if (!world.has_resource<BrushField>()) {
        return 0.0f; // a world with no brush field has no brush: not a failure
    }
    return brush_density_at(world.resource<BrushField>(), feet, body_radius);
}

} // namespace dfn::gameplay
