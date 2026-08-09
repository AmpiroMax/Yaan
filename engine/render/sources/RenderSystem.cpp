/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 20:46:00
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
- 09:08:2026 - 11:57:20: Stage 3b — surface-truth terrain upload, scatter
  batches (trees + GRASS_VIEW_DISTANCE micro tiles), per-body water (lake
  planes + river ribbons), site placeholder meshes under blessed ids 1..7,
  ECS submissions on the "prop" program.
- 09:08:2026 - 17:33:00: Map screen: overlay quad + per-frame canvas texture
  (draw_overlay), map_.note_chunk on terrain upload, map_.note_site in the ECS
  pass (before the mesh lookup, so the mesh-less castle ids 8..11 still map),
  DFN_MAP=1 opens the map at init for the tour evidence shot.
- 09:08:2026 - 19:42:00: upload_terrain_voxel (see the header UPD).
- 09:08:2026 - 19:52:00: DFN_TIME re-applied per frame (see the header UPD).
- 09:08:2026 - 20:21:13: Foliage pass (see the header UPD): "foliage" program,
  leaf mask atlas upload, second scatter submit. Flora agent, lead-granted
  Rule 25 exception.
- 09:08:2026 - 20:46:00: collect_point_lights — CarriedLight + Transform become
  the frame's point lights (interpolated, hand offset rotated by body yaw,
  first two flagged for cube shadows), plus DFN_TORCH / DFN_DARK hooks.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/ProcTexture.h"
#include "engine/render/sources/ScatterBatcher.h"
#include "engine/render/sources/SkyModel.h"
#include "engine/render/sources/TerrainMesher.h"
#include "engine/render/sources/VoxelMesher.h"
#include "engine/render/sources/Tour.h"
#include "engine/render/sources/WaterMesher.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

namespace dfn::render {

namespace {

// Cache keys for the procedural texture registry (params -> dense asset id).
constexpr uint64_t PROC_KEY_TERRAIN_ATLAS = 0x01;
constexpr uint64_t PROC_KEY_WATER = 0x02;
constexpr uint64_t PROC_KEY_LEAF_ATLAS = 0x03;

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
    prop_program_ = renderer.load_program("prop").id;
    foliage_program_ = renderer.load_program("foliage").id;

    // Placeholder site meshes under the lead-blessed RenderMesh ids 1..7
    // (dwelling..tower_ruin) — chunk streaming attaches exactly these ids to
    // site entities, so they render with no further wiring.
    for (uint32_t id = SITE_MESH_ID_FIRST; id <= SITE_MESH_ID_LAST; ++id) {
        const MeshData data = build_site_mesh(id);
        if (data.vertices.empty()) {
            continue;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(data.vertices, data.indices);
        if (handle.valid()) {
            mesh_cache_.emplace(id, handle.id);
        }
    }

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

        // The leaf mask atlas (flora's zone): tile columns are leaf SHAPES,
        // tile rows are leaf COLOURS, so a card's uv already carries both and
        // no vertex byte is spent on colour. A SEASON CHANGE IS THIS CALL
        // AGAIN with a different enum plus a re-upload — no mesh is rebuilt,
        // no chunk is re-baked, no per-card jitter is invalidated.
        const LeafAtlas leaves =
            generate_leaf_atlas(LEAF_ATLAS_TILE_PX, FloraSeason::Summer);
        leaf_texture_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_LEAF_ATLAS, LEAF_ATLAS_TILE_PX,
                     static_cast<uint32_t>(FloraSeason::Summer)),
            leaves.width, leaves.height, leaves.pixels.data());
    }

    // Screen overlay quad (map screen now, menus later): a unit quad in model
    // space, placed in front of the camera per frame by draw_overlay. Row 0 of
    // a canvas is the TOP of the screen, hence v = 0 on the +y corners.
    {
        const glm::vec3 face{0.0f, 0.0f, 1.0f};
        const std::array<platform::Vertex, 4> vertices{{
            {{-1.0f, 1.0f, 0.0f}, face, {0.0f, 0.0f}, 0xFFFFFFFFu},
            {{1.0f, 1.0f, 0.0f}, face, {1.0f, 0.0f}, 0xFFFFFFFFu},
            {{1.0f, -1.0f, 0.0f}, face, {1.0f, 1.0f}, 0xFFFFFFFFu},
            {{-1.0f, -1.0f, 0.0f}, face, {0.0f, 1.0f}, 0xFFFFFFFFu},
        }};
        const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
        overlay_mesh_ = renderer.create_mesh(vertices, indices).id;
    }
    // Canvas size = the internal target, so one canvas pixel is one screen
    // pixel. The app should confirm it via set_internal_resolution (settings.cfg
    // can override both the constant and the env var).
    internal_res_ = Tour::internal_res_from_env(
        {static_cast<uint32_t>(config::INTERNAL_RES_W),
         static_cast<uint32_t>(config::INTERNAL_RES_H)});
    // Verification hook (Rule 27): the screenshot tour cannot press M, so
    // DFN_MAP=1 opens the map from the first frame.
    if (const char* menv = std::getenv("DFN_MAP"); menv != nullptr && menv[0] == '1') {
        map_.set_open(true);
    }

    environment_ = make_default_environment();
    // Verification hook (Rule 27): DFN_TIME=<0..1 day fraction> freezes the sky
    // at an hour (0 = midnight, 0.5 = noon), DFN_MOON=<0..1> sets the phase.
    // In play the app drives the same function every frame from its clock.
    if (const char* tenv = std::getenv("DFN_TIME"); tenv != nullptr && *tenv != '\0') {
        float day = 0.5f;
        float phase = 0.5f;
        if (std::sscanf(tenv, "%f", &day) == 1) {
            if (const char* menv = std::getenv("DFN_MOON"); menv != nullptr) {
                std::sscanf(menv, "%f", &phase);
            }
            sky_frozen_ = true;
            frozen_day_ = day;
            frozen_moon_phase_ = phase;
            apply_sky_time(environment_, day, phase);
        }
    }
    // Verification hooks for the interior shoot (Rule 27). DFN_TORCH=1 lights a
    // carried flame at the camera's hand while the tour holds the player still;
    // DFN_DARK=<0..1> pins the authored darkness the app drives in play. Both
    // are screenshot hooks — the shipping paths are gameplay's CarriedLight and
    // the app's darkness ramp, and the torch hook stands down as soon as a real
    // CarriedLight exists.
    if (const char* tor = std::getenv("DFN_TORCH"); tor != nullptr && tor[0] != '\0') {
        torch_debug_ = tor[0] != '0';
        // "2" stands the flame off ahead of the camera (the brazier probe, see
        // collect_point_lights) instead of holding it at the hand.
        torch_ahead_m_ = tor[0] == '2' ? 6.0f : 0.0f;
    }
    // A/B hook: the same frame with the carried light's cube shadows OFF. A
    // shadow is only provable against its own absence, and this is also the
    // switch that measures what the cube pass costs.
    if (const char* nps = std::getenv("DFN_NO_POINT_SHADOW");
        nps != nullptr && nps[0] == '1') {
        point_shadows_off_ = true;
    }
    if (const char* denv = std::getenv("DFN_DARK"); denv != nullptr && *denv != '\0') {
        float dark = 0.0f;
        if (std::sscanf(denv, "%f", &dark) == 1) {
            dark_frozen_ = true;
            frozen_darkness_ = dark < 0.0f ? 0.0f : (dark > 1.0f ? 1.0f : dark);
        }
    }
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

    // foliage_program_ is NOT required for init to succeed: the null backend
    // hands out valid ids for everything, but a platform without the embedded
    // shader pair would otherwise take the whole renderer down over leaves.
    return terrain_program_ != 0 && unlit_program_ != 0 && water_program_ != 0
        && prop_program_ != 0;
}

void RenderSystem::shutdown(platform::IRenderer& renderer) {
    clear_water(renderer);
    clear_water_bodies(renderer);
    if (overlay_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{overlay_mesh_});
        overlay_mesh_ = 0;
    }
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    for (const auto& [coord, mesh_id] : terrain_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    terrain_meshes_.clear();
    for (auto& [coord, scatter] : scatter_meshes_) {
        if (scatter.trees_mesh_id != 0) {
            renderer.destroy_mesh(platform::MeshHandle{scatter.trees_mesh_id});
        }
        if (scatter.foliage_mesh_id != 0) {
            renderer.destroy_mesh(platform::MeshHandle{scatter.foliage_mesh_id});
        }
        for (const MicroTileRes& tile : scatter.micro) {
            renderer.destroy_mesh(platform::MeshHandle{tile.mesh_id});
        }
    }
    scatter_meshes_.clear();
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
    leaf_texture_asset_ = 0;
    next_texture_asset_ = 1;
    renderer.destroy_program(platform::ProgramHandle{terrain_program_});
    renderer.destroy_program(platform::ProgramHandle{unlit_program_});
    renderer.destroy_program(platform::ProgramHandle{water_program_});
    renderer.destroy_program(platform::ProgramHandle{prop_program_});
    renderer.destroy_program(platform::ProgramHandle{foliage_program_});
    terrain_program_ = 0;
    unlit_program_ = 0;
    water_program_ = 0;
    prop_program_ = 0;
    foliage_program_ = 0;
}

void RenderSystem::render(ecs::World& world, platform::IRenderer& renderer,
                          const FirstPersonCamera& camera, float alpha) {
    renderer.begin_frame(camera.view(alpha), camera.proj());

    // Frame environment: visual clock drives water/UV animation only (never
    // simulation — Rule 12 keeps gameplay off the wall clock; this is render).
    environment_.time_seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - clock_start_).count();
    // Screenshot determinism: the app writes the sky from its own clock every
    // frame, so the frozen hour has to be re-asserted here, after it.
    if (sky_frozen_) {
        apply_sky_time(environment_, frozen_day_, frozen_moon_phase_);
    }
    // Carried lights (the torch) are gathered from the ECS every frame, AFTER
    // the sky: apply_sky_time never touches the light array, and the light has
    // to be in the environment before set_environment or the backend builds
    // this frame's cube faces around a stale flame.
    collect_point_lights(world, camera, alpha);
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

    // Scatter batches (stage 3b): trees always; bush/stone micro tiles only
    // within GRASS_VIEW_DISTANCE of the eye (LANDSCAPE §2.3 micro contract).
    const platform::ProgramHandle prop{prop_program_};
    const glm::vec3 eye = camera.interpolated_pose(alpha).position;
    const auto micro_range = static_cast<float>(config::GRASS_VIEW_DISTANCE);
    const platform::ProgramHandle foliage{foliage_program_};
    platform::TextureHandle leaf_atlas{};
    if (const auto it = texture_cache_.find(leaf_texture_asset_);
        it != texture_cache_.end()) {
        leaf_atlas.id = it->second;
    }
    for (const auto& [coord, scatter] : scatter_meshes_) {
        if (scatter.trees_mesh_id != 0) {
            renderer.submit(platform::MeshHandle{scatter.trees_mesh_id}, prop,
                            identity);
        }
        // Leaf cards: their own program (alpha test + wind + leaf
        // translucency) and their own texture, which the backend also binds on
        // the shadow-cutout caster so the canopy punches its holes through the
        // depth map instead of casting solid rectangles.
        if (scatter.foliage_mesh_id != 0 && foliage_program_ != 0) {
            renderer.submit(platform::MeshHandle{scatter.foliage_mesh_id}, foliage,
                            identity, leaf_atlas);
        }
        for (const MicroTileRes& tile : scatter.micro) {
            const glm::vec2 d = tile.center_xz - glm::vec2{eye.x, eye.z};
            const float max_dist = micro_range + tile.radius_m;
            if (glm::dot(d, d) <= max_dist * max_dist) {
                renderer.submit(platform::MeshHandle{tile.mesh_id}, prop, identity);
            }
        }
    }

    // ECS renderables: interpolated fixed-step transforms (Rule 12). Site
    // entities carry the blessed placeholder mesh ids 1..7 (registered at
    // init); drawn lit+fogged via "prop".
    world.view<components::Transform, components::PreviousTransform,
               components::RenderMesh>()
        .each([&](ecs::EntityId, components::Transform& curr,
                  components::PreviousTransform& prev, components::RenderMesh& rm) {
            // Map discovery: a site is remembered as soon as its chunk is
            // resident, BEFORE the mesh lookup — the castle parts (ids 8..11)
            // have no placeholder mesh yet but must still appear on the map.
            map_.note_site(rm.mesh_asset, curr.position);
            const auto mesh_it = mesh_cache_.find(rm.mesh_asset);
            if (mesh_it == mesh_cache_.end()) {
                return; // asset not resident — nothing to draw
            }
            platform::TextureHandle texture{};
            const auto tex_it = texture_cache_.find(rm.texture_asset);
            if (tex_it != texture_cache_.end()) {
                texture.id = tex_it->second;
            }
            renderer.submit(platform::MeshHandle{mesh_it->second}, prop,
                            interpolated_transform(prev, curr, alpha), texture);
        });

    // Water: transparent, so submitted after all opaques (the backend renders
    // the scene view sequentially and gives "water" a no-depth-write blend).
    platform::TextureHandle water_tex{};
    if (const auto it = texture_cache_.find(water_texture_asset_);
        it != texture_cache_.end()) {
        water_tex.id = it->second;
    }
    const platform::ProgramHandle water{water_program_};
    for (const uint32_t mesh_id : water_body_meshes_) {
        renderer.submit(platform::MeshHandle{mesh_id}, water, identity, water_tex);
    }
    if (water_mesh_ != 0) { // debug fallback plane (set_water / DFN_WATER)
        renderer.submit(platform::MeshHandle{water_mesh_}, water, identity,
                        water_tex);
    }

    // Map screen: last submit of the frame, opaque, covering everything. The
    // world behind it is still drawn (a few hundred microseconds at these
    // budgets) so toggling the map is instant and needs no app-loop change.
    if (map_.open()) {
        const CameraPose pose = camera.interpolated_pose(alpha);
        draw_overlay(renderer,
                     map_.compose(internal_res_.x, internal_res_.y, pose.position,
                                  pose.yaw),
                     camera, alpha);
    }

    renderer.end_frame();
}

void RenderSystem::collect_point_lights(ecs::World& world,
                                        const FirstPersonCamera& camera,
                                        float alpha) {
    uint32_t count = 0;
    const auto add = [&](const glm::vec3& position, float radius,
                         const glm::vec3& color) {
        if (count >= platform::MAX_POINT_LIGHTS || radius <= 0.0f) {
            return;
        }
        platform::PointLight& out = environment_.point_lights[count];
        out.position = position;
        out.radius_m = radius;
        out.color = color;
        // WHICH lights cast is render's decision, never gameplay's: the first
        // MAX_SHADOW_POINT_LIGHTS get a cube map and the rest light without
        // one. Ordering is collection order, i.e. the carried torch — the one
        // light the player owns — is always among the shadowed ones.
        out.casts_shadow = count < platform::MAX_SHADOW_POINT_LIGHTS
                        && !point_shadows_off_;
        ++count;
    };

    world.view<components::CarriedLight, components::Transform>().each(
        [&](ecs::EntityId id, components::CarriedLight& light,
            components::Transform& curr) {
            if (!light.active) {
                return; // held but not lit — gameplay keeps the component
            }
            // Interpolate like any other renderable (Rule 12); a light that
            // moved at the fixed rate would strobe against 60+ fps geometry.
            glm::vec3 position = curr.position;
            glm::quat rotation = curr.rotation;
            if (const auto* prev = world.get<components::PreviousTransform>(id)) {
                position = glm::mix(prev->position, curr.position, alpha);
                rotation = glm::slerp(prev->rotation, curr.rotation, alpha);
            }
            // The offset is CARRIER-LOCAL: sim writes the hand, ~1.45 m above
            // the feet and 0.35 m to the right, and rotating it by the body
            // yaw is what makes the shadows swing when the player turns.
            const float radius = light.radius_m > 0.0f ? light.radius_m
                                                       : TORCH_RADIUS_M;
            glm::vec3 color = TORCH_COLOR;
            if (light.color_rgb != 0u) {
                color = {static_cast<float>((light.color_rgb >> 16) & 0xFFu) / 255.0f,
                         static_cast<float>((light.color_rgb >> 8) & 0xFFu) / 255.0f,
                         static_cast<float>(light.color_rgb & 0xFFu) / 255.0f};
            }
            add(position + rotation * light.offset, radius, color);
        });

    // Verification hook only (Rule 27): the tour freezes the player and no
    // entity carries a torch during a screenshot run, so DFN_TORCH=1 lights
    // one at the camera's hand. It stands down the moment a real CarriedLight
    // exists, so it can never quietly become the shipping path.
    if (torch_debug_ && count == 0) {
        const CameraPose pose = camera.interpolated_pose(alpha);
        const glm::vec3 right = camera.right(alpha);
        if (torch_ahead_m_ > 0.0f) {
            // BRAZIER PROBE (DFN_TORCH=2): the light stands away from the eye,
            // in front of the camera. A carried flame sits 0.35 m from the eye,
            // so nearly every shadow it casts hides BEHIND its own caster —
            // true of any real hand-held light and the reason interiors, not
            // open ground, are its acceptance test. Standing the same light off
            // at a distance puts casters between eye and flame, which is what
            // makes the cube map's work visible in one open-ground frame.
            const glm::vec3 fwd = camera.forward(alpha);
            const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                                  + glm::vec3{1e-4f, 0.0f, 0.0f});
            add(pose.position + flat * torch_ahead_m_, TORCH_RADIUS_M, TORCH_COLOR);
            environment_.point_light_count = count;
            if (dark_frozen_) {
                environment_.ambient_darkness = frozen_darkness_;
            }
            return;
        }
        // Flattened forward: a hand does not rise when the eyes look up, and
        // sim's real CarriedLight is yaw-only for exactly the same reason.
        const glm::vec3 fwd = camera.forward(alpha);
        const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                              + glm::vec3{1e-4f, 0.0f, 0.0f});
        add(pose.position + right * 0.35f + flat * 0.15f
                - glm::vec3{0.0f, 0.25f, 0.0f},
            TORCH_RADIUS_M, TORCH_COLOR);
    }
    environment_.point_light_count = count;
    if (dark_frozen_) {
        environment_.ambient_darkness = frozen_darkness_;
    }
}

void RenderSystem::set_internal_resolution(uint32_t width, uint32_t height) {
    if (width > 0 && height > 0) {
        internal_res_ = {width, height};
    }
}

void RenderSystem::draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                                const FirstPersonCamera& camera, float alpha) {
    if (overlay_mesh_ == 0 || unlit_program_ == 0 || canvas.width() == 0) {
        return;
    }
    // One upload per frame: IRenderer has no texture update (frozen contract),
    // so the canvas texture is recreated. At 640x360 that is 900 KB — cheap,
    // and only while a screen is open.
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    const platform::TextureHandle texture =
        renderer.create_texture(canvas.width(), canvas.height(),
                                platform::TextureFormat::RGBA8, canvas.pixels());
    if (!texture.valid()) {
        return;
    }
    overlay_texture_ = texture.id;

    // Quad placed just past the near plane, sized to EXACTLY fill the frustum
    // there (half height = d * tan(fov/2)), so one canvas pixel lands on one
    // internal pixel and the point sampler stays crisp (an overscan factor
    // duplicated pixel columns and softened the map). Depth test LESS lets it
    // cover every earlier submit.
    const float depth = std::max(camera.near_plane(), 0.01f) * 1.5f;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const glm::mat4 model = glm::inverse(camera.view(alpha))
                          * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, -depth})
                          * glm::scale(glm::mat4(1.0f), {half_w, half_h, 1.0f});
    renderer.submit(platform::MeshHandle{overlay_mesh_},
                    platform::ProgramHandle{unlit_program_}, model, texture);
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
    upload_terrain(renderer, field, nullptr);
}

void RenderSystem::upload_terrain(platform::IRenderer& renderer,
                                  const math::HeightFieldView& field,
                                  const math::SurfaceFieldView* surface) {
    // Explored map: the chunk is baked into the map the moment it streams in,
    // and stays there after unload (drop_terrain frees the GPU mesh only).
    map_.note_chunk(field, surface);
    const TerrainMeshData data = build_terrain_mesh(field, surface);
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

void RenderSystem::upload_terrain_voxel(platform::IRenderer& renderer,
                                       const math::VoxelMeshView& mesh) {
    const TerrainMeshData data = build_voxel_terrain_mesh(mesh);
    if (data.vertices.empty()) {
        return; // solid or empty chunk
    }
    const platform::MeshHandle handle = renderer.create_mesh(data.vertices, data.indices);
    if (!handle.valid()) {
        return;
    }
    // Same key as the heightfield upload: whichever source ran last owns the
    // chunk, so switching the ferry over never draws both.
    const auto it = terrain_meshes_.find(mesh.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second});
        it->second = handle.id;
    } else {
        terrain_meshes_.emplace(mesh.chunk_coord, handle.id);
    }
}

void RenderSystem::drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    const auto it = terrain_meshes_.find(chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second});
        terrain_meshes_.erase(it);
    }
}

void RenderSystem::upload_scatter(platform::IRenderer& renderer,
                                  glm::ivec2 chunk_coord,
                                  std::span<const math::ScatterInstance> instances) {
    drop_scatter(renderer, chunk_coord); // idempotent per coord
    if (instances.empty()) {
        return;
    }
    const auto chunk_size = static_cast<float>(config::CHUNK_SIZE);
    const glm::vec2 origin{static_cast<float>(chunk_coord.x) * chunk_size,
                           static_cast<float>(chunk_coord.y) * chunk_size};
    ScatterBatches batches = build_scatter_batches(instances, origin, chunk_size);

    ChunkScatterRes res;
    if (!batches.trees.vertices.empty()) {
        const platform::MeshHandle handle =
            renderer.create_mesh(batches.trees.vertices, batches.trees.indices);
        res.trees_mesh_id = handle.id;
    }
    if (!batches.foliage.vertices.empty()) {
        const platform::MeshHandle handle =
            renderer.create_mesh(batches.foliage.vertices, batches.foliage.indices);
        res.foliage_mesh_id = handle.id;
    }
    for (const MicroTile& tile : batches.micro) {
        const platform::MeshHandle handle =
            renderer.create_mesh(tile.mesh.vertices, tile.mesh.indices);
        if (handle.valid()) {
            res.micro.push_back({tile.center_xz, tile.radius_m, handle.id});
        }
    }
    if (res.trees_mesh_id != 0 || res.foliage_mesh_id != 0 || !res.micro.empty()) {
        scatter_meshes_.emplace(chunk_coord, std::move(res));
    }
}

void RenderSystem::drop_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    const auto it = scatter_meshes_.find(chunk_coord);
    if (it == scatter_meshes_.end()) {
        return;
    }
    if (it->second.trees_mesh_id != 0) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.trees_mesh_id});
    }
    if (it->second.foliage_mesh_id != 0) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.foliage_mesh_id});
    }
    for (const MicroTileRes& tile : it->second.micro) {
        renderer.destroy_mesh(platform::MeshHandle{tile.mesh_id});
    }
    scatter_meshes_.erase(it);
}

void RenderSystem::set_water_bodies(platform::IRenderer& renderer,
                                    std::span<const math::LakePlane> lakes,
                                    std::span<const math::RiverStation> river_stations,
                                    std::span<const uint32_t> river_segment_offsets) {
    clear_water_bodies(renderer);

    const auto upload = [&](const MeshData& data) {
        if (data.vertices.empty()) {
            return;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(data.vertices, data.indices);
        if (handle.valid()) {
            water_body_meshes_.push_back(handle.id);
        }
    };

    for (const math::LakePlane& lake : lakes) {
        upload(build_lake_mesh(lake, LOOKDEV_WATER_UV_TILE_M,
                               LOOKDEV_WATER_EDGE_MARGIN_M));
    }
    // Segment i = stations [offsets[i], offsets[i+1]); a trailing offset equal
    // to the station count is tolerated but not required.
    const size_t station_count = river_stations.size();
    for (size_t i = 0; i < river_segment_offsets.size(); ++i) {
        const size_t begin = river_segment_offsets[i];
        const size_t end = i + 1 < river_segment_offsets.size()
                               ? river_segment_offsets[i + 1]
                               : station_count;
        if (begin >= end || end > station_count) {
            continue;
        }
        upload(build_river_mesh(river_stations.subspan(begin, end - begin),
                                LOOKDEV_WATER_UV_TILE_M,
                                LOOKDEV_WATER_EDGE_MARGIN_M));
    }
}

void RenderSystem::clear_water_bodies(platform::IRenderer& renderer) {
    for (const uint32_t mesh_id : water_body_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    water_body_meshes_.clear();
}

} // namespace dfn::render
