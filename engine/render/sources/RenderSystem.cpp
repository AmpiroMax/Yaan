/*
Created: 09:08:2026 - 00:45:00
Last updated: 10:08:2026 - 20:17:40
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
- 09:08:2026 - 21:14:00: FRUSTUM CULLING for terrain and scatter, with the
  shadow-caster exemption (visible_or_casting): chunk bounds are measured at
  upload, and an off-screen mesh within LOOKDEV_SHADOW_CASTER_KEEP_M is still
  submitted because the backend double-submits opaques into the sun map — a
  naive cull would delete shadows along with the geometry.
- 09:08:2026 - 22:33:00: DFN_NO_SCATTER=1 — the trees-off half of a silhouette
  A/B. A landmark verdict taken with the forest in frame is a verdict on the
  forest.
- 09:08:2026 - 22:36:47: A MISSING MESH IS LOUD (lead's instruction). Both
  silent paths that hid the invisible castle now print: a blessed site id with
  no geometry at init, and an unregistered asset id in the ECS pass (once per
  id, not per frame).
- 09:08:2026 - 22:44:28: MAP REGRESSION FIX — upload_terrain_voxel records the
  explored chunk. Since the ferry moved to the voxel path the map had been
  recording only chunks WITHOUT a voxel mesh, i.e. almost none.
- 09:08:2026 - 23:50:06: HUD layer (transparent overlay + DFN_FONT_PROBE specimen),
  water bodies merged into world-grid buckets and frustum-culled, per-path
  upload accounting, and a LOUD first-failure report — a terrain chunk that
  fails to upload used to `return;` in silence, which is the same "absence
  looks neutral" family as the invisible castle.
- 10:08:2026 - 00:20:00: The font probe forces the HUD layer on instead of riding on
  hud_visible_. The app now owns that flag (it wires the real interaction
  prompt) and the first frame after it landed came back EMPTY, with nothing
  saying the hook had been switched off.
- 10:08:2026 - 03:08:00: CLOUDS (W4): apply_wind + apply_clouds driven from
  render() each frame (apply_wind had NO live call site — the 0.0 default
  read as a calm day: absence presenting as neutral, the invisible-castle
  family); DFN_CLOUD / DFN_VISTIME hooks for the acceptance shoot.
- 10:08:2026 - 20:17:40: RenderMesh::mesh_asset 0 is the documented "none"
  sentinel and is produced deliberately (hidden bone segment, empty item
  slot), so the missing-asset warning was firing on correct code every
  launch — Rule 38's failure mode in a log rather than a test. Skipped
  before the lookup; a genuinely unregistered id still warns once.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/CloudModel.h"
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
#include "engine/render/sources/WindModel.h"

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
constexpr uint64_t PROC_KEY_PATH_ATLAS = 0x04;

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

math::Aabb bounds_of(const std::vector<platform::Vertex>& vertices) {
    math::Aabb box;
    for (const platform::Vertex& v : vertices) {
        box.expand(v.position);
    }
    return box;
}

} // namespace

bool RenderSystem::visible_or_casting(const math::Frustum& frustum,
                                      const math::Aabb& box, const glm::vec3& eye) {
    if (!box.valid()) {
        return true; // no bounds measured: never cull blind
    }
    if (frustum.visible(box)) {
        return true;
    }
    // Off screen, but still a sun caster? The backend renders every opaque
    // submit into the shadow map, so culling here would delete the shadow with
    // the mesh. Cheap conservative test: sphere around the box centre.
    const glm::vec3 d = box.center() - eye;
    const float reach = LOOKDEV_SHADOW_CASTER_KEEP_M
                      + glm::length(box.half_extents());
    return glm::dot(d, d) <= reach * reach;
}

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
    overlay_program_ = renderer.load_program("overlay").id;
    path_program_ = renderer.load_program("path").id;

    // A LOGICAL PROGRAM THAT FAILS TO LOAD DRAWS NOTHING, SILENTLY. The
    // backend's submit early-returns on an unknown program id, so a missing
    // shader looks exactly like geometry that was never built — which is how
    // an hour goes into hunting a mesher that was correct all along.
    {
        const struct { const char* name; uint32_t id; } required[] = {
            {"terrain", terrain_program_}, {"unlit", unlit_program_},
            {"water", water_program_},     {"prop", prop_program_},
            {"foliage", foliage_program_}, {"overlay", overlay_program_},
            {"path", path_program_},
        };
        for (const auto& r : required) {
            if (r.id == 0) {
                std::fprintf(stderr,
                             "[render] PROGRAM \"%s\" FAILED TO LOAD — every draw "
                             "that uses it is silently dropped.\n", r.name);
            }
        }
    }

    // Placeholder site meshes under the lead-blessed RenderMesh ids (1..12:
    // dwelling..tower_ruin, then the castle mass) — chunk streaming attaches
    // exactly these ids to site entities, so they render with no further
    // wiring.
    //
    // A GAP IN THIS RANGE IS LOUD NOW, AND THAT IS THE POINT. It used to
    // `continue`: build_site_mesh returned an empty mesh for every id above 7,
    // the loop skipped it without a word, and the ECS pass below swallowed the
    // resulting cache miss just as quietly — so Harrowward was invisible in the
    // world for an entire stage, and (because sim builds collision from these
    // same triangles) intangible with it. Nothing anywhere said so, because
    // "no mesh" and "nothing to draw" were the same code path. Absence
    // presenting as a neutral state is this project's most expensive recurring
    // bug; a blessed id with no geometry must never again be a silent skip.
    for (uint32_t id = SITE_MESH_ID_FIRST; id <= SITE_MESH_ID_LAST; ++id) {
        const MeshData data = build_site_mesh(id);
        if (data.vertices.empty()) {
            std::fprintf(stderr,
                         "[render] BLESSED SITE MESH ID %u HAS NO GEOMETRY — "
                         "worldgen attaches this id to entities that will be "
                         "invisible AND uncollidable. Add it to "
                         "build_site_mesh or shrink SITE_MESH_ID_LAST.\n",
                         id);
            continue;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(data.vertices, data.indices);
        if (handle.valid()) {
            mesh_cache_.emplace(id, handle.id);
        } else {
            std::fprintf(stderr,
                         "[render] site mesh id %u failed to upload\n", id);
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

        // The §8.1 path atlas: cell index IS core's PathClass ordinal.
        const auto path_atlas =
            generate_path_atlas(LOOKDEV_ATLAS_CELL_PX, LOOKDEV_TEXTURE_SEED);
        path_atlas_asset_ = procedural_texture_asset(
            renderer,
            proc_key(PROC_KEY_PATH_ATLAS, LOOKDEV_ATLAS_CELL_PX,
                     LOOKDEV_TEXTURE_SEED),
            LOOKDEV_ATLAS_CELL_PX * 2, LOOKDEV_ATLAS_CELL_PX * 2, path_atlas.data());

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
    hud_.resize(internal_res_.x, internal_res_.y);
    // Verification hook (Rule 27): the screenshot tour cannot press M, so
    // DFN_MAP=1 opens the map from the first frame.
    if (const char* menv = std::getenv("DFN_MAP"); menv != nullptr && menv[0] == '1') {
        map_.set_open(true);
    }
    // Verification hook (Rule 27): DFN_FONT_PROBE=1 draws the font specimen
    // into the HUD every frame. It is the ONLY thing in engine/ that puts text
    // on screen without a caller, and it exists because the font's acceptance
    // frame must show every glyph plus the missing-glyph block plus unplated
    // text over real terrain. It stands down the instant the app draws a real
    // prompt, exactly like DFN_TORCH did.
    if (const char* fenv = std::getenv("DFN_FONT_PROBE");
        fenv != nullptr && fenv[0] == '1') {
        font_probe_ = true;
        hud_visible_ = true;
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
    if (const char* ns = std::getenv("DFN_NO_SCATTER"); ns != nullptr && ns[0] == '1') {
        scatter_off_ = true;
    }
    // Cloud hooks (Rule 27/30). DFN_CLOUD=<0..1> pins the coverage amount:
    // 0 is the CONTROL of the whole pass — the sheet, the cumulus and the
    // ground shadows must all vanish in one move because they are one field
    // (a shadow surviving cover 0 would be the two-copies defect made
    // visible). DFN_VISTIME=<seconds> pins the visual clock, which pins the
    // drift: two runs 30 s of pinned time apart are the deterministic
    // acceptance pair proving coverage moves along the wind.
    if (const char* cenv = std::getenv("DFN_CLOUD"); cenv != nullptr && *cenv != '\0') {
        float cover = 0.0f;
        if (std::sscanf(cenv, "%f", &cover) == 1) {
            cloud_pinned_ = true;
            frozen_cloud_cover_ =
                cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
        }
    }
    if (const char* venv = std::getenv("DFN_VISTIME"); venv != nullptr && *venv != '\0') {
        float t = 0.0f;
        if (std::sscanf(venv, "%f", &t) == 1) {
            vis_time_frozen_ = true;
            frozen_vis_time_ = t;
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

void RenderSystem::report_upload_failure(const char* what) {
    // ONE line per run, not one per failure: the tour produced 13903 of these
    // and the useful information is the first one plus the totals.
    if (upload_failure_reported_) {
        return;
    }
    upload_failure_reported_ = true;
    std::fprintf(stderr,
                 "[render] A %s FAILED TO UPLOAD and will not be drawn. The GPU "
                 "buffer budget is spent; everything created after this point is "
                 "missing from the world, silently. Counts so far: terrain %llu, "
                 "voxel %llu, scatter chunks %llu, scatter meshes %llu.\n",
                 what,
                 static_cast<unsigned long long>(uploads_.terrain),
                 static_cast<unsigned long long>(uploads_.voxel),
                 static_cast<unsigned long long>(uploads_.scatter_chunks),
                 static_cast<unsigned long long>(uploads_.scatter_meshes));
}

void RenderSystem::shutdown(platform::IRenderer& renderer) {
    if (uploads_.failed > 0 || std::getenv("DFN_MESH_STATS") != nullptr) {
        std::fprintf(stderr,
                     "[render] uploads: terrain %llu, voxel %llu, scatter chunks "
                     "%llu (%llu meshes), FAILED %llu.\n",
                     static_cast<unsigned long long>(uploads_.terrain),
                     static_cast<unsigned long long>(uploads_.voxel),
                     static_cast<unsigned long long>(uploads_.scatter_chunks),
                     static_cast<unsigned long long>(uploads_.scatter_meshes),
                     static_cast<unsigned long long>(uploads_.failed));
    }
    clear_water(renderer);
    clear_water_bodies(renderer);
    clear_path_surface(renderer);
    if (overlay_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{overlay_mesh_});
        overlay_mesh_ = 0;
    }
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    for (const auto& [coord, res] : terrain_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{res.mesh_id});
    }
    terrain_meshes_.clear();
    lod_.destroy_all(renderer);
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
    renderer.destroy_program(platform::ProgramHandle{path_program_});
    renderer.destroy_program(platform::ProgramHandle{water_program_});
    renderer.destroy_program(platform::ProgramHandle{prop_program_});
    renderer.destroy_program(platform::ProgramHandle{foliage_program_});
    terrain_program_ = 0;
    unlit_program_ = 0;
    water_program_ = 0;
    path_program_ = 0;
    path_atlas_asset_ = 0;
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
    // DFN_VISTIME pins the visual clock: wind envelope, water scroll and the
    // cloud drift all become pure functions of the pinned value, which is
    // what makes the two-timestamp drift pair deterministic evidence.
    if (vis_time_frozen_) {
        environment_.time_seconds = frozen_vis_time_;
    }
    // Screenshot determinism: the app writes the sky from its own clock every
    // frame, so the frozen hour has to be re-asserted here, after it.
    if (sky_frozen_) {
        apply_sky_time(environment_, frozen_day_, frozen_moon_phase_);
    }
    // THE SHARED WIND (W3) drives everything that moves: foliage sway, the
    // audio bed's gain (sim reads env.wind_strength), and the cloud drift.
    // Called HERE and not in the app: grep found no live apply_wind call
    // site anywhere — env.wind_strength sat at its 0.0 default and the zero
    // read as a calm day (absence presenting as neutral), so the model is
    // now driven from render's own frame path where it cannot be dropped.
    apply_wind(environment_, environment_.time_seconds);
    // Clouds (W4): ONE coverage field, drifting along the wind just applied.
    // The offset written here is read by BOTH samplers (sky sheet + ground
    // shadow); the state tuple (cover/cumulus/shadow) stays whatever the app
    // or the "scattered" defaults put there.
    if (cloud_pinned_) {
        environment_.cloud_cover = frozen_cloud_cover_;
        // Cumulus follows the pin to zero so DFN_CLOUD=0 empties the WHOLE
        // sky (the Rule 30 control), not just the sheet.
        if (frozen_cloud_cover_ <= 0.0f) {
            environment_.cloud_cumulus = 0.0f;
        }
    }
    apply_clouds(environment_, environment_.time_seconds);
    // Carried lights (the torch) are gathered from the ECS every frame, AFTER
    // the sky: apply_sky_time never touches the light array, and the light has
    // to be in the environment before set_environment or the backend builds
    // this frame's cube faces around a stale flame.
    collect_point_lights(world, camera, alpha);
    renderer.set_environment(environment_);

    // Frustum culling (core's math). Culling is NOT free of consequences here:
    // see visible_or_casting — off-screen meshes near the eye are kept because
    // they still cast into the sun shadow map.
    const math::Frustum frustum =
        math::Frustum::from_view_proj(camera.proj() * camera.view(alpha));
    const glm::vec3 cull_eye = camera.interpolated_pose(alpha).position;

    // Terrain: world-space meshes, identity transform, splat atlas bound.
    const glm::mat4 identity(1.0f);
    const platform::ProgramHandle terrain{terrain_program_};
    platform::TextureHandle atlas{};
    if (const auto it = texture_cache_.find(atlas_texture_asset_);
        it != texture_cache_.end()) {
        atlas.id = it->second;
    }
    for (const auto& [coord, res] : terrain_meshes_) {
        if (!visible_or_casting(frustum, res.bounds, cull_eye)) {
            continue;
        }
        renderer.submit(platform::MeshHandle{res.mesh_id}, terrain, identity, atlas);
    }

    // Coarse LOD nodes: the same program, the same atlas, the same splat — the
    // only difference is the sample step and the per-draw fade. They are
    // submitted AFTER the chunk terrain so that in the one case the two can
    // overlap (a streamed rectangle not aligned to the 128 m node grid) the
    // near, finer surface has already written depth.
    lod_.draw(renderer, frustum, terrain, atlas);

    // The §8.1 PATH SURFACE, drawn after the ground it lies on. Depth alone
    // would resolve the order (the tread sits PATH_GROOVE_DEPTH proud of the
    // flattened ground core sank for it), but the LOD nodes above may overlap
    // the chunk terrain, and a path is the one surface that must never lose
    // that tie. Never a shadow caster — see NON_CASTING_PROGRAMS.
    if (!path_meshes_.empty()) {
        const platform::ProgramHandle path{path_program_};
        platform::TextureHandle path_atlas{};
        if (const auto it = texture_cache_.find(path_atlas_asset_);
            it != texture_cache_.end()) {
            path_atlas.id = it->second;
        }
        // aux0 carries the material's tiles per metre: the path atlas has its
        // own scale and lives on a mesh whose uv is already in metres, so it
        // cannot ride on u_terrainTiles (which is per CHUNK).
        platform::DrawParams params;
        params.aux0 = PATH_TILES_PER_M;
        for (const WaterBucket& piece : path_meshes_) {
            if (!frustum.visible(piece.bounds)) {
                continue;
            }
            renderer.submit(platform::MeshHandle{piece.mesh_id}, path, identity,
                            path_atlas, params);
        }
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
        const bool chunk_visible =
            visible_or_casting(frustum, scatter.bounds, cull_eye);
        if (chunk_visible && scatter.trees_mesh_id != 0) {
            renderer.submit(platform::MeshHandle{scatter.trees_mesh_id}, prop,
                            identity);
        }
        // Leaf cards: their own program (alpha test + wind + leaf
        // translucency) and their own texture, which the backend also binds on
        // the shadow-cutout caster so the canopy punches its holes through the
        // depth map instead of casting solid rectangles.
        if (chunk_visible && scatter.foliage_mesh_id != 0 && foliage_program_ != 0) {
            renderer.submit(platform::MeshHandle{scatter.foliage_mesh_id}, foliage,
                            identity, leaf_atlas);
        }
        for (const MicroTileRes& tile : scatter.micro) {
            if (!chunk_visible) {
                break; // the whole chunk is behind us
            }
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
            // resident, BEFORE the mesh lookup, so a site with no mesh is
            // still discoverable on the map rather than doubly absent.
            map_.note_site(rm.mesh_asset, curr.position);
            // THE SENTINEL IS NOT A MISSING ASSET. `RenderMesh::mesh_asset`
            // documents 0 as "none" (engine/core/components), and it is
            // PRODUCED DELIBERATELY — a hidden bone segment, an empty item
            // slot. The warning below was firing on all of it, every launch,
            // which is Rule 38's failure mode moved from the test suite into
            // the log: a check that goes red on correct code does not get
            // argued with, it gets ignored, and then it cannot report the one
            // id that really is unregistered either. Drawing nothing for "no
            // mesh" is the correct outcome, so there is nothing to look up.
            if (rm.mesh_asset == 0) {
                return;
            }
            const auto mesh_it = mesh_cache_.find(rm.mesh_asset);
            if (mesh_it == mesh_cache_.end()) {
                // ONCE per id, never per frame: a per-frame warning at 60 fps
                // is noise nobody reads, which is the same silence with extra
                // steps. Once is enough to make an unregistered asset id
                // impossible to ship unnoticed.
                if (warned_missing_meshes_.insert(rm.mesh_asset).second) {
                    std::fprintf(stderr,
                                 "[render] entity wants mesh asset %u, which is "
                                 "not registered — it draws as NOTHING\n",
                                 rm.mesh_asset);
                }
                return;
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
    for (const WaterBucket& bucket : water_body_meshes_) {
        // Water never casts a sun shadow (transparent programs skip the depth
        // pass), so this is a plain frustum test with no caster exemption.
        if (!frustum.visible(bucket.bounds)) {
            continue;
        }
        renderer.submit(platform::MeshHandle{bucket.mesh_id}, water, identity,
                        water_tex);
    }
    if (water_mesh_ != 0) { // debug fallback plane (set_water / DFN_WATER)
        renderer.submit(platform::MeshHandle{water_mesh_}, water, identity,
                        water_tex);
    }

    // HUD: transparent, over the world, UNDER any full-screen screen. The
    // caller draws into hud() each frame; the probe hook is the only in-engine
    // author (see init()).
    // The probe forces the layer ON rather than riding on hud_visible_. The app
    // legitimately owns that flag — it sets it false when there is nothing to
    // prompt — and the first time it did, the font's acceptance frame came back
    // EMPTY with nothing anywhere saying the hook had been switched off. A
    // verification hook a peer's correct change can silently disable is a hook
    // that will lie to the next agent who trusts it.
    const bool show_hud = (hud_visible_ || font_probe_)
                          && hud_.width() == internal_res_.x
                          && hud_.height() == internal_res_.y;
    if (show_hud) {
        if (font_probe_) {
            hud_.clear_transparent();
            draw_font_specimen(hud_);
        }
        draw_overlay(renderer, hud_, camera, alpha, true);
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


void RenderSystem::set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz) {
    lod_.set_world_bounds(min_xz, max_xz);
}

void RenderSystem::set_streamed_rect(glm::vec2 min_xz, glm::vec2 max_xz) {
    lod_.set_resident_rect(min_xz, max_xz);
}

void RenderSystem::update_lod(const glm::vec3& eye, float dt_seconds) {
    lod_.update(eye, dt_seconds);
}

void RenderSystem::upload_lod_node(platform::IRenderer& renderer, const LodNode& node,
                                   const math::HeightFieldView& field,
                                   const math::SurfaceFieldView* surface) {
    lod_.upload(renderer, node, field, surface);
}

void RenderSystem::drop_lod_node(platform::IRenderer& renderer, const LodNode& node) {
    lod_.drop(renderer, node);
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
        ++uploads_.failed;
        report_upload_failure("terrain chunk");
        return;
    }
    ++uploads_.terrain;
    // Idempotent per coord: replace (and free) any previous upload.
    const TerrainRes res{handle.id, bounds_of(data.vertices)};
    const auto it = terrain_meshes_.find(field.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        it->second = res;
    } else {
        terrain_meshes_.emplace(field.chunk_coord, res);
    }
}

void RenderSystem::upload_terrain_voxel(platform::IRenderer& renderer,
                                        const math::VoxelMeshView& mesh,
                                        const math::HeightFieldView* field,
                                        const math::SurfaceFieldView* surface) {
    // THE MAP IS RECORDED HERE FIRST, and before any early-out below. A chunk
    // the player streamed in is explored whether or not its voxel mesh turned
    // out to be empty — and putting this after the early-out is a smaller
    // version of the bug it fixes.
    if (field != nullptr) {
        map_.note_chunk(*field, surface);
    }
    const TerrainMeshData data = build_voxel_terrain_mesh(mesh);
    if (data.vertices.empty()) {
        return; // solid or empty chunk
    }
    const platform::MeshHandle handle = renderer.create_mesh(data.vertices, data.indices);
    if (!handle.valid()) {
        ++uploads_.failed;
        report_upload_failure("voxel chunk");
        return;
    }
    ++uploads_.voxel;
    // Same key as the heightfield upload: whichever source ran last owns the
    // chunk, so switching the ferry over never draws both.
    const TerrainRes res{handle.id, bounds_of(data.vertices)};
    const auto it = terrain_meshes_.find(mesh.chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        it->second = res;
    } else {
        terrain_meshes_.emplace(mesh.chunk_coord, res);
    }
}

void RenderSystem::drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord) {
    const auto it = terrain_meshes_.find(chunk_coord);
    if (it != terrain_meshes_.end()) {
        renderer.destroy_mesh(platform::MeshHandle{it->second.mesh_id});
        terrain_meshes_.erase(it);
    }
}

void RenderSystem::upload_scatter(platform::IRenderer& renderer,
                                  glm::ivec2 chunk_coord,
                                  std::span<const math::ScatterInstance> instances) {
    drop_scatter(renderer, chunk_coord); // idempotent per coord
    // DFN_NO_SCATTER=1: verification hook (Rule 27), never a shipping path.
    // It exists because A LANDMARK'S SILHOUETTE CANNOT BE JUDGED WITHOUT IT.
    // Core measured the conservative canopy envelope owning 54-79 % of the
    // skyline at the crag's acceptance distances, which means every shape
    // verdict taken so far may have been a verdict on a pine stand. Shooting
    // the same vantage twice, trees on and trees off, separates "the mountain
    // is a dome" from "the mountain is behind a forest" — and those two have
    // completely different owners.
    if (scatter_off_ || instances.empty()) {
        return;
    }
    const auto chunk_size = static_cast<float>(config::CHUNK_SIZE);
    const glm::vec2 origin{static_cast<float>(chunk_coord.x) * chunk_size,
                           static_cast<float>(chunk_coord.y) * chunk_size};
    ScatterBatches batches = build_scatter_batches(instances, origin, chunk_size);

    ChunkScatterRes res;
    res.bounds.expand(bounds_of(batches.trees.vertices));
    res.bounds.expand(bounds_of(batches.foliage.vertices));
    ++uploads_.scatter_chunks;
    const auto upload_batch = [&](const MeshData& mesh) -> uint32_t {
        const platform::MeshHandle handle =
            renderer.create_mesh(mesh.vertices, mesh.indices);
        if (!handle.valid()) {
            ++uploads_.failed;
            report_upload_failure("scatter batch");
            return 0;
        }
        ++uploads_.scatter_meshes;
        return handle.id;
    };
    if (!batches.trees.vertices.empty()) {
        res.trees_mesh_id = upload_batch(batches.trees);
    }
    if (!batches.foliage.vertices.empty()) {
        res.foliage_mesh_id = upload_batch(batches.foliage);
    }
    for (const MicroTile& tile : batches.micro) {
        const uint32_t id = upload_batch(tile.mesh);
        if (id != 0) {
            res.micro.push_back({tile.center_xz, tile.radius_m, id});
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

} // namespace dfn::render
