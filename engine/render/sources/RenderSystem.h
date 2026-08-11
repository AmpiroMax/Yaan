/*
Created: 09:08:2026 - 00:16:00
Last updated: 12:08:2026 - 00:52:40
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
- 09:08:2026 - 22:33:00: DFN_NO_SCATTER verification hook (scatter_off_).
- 09:08:2026 - 22:36:47: warned_missing_meshes_ — the once-per-id missing-asset warning.
- 09:08:2026 - 22:39:28: lod_pending() — the accessor the app ferry retries against.
- 09:08:2026 - 22:44:28: upload_terrain_voxel takes the heightfield for the MAP
  (defaulted, source-compatible). Regression fix: the map recorded nothing at
  all once the ferry moved to the voxel path.
- 09:08:2026 - 23:50:06: hud()/set_hud_visible (the caller draws the interaction
  prompt; render owns the surface and the blit, never the string — Rule 5),
  overlay_program_, WaterBucket (bodies merged per CHUNK_SIZE cell with an AABB
  for culling), upload counters.
- 10:08:2026 - 02:30:08: register_mesh — caller-authored geometry enters the
  asset registry (character zone's body segments 34..49 via the app ferry;
  render cannot include engine/anim). Additive; refusals are loud.
- 10:08:2026 - 03:08:00: CLOUDS (W4): render() now drives apply_wind (no live
  call site existed anywhere — wind_strength sat at 0.0 and read as calm) and
  apply_clouds (the one coverage-field drift) each frame; DFN_CLOUD pins
  cover (0 = the pass's control), DFN_VISTIME pins the visual clock for the
  drift acceptance pair.
- 10:08:2026 - 20:15:40: lod_selected_count / lod_resident_count /
  lod_draw_count. A readout built on lod_pending() reads a HEALTHY ring as
  zero, because pending() is the awaiting-upload list; two zones read that
  zero as "the far-detail ring never populates" on the same day.
- 12:08:2026 - 00:52:40: GROUND TUFTS — the sparse near-field grass layer
  (GroundTufts.h). Spots are harvested once per chunk off the DRAWN voxel mesh
  in upload_terrain_voxel; the geometry is ONE eye-local mesh regrown only when
  the eye has walked TUFT_REBUILD_STEP_M, because at the Rule 33 view distance
  only a couple of hundred clumps are ever visible and baking a whole chunk of
  blades would spend tens of megabytes to draw a hundredth of them. Every
  setting is derived from an approved row (tuft_params(), Rule 14).
  DFN_NO_TUFTS=1 is the counterfactual arm.
*/

#pragma once

#include "engine/core/math/sources/Aabb.h"
#include "engine/core/math/sources/Frustum.h"
#include "engine/core/math/sources/SurfaceField.h"
#include "engine/core/math/sources/VoxelField.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/GroundTufts.h"
#include "engine/render/sources/LodTerrain.h"
#include "engine/render/sources/MapScreen.h"

#include <chrono>
#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <unordered_map>
#include <unordered_set>
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
    // Registers caller-authored geometry under a RenderMesh asset id — the
    // seam for zones whose meshes render cannot build (the DAG forbids
    // render -> anim includes, so the app ferries engine/anim's body segments
    // through this at init). LOUD ON EVERY REFUSAL, silent never:
    //  - an id already registered is REFUSED (stderr + false): a collision is
    //    two zones disagreeing about the id map, and silently replacing a
    //    mesh is how that drift would hide. Live replacement is deliberately
    //    not offered until something needs it — ask, do not overload this.
    //  - ids inside ranges owned by other mechanisms are REFUSED even when
    //    free: 1..31 (site table + growth), 32..33 (view model), 64..127
    //    (items). A typo must not shadow a blessed id.
    // Draws on the "prop" program like every untextured mesh in the cache.
    [[nodiscard]] bool register_mesh(platform::IRenderer& renderer, uint32_t mesh_asset,
                                     std::span<const platform::Vertex> vertices,
                                     std::span<const uint32_t> indices);

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
    // `field` is NOT used for geometry — the voxel mesh is the geometry. It is
    // the MAP's source, and it is a defaulted parameter because leaving it out
    // is what broke the map screen: the app switched the terrain ferry to the
    // voxel path, `note_chunk` only ever ran on the heightfield path, and the
    // explored map quietly stopped recording anything. The map wants one
    // height per column, which is exactly a heightfield and is NOT derivable
    // from a surface mesh without re-deriving a world fact (the bug class this
    // zone is forbidden from re-entering). Pass the same view you would have
    // passed to upload_terrain; nullptr keeps the old behaviour.
    void upload_terrain_voxel(platform::IRenderer& renderer,
                              const math::VoxelMeshView& mesh,
                              const math::HeightFieldView* field = nullptr,
                              const math::SurfaceFieldView* surface = nullptr);
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
    // Selected but not yet delivered. THE FERRY RETRIES coarse_heightfield
    // AGAINST THIS, not against lod_to_load(): to_load is a per-frame diff that
    // names a node exactly once, while core admits nodes under a budget and
    // answers several frames later — a ferry built on to_load alone requests
    // nodes it then never collects, and the ground never appears.
    [[nodiscard]] std::span<const LodNode> lod_pending() const { return lod_.pending(); }
    // FOR A READOUT, USE lod_draw_count(), NOT lod_pending().size(). Earned:
    // the debug overlay showed "lod 0" from pending(), two zones read that as
    // "the far-detail ring never populates", and one of them (me) nearly
    // published it as a defect in a subsystem that was healthy — pending() is
    // the AWAITING-UPLOAD list, so zero is the steady state, and it reads as
    // absence exactly when everything is fine. The three counters answer three
    // different questions and only the last one goes to zero when the ring is
    // CULLED rather than missing:
    //   lod_selected_count() — what the policy asked for, from the eye alone
    //   lod_resident_count() — what core has actually delivered
    //   lod_draw_count()     — what survived the frustum and reached the GPU
    [[nodiscard]] size_t lod_selected_count() const { return lod_.selected_count(); }
    [[nodiscard]] size_t lod_resident_count() const { return lod_.resident_count(); }
    [[nodiscard]] size_t lod_draw_count() const { return lod_.last_draw_count(); }
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

    // --- The §8.1 path surface ------------------------------------------------
    //
    // The app ferries ChunkManager::path_surface() here once per world, the
    // same way it ferries water_bodies(): the network is whole-world and built
    // at open, not streamed. Render draws the tread; core routed it; flora
    // populates the margin (в24).
    void set_path_surface(platform::IRenderer& renderer,
                          std::span<const math::PathStation> stations,
                          std::span<const uint32_t> route_offsets);
    void clear_path_surface(platform::IRenderer& renderer);
    /// Drawable pieces currently resident (diagnostics and tests).
    [[nodiscard]] std::size_t path_piece_count() const { return path_meshes_.size(); }

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

    // HUD layer (the interaction prompt, and later the crosshair / status) ----
    // A TRANSPARENT screen-space canvas the game draws into every frame, blitted
    // over the world through the same overlay path as the map but with an alpha
    // blend. The caller owns its contents: clear it, draw into it with
    // BitmapFont.h's draw_text, and render() composites it.
    //
    // WHY THE CALLER DRAWS AND NOT render: the prompt's TEXT is a localization
    // string resolved from components::Highlightable::prompt_key (Rule 5).
    // render must never contain a user-facing string, so it cannot own the
    // content — only the surface and the blit.
    //
    // The canvas is sized to the internal resolution (one canvas pixel = one
    // screen pixel) by init() and set_internal_resolution(); do not resize it.
    [[nodiscard]] PixelCanvas& hud() { return hud_; }
    void set_hud_visible(bool visible) { hud_visible_ = visible; }
    [[nodiscard]] bool hud_visible() const { return hud_visible_; }

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
    // `blended` picks the alpha-blended "overlay" program (HUD) over the opaque
    // "unlit" one (full-screen screens like the map).
    void draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                      const FirstPersonCamera& camera, float alpha,
                      bool blended = false);

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

    // GROUND TUFTS (GroundTufts.h): the sparse near-field grass. Two halves,
    // and the split is the whole design.
    //
    // The SPOTS are harvested once per chunk upload, off the drawn mesh, and
    // are cheap to keep (a 256 m chunk at the design floor density is a few
    // hundred KB). The GEOMETRY is one eye-local mesh rebuilt only when the eye
    // has walked TUFT_REBUILD_STEP_M, because at the Rule 33 view distance only
    // a couple of hundred tufts are ever visible — baking blades for a whole
    // chunk would spend tens of megabytes to draw a hundredth of them.
    /// The tuft settings, DERIVED from approved rows rather than typed
    /// (Rule 14). Defined in the .cpp beside the Rule 33 arithmetic.
    [[nodiscard]] static GroundTuftParams tuft_params();
    /// Regrows the eye-local tuft mesh if the eye has moved far enough.
    void refresh_ground_tufts(platform::IRenderer& renderer, glm::vec3 eye);

    std::unordered_map<glm::ivec2, std::vector<TuftSpot>, ChunkKeyHash> tuft_spots_;
    uint32_t tuft_mesh_id_ = 0;
    glm::vec3 tuft_built_at_{0.0f};
    bool tuft_built_ = false;
    bool tufts_off_ = false; // DFN_NO_TUFTS=1 — the counterfactual arm
    std::unordered_map<uint32_t, uint32_t> mesh_cache_;    // mesh_asset id -> MeshHandle.id
    std::unordered_map<uint32_t, uint32_t> texture_cache_; // texture_asset id -> TextureHandle.id
    std::unordered_map<uint64_t, uint32_t> proc_texture_ids_; // params key -> asset id
    // Water bodies are MERGED INTO WORLD-GRID BUCKETS, not uploaded one mesh
    // per body. Core's hydrology emits a LakePlane per pond: 17336 of them on
    // the 2x2 km testbed, which spent every one of bgfx's 4096 vertex-buffer
    // handles before a single chunk of terrain could upload, and cost 4078 draw
    // calls a frame besides. One bucket per CHUNK_SIZE cell keeps the count
    // proportional to world area / chunk area and gives the frustum something
    // to cull.
    struct WaterBucket {
        uint32_t mesh_id = 0;
        math::Aabb bounds{};
    };
    std::vector<WaterBucket> water_body_meshes_;
    // A path piece is one class over ~128 m of tread; the class picks the atlas
    // cell and is carried in the vertices, so this only needs the mesh and its
    // bounds — same shape as a water bucket, same reason (something to cull).
    std::vector<WaterBucket> path_meshes_;
    uint32_t next_texture_asset_ = 1; // dense id allocator (0 = none)
    uint32_t terrain_program_ = 0; // ProgramHandle.id
    uint32_t unlit_program_ = 0;   // ProgramHandle.id
    uint32_t water_program_ = 0;   // ProgramHandle.id
    uint32_t prop_program_ = 0;    // ProgramHandle.id (lit+fog vertex color)
    uint32_t foliage_program_ = 0; // ProgramHandle.id (alpha-cutout leaf cards)
    uint32_t overlay_program_ = 0; // ProgramHandle.id ("unlit" + alpha blend)
    uint32_t path_program_ = 0;    // ProgramHandle.id (§8.1 path surface)
    uint32_t atlas_texture_asset_ = 0; // terrain splat atlas (engine asset id)
    uint32_t water_texture_asset_ = 0; // water surface texture (engine asset id)
    uint32_t leaf_texture_asset_ = 0;  // leaf mask atlas (engine asset id)
    uint32_t path_atlas_asset_ = 0;    // §8.1 path surface atlas (engine asset id)
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
    // DFN_CLOUD pins cloud cover (0 = the pass's Rule 30 control: sheet,
    // cumulus AND ground shadows must vanish together — one field). Re-applied
    // per frame like the sky freeze so a future app-side schedule write cannot
    // overrule a screenshot pin.
    bool cloud_pinned_ = false;
    float frozen_cloud_cover_ = 0.0f;
    // DFN_VISTIME pins the visual clock (wind envelope, water scroll, cloud
    // drift): the deterministic half of the drift acceptance pair.
    bool vis_time_frozen_ = false;
    float frozen_vis_time_ = 0.0f;
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
    // DFN_NO_SCATTER=1 drops every scatter batch at upload (see upload_scatter):
    // the trees-off half of a landmark-silhouette A/B, and nothing else.
    // Asset ids already reported missing, so the warning fires once rather
    // than 60 times a second (see the ECS pass).
    std::unordered_set<uint32_t> warned_missing_meshes_;
    bool scatter_off_ = false;
    bool dark_frozen_ = false;
    float frozen_darkness_ = 0.0f;
    MapScreen map_;
    PixelCanvas hud_;              // transparent HUD layer, drawn by the caller
    bool hud_visible_ = false;
    bool font_probe_ = false;      // DFN_FONT_PROBE: draw the glyph specimen

    // Upload accounting. GPU buffer handles are a HARD, SMALL budget (bgfx
    // hands out 4096) and every path below spends them; when the budget ran out
    // the only symptom was a crash at exit. Counted per entry point so the next
    // report says WHICH path is spending, not just that spending happened.
    struct UploadCounts {
        uint64_t terrain = 0;
        uint64_t voxel = 0;
        uint64_t scatter_chunks = 0;
        uint64_t scatter_meshes = 0;
        uint64_t failed = 0;
    } uploads_;
    bool upload_failure_reported_ = false;

    // Prints the FIRST upload failure of the run with the counts that caused
    // it. Silent afterwards.
    void report_upload_failure(const char* what);
    LodTerrain lod_; // coarse terrain beyond the streamed chunk ring
    platform::RenderEnvironment environment_{};
    std::chrono::steady_clock::time_point clock_start_{}; // visual time origin
};

} // namespace dfn::render
