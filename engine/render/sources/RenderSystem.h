/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 17:33:00
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
*/

#pragma once

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
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
    void drop_terrain(platform::IRenderer& renderer, glm::ivec2 chunk_coord);

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

    // Blits a CPU screen canvas over the frame: uploads it as one RGBA8
    // texture and draws the unlit quad that exactly fills the frustum just
    // past the near plane. Generic on purpose — the future menu screen draws
    // through the same path (no IRenderer contract change, Rule 26).
    void draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                      const FirstPersonCamera& camera, float alpha);

    // One micro-scatter tile resident on the GPU (culling data + mesh).
    struct MicroTileRes {
        glm::vec2 center_xz{0.0f};
        float radius_m = 0.0f;
        uint32_t mesh_id = 0; // MeshHandle.id
    };
    struct ChunkScatterRes {
        uint32_t trees_mesh_id = 0; // 0 = no trees in this chunk
        std::vector<MicroTileRes> micro;
    };

    // Resource bookkeeping only — never game state (Rule 10).
    std::unordered_map<glm::ivec2, uint32_t, ChunkKeyHash> terrain_meshes_; // coord -> MeshHandle.id
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
    uint32_t atlas_texture_asset_ = 0; // terrain splat atlas (engine asset id)
    uint32_t water_texture_asset_ = 0; // water surface texture (engine asset id)
    uint32_t water_mesh_ = 0;          // MeshHandle.id, 0 = no debug water plane
    uint32_t overlay_mesh_ = 0;        // MeshHandle.id, screen-filling quad
    uint32_t overlay_texture_ = 0;     // TextureHandle.id, re-uploaded per frame
    glm::uvec2 internal_res_{0, 0};    // overlay canvas size (internal pixels)
    MapScreen map_;
    platform::RenderEnvironment environment_{};
    std::chrono::steady_clock::time_point clock_start_{}; // visual time origin
};

} // namespace dfn::render
