/*
Created: 09:08:2026 - 23:48:30
Last updated: 09:08:2026 - 23:48:30
Module: engine/render
File: engine/render/sources/FloraBuild.cpp

Responsibility:
- The tree build primitives: tapered tubes, faceted blobs, crossed leaf-card
  clusters, and the envelope/clearance containment every foliage cluster obeys.

Key items:
- tube_segment(), blob_cluster(), emit_cluster(), emit_card_cluster(),
  crown_volume(), envelope_radius(), clip_to_envelope(), shy_scale().

Dependencies:
- Uses: FloraBuild.h, Constants.h, ProcMesh.h (tri/quad), FloraCards.h.
- Used by: ProcFlora.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md.
- PURE + DETERMINISTIC. All randomness from the local splitmix64 keyed by
  (species, variant, node) — never a global RNG, never time.
- THE CONTAINMENT RULES HERE EACH COST A DEBUGGING ROUND. Enforce on the thing
  that actually REACHES (a card's corner), never on the notional element; and
  when a cluster does not fit, SHRINK it, never slide it to the axis. Both
  lessons are flora.md §3.7.
*/
/*
UPD:
- 09:08:2026 - 23:48:30: Split out of ProcFlora.cpp (Rule 21) with the
  space-colonization rewrite. `cluster` renamed `blob_cluster`; the card sway
  origin is now the skeleton ANCHOR node rather than the crown base, which is
  possible for the first time because a cluster now knows what it hangs on.
*/

#include "engine/render/sources/FloraBuild.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {
constexpr float CLEARANCE_MIN = static_cast<float>(config::CANOPY_CLEARANCE_MIN);
} // namespace

uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

Rng::Rng(uint64_t seed) : s(mix64(seed)) {}
float Rng::unit() {
    s = mix64(s);
    return static_cast<float>(s >> 40) / 16777216.0f;
}
float Rng::sym() { return unit() * 2.0f - 1.0f; }

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

/// Faceted ellipsoid cluster — bushes and the Silhouette LOD only.
void blob_cluster(MeshData& m, glm::vec3 c, glm::vec3 radii, int slices, int bands,
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

/// The species silhouette envelope, resolved to numbers. Under space
/// colonization this is not only a clip: it is the volume the attraction points
/// fill, i.e. the CAUSE of the branching (flora_algorithms.md §1.3.2). One
/// definition, in FloraSkeleton, so the thing that grows the crown and the thing
/// that contains it can never disagree.
CrownVolume crown_volume(const Tree& t) {
    return CrownVolume{t.sp.envelope, t.crown_base, t.crown_top, t.crown_r};
}

float envelope_radius(const Tree& t, float y) {
    return envelope_radius_at(crown_volume(t), y);
}

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
    const float env = envelope_radius(t, p.y);
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
    // RADIAL RE-CONTAINMENT, and it is the SAME DEFECT AS §3.7 FOR A FOURTH
    // TIME. emit_cluster clipped this cluster against the envelope at its
    // ORIGINAL height; the two clamps above then MOVE it, and in a cone (or any
    // envelope that narrows upward) moving up shrinks the envelope out from
    // under a card that was legally contained a moment earlier. Measured: pine
    // cards reached 6.7 m where the envelope allowed 2.6. A containment check
    // must run against the height the geometry ACTUALLY ENDS UP AT, not the one
    // it was proposed at.
    {
        const glm::vec2 rxz{at.x - t.stem_off.x, at.z - t.stem_off.z};
        // Shyness belongs in the limit, not only in the cluster's size: this
        // re-check runs AFTER clip_to_envelope already applied it, so leaving it
        // out here silently gives the shy side its full envelope back.
        const float env_final = envelope_radius(t, at.y) * shy_scale_xz(t, rxz);
        if (env_final > 0.0f) {
            const float len = glm::length(rxz);
            const float corner = std::sqrt(p.half_width * p.half_width
                                           + p.half_height * p.half_height);
            if (len + corner > env_final) {
                // Shrink first, slide only if shrinking alone cannot fit — the
                // §3.7.5 order, because sliding to the axis is what stacked the
                // birch into a drill bit.
                const float allow = std::max(env_final - len, env_final * 0.35f);
                if (corner > allow) {
                    const float k = allow / corner;
                    p.half_width *= k;
                    p.half_height *= k;
                }
                const float corner2 = std::sqrt(p.half_width * p.half_width
                                                + p.half_height * p.half_height);
                if (len + corner2 > env_final && len > 1e-5f) {
                    const glm::vec2 c = rxz * (std::max(env_final - corner2, 0.0f) / len);
                    at.x = t.stem_off.x + c.x;
                    at.z = t.stem_off.z + c.y;
                }
            }
        }
        if (p.half_width < 0.18f * t.crown_r) return;
    }
    // The attachment (sway weight 0) is the stem at the base of the crown, so
    // the whole crown's sway grows outward and upward from the trunk exactly
    // as a real one does, and the gradient ACROSS each card — its inner corner
    // nearer the stem than its outer one — is what makes the card bend instead
    // of sliding like a flag.
    p.sway_origin = t.sway_from;
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

void emit_cluster(MeshData& m, Tree& t, glm::vec3 at, float radius, int card_count) {
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
    const float env = envelope_radius(t, at.y);
    const float len = glm::length(rxz);
    if (env > 0.0f && len + radius > env) {
        // SHRINK TO FIT WHERE IT STANDS. §3.7.5's lesson, and the previous
        // implementation of it was still half a fix: it clamped `radius` to
        // `env` and THEN slid by (env - radius)/len, which is exactly zero when
        // the clamp bit — so every over-sized cluster still landed on the trunk
        // axis. Measured on the shy oak: five clusters at (0, y, 0) with 6.2 m
        // of card reach, i.e. the drill-bit stack rebuilt, and an axis-centred
        // cluster also escapes crown shyness entirely because it has no azimuth
        // to be shy on. The cure is to take the size, never the position:
        // clip_to_envelope already guarantees len <= env, so `env - len` is the
        // radius that fits without moving at all.
        radius = std::min(radius, std::max(env - len, env * 0.30f));
        // The 0.30 floor is a legibility floor, not a containment relaxation, so
        // if it bites we still pull the centre in — but by at most 30 %, never
        // to the axis.
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
    blob_cluster(m, at, glm::vec3{radius, radius * 0.85f, radius}, slices, bands, t.leaf);
}
} // namespace dfn::render
