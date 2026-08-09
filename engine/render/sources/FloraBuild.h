/*
Created: 09:08:2026 - 23:48:30
Last updated: 09:08:2026 - 23:48:30
Module: engine/render
File: engine/render/sources/FloraBuild.h

Responsibility:
- The shared build state of one tree (Tree) and the primitives that turn a
  point plus a radius into geometry: tapered tubes, faceted blobs, crossed leaf
  card clusters, and the envelope containment every one of them obeys.

Key items:
- Tree, tube_segment(), blob_cluster(), emit_cluster(), emit_card_cluster(),
  crown_volume(), envelope_radius(), clip_to_envelope(), shy_scale(),
  trunk_height_frac(), safe_normalize(), perp_of(), FLARE_*, SHADOW_MIN_DIAMETER.

Dependencies:
- Uses: FloraSkeleton.h (CrownVolume), FloraSpecies.h, FloraCards.h, ProcMesh.h.
- Used by: ProcFlora.cpp only. This is an INTERNAL header of the flora zone; no
  other zone includes it and its contents are not a frozen contract.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE AND DETERMINISTIC.
- Split out of ProcFlora.cpp for Rule 21 (800 LOC) when the space-colonization
  rewrite landed; the containment rules here cost a debugging round EACH and
  their comments are the record of why (flora.md §3.7).
*/
/*
UPD:
- 09:08:2026 - 23:48:30: Created — extracted from ProcFlora.cpp unchanged except
  for `cluster` being renamed `blob_cluster` (it collided with the local
  variable name in every caller) and the sway origin becoming the skeleton
  ANCHOR rather than the crown base.
*/

#pragma once

#include "engine/render/sources/FloraCards.h"
#include "engine/render/sources/FloraSkeleton.h"
#include "engine/render/sources/FloraSpecies.h"
#include "engine/render/sources/ProcFlora.h"
#include "engine/render/sources/ProcMesh.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

namespace dfn::render {

constexpr float TAU = 6.28318530717958647692f;
constexpr float GOLDEN_ANGLE = 2.39996322972865332f; // phyllotaxis

// Root flare (flora.md §3.5): a 1.2 m trunk on TREE_SLOPE_MAX spans
// 1.2*tan(35 deg) = 0.84 m of ground drop across its OWN base, before design's
// micro relief. The skirt buries itself instead of a global sink fudge.
constexpr float FLARE_HEIGHT = 1.2f;
constexpr float FLARE_WIDEN = 1.6f;
constexpr float FLARE_DEPTH = 1.0f;
// Render's constraint: the flare must stay above the shadow-caster floor all
// the way down, or the tree reads as hovering even while correctly buried.
constexpr float SHADOW_MIN_DIAMETER = 0.35f;

/// splitmix64 — local, deterministic, no shared state.
[[nodiscard]] uint64_t mix64(uint64_t x);

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed);
    float unit(); ///< [0,1)
    float sym();  ///< [-1,1)
};

[[nodiscard]] glm::vec3 safe_normalize(glm::vec3 v, glm::vec3 fallback);
/// Any vector perpendicular to `n` (stable basis for tube rings).
[[nodiscard]] glm::vec3 perp_of(glm::vec3 n);

/// Fraction of the species height carried by the TRUNK, the rest being filled
/// by branches and their foliage. Not cosmetic: the species height band is a
/// CROSS-ZONE CONTRACT (core's canopy occlusion and design's C4 arithmetic use
/// OAK/PINE/BIRCH_HEIGHT_MAX).
[[nodiscard]] float trunk_height_frac(CrownEnvelope e);

/// Everything one tree's builders share.
struct Tree {
    const SpeciesParams& sp;
    FloraShape shape;
    float height;
    float crown_base;
    float crown_top;
    float crown_r;
    float trunk_r;
    uint32_t wood;
    uint32_t twig; ///< thin-branch value; see SpeciesParams::twig_color
    uint32_t leaf;
    FloraLod lod;
    Rng rng;
    float flare_h = FLARE_HEIGHT;
    float flare_depth = FLARE_DEPTH;
    /// Null when the species has no card foliage, or winter has stripped it.
    MeshData* cards = nullptr;
    /// The walkability floor THIS tree's foliage obeys. CANOPY_CLEARANCE_MIN
    /// for canopy species, 0 for everything else — a krummholz pine and a bush
    /// carry foliage to the ground BY DESIGN (obstacles you walk around, §3.5
    /// exempts them). This used to be hard-wired to the canopy constant inside
    /// emit_card_cluster, which was a canopy rule applied by a shared helper
    /// to every card species — invisible until the first non-canopy card
    /// species existed, then three symptoms from one mechanism (cards shoved
    /// to 2.2 m, cards shrunk against the reduced span, cards torn off their
    /// anchors in gap units).
    float clearance_floor = 0.0f;
    glm::vec3 stem_off{0.0f}; ///< the stem this geometry belongs to (clumps)
    /// The point a foliage cluster's sway weight is measured FROM (vertex RED is
    /// 0 here and 1 at the free edge). Under the old envelope-scattered crown
    /// this could only be the crown base, because no cluster knew what it hung
    /// on. It is now the ANCHOR NODE, so the gradient runs outward from the limb
    /// the leaves actually grow on — physically right, and a better rustle.
    glm::vec3 sway_from{0.0f};
    float phase = 0.0f; ///< per-instance wind phase -> vertex GREEN
};

[[nodiscard]] CrownVolume crown_volume(const Tree& t);
[[nodiscard]] float envelope_radius(const Tree& t, float y);
[[nodiscard]] bool emits_clusters(const SpeciesParams& sp);
[[nodiscard]] float shy_scale_xz(const Tree& t, glm::vec2 d);
[[nodiscard]] float shy_scale(const Tree& t, glm::vec3 dir_xz);
[[nodiscard]] glm::vec3 clip_to_envelope(const Tree& t, glm::vec3 p);

/// One tapered tube segment ring-to-ring. Flat-shaded (faces own vertices).
void tube_segment(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis, float r0,
                  float r1, int sides, uint32_t color);

/// Faceted ellipsoid — bushes and the Silhouette LOD only.
void blob_cluster(MeshData& m, glm::vec3 c, glm::vec3 radii, int slices, int bands,
                  uint32_t color);

/// One CROSSED CARD CLUSTER: 1-3 flat quads intersecting at a shared centre.
void emit_card_cluster(Tree& t, glm::vec3 at, float reach, int card_count);

/// Places one foliage cluster, in whichever medium the species uses, after
/// running the envelope containment rules.
void emit_cluster(MeshData& m, Tree& t, glm::vec3 at, float radius,
                  int card_count = -1);

} // namespace dfn::render
