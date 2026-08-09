/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 22:12:57
Module: engine/render
File: engine/render/sources/RenderSystem.h

Responsibility:
- The render facade: walks the ECS view of renderable entities and submits them
  to IRenderer with interpolated transforms (Rule 12). Owns the mapping from
  engine-level asset ids to renderer handles.

Key items:
- RenderSystem: render(world, renderer, camera, alpha), upload_terrain /
  drop_terrain (HeightFieldView -> terrain mesh, boundary agreed with core).

Dependencies:
- Uses: engine/core (ecs World, math HeightFieldView), engine/platform/render
  (IRenderer). Never bgfx (Rule 1).
- Used by: engine/app (main loop), engine/editor.

Notes:
- Rule 9: platform interfaces arrive as parameters and are never stored. The
  cached state here is resource bookkeeping (asset id -> MeshHandle/Texture-
  Handle), not game state; the ECS World stays the single source of truth
  (Rule 10).
- Renderable entities carry shared components (lead-owned, agreed stage 1):
  Transform + PreviousTransform (written by sim each fixed step), RenderMesh
  { mesh_asset, texture_asset : uint32 name hashes }, LocalBounds (model-space
  AABB). This header only forward-declares World; component includes appear in
  the stage-2 .cpp once engine/core/components exists.
- Terrain: core's ChunkManager emits ChunkLoaded/ChunkUnloaded; the app layer
  subscribes and calls upload_terrain/drop_terrain here (render cannot include
  world per the DAG — wiring agreed with core, confirmed at the sync).
- Culling (stage 2+): frustum vs LocalBounds transformed to world space; frustum
  math lives in engine/core/math.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Keep submission logic here; never let gameplay call IRenderer directly.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
- 09:08:2026 - 11:02:00: Stage 3 — procedural terrain atlas + water texture
  (ProcTexture, registry-assigned dense asset ids), RenderEnvironment per
  frame (Materials.h defaults, environment() accessor for tuning), water
  plane capability (set_water/clear_water + DFN_WATER=<height_m> debug env),
  render-side visual clock for water animation.
- 09:08:2026 - 11:57:20: Stage 3b (lead-approved batch): upload_terrain
  overload with SurfaceFieldView (surface-truth splat), upload_scatter/
  drop_scatter (batched P5 scatter with GRASS_VIEW_DISTANCE micro culling),
  set_water_bodies/clear_water_bodies (lake planes + river ribbons),
  placeholder site meshes registered under the blessed RenderMesh ids 1..7,
  ECS submissions moved to the lit+fogged "prop" program.
- 09:08:2026 - 17:33:00: Map screen (user request "миникарта как в скайриме"):
  toggle_map/set_map_open/map_open + set_internal_resolution, a MapScreen
  member fed by upload_terrain (explored chunks) and the ECS pass (site
  markers), and the generic draw_overlay path that blits a PixelCanvas over
  the frame as an unlit screen-filling quad (no IRenderer change, Rule 26).
- 09:08:2026 - 19:42:00: upload_terrain_voxel — render draws core's voxel
  surface, so carved interiors (tunnel, barrows) exist on screen at all.
- 09:08:2026 - 19:52:00: DFN_TIME now FREEZES the sky each frame (the app's
  clock would otherwise overwrite the screenshot hook every frame).
- 09:08:2026 - 20:21:13: FOLIAGE PASS: the "foliage" program, the procedural
  leaf mask atlas (flora's generate_leaf_atlas, uploaded like the terrain
  atlas) and a second per-chunk scatter mesh for the alpha-cutout leaf cards.
  EDITED BY THE FLORA AGENT under an explicit lead-granted Rule 25 exception
  while render's zone was unowned; wiring only — no shader, material, wind or
  backend change. Not a precedent. (Reviewed and KEPT by render, 20:25.)
- 09:08:2026 - 20:44:00: INTERIOR LIGHTING: collect_point_lights walks
  CarriedLight + Transform into the frame's point-light array (hand offset
  rotated by the carrier's interpolated rotation, first two lights flagged for
  cube shadows), plus the DFN_TORCH / DFN_DARK verification hooks.
- 09:08:2026 - 21:14:00: Frustum culling: per-chunk world bounds measured at
  upload (TerrainRes / ChunkScatterRes::bounds) and visible_or_casting, which
  keeps off-screen shadow casters alive.
- 09:08:2026 - 22:12:57: TERRAIN LOD, the drawing half. lod_* forwarding calls
  over a LodTerrain member: the app sets the world bounds and the streamed
  rectangle, ferries lod_to_load()/lod_to_release() to core's coarse-node
  calls, and hands meshes back through upload_lod_node. Nodes draw after the
  chunk terrain with DrawParams::fade carrying the cross-fade. Also: the
  resource/screen half of this class moved to RenderSystemResources.cpp, which
  is the same class split for the 800-line limit (Rule 21).
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/LodTerrain.h"
#include "engine/render/sources/MapScreen.h"

#include <chrono>
#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <unordered_map>
#include <vector>

namespace dfn::ecs {
class World; // engine/core/ecs/sources/World.h (core zone)
}
namespace dfn::math {
struct HeightFieldView; // engine/core/math/sources/HeightField.h (core zone)
}

namespace dfn::render {

class RenderSystem {
public:
    // Prepares shared programs ("terrain", "unlit") via IRenderer::load_program.
    // Called once by the app after IRenderer::init.
    [[nodiscard]] bool init(platform::IRenderer& renderer);
    // Releases every handle this system created. Called before IRenderer::shutdown.
    void shutdown(platform::IRenderer& renderer);

    // One render frame: begin_frame with the camera blended at alpha (Rule 12),
    // then iterate world.view<Transform, PreviousTransform, RenderMesh>(),
    // interpolate each transform, cull against LocalBounds, submit, end_frame.
    void render(ecs::World& world, platform::IRenderer& renderer,
                const FirstPersonCamera& camera, float alpha);

    // Terrain (boundary agreed with core, stage-1 sync) ------------------------
    // Triangulates the heightfield (TerrainMesher, stage 2) and uploads the chunk
    // mesh. Idempotent per chunk_coord: re-upload replaces the previous mesh.
    void upload_terrain(platform::IRenderer& renderer, const math::HeightFieldView& field);
    // Stage 3b: same, with the chunk's SurfaceFieldView (core boundary, spec
    // Dependencies item 8) baking design-truth splat weights (sand by shore,
    // rock by surface class, water-bed darkening). surface may be nullptr.
    void upload_terrain(platform::IRenderer& renderer, const math::HeightFieldView& field,
                        const math::SurfaceFieldView* surface);
    // Destroys the chunk's mesh. Must run before core frees the heightmap
    // (ChunkUnloaded fires before the data is freed — agreed lifetime).
    // The VOXEL surface — the true world geometry, carves included. Terrain
    // was drawn from the heightfield, which is a function of (x, z) and cannot
    // represent a ceiling, so the inside of the crag tunnel and the barrows
    // was never submitted at all: walkable, invisible. Uploading the voxel
    // mesh for a chunk REPLACES its heightfield mesh (same key), so the app
    // ferry can switch over per chunk without a flicker of double geometry.
    // This is also the first step of terrain LOD — core's coarse nodes arrive
    // through the same VoxelMeshView shape.
    void upload_terrain_voxel(platform::IRenderer& renderer,
                              const math::VoxelMeshView& mesh);
    void drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord);

    // Terrain LOD (the drawing half; core owns streaming and node meshes) ------
    //
    // THE PROBLEM THIS SOLVES IS NOT PERFORMANCE. Chunk streaming reaches
    // ~512 m while CAMERA_FAR is 8 km and the landmark rules are written for
    // 4 km, so the world simply STOPS EXISTING before the distance design
    // judges landmarks from — the 717 m acceptance frame of the massif could
    // not be photographed at all. LOD is what makes those frames exist.
    //
    // App wiring, in the order the app should call it:
    //   set_world_bounds(core's world_bounds_xz)          — once, after open
    //   set_lod_enabled(true)                             — once core answers
    //   set_streamed_rect(focus chunk +/- load_radius)    — when it changes
    //   update_lod(eye, dt)                               — every frame
    //   ferry lod_to_load()  -> core request_coarse_nodes
    //   ferry lod_to_release() -> core release_coarse_node + drop_lod_node
    //   upload_lod_node(...) when core's coarse_heightfield turns non-null
    // Nothing here is required for the app to run: with LOD disabled the
    // renderer behaves exactly as before.
    void set_world_bounds(glm::vec2 min_xz, glm::vec2 max_xz);
    // The ground core has streamed at FULL chunk detail. Coarse nodes are
    // excluded from it — see select_lod_nodes: a level-0 node is 1 m voxels
    // where a chunk is 2 m, so without this the two draw the same ground.
    void set_streamed_rect(glm::vec2 min_xz, glm::vec2 max_xz);
    void set_lod_enabled(bool enabled) { lod_.set_enabled(enabled); }
    [[nodiscard]] bool lod_enabled() const { return lod_.enabled(); }
    // Selection + fades. `dt_seconds` is the RENDER frame delta, not the sim
    // step: the cross-fade is a visual effect and must run at frame rate.
    void update_lod(const glm::vec3& eye, float dt_seconds);
    [[nodiscard]] std::span<const LodNode> lod_to_load() const { return lod_.to_load(); }
    [[nodiscard]] std::span<const LodNode> lod_to_release() const {
        return lod_.to_release();
    }
    // Meshes one coarse node (129 samples, step = the level's voxel size) and
    // uploads it. `surface` may be nullptr — core ships coarse surface fields
    // after the geometry, and slope-only splat is the agreed fallback.
    void upload_lod_node(platform::IRenderer& renderer, const LodNode& node,
                         const math::HeightFieldView& field,
                         const math::SurfaceFieldView* surface);
    void drop_lod_node(platform::IRenderer& renderer, const LodNode& node);
    [[nodiscard]] const LodTerrain& lod() const { return lod_; }

    // Scatter (stage 3b, data-only P5 instances — never entities) --------------
    // Bakes the chunk's scatter span into batched meshes (ScatterBatcher):
    // trees always drawn; bush/stone micro tiles culled by GRASS_VIEW_DISTANCE
    // from the eye. Idempotent per chunk_coord, mirrors upload_terrain.
    void upload_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord,
                        std::span<const math::ScatterInstance> instances);
    void drop_scatter(platform::IRenderer& renderer, glm::ivec2 chunk_coord);

    // Water bodies (stage 3b) --------------------------------------------------
    // Builds one mesh per lake (ellipse plane) and per river segment (ribbon
    // along stations, width per station, surface descending source -> mouth;
    // segment i = stations [offsets[i], offsets[i+1])). Replaces any previous
    // bodies. The global set_water debug plane remains a separate fallback.
    void set_water_bodies(platform::IRenderer& renderer,
                          std::span<const math::LakePlane> lakes,
                          std::span<const math::RiverStation> river_stations,
                          std::span<const uint32_t> river_segment_offsets);
    void clear_water_bodies(platform::IRenderer& renderer);

    // Environment (stage 3) ----------------------------------------------------
    // The frame environment sent to IRenderer::set_environment each render.
    // Defaults from Materials.h; mutate to tune atmosphere/splat/water live.
    // time_seconds is overwritten each frame from the render-side visual clock.
    [[nodiscard]] platform::RenderEnvironment& environment() { return environment_; }

    // Water plane capability (stage 3) -----------------------------------------
    // Creates (or replaces) a flat water plane at world height `height_m`
    // covering the square [center - half_extent, center + half_extent] on x/z.
    // Also raises the terrain sand line to just above the waterline. Placement
    // is data-driven later (design doc); the app/editor wires real calls.
    // Debug: DFN_WATER=<height_m> in init() enables a testbed-covering plane.
    void set_water(platform::IRenderer& renderer, float height_m,
                   glm::vec2 center_xz, float half_extent_m);
    void clear_water(platform::IRenderer& renderer);
    [[nodiscard]] bool water_enabled() const { return water_mesh_ != 0; }

    // Map screen (user request: a Skyrim-style map on a key) -------------------
    // The map fills the frame as an opaque overlay while open; the world keeps
    // rendering behind it (no app-loop change, and reopening costs nothing).
    // The app owns the key binding — it calls toggle_map() on Key::M.
    // Debug/verification: DFN_MAP=1 opens the map at init (tour evidence).
    void toggle_map() { map_.toggle(); }
    void set_map_open(bool open) { map_.set_open(open); }
    [[nodiscard]] bool map_open() const { return map_.open(); }
    // The internal (low-res) target size the overlay canvas is drawn at, so
    // one canvas pixel is one screen pixel. Defaults to INTERNAL_RES /
    // DFN_INTERNAL_RES; the app should pass the value it actually gave
    // RendererInitParams (settings.cfg may override both).
    void set_internal_resolution(uint32_t width, uint32_t height);
    [[nodiscard]] const MapScreen& map() const { return map_; }

private:
    struct ChunkKeyHash {
        size_t operator()(const glm::ivec2& v) const;
    };

    // Registry-assigned dense id for a procedural texture, cached by params
    // (stage-1 registry decision); creates and uploads on first use.
    uint32_t procedural_texture_asset(platform::IRenderer& renderer, uint64_t key,
                                      uint32_t width, uint32_t height,
                                      const uint8_t* pixels);

    // Collects every entity with CarriedLight + Transform into the frame
    // environment's point-light array (torches, lanterns, an NPC's lamp).
    // Gameplay owns WHETHER a light is on; this owns how it looks and which
    // ones get a shadow map. The flame sits at CarriedLight::offset in CARRIER
    // space — a light at the eye casts no visible shadow by construction, so
    // the offset is the feature, not a detail.
    void collect_point_lights(ecs::World& world, const FirstPersonCamera& camera,
                              float alpha);

    // Blits a CPU screen canvas over the frame: uploads it as one RGBA8
    // texture and draws the unlit quad that exactly fills the frustum just
    // past the near plane. Generic on purpose — the future menu screen draws
    // through the same path (no IRenderer contract change, Rule 26).
    void draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                      const FirstPersonCamera& camera, float alpha);

    // A resident chunk mesh plus the world AABB it occupies. The bounds are
    // measured at upload (the mesher's vertices are right there) and exist for
    // frustum culling — recomputing them per frame from the GPU is impossible
    // and storing them is 24 bytes.
    struct TerrainRes {
        uint32_t mesh_id = 0;
        math::Aabb bounds{};
    };

    // One micro-scatter tile resident on the GPU (culling data + mesh).
    struct MicroTileRes {
        glm::vec2 center_xz{0.0f};
        float radius_m = 0.0f;
        uint32_t mesh_id = 0; // MeshHandle.id
    };
    struct ChunkScatterRes {
        uint32_t trees_mesh_id = 0;   // 0 = no trees in this chunk
        uint32_t foliage_mesh_id = 0; // alpha-cutout leaf cards ("foliage")
        math::Aabb bounds{};          // trees + cards, for frustum culling
        std::vector<MicroTileRes> micro;
    };

    // True if the box should be drawn: inside the frustum, OR close enough to
    // the eye that it is still a SUN SHADOW CASTER. The second half is not an
    // optimization detail — the backend double-submits every opaque draw into
    // the shadow map, so a box culled here loses its shadow too, and a tree
    // just off the left edge of the screen would stop shading the ground the
    // player is looking at.
    [[nodiscard]] static bool visible_or_casting(const math::Frustum& frustum,
                                                 const math::Aabb& box,
                                                 const glm::vec3& eye);

    // Resource bookkeeping only — never game state (Rule 10).
    std::unordered_map<glm::ivec2, TerrainRes, ChunkKeyHash> terrain_meshes_;
    std::unordered_map<glm::ivec2, ChunkScatterRes, ChunkKeyHash> scatter_meshes_;
    std::unordered_map<uint32_t, uint32_t> mesh_cache_;    // mesh_asset id -> MeshHandle.id
    std::unordered_map<uint32_t, uint32_t> texture_cache_; // texture_asset id -> TextureHandle.id
    std::unordered_map<uint64_t, uint32_t> proc_texture_ids_; // params key -> asset id
    std::vector<uint32_t> water_body_meshes_; // MeshHandle.ids (lakes + river segments)
    uint32_t next_texture_asset_ = 1; // dense id allocator (0 = none)
    uint32_t terrain_program_ = 0; // ProgramHandle.id
    uint32_t unlit_program_ = 0;   // ProgramHandle.id
    uint32_t water_program_ = 0;   // ProgramHandle.id
    uint32_t prop_program_ = 0;    // ProgramHandle.id (lit+fog vertex color)
    uint32_t foliage_program_ = 0; // ProgramHandle.id (alpha-cutout leaf cards)
    uint32_t atlas_texture_asset_ = 0; // terrain splat atlas (engine asset id)
    uint32_t water_texture_asset_ = 0; // water surface texture (engine asset id)
    uint32_t leaf_texture_asset_ = 0;  // leaf mask atlas (engine asset id)
    uint32_t water_mesh_ = 0;          // MeshHandle.id, 0 = no debug water plane
    uint32_t overlay_mesh_ = 0;        // MeshHandle.id, screen-filling quad
    uint32_t overlay_texture_ = 0;     // TextureHandle.id, re-uploaded per frame
    glm::uvec2 internal_res_{0, 0};    // overlay canvas size (internal pixels)
    // DFN_TIME/DFN_MOON freeze the sky for deterministic screenshots. Re-applied
    // every frame, not just at init: the app drives apply_sky_time from its own
    // clock now, so an init-time value would be overwritten before frame one.
    bool sky_frozen_ = false;
    float frozen_day_ = 0.5f;
    float frozen_moon_phase_ = 0.5f;
    // Verification hooks for the interior shoot (Rule 27), NOT the feature.
    // DFN_TORCH=1 lights a carried flame at the CAMERA's hand position, because
    // the tour freezes the player and no gameplay entity carries a torch during
    // a screenshot run. The shipping path is components::CarriedLight, written
    // by gameplay; if any entity carries one, it wins and this hook stands down.
    bool torch_debug_ = false;
    // DFN_TORCH=2: metres AHEAD of the camera to stand the debug flame off, so
    // casters fall between eye and light and the cube map's work is visible in
    // an open-ground frame. 0 = the flame is at the hand (DFN_TORCH=1).
    float torch_ahead_m_ = 0.0f;
    // DFN_NO_POINT_SHADOW=1 keeps the lights and drops their cube maps — the
    // A/B half of the acceptance shoot, and the cost measurement.
    bool point_shadows_off_ = false;
    // DFN_DARK=<0..1> pins ambient_darkness (the app drives it in play).
    bool dark_frozen_ = false;
    float frozen_darkness_ = 0.0f;
    MapScreen map_;
    LodTerrain lod_; // coarse terrain beyond the streamed chunk ring
    platform::RenderEnvironment environment_{};
    std::chrono::steady_clock::time_point clock_start_{}; // visual time origin
};

} // namespace dfn::render
