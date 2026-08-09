/*
Created: 09:08:2026 - 19:31:02
Last updated: 09:08:2026 - 21:02:02
Module: engine/render
File: engine/render/sources/ProcFlora.cpp

Responsibility:
- The parametric branching generator: trunk sweep with root flare, recursive
  branch skeleton with phototropism and gravity droop, foliage clusters clipped
  to the species silhouette envelope, and the neighbourhood analysis that gives
  crown shyness, lean-away and understory.

Key items:
- build_flora_mesh, append_flora, emit_card_cluster, analyse_neighbourhood,
  flora_variant_for, species metadata.

Dependencies:
- Uses: ProcFlora.h, FloraSpecies.h, FloraCards.h (the card emitter and the
  leaf atlas vocabulary), ProcMesh.h (tri/quad/pack), Constants.h.
- Used by: ScatterBatcher (render), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE + DETERMINISTIC. All randomness comes from the local splitmix64 keyed by
  (species, variant, node) — never a global RNG, never time.
- TWO HARD FLOORS, both load-bearing (§3.5): branches terminate at
  MIN_BRANCH_DIAMETER (thinner casts NO shadow at SHADOW_TEXEL_M 0.156 and
  shimmers at 640x360 — we do not model twigs), and canopy species keep
  CANOPY_CLEARANCE_MIN of clear trunk.
*/
/*
UPD:
- 09:08:2026 - 19:31:02: Created — stage-4 parametric branching system.
- 09:08:2026 - 20:21:13: Broadleaf foliage is now ALPHA-CUTOUT CARDS. Solid
  blob clusters survive only for bushes and the Silhouette LOD. A tree is built
  as two streams (FloraMesh.wood / .cards) because the two render programs read
  vertex colour differently; per-instance wind phase is derived from the
  instance position in analyse_neighbourhood; append_flora() bakes both streams
  for the batcher.
- 09:08:2026 - 21:02:02: Card legibility floor: a card shrunk by envelope
  containment below a quarter of the crown radius is not emitted, because it
  renders as a detached scrap of foliage rather than as part of the crown.
  Reduced LOD now spends its saving on fewer/larger clusters and one plane less
  per cluster, rather than only on the skeleton.
*/

#include "engine/render/sources/ProcFlora.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

constexpr float TAU = 6.28318530717958647692f;
constexpr float GOLDEN_ANGLE = 2.39996322972865332f; // phyllotaxis
constexpr float CLEARANCE_MIN = static_cast<float>(config::CANOPY_CLEARANCE_MIN);

// Root flare (§3.5): a 1.2 m trunk on TREE_SLOPE_MAX spans 1.2*tan(35 deg) =
// 0.84 m of ground drop across its OWN base, before design's micro relief.
// The skirt buries itself instead of a global sink fudge.
constexpr float FLARE_HEIGHT = 1.2f;
constexpr float FLARE_WIDEN = 1.6f;
constexpr float FLARE_DEPTH = 1.0f;
// Render's constraint: the flare must stay above the shadow-caster floor all
// the way down, or the tree reads as hovering even while correctly buried.
constexpr float SHADOW_MIN_DIAMETER = 0.35f;

/// splitmix64 — local, deterministic, no shared state.
uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(mix64(seed)) {}
    float unit() { // [0,1)
        s = mix64(s);
        return static_cast<float>(s >> 40) / 16777216.0f;
    }
    float sym() { return unit() * 2.0f - 1.0f; } // [-1,1)
};

glm::vec3 safe_normalize(glm::vec3 v, glm::vec3 fallback) {
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

/// Any vector perpendicular to `n` (stable basis for tube rings).
glm::vec3 perp_of(glm::vec3 n) {
    const glm::vec3 a = std::fabs(n.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f}
                                              : glm::vec3{1.0f, 0.0f, 0.0f};
    return safe_normalize(glm::cross(a, n), glm::vec3{1.0f, 0.0f, 0.0f});
}

/// One tapered tube segment ring-to-ring. Flat-shaded (faces own vertices).
void tube_segment(MeshData& m, glm::vec3 p0, glm::vec3 p1, glm::vec3 axis,
                  float r0, float r1, int sides, uint32_t color) {
    const glm::vec3 u = perp_of(axis);
    const glm::vec3 v = glm::cross(axis, u);
    for (int i = 0; i < sides; ++i) {
        const float a0 = TAU * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = TAU * static_cast<float>(i + 1) / static_cast<float>(sides);
        const glm::vec3 d0 = u * std::cos(a0) + v * std::sin(a0);
        const glm::vec3 d1 = u * std::cos(a1) + v * std::sin(a1);
        if (r1 <= 1e-4f) {
            tri(m, p0 + d0 * r0, p1, p0 + d1 * r0, color);
        } else {
            quad(m, p0 + d0 * r0, p1 + d0 * r1, p1 + d1 * r1, p0 + d1 * r0, color);
        }
    }
}

/// Faceted ellipsoid cluster — the cheapest triangles that carry value, and
/// what actually casts the canopy shadow.
void cluster(MeshData& m, glm::vec3 c, glm::vec3 radii, int slices, int bands,
             uint32_t color) {
    auto point = [&](int b, int s) {
        const float t = -glm::half_pi<float>()
            + glm::pi<float>() * static_cast<float>(b) / static_cast<float>(bands);
        const float p = TAU * static_cast<float>(s) / static_cast<float>(slices);
        return c + glm::vec3{radii.x * std::cos(t) * std::cos(p), radii.y * std::sin(t),
                             radii.z * std::cos(t) * std::sin(p)};
    };
    for (int b = 0; b < bands; ++b) {
        for (int s = 0; s < slices; ++s) {
            const glm::vec3 p00 = point(b, s), p01 = point(b, s + 1);
            const glm::vec3 p10 = point(b + 1, s), p11 = point(b + 1, s + 1);
            if (b == 0) {
                tri(m, p01, p11, p10, color);
            } else if (b + 1 == bands) {
                tri(m, p00, p10, p01, color);
            } else {
                quad(m, p00, p10, p11, p01, color);
            }
        }
    }
}

/// The species silhouette envelope: crown half-width at height `y`, given the
/// crown spanning [base, top]. This is what GUARANTEES the 8 px read.
float envelope_radius(const SpeciesParams& sp, float y, float base, float top,
                      float max_r) {
    if (y <= base || top <= base) return 0.0f;
    const float t = std::clamp((y - base) / (top - base), 0.0f, 1.0f);
    switch (sp.envelope) {
    case CrownEnvelope::Sphere: {
        const float s = std::sin(t * glm::pi<float>());
        return max_r * (0.35f + 0.65f * s);
    }
    case CrownEnvelope::Cone:
        return max_r * (1.0f - t);
    case CrownEnvelope::Vase:
        return max_r * (0.30f + 0.70f * t);
    case CrownEnvelope::Weeping: {
        // Wide shoulder low, falling skirt: fullest just above the base.
        const float s = std::sin(std::pow(t, 0.55f) * glm::pi<float>());
        return max_r * (0.45f + 0.55f * s);
    }
    case CrownEnvelope::None:
    default:
        return 0.0f;
    }
}

/// Fraction of the species height carried by the TRUNK, the rest being filled
/// by branches and their foliage. Not cosmetic: the species height band is a
/// CROSS-ZONE CONTRACT (core's canopy occlusion and design's C4 arithmetic use
/// OAK/PINE/BIRCH_HEIGHT_MAX), so a tree whose branches overshoot its band is
/// silently taller than the model everyone else validates against. Broadleaf
/// leaders dissolve into the crown well below the top — which is both the
/// botanically correct shape and the fix.
float trunk_height_frac(CrownEnvelope e) {
    switch (e) {
    case CrownEnvelope::Sphere:
        return 0.68f;
    case CrownEnvelope::Weeping: // the skirt falls, so the leader must sit high
        return 0.86f;
    case CrownEnvelope::Vase: // birch keeps a high leader; the crown is small
        return 0.82f;
    case CrownEnvelope::Cone: // conifer leader IS the top
    case CrownEnvelope::None:
    default:
        return 1.0f;
    }
}

struct Tree {
    const SpeciesParams& sp;
    FloraShape shape;
    float height;
    float crown_base;
    float crown_top;
    float crown_r;
    float trunk_r;
    uint32_t wood;
    uint32_t leaf;
    FloraLod lod;
    Rng rng;
    float flare_h = FLARE_HEIGHT;
    float flare_depth = FLARE_DEPTH;
    // --- leaf cards (§3.8). `cards` is null when the species has no card
    // foliage, or when winter has stripped it.
    MeshData* cards = nullptr;
    glm::vec3 stem_off{0.0f}; ///< the stem this geometry belongs to (clumps)
    float phase = 0.0f;       ///< per-instance wind phase -> vertex GREEN
};

/// Does this species put anything in a foliage cluster at all?
bool emits_clusters(const SpeciesParams& sp) {
    return (sp.foliage == FoliageShape::Blob || sp.foliage == FoliageShape::Card)
        && sp.cluster_count > 0;
}

/// Crown shyness: the crown is pulled back on the side facing a close
/// neighbour. Interpenetrating crowns read as one mud-coloured mass at low
/// resolution; separated crowns read as trees.
float shy_scale_xz(const Tree& t, glm::vec2 d) {
    if (t.shape.shyness <= 0.0f) return 1.0f;
    const float l = glm::length(d);
    if (l < 1e-5f) return 1.0f;
    const float align = glm::dot(d / l, t.shape.shy_dir); // -1..1
    return 1.0f - t.shape.shyness * std::max(0.0f, align);
}

float shy_scale(const Tree& t, glm::vec3 dir_xz) {
    return shy_scale_xz(t, glm::vec2{dir_xz.x, dir_xz.z});
}

/// Pulls a point inside the species envelope, radially and vertically. THIS IS
/// THE MECHANISM that makes a silhouette guaranteed rather than emergent
/// (docs/specs/flora.md §3.1 stage D) — without the radial half, branches reach
/// wherever the growth rules take them and an oak comes out twice its design
/// width, which then breaks the spacing that was derived FROM that width.
glm::vec3 clip_to_envelope(const Tree& t, glm::vec3 p) {
    p.y = std::min(p.y, t.crown_top);
    if (t.sp.envelope == CrownEnvelope::None) return p; // snags/logs have no crown
    const float env = envelope_radius(t.sp, p.y, t.crown_base, t.crown_top, t.crown_r);
    // Below the crown the trunk owns the space; above the base the envelope does.
    // NOTE: env == 0 must still CLAMP, not skip. A cone's envelope goes to zero
    // at the apex, and an "env <= 0 -> return unclipped" early-out disabled the
    // clip exactly at the tip — which let a whorl branch stick 7.6 m out of the
    // top of a pine whose whole crown is meant to be 4 m in radius.
    if (p.y <= t.crown_base) return p;
    const glm::vec2 r{p.x, p.z};
    const float len = glm::length(r);
    const float limit = env * shy_scale_xz(t, r);
    if (len > limit && len > 1e-5f) {
        const glm::vec2 c = r * (limit / len);
        p.x = c.x;
        p.z = c.y;
    }
    return p;
}

/// One CROSSED CARD CLUSTER: 1-3 flat quads intersecting at a shared centre so
/// the group reads as volume from any azimuth.
///
/// The cards are FIXED-ORIENTATION, never camera-facing billboards. A billboard
/// rotates visibly at 640x360, shimmers under palette quantization, and is
/// wrong in the shadow pass by construction — a card turned to face the eye
/// casts a rotating shadow. Orientation comes from where the cluster sits in
/// the crown: outward from the crown axis, with alternating tilt.
/// `reach` is the CORNER radius of a card (not its half-width): a card's far
/// corner, not its edge midpoint, is what the species width band and the
/// envelope actually have to contain.
void emit_card_cluster(Tree& t, glm::vec3 at, float reach, int card_count) {
    if (t.cards == nullptr || reach <= 0.05f) return;
    const SpeciesParams& sp = t.sp;
    const int n = std::clamp(card_count, 1, 4);
    const float diag = std::sqrt(1.0f + sp.card_aspect * sp.card_aspect);

    LeafCardParams p;
    p.half_width = reach / diag;
    p.half_height = p.half_width * sp.card_aspect;

    // LEGIBILITY FLOOR. Containment shrinks a card to fit the envelope, and
    // where the envelope is narrow (the bottom of a vase, the tip of a cone)
    // that shrinking runs past the point where the card can join the mass: it
    // then renders as a detached scrap of foliage hanging under the crown, which
    // is worse than nothing there. A card under a quarter of the crown radius
    // cannot read as part of the crown at 640x360, so it is not emitted.
    // Fraction, not metres, so it scales with maturity for free.
    if (p.half_width < 0.22f * t.crown_r) return;

    // VERTICAL REACH, not half_height. THIS IS THE SAME MISTAKE A THIRD TIME
    // (§3.7): a card is tilted in elevation and rolled in its own plane, so the
    // lowest and highest points it reaches are its CORNERS, up to
    // hypot(half_width, half_height) away — not the midpoints of its edges.
    // Clamping on half_height let cards hang below the crown base and pushed the
    // measured foliage box well outside the container, which is precisely the
    // quantity design's CROWN_ASPECT_MAX is measured on.
    auto vertical_reach = [&] {
        return std::sqrt(p.half_width * p.half_width + p.half_height * p.half_height);
    };
    // The species HEIGHT band is a cross-zone contract (§3.7.1).
    if (at.y + vertical_reach() > t.crown_top) {
        at.y = t.crown_top - vertical_reach();
    }
    // CANOPY CLEARANCE, measured where it is actually felt: the lowest FOLIAGE
    // vertex, not the nominal crown base (§3.5 — drooping species are the whole
    // reason the rule is worded that way, and a card hangs below its centre).
    // Remedy order matters and it is the §3.7.5 lesson: RAISE the cluster, and
    // only shrink it when raising would push it out of the envelope. Sacrificing
    // the arrangement to preserve a declared size is what stacked the birch.
    const float floor_y = std::max(t.crown_base, CLEARANCE_MIN);
    if (at.y - vertical_reach() < floor_y) {
        const float raised = floor_y + vertical_reach();
        if (raised + vertical_reach() <= t.crown_top) {
            at.y = raised;
        } else {
            const float span = std::max((t.crown_top - floor_y) * 0.5f, 0.05f);
            p.half_height = span / diag * sp.card_aspect;
            p.half_width = span / diag;
            at.y = floor_y + vertical_reach();
        }
    }
    if (p.half_width < 0.22f * t.crown_r) return; // re-check after any shrink
    // The attachment (sway weight 0) is the stem at the base of the crown, so
    // the whole crown's sway grows outward and upward from the trunk exactly
    // as a real one does, and the gradient ACROSS each card — its inner corner
    // nearer the stem than its outer one — is what makes the card bend instead
    // of sliding like a flag.
    p.sway_origin = {t.stem_off.x, t.crown_base, t.stem_off.z};
    p.sway_span = glm::length(at - p.sway_origin) + p.half_width;
    p.phase = t.phase;

    const glm::vec2 out_xz{at.x - t.stem_off.x, at.z - t.stem_off.z};
    const float base_az = glm::length(out_xz) > 1e-4f
                              ? std::atan2(out_xz.y, out_xz.x)
                              : t.rng.unit() * TAU;
    for (int k = 0; k < n; ++k) {
        const float az =
            base_az + glm::pi<float>() * static_cast<float>(k) / static_cast<float>(n)
            + t.rng.sym() * 0.18f;
        // Alternating elevation: a cluster of purely vertical planes is
        // invisible from directly above, which is exactly the view a player
        // gets of a crown from a hillside.
        const float el = (k % 2 == 0 ? 0.28f : -0.34f) + t.rng.sym() * 0.12f;
        p.normal = {std::cos(el) * std::cos(az), std::sin(el),
                    std::cos(el) * std::sin(az)};
        p.roll = t.rng.sym() * 0.35f;
        // Shape and TONE are drawn independently — that is the whole point of a
        // SHAPE x COLOUR atlas: one outline may appear light and dark in the
        // same crown, which is what gives a nearly opaque crown its volume.
        p.shape = (t.rng.unit() < 0.62f) ? sp.card_shape_a : sp.card_shape_b;
        const auto tone_i = static_cast<uint32_t>(sp.tone_first)
            + static_cast<uint32_t>(t.rng.unit()
                                    * static_cast<float>(std::max<uint8_t>(sp.tone_count, 1)));
        p.tone = static_cast<LeafTone>(
            std::min<uint32_t>(tone_i, LEAF_ATLAS_TONES - 1));
        // b: ONE value per card, all six vertices. Per-vertex would make it a
        // gradient across the card, which is not what a value jitter is.
        p.value_jitter = t.rng.unit();
        p.center = at;
        emit_leaf_card(*t.cards, p);
    }
}

void emit_cluster(MeshData& m, Tree& t, glm::vec3 at, float radius, // NOLINT
                  int card_count = -1) {
    if (radius <= 0.05f) return;
    // For cards the containment must be run on the CARD's CORNER reach, not on
    // the notional cluster radius — otherwise the crown quietly grows by
    // card_width_frac times the card diagonal, and crown width is load-bearing
    // (design derived TREE_SPACING_FOREST from it, §3.7.2).
    if (t.sp.foliage == FoliageShape::Card) {
        radius *= t.sp.card_width_frac
            * std::sqrt(1.0f + t.sp.card_aspect * t.sp.card_aspect);
    }
    // Foliage may not push the silhouette outside the species envelope.
    at = clip_to_envelope(t, at);
    at.y = std::min(at.y, t.crown_top - radius * 0.85f);
    const glm::vec2 rxz{at.x, at.z};
    const float env = envelope_radius(t.sp, at.y, t.crown_base, t.crown_top, t.crown_r);
    const float len = glm::length(rxz);
    if (env > 0.0f && len + radius > env) {
        // Shrink the cluster to fit rather than sliding its centre inward.
        // Sliding collapsed every over-sized cluster onto the trunk axis, which
        // on a narrow vase crown stacked them into a drill-bit of discs — the
        // second incarnation of the same "not a mass" defect.
        radius = std::min(radius, env);
        if (len + radius > env && len > 1e-5f) {
            const glm::vec2 c = rxz * (std::max(env - radius, 0.0f) / len);
            at.x = c.x;
            at.z = c.y;
        }
    }
    if (t.sp.foliage == FoliageShape::Card) {
        emit_card_cluster(t, at,
                          radius,
                          card_count > 0 ? card_count : t.sp.cards_per_cluster);
        return;
    }
    const int slices = t.sp.cluster_slices;
    const int bands = t.sp.cluster_bands;
    cluster(m, at, glm::vec3{radius, radius * 0.85f, radius}, slices, bands, t.leaf);
}

/// Where in the crown span the foliage starts filling. A vase/column crown is
/// tall and narrow, so spreading clusters over its whole span leaves visible
/// gaps between them; concentrating them up top gives the "small loose crown on
/// a slim pale trunk" the birch brief asks for (LANDSCAPE §5.3).
float crown_fill_start(CrownEnvelope e) {
    switch (e) {
    case CrownEnvelope::Vase:
        return 0.35f;
    case CrownEnvelope::Weeping:
        return 0.10f;
    default:
        return 0.0f;
    }
}

/// Distributes the species' foliage clusters over the crown envelope. The
/// crown must exist whether or not the branch skeleton survived the shadow
/// floor — foliage is what READS at distance (silhouette + value, §1.5), while
/// branches are structure you only resolve up close.
void scatter_envelope_clusters(MeshData& m, Tree& t, glm::vec3 off, float radial_frac) {
    const SpeciesParams& sp = t.sp;
    if (!emits_clusters(sp)) return;
    if (sp.foliage == FoliageShape::Card && t.cards == nullptr) return;
    const float span = t.crown_top - t.crown_base;
    if (span <= 0.0f) return;
    // Reduced LOD spends its saving on the CROWN, not on the skeleton: fewer,
    // correspondingly larger clusters and one plane less per cluster. The
    // silhouette is preserved because the envelope still governs; what is lost
    // is interior card overlap, which is only resolvable close up — and close
    // up is not where Reduced is used.
    const bool reduced = t.lod == FloraLod::Reduced;
    const int count = reduced ? std::max(2, sp.cluster_count * 2 / 3) : sp.cluster_count;
    const float size_boost = reduced
        ? std::sqrt(static_cast<float>(sp.cluster_count) / static_cast<float>(count))
        : 1.0f;
    const int cards = reduced ? std::max(1, sp.cards_per_cluster - 1) : -1;
    for (int i = 0; i < count; ++i) {
        const float u_raw = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        // Fill the UPPER crown and vary the radius per cluster. One cluster per
        // evenly-spaced height at a fixed radius produces a helix of separated
        // blobs — which rendered as a ladder of floating discs up the birch
        // trunk, not a crown. A crown must read as ONE MASS at 640x360.
        const float u = crown_fill_start(sp.envelope)
            + (1.0f - crown_fill_start(sp.envelope)) * std::pow(u_raw, 0.8f);
        const float y = t.crown_base + span * u;
        const float env = envelope_radius(sp, y, t.crown_base, t.crown_top, t.crown_r);
        // Deterministic per-index radial spread so clusters occupy the crown
        // VOLUME instead of one shell, and overlap into a continuous mass.
        const float wobble = std::fabs(std::sin(GOLDEN_ANGLE * static_cast<float>(i) * 1.7f));
        const float rf = radial_frac * (0.30f + 0.70f * wobble);
        const float az = GOLDEN_ANGLE * static_cast<float>(i);
        const glm::vec3 at{std::cos(az) * env * rf, y, std::sin(az) * env * rf};
        emit_cluster(m, t, at + off,
                     t.crown_r * sp.cluster_radius_frac * shy_scale(t, at) * size_boost,
                     cards);
    }
}

/// Grows one branch, and recursively its children. Returns nothing — geometry
/// goes straight into `m`. `gen` is the generation index (0 = primary).
void grow_branch(MeshData& m, Tree& t, glm::vec3 base, glm::vec3 dir, float length,
                 float radius, int gen) {
    const SpeciesParams& sp = t.sp;
    // SHADOW FLOOR: a thinner branch casts nothing and shimmers, so we do not
    // model it — but its FOLIAGE still has to exist, attached to the parent.
    // Returning without emitting it left birches as bare poles: their primary
    // branches (0.17 m) are under the floor, so the entire crown vanished.
    if (radius * 2.0f < sp.min_branch_diameter || length < 0.4f) {
        if (emits_clusters(sp)) {
            const glm::vec3 at = clip_to_envelope(t, base);
            if (at.y > t.crown_base) {
                // One card at a terminated branch, not a whole crossed trio:
                // the crown MASS is the envelope scatter's job (§3.7.3), this
                // is only the visual join between limb and foliage.
                emit_cluster(m, t, at,
                             t.crown_r * sp.cluster_radius_frac * shy_scale(t, at), 1);
            }
        }
        return;
    }
    const int segments = (t.lod == FloraLod::Full) ? (gen == 0 ? 3 : 2) : 2;
    const float seg_len = length / static_cast<float>(segments);
    glm::vec3 p = base;
    glm::vec3 d = dir;
    const int sides = 4;

    for (int s = 0; s < segments; ++s) {
        // Phototropism: blend toward light. Under a canopy light is overhead;
        // for an edge tree it comes from the open side (shape.lean_dir).
        glm::vec3 light{0.0f, 1.0f, 0.0f};
        if (t.shape.lean > 0.0f) {
            light = safe_normalize(
                glm::vec3{t.shape.lean_dir.x * 0.6f, 1.0f, t.shape.lean_dir.y * 0.6f},
                light);
        }
        d = safe_normalize(d + light * (sp.phototropism * seg_len / length), d);
        // Gravity droop: thin tips fall, thick limbs do not. Negative = upsweep.
        const float thinness = std::clamp(0.25f / std::max(radius, 0.05f), 0.0f, 4.0f);
        d = safe_normalize(
            d + glm::vec3{0.0f, -1.0f, 0.0f}
                    * (sp.droop * thinness * seg_len / std::max(length, 0.1f)),
            d);

        const float t0 = static_cast<float>(s) / static_cast<float>(segments);
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(segments);
        const float r0 = radius * (1.0f - 0.75f * t0);
        const float r1 = radius * (1.0f - 0.75f * t1);
        // Envelope clip, radial AND vertical: a crown flattens and narrows
        // under competition rather than spiking past the species band.
        const glm::vec3 next = clip_to_envelope(t, p + d * seg_len);
        const glm::vec3 step = safe_normalize(next - p, d);
        tube_segment(m, p, next, step, r0, std::max(r1, 0.02f), sides, t.wood);
        p = next;
    }

    const bool last_gen = gen + 1 >= sp.generations;
    if (!last_gen && t.lod == FloraLod::Full) {
        const int count = sp.branch_count[gen + 1];
        const glm::vec3 side = perp_of(d);
        const glm::vec3 side2 = glm::cross(d, side);
        for (int i = 0; i < count; ++i) {
            const float az = GOLDEN_ANGLE * static_cast<float>(i) + t.rng.unit() * 0.5f;
            const float pitch = sp.branch_angle[gen + 1] * (0.85f + t.rng.unit() * 0.3f);
            const glm::vec3 out = side * std::cos(az) + side2 * std::sin(az);
            const glm::vec3 cd =
                safe_normalize(d * std::cos(pitch) + out * std::sin(pitch), d);
            const float frac = sp.branch_start_frac[gen + 1]
                + (1.0f - sp.branch_start_frac[gen + 1])
                      * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            const glm::vec3 attach = base + (p - base) * frac;
            grow_branch(m, t, attach, cd, length * sp.length_decay[gen + 1],
                        radius * sp.radius_ratio[gen + 1], gen + 1);
        }
    }

    // Foliage at the tip, clipped to the envelope so the silhouette holds.
    if (emits_clusters(sp)) {
        const float env =
            envelope_radius(sp, p.y, t.crown_base, t.crown_top, t.crown_r);
        if (env > 0.0f) {
            const float r = std::min(t.crown_r * sp.cluster_radius_frac, env * 0.9f);
            emit_cluster(m, t, p, r * shy_scale(t, p),
                         sp.foliage == FoliageShape::Card ? 1 : -1);
        }
    }
}

/// Trunk with root flare. Returns the top point and its direction.
glm::vec3 build_trunk(MeshData& m, Tree& t, glm::vec3 base, float height,
                      float radius, glm::vec3* out_dir) {
    const SpeciesParams& sp = t.sp;
    const int segments = std::max<int>(2, (t.lod == FloraLod::Silhouette)
                                              ? 2
                                              : sp.trunk_segments);
    const int sides = sp.trunk_sides;

    // Root flare: widen and sink so the skirt buries itself in whatever the
    // terrain does. Kept above the shadow-caster floor at its narrowest.
    const float flare_r = std::max(radius * FLARE_WIDEN, SHADOW_MIN_DIAMETER * 0.5f);
    tube_segment(m, base + glm::vec3{0.0f, -t.flare_depth, 0.0f},
                 base + glm::vec3{0.0f, t.flare_h, 0.0f},
                 glm::vec3{0.0f, 1.0f, 0.0f}, flare_r,
                 std::max(radius, SHADOW_MIN_DIAMETER * 0.5f), sides, t.wood);

    glm::vec3 p = base + glm::vec3{0.0f, t.flare_h, 0.0f};
    glm::vec3 d{0.0f, 1.0f, 0.0f};
    const float span = std::max(height - t.flare_h, 0.5f);
    const float seg_len = span / static_cast<float>(segments);
    const glm::vec3 sweep_dir =
        t.shape.lean > 0.0f
            ? safe_normalize(glm::vec3{t.shape.lean_dir.x, 0.0f, t.shape.lean_dir.y},
                             glm::vec3{1.0f, 0.0f, 0.0f})
            : glm::vec3{1.0f, 0.0f, 0.0f};
    const float bend = sp.trunk_sweep + t.shape.lean;

    for (int s = 0; s < segments; ++s) {
        d = safe_normalize(d + sweep_dir * (bend / static_cast<float>(segments)), d);
        const float t0 = static_cast<float>(s) / static_cast<float>(segments);
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(segments);
        const float r0 = radius * std::pow(1.0f - t0 * 0.92f, sp.taper_exp);
        const float r1 = radius * std::pow(1.0f - t1 * 0.92f, sp.taper_exp);
        const glm::vec3 next = p + d * seg_len;
        tube_segment(m, p, next, d, std::max(r0, 0.03f), std::max(r1, 0.03f), sides,
                     t.wood);
        p = next;
    }
    if (out_dir) *out_dir = d;
    return p;
}

/// Conifer tiers (§5.2: 2-3 stacked cones, top cone >= 1.5 m wide so the tip
/// survives quantization). Cheap, and it IS the pine's identity.
void build_cone_tiers(MeshData& m, Tree& t) {
    const int tiers = std::max<int>(2, t.sp.cluster_count);
    const float span = t.crown_top - t.crown_base;
    if (span <= 0.0f) return;
    for (int i = 0; i < tiers; ++i) {
        const float f0 = static_cast<float>(i) / static_cast<float>(tiers);
        const float y0 = t.crown_base + span * f0;
        const float y1 = t.crown_base + span * std::min(1.0f, f0 + 1.0f / tiers + 0.28f);
        const float r = std::max(envelope_radius(t.sp, y0, t.crown_base, t.crown_top,
                                                 t.crown_r),
                                 0.75f);
        const int sides = (t.lod == FloraLod::Full) ? 8 : 6;
        tube_segment(m, glm::vec3{0.0f, y0, 0.0f}, glm::vec3{0.0f, y1, 0.0f},
                     glm::vec3{0.0f, 1.0f, 0.0f}, r * shy_scale(t, {1.0f, 0.0f, 0.0f}),
                     0.0f, sides, t.leaf);
    }
}

/// Silhouette LOD: trunk column + one envelope shell. Deliberately close to the
/// pre-flora mesh — at that range the silhouette is the entire information.
void build_silhouette(MeshData& m, Tree& t) {
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    build_trunk(m, t, glm::vec3{0.0f}, t.height, t.trunk_r, &dir);
    if (t.sp.envelope == CrownEnvelope::None) return;
    if (t.sp.envelope == CrownEnvelope::Cone) {
        build_cone_tiers(m, t);
        return;
    }
    const float mid = (t.crown_base + t.crown_top) * 0.5f;
    cluster(m, glm::vec3{0.0f, mid, 0.0f},
            glm::vec3{t.crown_r, (t.crown_top - t.crown_base) * 0.5f, t.crown_r}, 6, 3,
            t.leaf);
}

} // namespace

uint32_t flora_variant_for(glm::vec2 world_xz) {
    const auto xi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.x * 4.0f)));
    const auto zi = static_cast<uint64_t>(static_cast<int64_t>(std::lround(world_xz.y * 4.0f)));
    return static_cast<uint32_t>(mix64(xi * 0x9E3779B1ull ^ mix64(zi)) % FLORA_VARIANTS);
}

FloraSpecies flora_species_of(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return FloraSpecies::DaleOak;
    case math::ScatterSpecies::PineTree: return FloraSpecies::HighlandPine;
    case math::ScatterSpecies::BirchTree: return FloraSpecies::RiverBirch;
    case math::ScatterSpecies::Bush: return FloraSpecies::Bush;
    default: return FloraSpecies::Bush;
    }
}

float species_nominal_height(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return (sp.height_min + sp.height_max) * 0.5f;
}

float species_crown_radius(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.crown_width_frac * 0.5f;
}

float species_crown_base(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.crown_base_frac;
}

float species_trunk_radius(FloraSpecies s) {
    const SpeciesParams& sp = species_params(s);
    return species_nominal_height(s) * sp.trunk_radius_frac * FLARE_WIDEN;
}

FloraMesh build_flora_mesh(FloraSpecies species, uint32_t variant,
                           const FloraShape& shape, FloraLod lod,
                           FloraSeason season) {
    const SpeciesParams& sp = species_params(species);
    FloraMesh parts;
    MeshData& m = parts.wood;

    Rng rng{mix64(static_cast<uint64_t>(species) * 0x1000193ull
                  ^ (static_cast<uint64_t>(variant) + 1) * 0x9E3779B97F4A7C15ull)};

    float height = sp.height_min + rng.unit() * (sp.height_max - sp.height_min);
    height *= std::max(0.2f, shape.maturity);

    float crown_base_frac = sp.crown_base_frac;
    float crown_width_frac = sp.crown_width_frac;
    if (shape.understory) {
        crown_base_frac = std::min(0.75f, crown_base_frac + 0.10f);
        crown_width_frac *= 0.8f;
    }
    // CROWN ASPECT CEILING (design's ruling, §5). The crown's container is
    // (1 - crown_base_frac) tall by crown_width_frac wide, both in units of
    // height, so the ceiling is a lower bound on the crown base and the base is
    // DERIVED rather than exempted per species. Conifers are exempt as a
    // property of their silhouette brief — a cone is MEANT to be narrow, and a
    // pine at 4.2 is the anti-oak, not a defect.
    //
    // Why this exists at all, in one line, because it is the most expensive
    // lesson in the zone: two authored rules multiplied into a container 2.3x
    // taller than wide, and FOUR attempts at rearranging its CONTENTS failed
    // because the mass IS the container. Checked here, on the first build,
    // instead of on the fourth screenshot.
    if (sp.envelope != CrownEnvelope::Cone && sp.envelope != CrownEnvelope::None) {
        const auto ceiling = static_cast<float>(config::CROWN_ASPECT_MAX);
        // 0.97: derive just inside the ceiling, so the assertion on the BUILT
        // tree has somewhere to fail if the geometry ever drifts outward again.
        const float from_aspect = 1.0f - ceiling * 0.97f * crown_width_frac;
        crown_base_frac = std::max(crown_base_frac, from_aspect);
    }

    const bool woody_tree = is_canopy_tree(species) || species == FloraSpecies::Snag;
    Tree t{sp,
           shape,
           height,
           height * crown_base_frac,
           height,
           height * crown_width_frac * 0.5f,
           height * sp.trunk_radius_frac,
           pack(sp.trunk_color),
           pack(sp.foliage_color),
           lod,
           rng,
           woody_tree ? std::clamp(height * 0.045f, 0.5f, 1.4f)
                      : std::clamp(height * 0.08f, 0.08f, 0.4f),
           woody_tree ? FLARE_DEPTH : std::clamp(height * 0.10f, 0.10f, 0.4f)};

    // Card foliage exists unless winter has stripped it. WINTER IS ONE BOOLEAN
    // (LANDSCAPE §5.11): do not emit the cards and the already-generated
    // skeleton IS the bare tree. Conifers keep their needles.
    // The Silhouette LOD deliberately keeps its solid shell: at that range the
    // silhouette is the entire information and a cutout buys nothing but
    // shimmer and overdraw.
    if (sp.foliage == FoliageShape::Card && lod != FloraLod::Silhouette
        && leaf_tone_has_foliage(sp.tone_first, season)) {
        t.cards = &parts.cards;
    }
    t.phase = shape.wind_phase;

    // HARD FLOOR (§3.5): canopy species keep CANOPY_CLEARANCE_MIN of clear
    // trunk. Enforced by construction, never by inspection.
    if (is_canopy_tree(species) && t.crown_base < CLEARANCE_MIN) {
        t.crown_base = CLEARANCE_MIN;
    }

    if (species == FloraSpecies::FallenLog || species == FloraSpecies::Deadfall) {
        // A log IS the trunk generator, laid down: built along +X, half-sunk.
        // Placement lays it ACROSS the fall line (design's binding doctrine);
        // the yaw for that is the batcher's, not ours.
        const float len = height;
        const float r = len * sp.trunk_radius_frac;
        const int segs = sp.trunk_segments;
        glm::vec3 p{-len * 0.5f, r * 0.45f, 0.0f};
        const glm::vec3 d{1.0f, 0.0f, 0.0f};
        for (int s = 0; s < segs; ++s) {
            const float f0 = static_cast<float>(s) / static_cast<float>(segs);
            const float f1 = static_cast<float>(s + 1) / static_cast<float>(segs);
            const glm::vec3 next = p + d * (len / static_cast<float>(segs));
            tube_segment(m, p, next, d, r * (1.0f - 0.35f * f0),
                         r * (1.0f - 0.35f * f1), sp.trunk_sides, t.wood);
            p = next;
        }
        return parts;
    }

    if (lod == FloraLod::Silhouette) {
        build_silhouette(m, t);
        return parts;
    }

    // Multi-trunk clumps (the user's "сколько стволов"): birch is 2-3 stems.
    const int stems = sp.trunk_count_min
        + static_cast<int>(t.rng.unit()
                           * static_cast<float>(sp.trunk_count_max - sp.trunk_count_min + 1));
    const int stem_count = std::clamp(stems, static_cast<int>(sp.trunk_count_min),
                                      static_cast<int>(sp.trunk_count_max));

    for (int k = 0; k < stem_count; ++k) {
        const float ang = TAU * static_cast<float>(k) / static_cast<float>(stem_count)
            + t.rng.unit() * 0.6f;
        const glm::vec3 off = stem_count > 1
            ? glm::vec3{std::cos(ang) * sp.trunk_spread, 0.0f,
                        std::sin(ang) * sp.trunk_spread}
            : glm::vec3{0.0f};
        // Cards measure their sway from the stem they hang on, not from the
        // model origin — in a birch clump those are metres apart.
        t.stem_off = off;
        // In a clump the LEAD stem carries the species height and the others
        // are shorter — that is what makes it read as one multi-stemmed tree
        // rather than as N small trees. Shrinking every stem (an earlier bug)
        // just made the whole species shorter than its band.
        const float stem_scale = (k == 0) ? 1.0f : (0.74f + t.rng.unit() * 0.22f);
        const float stem_h = height * trunk_height_frac(sp.envelope) * stem_scale;
        glm::vec3 dir{0.0f, 1.0f, 0.0f};
        const glm::vec3 top = build_trunk(m, t, off, stem_h, t.trunk_r, &dir);

        if (sp.envelope == CrownEnvelope::Cone) {
            build_cone_tiers(m, t);
        }
        if (sp.generations == 0) {
            scatter_envelope_clusters(m, t, off, 0.45f); // bushes: no skeleton
            continue;
        }
        // Canopy species: the crown shell exists independently of the skeleton.
        scatter_envelope_clusters(m, t, off, 0.68f);

        // Primary branches off the trunk, phyllotaxis or whorls.
        const int count = sp.branch_count[0];
        const int whorls = sp.whorled ? 3 : 1;
        for (int w = 0; w < whorls; ++w) {
            for (int i = 0; i < count; ++i) {
                const float az = sp.whorled
                    ? TAU * static_cast<float>(i) / static_cast<float>(count)
                          + static_cast<float>(w) * 0.7f
                    : GOLDEN_ANGLE * static_cast<float>(i + w * count);
                const float pitch = sp.branch_angle[0] * (0.85f + t.rng.unit() * 0.3f);
                const glm::vec3 side = perp_of(dir);
                const glm::vec3 side2 = glm::cross(dir, side);
                const glm::vec3 out = side * std::cos(az) + side2 * std::sin(az);
                const glm::vec3 cd =
                    safe_normalize(dir * std::cos(pitch) + out * std::sin(pitch), dir);
                const float base_frac = sp.branch_start_frac[0]
                    + (1.0f - sp.branch_start_frac[0])
                          * ((static_cast<float>(i) + 0.5f) / static_cast<float>(count)
                             + static_cast<float>(w))
                          / static_cast<float>(whorls);
                const glm::vec3 attach = off + (top - off) * std::min(base_frac, 0.97f);
                // Branch length is clipped so the tip cannot leave the envelope: the
                // species height band is a cross-zone contract, not a suggestion.
                float len = stem_h * sp.length_decay[0] * shy_scale(t, cd)
                    * (0.8f + t.rng.unit() * 0.4f);
                const float headroom = t.crown_top - attach.y;
                if (cd.y > 0.05f && headroom > 0.0f) {
                    len = std::min(len, headroom / cd.y);
                }
                grow_branch(m, t, attach, cd, len, t.trunk_r * sp.radius_ratio[0], 0);
            }
        }
    }
    return parts;
}

void append_flora(MeshData& wood, MeshData& cards, FloraSpecies species,
                  uint32_t variant, const FloraShape& shape, FloraLod lod,
                  glm::vec3 position, float yaw, FloraSeason season) {
    const FloraMesh parts = build_flora_mesh(species, variant, shape, lod, season);
    // Scale 1.0: the generator has ALREADY applied FloraShape::maturity, and a
    // tree does not sink — it stands on its root flare (§3.5).
    append_transformed(wood, parts.wood, position, yaw, 1.0f);
    append_transformed(cards, parts.cards, position, yaw, 1.0f);
}

std::vector<FloraShape> analyse_neighbourhood(std::span<const math::ScatterInstance> all,
                                              size_t count) {
    std::vector<FloraShape> out(std::min(count, all.size()));
    for (size_t i = 0; i < out.size(); ++i) {
        const math::ScatterInstance& a = all[i];
        const FloraSpecies fs = flora_species_of(a.species);
        FloraShape& sh = out[i];
        sh.maturity = a.scale;
        // Wind phase from the instance POSITION, not from the variant: twelve
        // skeletons would give twelve phases, and a stand where every twelfth
        // tree moves in lockstep reads as a repeating pattern the moment the
        // wind gusts. Position-derived, it is unique per tree and still
        // deterministic across runs and chunk borders.
        sh.wind_phase = static_cast<float>(
                            mix64(static_cast<uint64_t>(
                                      static_cast<int64_t>(std::lround(a.position.x * 8.0f)))
                                      * 0x9E3779B1ull
                                  ^ mix64(static_cast<uint64_t>(static_cast<int64_t>(
                                        std::lround(a.position.z * 8.0f)))))
                            >> 40)
            / 16777216.0f;
        if (!is_canopy_tree(fs)) continue;

        const float r_a = species_crown_radius(fs) * a.scale;
        const glm::vec2 pa{a.position.x, a.position.z};
        glm::vec2 pressure{0.0f};
        glm::vec2 worst_dir{0.0f};
        float worst_overlap = 0.0f;
        float tallest_neighbour = 0.0f;

        for (size_t j = 0; j < all.size(); ++j) {
            if (j == i) continue;
            const math::ScatterInstance& b = all[j];
            const FloraSpecies fb = flora_species_of(b.species);
            if (!is_canopy_tree(fb)) continue;
            const glm::vec2 pb{b.position.x, b.position.z};
            const glm::vec2 d = pb - pa;
            const float dist = glm::length(d);
            if (dist < 1e-3f || dist > 40.0f) continue;
            const float r_b = species_crown_radius(fb) * b.scale;
            const float overlap = r_a + r_b - dist;
            if (overlap <= 0.0f) continue;
            const glm::vec2 dir = d / dist;
            pressure += dir / (dist * dist);
            if (overlap > worst_overlap) {
                worst_overlap = overlap;
                worst_dir = dir;
            }
            const float h_b = species_nominal_height(fb) * b.scale;
            tallest_neighbour = std::max(tallest_neighbour, h_b);
        }

        const SpeciesParams& sp = species_params(fs);
        if (worst_overlap > 0.0f) {
            sh.shy_dir = worst_dir;
            sh.shyness = std::min(sp.shyness, worst_overlap / (2.0f * std::max(r_a, 0.1f)));
        }
        const float press = glm::length(pressure);
        if (press > 1e-5f) {
            // Lean AWAY from crowding — this is the visible difference between
            // a forest edge and a forest interior.
            sh.lean_dir = -pressure / press;
            sh.lean = std::min(sp.lean_response * press * 40.0f, 0.12f);
        }
        // Understory: a small tree under a much taller crowded canopy is drawn
        // out and reaching, not a scale model of a mature one.
        const float own_h = species_nominal_height(fs) * a.scale;
        sh.understory = a.scale < 0.75f && tallest_neighbour > own_h * 1.5f;
    }
    return out;
}

} // namespace dfn::render
