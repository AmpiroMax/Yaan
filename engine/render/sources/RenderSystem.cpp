/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:03:00
Module: engine/render
File: engine/render/sources/RenderSystem.cpp

Responsibility:
- RenderSystem implementation: frame orchestration (begin/end), terrain chunk
  meshes, interpolated ECS submissions (Rule 12).

Key items:
- RenderSystem::init/shutdown/render/upload_terrain/drop_terrain.

Dependencies:
- Uses: TerrainMesher, FirstPersonCamera, IRenderer, ecs::World, shared
  components (engine/core/components).
- Used by: dfn_render target; driven by engine/app.

Notes:
- Stage 2: the ECS RenderMesh path is implemented but inert — no asset
  pipeline exists yet, so mesh_cache_ has no entries and unresolved asset ids
  are skipped. Terrain + debug draw carry the stage-2 acceptance (Q51).
- Frustum culling deferred to stage 3 with core's math types (documented in
  the spec).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 9: IRenderer arrives as a parameter, never stored.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 11:03:00: Stage 3 — terrain splat atlas + water texture via
  ProcTexture (dense asset ids, cached by params), frame RenderEnvironment
  (Materials.h), water plane (set_water/clear_water, DFN_WATER debug env),
  visual clock for water scroll.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/ProcTexture.h"
#include "engine/render/sources/TerrainMesher.h"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::render {

namespace {

// Cache keys for the procedural texture registry (params -> dense asset id).
constexpr uint64_t PROC_KEY_TERRAIN_ATLAS = 0x01;
constexpr uint64_t PROC_KEY_WATER = 0x02;

uint64_t proc_key(uint64_t kind, uint32_t size, uint32_t seed) {
    return (kind << 56) | (static_cast<uint64_t>(size) << 32) | seed;
}

glm::mat4 interpolated_transform(const components::PreviousTransform& prev,
                                 const components::Transform& curr, float alpha) {
    const glm::vec3 position = glm::mix(prev.position, curr.position, alpha);
    const glm::quat rotation = glm::slerp(prev.rotation, curr.rotation, alpha);
    const glm::vec3 scale = glm::mix(prev.scale, curr.scale, alpha);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
    return glm::scale(m, scale);
}

} // namespace

size_t RenderSystem::ChunkKeyHash::operator()(const glm::ivec2& v) const {
    // 2D grid hash: large odd multipliers, good spread for small coords.
    const auto x = static_cast<uint64_t>(static_cast<uint32_t>(v.x));
    const auto y = static_cast<uint64_t>(static_cast<uint32_t>(v.y));
    return static_cast<size_t>(x * 0x9E3779B97F4A7C15ull ^ (y * 0xC2B2AE3D27D4EB4Full));
}

uint32_t RenderSystem::procedural_texture_asset(platform::IRenderer& renderer,
                                                uint64_t key, uint32_t width,
                                                uint32_t height,
                                                const uint8_t* pixels) {
    if (const auto it = proc_texture_ids_.find(key); it != proc_texture_ids_.end()) {
        return it->second; // cached by parameters
    }
    const platform::TextureHandle handle = renderer.create_texture(
        width, height, platform::TextureFormat::RGBA8,
        {pixels, static_cast<size_t>(width) * height * 4});
    if (!handle.valid()) {
        return 0;
    }
    const uint32_t asset_id = next_texture_asset_++;
    texture_cache_.emplace(asset_id, handle.id);
    proc_texture_ids_.emplace(key, asset_id);
    return asset_id;
}

bool RenderSystem::init(platform::IRenderer& renderer) {
    terrain_program_ = renderer.load_program("terrain").id;
    unlit_program_ = renderer.load_program("unlit").id;
    water_program_ = renderer.load_program("water").id;

    // Procedural textures (Q4в): generated in code, uploaded once, cached.
    {
        const auto atlas =
            generate_terrain_atlas(LOOKDEV_ATLAS_CELL_PX, LOOKDEV_TEXTURE_SEED);
        atlas_texture_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_TERRAIN_ATLAS, LOOKDEV_ATLAS_CELL_PX,
                     LOOKDEV_TEXTURE_SEED),
            LOOKDEV_ATLAS_CELL_PX * 2, LOOKDEV_ATLAS_CELL_PX * 2, atlas.data());

        ProcTextureDesc water_desc;
        water_desc.kind = ProcTextureKind::WATER;
        water_desc.size = LOOKDEV_WATER_TEX_PX;
        water_desc.seed = LOOKDEV_TEXTURE_SEED;
        const auto water = generate_proc_texture(water_desc);
        water_texture_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_WATER, LOOKDEV_WATER_TEX_PX, LOOKDEV_TEXTURE_SEED),
            LOOKDEV_WATER_TEX_PX, LOOKDEV_WATER_TEX_PX, water.data());
    }

    environment_ = make_default_environment();
    clock_start_ = std::chrono::steady_clock::now();

    // Debug water toggle (stage 3): DFN_WATER=<height_m> covers the testbed
    // area. Proper placement is design-doc-driven via set_water from app/editor.
    if (const char* wenv = std::getenv("DFN_WATER"); wenv != nullptr && *wenv != '\0') {
        float height = 0.0f;
        if (std::sscanf(wenv, "%f", &height) == 1) {
            const float chunk = static_cast<float>(config::CHUNK_SIZE);
            set_water(renderer, height, {chunk * 2.0f, chunk * 2.0f}, chunk * 4.0f);
        } else {
            std::fprintf(stderr, "[render] malformed DFN_WATER '%s' (want meters)\n",
                         wenv);
        }
    }

    return terrain_program_ != 0 && unlit_program_ != 0 && water_program_ != 0;
}

void RenderSystem::shutdown(platform::IRenderer& renderer) {
    clear_water(renderer);
    for (const auto& [coord, mesh_id] : terrain_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    terrain_meshes_.clear();
    for (const auto& [asset, mesh_id] : mesh_cache_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    mesh_cache_.clear();
    for (const auto& [asset, tex_id] : texture_cache_) {
        renderer.destroy_texture(platform::TextureHandle{tex_id});
    }
    texture_cache_.clear();
    proc_texture_ids_.clear();
    atlas_texture_asset_ = 0;
    water_texture_asset_ = 0;
    next_texture_asset_ = 1;
    renderer.destroy_program(platform::ProgramHandle{terrain_program_});
    renderer.destroy_program(platform::ProgramHandle{unlit_program_});
    renderer.destroy_program(platform::ProgramHandle{water_program_});
    terrain_program_ = 0;
    unlit_program_ = 0;
    water_program_ = 0;
}

void RenderSystem::render(ecs::World& world, platform::IRenderer& renderer,
                          const FirstPersonCamera& camera, float alpha) {
    renderer.begin_frame(camera.view(alpha), camera.proj());

    // Frame environment: visual clock drives water/UV animation only (never
    // simulation — Rule 12 keeps gameplay off the wall clock; this is render).
    environment_.time_seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - clock_start_).count();
    renderer.set_environment(environment_);

    // Terrain: world-space meshes, identity transform, splat atlas bound.
    const glm::mat4 identity(1.0f);
    const platform::ProgramHandle terrain{terrain_program_};
    platform::TextureHandle atlas{};
    if (const auto it = texture_cache_.find(atlas_texture_asset_);
        it != texture_cache_.end()) {
        atlas.id = it->second;
    }
    for (const auto& [coord, mesh_id] : terrain_meshes_) {
        renderer.submit(platform::MeshHandle{mesh_id}, terrain, identity, atlas);
    }

    // ECS renderables: interpolated fixed-step transforms (Rule 12). Inert
    // until the asset pipeline fills mesh_cache_ (stage 3) — see header notes.
    const platform::ProgramHandle unlit{unlit_program_};
    world.view<components::Transform, components::PreviousTransform,
               components::RenderMesh>()
        .each([&](ecs::EntityId, components::Transform& curr,
                  components::PreviousTransform& prev, components::RenderMesh& rm) {
            const auto mesh_it = mesh_cache_.find(rm.mesh_asset);
            if (mesh_it == mesh_cache_.end()) {
                return; // asset not resident — nothing to draw
            }
            platform::TextureHandle texture{};
            const auto tex_it = texture_cache_.find(rm.texture_asset);
            if (tex_it != texture_cache_.end()) {
                texture.id = tex_it->second;
            }
            renderer.submit(platform::MeshHandle{mesh_it->second}, unlit,
                            interpolated_transform(prev, curr, alpha), texture);
        });

    // Water: transparent, so submitted after all opaques (the backend renders
    // the scene view sequentially and gives "water" a no-depth-write blend).
    if (water_mesh_ != 0) {
        platform::TextureHandle water_tex{};
        if (const auto it = texture_cache_.find(water_texture_asset_);
            it != texture_cache_.end()) {
            water_tex.id = it->second;
        }
        renderer.submit(platform::MeshHandle{water_mesh_},
                        platform::ProgramHandle{water_program_}, identity, water_tex);
    }

    renderer.end_frame();
}

void RenderSystem::set_water(platform::IRenderer& renderer, float height_m,
                             glm::vec2 center_xz, float half_extent_m) {
    clear_water(renderer);

    // One quad; uv in water-tile units so the texture repeats every
    // LOOKDEV_WATER_UV_TILE_M meters (sampler wraps, shader scrolls).
    const float x0 = center_xz.x - half_extent_m;
    const float x1 = center_xz.x + half_extent_m;
    const float z0 = center_xz.y - half_extent_m;
    const float z1 = center_xz.y + half_extent_m;
    const float inv_tile = 1.0f / LOOKDEV_WATER_UV_TILE_M;
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const std::array<platform::Vertex, 4> vertices{{
        {{x0, height_m, z0}, up, {x0 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z0}, up, {x1 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x0, height_m, z1}, up, {x0 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z1}, up, {x1 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
    }};
    const std::array<uint32_t, 6> indices{0, 3, 1, 0, 2, 3}; // CCW from +Y
    const platform::MeshHandle handle = renderer.create_mesh(vertices, indices);
    if (!handle.valid()) {
        return;
    }
    water_mesh_ = handle.id;
    // Beaches: sand band sits just above the waterline (tunable via environment()).
    environment_.sand_height_m = height_m + LOOKDEV_SAND_BLEND_M * 0.5f;
}

void RenderSystem::clear_water(platform::IRenderer& renderer) {
    if (water_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{water_mesh_});
        water_mesh_ = 0;
    }
}

void RenderSystem::upload_terrain(platform::IRenderer& renderer,
                                  const math::HeightFieldView& field) {
    const TerrainMeshData data = build_terrain_mesh(field);
    if (data.vertices.empty()) {
        return;
    }
    const platform::MeshHandle handle = renderer.create_mesh(data.vertices, data.indices);
    if (!handle.valid()) {
        return;
    }
    // Idempotent per coord: replace (and free) any previous upload.
    const auto it = terrain_meshes_.find(field.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second});
        it->second = handle.id;
    } else {
        terrain_meshes_.emplace(field.chunk_coord, handle.id);
    }
}

void RenderSystem::drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    const auto it = terrain_meshes_.find(chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second});
        terrain_meshes_.erase(it);
    }
}

} // namespace dfn::render
