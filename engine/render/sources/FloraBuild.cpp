/*
Created: 09:08:2026 - 23:48:30
Last updated: 13:08:2026 - 16:20:00
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
- 10:08:2026 - 20:15:51: CARD PLANE TILT is now a MIXTURE (в: «плоскости листвы... не
  больше чем 5-10 градусов, сейчас они перпендикулярны»). One card per
  cluster lies at 5-10 deg off the ground, the other two lean at 48-66 deg
  instead of standing at 63.6-80.8. Mean plane tilt 72.7 -> 40.9 deg. The
  minority is NOT a hedge: presented area at a level view is bought only by
  steep planes, and all-flat measures 150 m2/tree against 229 for the build
  the user already accepted (flora.md 3.8b).
- 12:08:2026 - 00:20:00: Containment moved onto the crown AXIS in both places
  that measure it (Rule 32: clip_to_envelope was moved and emit_cluster was
  not, for one run). The card legibility floor re-expressed against the
  species' own nominal cluster instead of against the crown radius, and taken
  as the MINIMUM of the two forms -- as a straight swap it silently cost the
  birch 65 % of its foliage. The old form was a latent bug that a 40 m crown
  detonated: it dropped EVERY card on the great oak and the tree photographed
  as a winter skeleton.
- 12:08:2026 - 00:36:00: THE SCRAP FLOOR EXISTED IN TWO PLACES AND ONLY ONE WAS
  RE-DERIVED, one screen apart, and the great oak measured ZERO CARDS on every
  variant because of it -- passed the first gate at 8.4 m of half-width,
  rejected by the second at 8.8 m, suite fully green, distant frame showing a
  bare branch system towering over a live forest. Both sites now read one
  `scrap_floor`. Rule 32 in the file whose own header states it.
- 12:08:2026 - 00:45:00: Both floor checks call card_scrap_floor() instead of
  restating it.
- 13:08:2026 - 16:20:00: THE THIRD COPY OF THE CARD LEGIBILITY FLOOR (Rule 32).
  The re-containment block still carried `0.18 * crown_r`, the retired form, one
  screen below the two sites unified on 12.08. It cost the great oak its entire
  crown a second time -- zero cards on every variant the moment its cluster
  fraction went under 0.18 -- and it now calls card_scrap_floor() like the
  others.
*/

#include "engine/render/sources/FloraBuild.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

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
    return CrownVolume{t.sp.envelope, t.crown_base, t.crown_top, t.crown_r,
                       t.crown_axis};
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
    // MEASURED FROM THE CROWN AXIS, not from the local origin. The two are the
    // same thing only for a plumb single-stem tree, which is what every tree
    // was until the lean band opened.
    const glm::vec2 r = glm::vec2{p.x, p.z} - t.crown_axis;
    const float len = glm::length(r);
    const float limit = env * shy_scale_xz(t, r);
    if (len > limit && len > 1e-5f) {
        const glm::vec2 c = t.crown_axis + r * (limit / len);
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
/// THE CROWN RADIUS THE SCRAP FLOOR IS MEASURED AGAINST, and it is the LARGER
/// of the two definitions in the tree because there are two and they disagree.
///
/// The emitter knows this INSTANCE's crown radius: the species ratio after
/// crown allometry, the per-instance width draw and the maturity tier. The
/// suite (and anything else reading a finished mesh) can only recover the
/// SPECIES' nominal — built height times `crown_width_frac` — because
/// allometry and the width draw are not recoverable from triangles. At the
/// sapling tier those differ by 1 / 0.4^0.35 = 1.39, so a card emitted exactly
/// at the instance floor sits 28 % under the nominal one.
///
/// Taking the max means a card that survives is legible under EITHER reading.
/// It can only ever drop more scraps, never admit one, so it cannot resurrect
/// the defect this floor exists to prevent — and it removes a disagreement
/// about one quantity, which is the third such disagreement this file has been
/// bitten by in two days. The right end state is one definition both sides
/// CALL; that needs the suite, which is not this zone's file, and it is
/// reported rather than worked around silently.
[[nodiscard]] static float crown_r_for_floor(const Tree& t) {
    return std::max(t.crown_r, t.height * t.sp.crown_width_frac * 0.5f);
}

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
    //
    // RE-EXPRESSED 12.08.2026, AND THE OLD FORM WAS A LATENT BUG THAT THE GREAT
    // OAK DETONATED. The floor was a fraction of the CROWN RADIUS, which is the
    // right order of magnitude only while every crown radius is about ten
    // metres. On a 40 m crown it became an 8.8 m minimum half-width — larger
    // than the species' own nominal cluster — so EVERY card was silently
    // dropped and the tree photographed as a winter skeleton with the branch
    // system perfect and no foliage at all. Nothing failed; the crown was just
    // absent, which is this project's favourite failure mode.
    //
    // The quantity the rule is actually about is "has containment shrunk this
    // card to a scrap OF WHAT IT WAS MEANT TO BE", so it is measured against
    // the species' own nominal cluster instead of against the crown. Chosen so
    // the number is UNCHANGED where it was tuned: for the oak of 10.08.2026,
    // 0.55 x 10.0 x 0.40 x 1.10 = 2.2 m, which is 0.22 x crown_r to two
    // figures. Same floor, on a quantity that survives a species eight times
    // the size.
    // AND IT IS THE **MINIMUM** OF THE TWO FORMS, which is the second thing
    // this cost. Swapping one for the other outright made the floor STRICTER
    // for species with a large cluster fraction on a small crown — the birch's
    // presented area fell 65 % in a single run, measured — because its clusters
    // are a big share of a small crown and the new form scales with exactly
    // that. A floor whose job is "do not emit scraps" must never start
    // rejecting foliage that used to be legible, so the change is allowed to
    // RELAX the floor where the old form was absurd (a 40 m crown radius) and
    // never to tighten it anywhere.
    // ONE DEFINITION, in FloraSpecies.h, and its docstring is the record of
    // what having three of it cost.
    const float scrap_floor = card_scrap_floor(sp, crown_r_for_floor(t));
    if (p.half_width < scrap_floor) return;

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
    // The floor is the TREE'S, not the constant: a non-canopy card species
    // (krummholz) legally carries foliage to the ground.
    const float floor_y = std::max(t.crown_base, t.clearance_floor);
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
    // RE-CHECK AFTER ANY SHRINK — AND IT IS THE SAME FLOOR, which it was not
    // until 12.08.2026 and that cost the great oak its entire crown. The floor
    // was re-derived above and this second site kept the old `0.22 * crown_r`
    // form, so on a 40 m crown radius the tree passed the first gate at 8.4 m
    // of half-width and was rejected by the second at 8.8 m: MEASURED ZERO
    // CARDS on every variant, with the suite green, and the distant frame
    // showing a bare branch system towering over a live forest.
    // Rule 32, in the file whose header already says so: fix the mechanism,
    // then check every consumer of it in the same change. One line apart.
    if (p.half_width < scrap_floor) return;
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
        // THE THIRD COPY OF THE CARD LEGIBILITY FLOOR, found 13.08.2026, and it
        // was still in the RETIRED FORM. On 12.08 the floor was re-derived and
        // "its three call sites now share one definition" — two of them did.
        // This one, twenty lines below the second and inside the re-containment
        // block so it only fires on clusters that were re-contained, kept
        // `0.18 * crown_r`: a species-independent fraction of the crown, which
        // is the exact form card_scrap_floor() exists to replace because it
        // does not survive a crown eight times the size.
        //
        // WHAT IT COST, MEASURED THE SAME DAY: taking the great oak's cluster
        // fraction from 0.19 to 0.12 (to open sky between its limbs) put its
        // card half-width at 0.132 of the crown radius, under this gate's 0.18,
        // and the species emitted ZERO CARDS on every variant — the identical
        // symptom, from the identical mechanism, that the 12.08 entry describes
        // as fixed. A shared rule spelled out per site is a rule with a
        // countdown on it, and this is the second time the countdown expired.
        if (p.half_width < card_scrap_floor(sp, crown_r_for_floor(t))) return;
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
        // CARD PLANE TILT FROM THE GROUND — a MIXTURE, and the mixture is the
        // whole content of this change (в: «плоскости листвы... не больше чем
        // 5-10 градусов, сейчас они перпендикулярны», 10.08.2026).
        //
        // The first card of every cluster lies in the user's band, 5-10 deg off
        // the ground: real broadleaf foliage sprays ARE near-horizontal because
        // leaves present their faces to the sun, and the shipped build was
        // measurably the opposite — every card plane stood at 63.6-80.8 deg,
        // mean 72.7.
        //
        // THE REST DO NOT, AND THAT IS NOT A HEDGE, IT IS ARITHMETIC. A plane
        // presents its area times |cos(view, normal)|, so a horizontal card is
        // edge-on to a horizontal view and contributes nothing to a canopy seen
        // from far away — where the eye->crown ray is nearly level (measured:
        // 13.0 deg at 80 m, 7.0 deg at 150 m for a 20.1 m oak crown). Holding
        // the presented area the shipped build ALREADY ACHIEVES at its own worst
        // view (229 m^2/tree, oak, at 60 deg elevation) needs >= 416 m^2 of
        // steeply-tilted card area, i.e. AT CONSTANT CARD AREA AT MOST ~43 % OF
        // FOLIAGE MAY LIE NEAR-HORIZONTAL. Cards all at 5-10 deg measure 150
        // m^2 at eye level — a third below a build the user has already
        // accepted, i.e. the canopy thinning out of existence at exactly the
        // distance a forest is a skyline. That is render's CARDS BUY ANGULAR
        // COVERAGE rule in its numeric form, and it is why "set every card to 7
        // degrees" is not on the table.
        //
        // The minority planes are therefore kept steep but NOT vertical: their
        // band drops from 63.6-80.8 to 48-66 deg, which is as far as it can go
        // before the low-elevation area falls under the accepted floor
        // (40-60 deg measures 230 m^2 against a 229 floor — no margin left).
        // Mean plane tilt over the whole crown: 72.7 -> 40.9 deg, and nothing
        // in the canopy stands near-perpendicular any more.
        //
        // Both bands are UNIFORM over their declared range (Rule 31); the
        // shipped elevation was two bumps dressed as one band. Sign alternates
        // so cards of one cluster are not co-planar.
        const bool flat = k < CARD_FLAT_PER_CLUSTER;
        const float tilt_lo = flat ? CARD_TILT_FLAT_MIN : CARD_TILT_LEAN_MIN;
        const float tilt_hi = flat ? CARD_TILT_FLAT_MAX : CARD_TILT_LEAN_MAX;
        const float tilt = tilt_lo + (tilt_hi - tilt_lo) * t.rng.unit();
        // The NORMAL's elevation is the complement of the PLANE's tilt: a plane
        // lying on the ground has a normal pointing straight up.
        const float el = (k % 2 == 0 ? 1.0f : -1.0f) * (glm::half_pi<float>() - tilt);
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
    // FROM THE CROWN AXIS. clip_to_envelope was moved onto the axis when the
    // lean band opened and this second, independent radial test was not — so a
    // leaning tree's clusters were contained against a circle centred on its
    // stump while its crown sat several metres downwind. Rule 32: the same
    // mechanism, every consumer, in one change.
    const glm::vec2 rxz = glm::vec2{at.x, at.z} - t.crown_axis;
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
            const glm::vec2 c =
                t.crown_axis + rxz * (std::max(env - radius, 0.0f) / len);
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
