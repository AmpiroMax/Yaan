/*
Created: 09:08:2026 - 22:21:30
Last updated: 09:08:2026 - 22:21:30
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
/*
UPD:
- 09:08:2026 - 22:21:30: Created — buildings and boulders become solid.
*/

#include "engine/gameplay/sources/PropCollision.h"

#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/physics/sources/CollisionLayers.h"
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

// Boulders. Trees are deliberately absent — see the header note on why an
// approximate trunk radius is worse than none.
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
    auto& state = world.resource<PropCollisionState>();

    // 1. Create what is resident and missing. Iterating the resident list (not
    //    the body map) keeps creation order tied to the streaming order.
    std::vector<uint64_t> resident;
    for (const world::ChunkCoord coord : chunks.loaded_chunks()) {
        const uint64_t key = world::chunk_group(coord);
        resident.push_back(key);
        if (state.bodies.contains(key)) {
            continue;
        }
        render::MeshData mesh;
        append_sites(mesh, world, coord);
        append_boulders(mesh, chunks, coord);
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
        }
    }

    // 2. Destroy what is no longer resident.
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
}

} // namespace dfn::gameplay
