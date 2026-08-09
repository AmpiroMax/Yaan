/*
Created: 09:08:2026 - 20:55:10
Last updated: 10:08:2026 - 01:47:53
Module: engine/render
File: engine/render/sources/TerrainLod.h

Responsibility:
- Terrain level of detail: which quadtree nodes are drawn at which level from a
  given eye, and how residency changes over time WITHOUT popping. Pure and
  GPU-free — no ECS, no IRenderer, no core includes — so the whole policy is
  testable headless and cannot drift into the backend.

Key items:
- LodNode (level + node grid coords), LOD_VOXEL_SIZE_M ladder, node geometry
  helpers (size, bounds, distance).
- select_lod_nodes(): the quadtree descent, derived from pixels per triangle.
- LodResidency: per-frame diff into load / release / draw lists with a
  TWO-LEVEL fade window, so the old level is still drawn while the new one
  fades in.

Dependencies:
- Uses: glm, stdlib. Nothing else on purpose.
- Used by: RenderSystem (submission), later the app ferry for core's
  coarse_mesh/release_node calls.

Notes:
- The node LADDER is a cross-zone agreement with core (voxel sizes
  1/4/8/16/32/64 m, ~30-40k triangles per node at every level). Core produces
  node meshes; this file decides what is asked for and what is drawn. Change
  the ladder only with core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this PURE: no GPU calls, no ECS, no wall clock. Time arrives as a
  delta parameter so tests can step it exactly.
*/
/*
UPD:
- 09:08:2026 - 20:55:10: Created — LOD node ladder, screen-error selection and
  the two-level fade window (render half of the LOD contract with core).
- 09:08:2026 - 22:01:04: The RESIDENT RECTANGLE (the ground core streams at
  full chunk detail) is now an input to the descent: nodes fully inside it are
  dropped and nodes straddling its border are forced to split, so coarse
  terrain and chunk terrain never draw the same ground. Plus lod_skirt_depth_m
  — the apron that hides T-junction cracks between adjacent levels.
- 09:08:2026 - 22:39:28: LodResidency::pending() — the standing set of selected-but-
  not-yet-delivered nodes, which is what the app ferry must retry against.
- 10:08:2026 - 01:47:53: Straddle-ring fix (core's finding): straddling nodes
  are no longer force-split to level 0; they are judged by the distance to
  their ground OUTSIDE the rectangle and clipped by the mesher instead. See
  select_lod_nodes' doc for the contract and the measurement.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace dfn::render {

/// Levels of the terrain quadtree. Level 0 is the finest.
inline constexpr uint32_t LOD_LEVEL_COUNT = 6;

/// Voxel size per level, in metres — the ladder agreed with core.
/// DERIVED, not chosen: see LOD_DISTANCE_PER_METRE below.
inline constexpr float LOD_VOXEL_SIZE_M[LOD_LEVEL_COUNT] = {1.0f,  4.0f,  8.0f,
                                                            16.0f, 32.0f, 64.0f};

/// Node side in VOXELS, constant across levels, which is what makes the
/// triangle budget per node constant (128x128 cells ~= 33k triangles, inside
/// the 30-40k agreed with core). A node's metre size therefore scales with its
/// level: 128 m at level 0, 8192 m at level 5.
inline constexpr uint32_t LOD_NODE_VOXELS = 128;

/// Metres of viewing distance needed per metre of triangle edge.
/// Derivation (kept here because a number nobody can re-derive gets "tuned"):
/// at 640x360 with CAMERA_FOV_Y the horizontal fov is ~2.33 rad, so the screen
/// carries ~275 pixels per radian. A triangle edge is worth drawing while it
/// covers about 2.5 pixels, i.e. 2.5/275 rad, so at distance d the useful edge
/// is d * 2.5/275 = d/110. Turned around: an edge of V metres is good enough
/// from 110*V metres away. That single relation produces the whole ladder.
inline constexpr float LOD_DISTANCE_PER_METRE = 110.0f;

/// Beyond this distance a node contributes SILHOUETTE ONLY: terrain still
/// draws, scatter and props do not (design's LANDMARK_HAZE_ONSET — past it a
/// landmark is a shape in haze, and individual trees stop being resolvable).
inline constexpr float LOD_SILHOUETTE_DISTANCE_M = 800.0f;

/// How long a level change takes to cross-fade, seconds. Long enough that the
/// swap is not a flicker, short enough that two levels of the same ground are
/// rarely both resident.
inline constexpr float LOD_FADE_SECONDS = 0.6f;

/// Skirt depth multiplier — see lod_skirt_depth_m(). The largest jump in the
/// ladder is 1 -> 4 m (every other step is a factor of two), so a fine node's
/// edge can disagree with a coarse neighbour's linear edge by up to the relief
/// across FOUR of its own cells. Multiplying the measured worst adjacent step
/// by that factor bounds the crack without knowing which neighbour turned up.
/// PROVISIONAL: core is measuring the real worst-case inter-level disagreement
/// per level pair on the testbed and will send a table; this is the bound that
/// is derivable without it, and it errs deep (a too-deep skirt is hidden under
/// the ground, a too-shallow one is a hole to the sky).
inline constexpr float LOD_SKIRT_LADDER_RATIO = 4.0f;

/// Floor on the skirt depth, in the node's own voxels. Perfectly flat ground
/// measures a zero step and would get a zero skirt, which is exactly where
/// floating-point disagreement between two different sample lattices still
/// shows a hairline.
inline constexpr float LOD_SKIRT_MIN_VOXELS = 1.0f;

/// One quadtree node. `x`/`z` are node coordinates AT THAT LEVEL (world
/// position = coord * node_size), so a node id is exact integer identity and
/// never a float comparison. Mirrors what core hands back from coarse_mesh.
struct LodNode {
    uint8_t level = 0;
    int32_t x = 0;
    int32_t z = 0;

    friend bool operator==(const LodNode& a, const LodNode& b) {
        return a.level == b.level && a.x == b.x && a.z == b.z;
    }
};

/// Node side length in metres.
[[nodiscard]] float lod_node_size_m(uint8_t level);

/// Distance at which a node of this level stops being good enough and must be
/// split into its four children (= LOD_DISTANCE_PER_METRE * its voxel size).
[[nodiscard]] float lod_split_distance_m(uint8_t level);

/// Shortest distance from `eye` to the node's footprint on the xz plane. The
/// vertical extent is deliberately ignored: heights are not known here, and
/// using the footprint is the conservative (finer) choice.
[[nodiscard]] float lod_node_distance_m(const LodNode& node, const glm::vec3& eye);

/// World-space rectangle on the xz plane. Empty (min >= max on either axis)
/// means "no such area", which is how an absent resident rectangle is spelled.
struct LodRect {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};

    [[nodiscard]] bool empty() const { return max.x <= min.x || max.y <= min.y; }
};

/// The set of nodes that should be drawn: a quadtree descent from the coarsest
/// level over the world rectangle [world_min, world_max] (metres, xz), split
/// where the node is closer than its split distance. Deterministic order.
/// `max_nodes` is a safety cap, not a policy: hitting it means the ladder or
/// the world size is wrong, and the selection stops rather than growing without
/// bound.
///
/// `resident` is THE GROUND CORE ALREADY STREAMS AT FULL CHUNK DETAIL. It is
/// not an optimisation: a level-0 node is 1 m voxels where a chunk heightfield
/// is 2 m, so without it the two systems draw the same ground twice, at
/// slightly different heights, and the overlap band interleaves per pixel.
/// The descent (a) DROPS a node that lies entirely inside the rectangle, and
/// (b) judges a node that STRADDLES its border by the distance to the ground
/// the node actually contributes — max(footprint distance, the eye's
/// clearance to the rectangle border) — because everything inside the
/// rectangle is removed at MESH time (TerrainMeshOptions::clip_*), never
/// drawn. THE CALLER OWES THE CLIP: a consumer that draws a returned
/// straddling node whole redraws chunk ground (LodTerrain::upload passes the
/// rectangle through; a pure caller with no resident rect owes nothing).
/// History: (b) used to force-split every straddler to level 0 to make the
/// inside/outside decision exact, which inverted the ladder — a chunk-aligned
/// 1280 m rectangle cuts the 512 m level-1 grid at odd 256 m multiples on two
/// edges EVERY frame, so ~80% of a frame's selection was level-0 nodes on
/// ground 500-700 m away (44 of 51, core's measurement), each the most
/// expensive thing core builds. Pass an empty rectangle when nothing is
/// streamed (the pure-LOD case).
[[nodiscard]] std::vector<LodNode> select_lod_nodes(const glm::vec3& eye,
                                                    glm::vec2 world_min,
                                                    glm::vec2 world_max,
                                                    const LodRect& resident,
                                                    size_t max_nodes = 4096);

/// Convenience form for the pure-LOD case (nothing streamed at chunk detail).
[[nodiscard]] std::vector<LodNode> select_lod_nodes(const glm::vec3& eye,
                                                    glm::vec2 world_min,
                                                    glm::vec2 world_max,
                                                    size_t max_nodes = 4096);

/// Depth of the vertical apron hung from a node's border, in metres.
///
/// WHY A SKIRT AT ALL: two nodes at different levels sample the same ground on
/// different lattices, so along a shared border the finer node's vertices do
/// not lie on the coarser node's straight edge. The gap is a T-junction crack —
/// a one-pixel line of SKY through the ground, which at 640x360 is not subtle.
/// Nothing in the ladder prevents it and no amount of matching the heights
/// does either, because the two edges have different vertex counts.
///
/// `max_border_step_m` is the largest height difference between two ADJACENT
/// samples along this node's border, measured from the field at bake time
/// rather than assumed: flat ground needs a millimetre and a cliff edge needs
/// metres, and a single constant would be wrong at both ends.
[[nodiscard]] float lod_skirt_depth_m(uint8_t level, float max_border_step_m);

/// True if the node is far enough that only its silhouette matters (no scatter,
/// no props).
[[nodiscard]] bool lod_node_is_silhouette(const LodNode& node, const glm::vec3& eye);

/// What to draw this frame, and with how much of it.
struct LodDraw {
    LodNode node;
    /// 0..1. A node that is fully in reads 1. During a swap the incoming node
    /// climbs 0 -> 1 while the outgoing one falls 1 -> 0, and BOTH are drawn:
    /// releasing the old level the instant the new one arrives is what
    /// guarantees a pop, whatever the renderer does afterwards.
    float fade = 1.0f;
};

/// Tracks which nodes are resident and turns a per-frame selection into
/// load / release / draw lists. It never touches the GPU or core; the caller
/// ferries `to_load` to core's coarse_mesh and `to_release` to release_node.
///
/// The two-level window is the whole point: core confirmed a node may be
/// resident at TWO levels at once and that render calls release_node
/// explicitly. That answer is what makes "no popping" achievable at all.
class LodResidency {
public:
    /// Advances the fades and diffs `selection` against what is resident.
    /// `dt_seconds` is passed in (Rule 12 spirit: no clock reads in here).
    void update(const std::vector<LodNode>& selection, float dt_seconds);

    /// Nodes whose mesh must be requested from core (newly selected).
    [[nodiscard]] const std::vector<LodNode>& to_load() const { return to_load_; }
    /// Nodes whose mesh may be freed: deselected AND fully faded out.
    [[nodiscard]] const std::vector<LodNode>& to_release() const { return to_release_; }
    /// Everything to draw this frame, incoming and outgoing, with its fade.
    [[nodiscard]] const std::vector<LodDraw>& to_draw() const { return draws_; }
    /// Nodes SELECTED but not yet resident — requested and still in flight.
    /// Unlike to_load() this is not a per-frame diff but the standing set, so
    /// the ferry can retry `coarse_heightfield` every frame until core answers.
    /// The distinction is not cosmetic: to_load() names a node exactly once,
    /// and core delivers under a budget several frames later, so a ferry built
    /// on to_load() alone requests nodes it then never collects.
    [[nodiscard]] const std::vector<LodNode>& pending() const { return pending_; }

    /// Marks a requested node as GPU-resident. Until this is called the node
    /// is selected but not drawn — a node cannot fade in before it exists, and
    /// pretending otherwise is how a hole in the ground appears.
    void mark_resident(const LodNode& node);

    [[nodiscard]] size_t resident_count() const { return entries_.size(); }
    [[nodiscard]] bool is_resident(const LodNode& node) const;

private:
    struct Entry {
        LodNode node;
        float fade = 0.0f;
        bool selected = false;
        bool resident = false; // mesh actually uploaded
    };
    std::vector<Entry> entries_;
    std::vector<LodNode> to_load_;
    std::vector<LodNode> to_release_;
    std::vector<LodNode> pending_;
    std::vector<LodDraw> draws_;
};

} // namespace dfn::render
