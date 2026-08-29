/*
Module: engine/render
File: engine/render/sources/TreeForgeV2.cpp

Responsibility:
- forge_tree_v2(): the second iteration's tree (see TreeForgeV2.h for the five
  differences it answers and why it is a second builder rather than a flag).

Dependencies:
- Uses: TreeForgeV2.h, FloraBuild.h (Rng, tube_segment, helpers), TreeBark.h
  (bark_tube, pack_wind), FloraCards.h (leaf_tile_uv), ProcMesh.h (pack).
- Used by: dfn_render target, tools/forge_trees.cpp, TreeForgeV2Tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic: every draw comes from the params' seed through one Rng.
- NOTHING HERE MAY CALL INTO forge_tree() OR CHANGE IT. The first iteration is
  frozen by the owner's ruling of 28.08 until he says otherwise.
*/

#include "engine/render/sources/TreeForgeV2.h"

#include "engine/render/sources/FloraBuild.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/TreeBark.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace dfn::render {
namespace {

/// One big crown lobe: the unit of MASS and the unit of SILHOUETTE. Difference
/// №1 and №5 of the Gothic-3 note are the same object seen twice — a crown made
/// of five to nine of these has depth (they stack front to back and hang down
/// the flanks) AND a coarse rim (the sky between them is lobe-sized, not
/// card-sized).
struct Lobe {
    glm::vec3 c;
    float r;
};

/// One sample of a stem: where it is, where it points, how thick it is.
struct Ring {
    glm::vec3 pos;
    glm::vec3 dir;
    float radius;
};

/// A foliage attachment: the tip or the flank of a twig, and the lobe it
/// belongs to (the lobe centre is what its sheets take their normal from —
/// Airborn's projected normals, one lit dome per lobe instead of per card).
struct Anchor {
    glm::vec3 pos;
    glm::vec3 dir;
    int lobe;
};

float smooth01(float t) {
    const float x = std::clamp(t, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// --- THE OVERHANG, AND WHY THE RECIPE'S WIDTH IS THE MEASURED ONE ------------
// A crown's outermost leaf does not stand at `crown_radius`; it stands at the
// furthest lobe CENTRE, plus that lobe's own radius, plus the reach of a sheet
// hanging on its rim. The first iteration left this unsaid and paid for it in
// its own index: great-forge-oak asks for W/H 0.87 and MEASURES 1.56, and the
// passports carry a paragraph explaining that the two metrics disagree.
//
// Here the three terms are named and divided out, so `crown_width_frac` is a
// promise about the SILHOUETTE — the thing the owner looks at — and the tests
// can assert it against measure_object() instead of against a recipe field.
constexpr float LOBE_CENTRE_MAX = 0.62f;  ///< furthest lobe centre, in crown radii
constexpr float LOBE_R_MAX = 0.42f;       ///< widest lobe, in crown radii
constexpr float SHEET_REACH_MAX = 0.85f;  ///< widest sheet, in LOBE radii
// A sheet is TANGENT to its lobe and bows INWARD, so it wraps the lobe rather
// than sticking out past it: the reach term is inside the lobe radius, not
// added to it. MEASURED, not assumed — with the reach added the solitary
// recipes came out at W/H 0.78 against a target of 1.00, and with it dropped
// they land at 0.95-1.05.
constexpr float CROWN_OVERHANG = LOBE_CENTRE_MAX + LOBE_R_MAX;

/// THE CROWN AS A FIELD OF LOBES.
///
/// The placement law is what makes the crown a cabbage instead of a lollipop,
/// so it is worth stating in one place:
///   - lobe HEIGHTS are spread over the WHOLE crown depth by a low-discrepancy
///     walk, with the first third of them deliberately in the bottom half. The
///     v1 crown put every leaf mass above `crown_base + 0.35 * depth`; here a
///     third of the mass hangs at shoulder height, which is what "спускается по
///     бокам" means in geometry.
///   - lobe DISTANCE from the axis follows a barrel profile — narrow at the
///     very top and the very bottom, widest at mid-crown — so the silhouette is
///     an egg standing on its blunt end, not a disc.
///   - lobe RADIUS is 0.30-0.42 of the crown radius. Two neighbours therefore
///     touch or nearly touch, and the gap between every second pair is real
///     sky: with seven lobes on a circle of 0.7*R the arc between centres is
///     ~0.6*R while their radii sum to ~0.72*R, so roughly every third gap
///     opens. That is the coarse rim, and it is arithmetic, not luck.
std::vector<Lobe> lobe_field(Rng& rng, int count, float crown_base,
                             float crown_top, float crown_r, glm::vec2 offset) {
    std::vector<Lobe> out;
    out.reserve(static_cast<size_t>(count));
    const float depth = std::max(crown_top - crown_base, 0.5f);
    for (int i = 0; i < count; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        // Heights: alternate low / high so no two consecutive lobes stack, and
        // bias the sequence low — the mass has to come DOWN the tree.
        const float band = (i % 3 == 0) ? (0.06f + 0.30f * u)
                         : ((i % 3 == 1) ? (0.34f + 0.38f * u)
                                         : (0.55f + 0.42f * u));
        const float h = std::clamp(band + rng.sym() * 0.06f, 0.03f, 0.98f);
        float y = crown_base + depth * h;
        // Barrel profile: sin() over the crown depth, floored so the top lobe
        // still sits off-axis and the crown never comes to a point. The 0.60
        // ceiling is not taste — it is the first term of CROWN_OVERHANG below,
        // and the three terms together are what makes the MEASURED width match
        // the width the recipe asked for.
        const float barrel = 0.18f + 0.44f * std::sin(3.14159265f * std::pow(h, 0.85f));
        const float rad = crown_r * std::clamp(barrel, 0.14f, LOBE_CENTRE_MAX)
                        * (0.82f + 0.30f * rng.unit());
        const float az = GOLDEN_ANGLE * static_cast<float>(i) + rng.sym() * 0.45f;
        Lobe l;
        l.r = crown_r * (0.30f + (LOBE_R_MAX - 0.30f) * rng.unit());
        // THE VERTICAL INSET, and it is the difference between a crown and a
        // bush. A lobe carries sheets on its underside, so its foliage reaches
        // r * (1 + SHEET_REACH_MAX) BELOW its own centre. Put the lowest lobe
        // at the crown base and that reach hangs past it — measured on the
        // first bake: a solitary oak whose leaves came down to 0.05 of its
        // height, i.e. no clean bole at all, which is the opposite of the
        // «чистого ствола четверть» the note asks for. Inset by the lobe's OWN
        // radius (not the maximum) so a small lobe may still sit low.
        // The inset is ASYMMETRIC because a sheet is a near-horizontal patch
        // that SAGS: it hangs almost a full sheet-width below its anchor and
        // barely rises above it. A symmetric inset therefore left the crown's
        // top two metres empty on a 14 m tree (measured), which reads as a
        // flat-topped crown — the lollipop again, from the other end.
        const float down = l.r * (1.0f + SHEET_REACH_MAX);
        const float upi = l.r * 0.45f;
        if (crown_base + down < crown_top - upi) {
            y = std::clamp(y, crown_base + down, crown_top - upi);
        } else {
            y = (crown_base + crown_top) * 0.5f;
        }
        // THE CROWN RIDES THE BOLE. `offset` is where the stem tops actually
        // stand, and without it the crown stayed centred on the ROOTS however
        // far the bole leaned — measured: a 0.25 rad lean moved the bole's top
        // 1.8 m sideways and the crown's centre of mass 0.29 m, because every
        // branch was aimed back at a lobe field that had not moved. An
        // individual lean that the crown ignores is not an individual tree,
        // it is a bent pole under a fixed ball.
        l.c = glm::vec3{offset.x + std::cos(az) * rad, y,
                        offset.y + std::sin(az) * rad};
        out.push_back(l);
    }
    return out;
}

/// Builds one stem and writes its bark. The bole is an ANALYTIC curve, not a
/// random walk: `lean` tilts it in its own azimuth and `curve` bends it into an
/// S inside that same plane, and both are gated by a RIGID BUTT — measured
/// boles hold their chord through the lower quarter and bend only above it
/// (TREE_MODELS_RESEARCH §1.7, and the user's own frame 16).
///
/// Why analytic: a zero-mean wander (the v1 law) gives a bole that is straight
/// ON AVERAGE and therefore identical from tree to tree at a glance, which is
/// exactly the complaint in §3.3 of the note — «исчез не изгиб-дуга, а всякая
/// индивидуальность». A per-tree lean and a per-tree S are individuality that
/// survives averaging, and the rigid butt is what keeps them from being the
/// banana the first iteration was cured of.
std::vector<Ring> build_stem(MeshData& bark_mesh, Rng& rng, glm::vec3 butt,
                             float y0, float y1, float r0, float lean_rad,
                             float lean_dir, float curve_frac, int segments,
                             int sides, glm::vec4 uv, float phase, float height,
                             float sway_scale, float arc0) {
    std::vector<Ring> rings;
    rings.reserve(static_cast<size_t>(segments) + 1);
    const float axis_len = std::max(y1 - y0, 0.5f);
    const glm::vec3 lean_axis{std::cos(lean_dir), 0.0f, std::sin(lean_dir)};
    const glm::vec3 side_axis{-std::sin(lean_dir), 0.0f, std::cos(lean_dir)};
    const float lean_reach = std::tan(std::clamp(lean_rad, -1.2f, 1.2f)) * axis_len;
    const float curve_reach = curve_frac * axis_len;
    // Per-stem grain: a small, fixed sideways offset schedule so two stems of
    // one tree are never congruent even at equal lean.
    const float grain = rng.sym() * 0.35f;
    const auto point_at = [&](float t) {
        const float w = smooth01((t - 0.22f) / 0.78f);
        // The S: out along the lean and back, one full period over the bending
        // part. sin(2*pi*w) integrates to zero, so the TOP of the bole lands on
        // the lean's own line and the curve is a bend, not a second lean.
        const float s = std::sin(6.28318531f * w) * curve_reach;
        const float g = std::sin(3.14159265f * w) * curve_reach * grain;
        return glm::vec3{lean_axis.x * (lean_reach * w + s) + side_axis.x * g,
                         y0 + axis_len * t,
                         lean_axis.z * (lean_reach * w + s) + side_axis.z * g}
             + butt;
    };
    glm::vec3 frame = perp_of(glm::vec3{0.0f, 1.0f, 0.0f});
    float arc = arc0;
    glm::vec3 prev = point_at(0.0f);
    const auto sway_at = [&](float y) {
        return 0.28f * sway_scale
             * std::pow(std::clamp(y / std::max(height, 1.0f), 0.0f, 1.0f), 1.6f);
    };
    rings.push_back({prev, glm::vec3{0.0f, 1.0f, 0.0f}, r0});
    for (int s = 0; s < segments; ++s) {
        const float t0 = static_cast<float>(s) / static_cast<float>(segments);
        const float t1 = static_cast<float>(s + 1) / static_cast<float>(segments);
        const glm::vec3 next = point_at(t1);
        const glm::vec3 d = safe_normalize(next - prev, glm::vec3{0.0f, 1.0f, 0.0f});
        const float ra = r0 * std::pow(std::max(1.0f - 0.80f * t0, 0.05f), 1.1f);
        const float rb = r0 * std::pow(std::max(1.0f - 0.80f * t1, 0.05f), 1.1f);
        const float len = glm::length(next - prev);
        bark_tube(bark_mesh, prev, next, d, std::max(ra, 0.05f), std::max(rb, 0.05f),
                  sides, uv, arc, TAU * std::max(ra, 0.05f),
                  pack_wind(sway_at(prev.y), phase), pack_wind(sway_at(next.y), phase),
                  &frame);
        arc += len;
        prev = next;
        rings.push_back({next, d, std::max(rb, 0.05f)});
    }
    return rings;
}

} // namespace

void resolve_v2_proportions(const TreeV2Params& in, float& width_frac,
                            float& depth_frac) {
    // THE NUMBERS OF DIFFERENCE №1 AND №2, in one place so a test can read them.
    //   Solitary: crown as wide as the tree is tall (1.00) and 0.78 of its
    //   height deep — i.e. a quarter of clean bole, which is what the note
    //   measures on the meadow oaks («чистого ствола видно четверть высоты»).
    //   Forest: the SAME species with the crown high — 0.42 deep, so the bole
    //   is 0.58 of the height and clean, and 0.64 wide because a tree in a
    //   stand is squeezed by its neighbours.
    //   MultiStem: wider than tall (1.42) and 0.72 deep — the acacia law.
    switch (in.habit) {
    case TreeHabit::Forest:
        width_frac = 0.64f;
        depth_frac = 0.42f;
        break;
    case TreeHabit::MultiStem:
        width_frac = 1.42f;
        depth_frac = 0.72f;
        break;
    case TreeHabit::Solitary:
    default:
        width_frac = 1.00f;
        depth_frac = 0.78f;
        break;
    }
    if (in.crown_width_frac > 0.01f) width_frac = in.crown_width_frac;
    if (in.crown_depth_frac > 0.01f) depth_frac = in.crown_depth_frac;
}

RegistryObject forge_tree_v2(const TreeV2Params& p) {
    RegistryObject obj;
    obj.name = p.name;
    obj.kind = "tree";
    obj.source = "forge:v2 seed=" + std::to_string(p.seed);

    // A DIFFERENT SEED SALT from forge_tree's, deliberately: two builders that
    // agree on their first ten draws would make "v2 is a different tree" a
    // claim about the parameters instead of about the construction.
    Rng rng(p.seed * 0x9E3779B97F4A7C15ull + 0x6A09E667F3BCC909ull);

    const LeafTone bark_row = p.bark.r > 0.6f ? LeafTone::BirchLight : LeafTone::OakMid;
    const LeafTone bark_moss_row = LeafTone::OakSunlit;
    const glm::vec4 bark_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_row);
    const glm::vec4 moss_uv = leaf_tile_uv(LeafShape::BarkPlate, bark_moss_row);
    const uint32_t bark_flat = pack(p.bark);
    const float phase = rng.unit();
    const uint32_t wind_still = pack_wind(0.0f, phase);
    const float sway_scale = std::min(1.0f, p.height / 14.0f);

    float wfrac = 1.0f;
    float dfrac = 0.78f;
    resolve_v2_proportions(p, wfrac, dfrac);
    // The recipe's width is the SILHOUETTE's width; the builder's own radius
    // is that divided by the overhang the crown adds on top of it.
    const float crown_r = p.height * wfrac * 0.5f / CROWN_OVERHANG;
    const float crown_top = p.height;
    const float crown_base = p.height * (1.0f - dfrac);
    const float crown_cy = (crown_base + crown_top) * 0.5f;

    const int lobe_count = std::clamp(p.lobes, 5, 9);

    const int bole_sides = p.far_lod ? 5 : 7;
    const float flare_h = std::clamp(p.trunk_radius * 2.4f, 0.35f, FLARE_HEIGHT);
    const float flare_r = p.trunk_radius * 1.6f;

    // --- THE BUTT: one flare and one set of buttress spurs, shared by however
    // many stems leave it. A multi-stemmed tree has ONE root system; giving each
    // stem its own flare is the tell that it is three trees in a bucket.
    bark_tube(obj.bark, glm::vec3{0.0f, -FLARE_DEPTH, 0.0f},
              glm::vec3{0.0f, flare_h, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f},
              flare_r, p.trunk_radius * (p.habit == TreeHabit::MultiStem ? 1.25f : 1.0f),
              bole_sides, moss_uv, 0.0f, TAU * flare_r, wind_still, wind_still);
    for (int k = 0; k < ROOT_SPUR_COUNT; ++k) {
        const float az = TAU * (static_cast<float>(k) + 0.5f + rng.sym() * 0.3f)
                       / static_cast<float>(ROOT_SPUR_COUNT);
        const glm::vec3 rd{std::cos(az), 0.0f, std::sin(az)};
        const float reach = flare_r * (1.05f + rng.unit() * 0.45f);
        const float r0 = std::max(p.trunk_radius * ROOT_SPUR_R_FRAC * 1.35f, 0.06f);
        const glm::vec3 pts[5] = {
            rd * (flare_r * 0.15f) + glm::vec3{0.0f, 0.62f, 0.0f},
            rd * (flare_r * 0.85f) + glm::vec3{0.0f, 0.26f, 0.0f},
            rd * (flare_r + reach * 0.35f) + glm::vec3{0.0f, ROOT_SPUR_RISE, 0.0f},
            rd * (flare_r + reach * 0.7f) + glm::vec3{0.0f, -0.05f, 0.0f},
            rd * (flare_r + reach) - glm::vec3{0.0f, ROOT_SPUR_SINK, 0.0f}};
        float rr = r0;
        for (int seg = 0; seg < 4; ++seg) {
            const float nr = seg == 3 ? 0.0f
                                      : r0 * (1.0f - 0.22f * static_cast<float>(seg + 1));
            bark_tube(obj.ground, pts[seg], pts[seg + 1],
                      safe_normalize(pts[seg + 1] - pts[seg], rd), rr,
                      std::max(nr, 0.0f), 4, moss_uv,
                      reach * 0.25f * static_cast<float>(seg), TAU * rr, wind_still,
                      wind_still);
            rr = std::max(nr, 0.02f);
        }
    }

    // --- THE STEMS. One for a normal tree; three or four FROM THE GROUND for
    // the multi-stemmed law (difference №3). Every stem carries its own lean,
    // its own S and its own grain, so «ни один ствол не параллелен соседнему»
    // is true by construction rather than by jitter.
    const bool multi = p.habit == TreeHabit::MultiStem;
    const int stem_count = multi ? std::clamp(p.stems, 2, 5) : 1;
    const float bole_frac = p.habit == TreeHabit::Forest ? 0.72f : 0.52f;
    std::vector<std::vector<Ring>> stems;
    stems.reserve(static_cast<size_t>(stem_count));
    const float stem_az0 = rng.unit() * TAU;
    for (int s = 0; s < stem_count; ++s) {
        float lean = p.lean_rad;
        float dir = p.lean_dir;
        float top_y = crown_base + (crown_top - crown_base) * bole_frac;
        float r0 = p.trunk_radius;
        glm::vec3 butt{0.0f, 0.0f, 0.0f};
        if (multi) {
            // Each stem leaves the butt on its own bearing and leans OUT hard:
            // the acacia's three heavy limbs, splitting at the ground.
            dir = stem_az0 + TAU * static_cast<float>(s) / static_cast<float>(stem_count)
                + rng.sym() * 0.32f;
            lean = 0.26f + rng.unit() * 0.20f;
            top_y = crown_base + (crown_top - crown_base) * (0.30f + rng.unit() * 0.34f);
            r0 = p.trunk_radius * (0.58f + rng.unit() * 0.16f);
            butt = glm::vec3{std::cos(dir), 0.0f, std::sin(dir)} * (p.trunk_radius * 0.42f);
        } else {
            lean = p.lean_rad * (0.85f + rng.unit() * 0.3f);
        }
        stems.push_back(build_stem(obj.bark, rng, butt, flare_h, top_y, r0, lean, dir,
                                   p.curve_frac * (0.7f + rng.unit() * 0.7f),
                                   multi ? 9 : 11, bole_sides, bark_uv, phase,
                                   p.height, sway_scale, FLARE_DEPTH + flare_h));
    }

    // THE CROWN'S OWN CENTRE: the average of where the stems END. One leaning
    // bole carries its crown downwind of its roots; three splayed stems carry
    // one crown over their common middle.
    glm::vec2 crown_off{0.0f, 0.0f};
    for (const std::vector<Ring>& st : stems) {
        crown_off += glm::vec2{st.back().pos.x, st.back().pos.z};
    }
    crown_off /= static_cast<float>(std::max<size_t>(stems.size(), 1));
    const std::vector<Lobe> lobes =
        lobe_field(rng, lobe_count, crown_base, crown_top, crown_r, crown_off);

    // --- SCAFFOLDS: one per lobe, leaving the stems at STAGGERED heights.
    // Difference №3's last clause: the branch attachments are spread by a
    // golden-ratio walk over the whole usable length of the stem, so no two
    // scaffolds share a node and the crown is not a fan. The v1 crown drew its
    // scaffolds from a span of about a fifth of the height; this one uses
    // three quarters of the stem.
    std::vector<Anchor> anchors;
    const auto wind_at = [&](glm::vec3 q) {
        const float horiz = std::sqrt(q.x * q.x + q.z * q.z) / std::max(crown_r, 0.5f);
        const float sway = std::clamp(
            sway_scale * (0.28f * std::pow(std::clamp(q.y / std::max(p.height, 1.0f),
                                                      0.0f, 1.0f), 1.6f)
                          + 0.30f * horiz), 0.0f, 0.60f);
        const auto to_byte = [](float v) {
            return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return 0x8C800000u | (to_byte(phase) << 8) | to_byte(sway);
    };

    /// Grows one limb from `pos` toward `target`, in short segments that each
    /// turn a little. Children leave it MID-SEGMENT (never at its base), which
    /// is the other half of "not a fan": a second-order branch that starts
    /// where its parent does draws a star.
    struct Grow {
        MeshData& mesh;
        Rng& rng;
        std::vector<Anchor>& anchors;
        glm::vec4 uv;
        bool far;
        const decltype(wind_at)& wind;

        void run(glm::vec3 pos, glm::vec3 target, float radius, int level, int lobe,
                 float lobe_r, float arc) {
            const int segs = level == 0 ? 6 : (level == 1 ? 4 : 3);
            glm::vec3 d = safe_normalize(target - pos, glm::vec3{0.0f, 1.0f, 0.0f});
            const float len = glm::length(target - pos);
            if (len < 0.2f || segs <= 0) {
                anchors.push_back({pos, d, lobe});
                return;
            }
            const float seg = len / static_cast<float>(segs);
            glm::vec3 frame = perp_of(d);
            float r = radius;
            for (int si = 0; si < segs; ++si) {
                // Turn toward the target, but never exactly at it — a limb that
                // aims straight at its lobe is a spoke. The pull weakens with
                // level so twigs wander freely inside the lobe.
                const glm::vec3 want = safe_normalize(target - pos, d);
                const glm::vec3 side = safe_normalize(
                    glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}), glm::vec3{1.0f, 0.0f, 0.0f});
                const float pull = 0.45f / static_cast<float>(1 + level);
                const float dip = rng.unit() < 0.28f ? -0.16f : 0.0f;
                d = safe_normalize(d + (want - d) * pull + side * (rng.sym() * 0.28f)
                                     + glm::vec3{0.0f, dip, 0.0f}, d);
                const glm::vec3 np = pos + d * seg;
                const float taper = 1.0f - 0.55f * (static_cast<float>(si + 1)
                                                    / static_cast<float>(segs));
                const float nr = std::max(radius * taper, 0.05f);
                const int sides = far ? (level == 0 ? 4 : 3)
                                      : (level == 0 ? 5 : (level == 1 ? 4 : 3));
                bark_tube(mesh, pos, np, d, r, nr, sides, uv, arc, TAU * r,
                          wind(pos), wind(np), &frame);
                arc += seg;
                if (level < 2 && si >= 1 && si % 2 == 1 && rng.unit() < 0.55f) {
                    // The child aims at a DIFFERENT point of the same lobe, so
                    // the lobe fills from several directions.
                    const glm::vec3 jitter{rng.sym(), rng.sym() * 0.7f, rng.sym()};
                    run(np, target + jitter * (lobe_r * 0.85f),
                        nr * (0.55f + rng.unit() * 0.12f), level + 1, lobe, lobe_r,
                        arc);
                }
                pos = np;
                r = nr;
            }
            anchors.push_back({pos, d, lobe});
            if (level >= 1 && rng.unit() < 0.30f) {
                anchors.push_back({pos - d * (len * 0.45f), d, lobe});
            }
        }
    } grow{obj.bark, rng, anchors, bark_uv, p.far_lod, wind_at};

    for (int i = 0; i < lobe_count; ++i) {
        const std::vector<Ring>& stem = stems[static_cast<size_t>(i % stem_count)];
        // STAGGERED ATTACH: a golden-ratio walk over the stem's usable length.
        // No two scaffolds land on one ring, and the sequence has no period —
        // which is what stops the eye reading shelves.
        const float frac = std::fmod(0.14f + 0.61803399f * static_cast<float>(i), 1.0f);
        const float t = std::clamp(0.16f + 0.78f * frac + rng.sym() * 0.05f, 0.05f, 0.98f);
        const size_t ri = std::min(stem.size() - 1,
                                   static_cast<size_t>(t * static_cast<float>(stem.size() - 1)));
        const Ring& at = stem[ri];
        const glm::vec3 to_lobe = lobes[static_cast<size_t>(i)].c - at.pos;
        const glm::vec3 out = safe_normalize(glm::vec3{to_lobe.x, 0.0f, to_lobe.z},
                                             glm::vec3{1.0f, 0.0f, 0.0f});
        // Rooted INSIDE the stem's own ring, so the joint is embedded and not
        // a tube leaning on a tube (§1.5's inflate-at-the-joint).
        const glm::vec3 start = at.pos + out * (at.radius * 0.45f);
        grow.run(start, lobes[static_cast<size_t>(i)].c,
                 at.radius * (0.44f + rng.unit() * 0.14f), 0, i,
                 lobes[static_cast<size_t>(i)].r, at.pos.y);
        // TWIGS INSIDE THE LOBE: three to five short runs from the lobe centre
        // outward, so the mass is filled from within and its rim is made of
        // twig ends rather than of one card ring.
        // FEWER, BIGGER (§1.6 «fewer cards better»): two or three twig runs
        // per lobe, each ending in one large sheet, instead of a swarm.
        const int twigs = p.far_lod ? 1 : 3 + static_cast<int>(rng.unit() * 2.0f);
        for (int w = 0; w < twigs; ++w) {
            const float az = GOLDEN_ANGLE * static_cast<float>(w) + rng.sym() * 0.6f;
            const float el = rng.sym() * 0.9f;
            const glm::vec3 dirw = safe_normalize(
                glm::vec3{std::cos(az), el, std::sin(az)}, glm::vec3{1.0f, 0.0f, 0.0f});
            const Lobe& L = lobes[static_cast<size_t>(i)];
            grow.run(L.c - dirw * (L.r * 0.35f), L.c + dirw * (L.r * 0.95f),
                     std::max(at.radius * 0.20f, 0.055f), 1, i, L.r, L.c.y);
        }
    }

    // --- DRY BROKEN SNAGS DOWN THE BOLE (difference №3, last clause: «короткие
    // сухие обломки сучьев по стволу»). They ride the WHOLE stem, not only its
    // lower part: in the forest recipe the band between the ground and the
    // crown is most of the tree, and a bare pole through it is the giveaway.
    if (!p.far_lod && p.snags > 0) {
        const std::vector<Ring>& stem = stems.front();
        for (int s = 0; s < p.snags; ++s) {
            const float t = 0.10f + 0.82f * rng.unit();
            const size_t ri = std::min(stem.size() - 1,
                                       static_cast<size_t>(t * static_cast<float>(stem.size() - 1)));
            const Ring& at = stem[ri];
            const float az = rng.unit() * TAU;
            const glm::vec3 out{std::cos(az), 0.0f, std::sin(az)};
            const glm::vec3 d1 = safe_normalize(
                out + glm::vec3{0.0f, -0.08f - 0.34f * rng.unit(), 0.0f}, out);
            const float len = 0.22f + 0.50f * rng.unit();
            const float rr = std::max(at.radius * 0.17f, 0.035f);
            const glm::vec3 base_p = at.pos + out * (at.radius * 0.5f);
            const glm::vec3 mid = base_p + d1 * len;
            const uint32_t w = wind_at(base_p);
            bark_tube(obj.bark, base_p, mid, d1, rr, rr * 0.7f, 4, bark_uv,
                      at.pos.y, TAU * rr, w, w);
            // The kink at the break: a real snag never ends square.
            const glm::vec3 d2 = safe_normalize(
                d1 + glm::vec3{rng.sym() * 0.7f, 0.45f * rng.sym(), rng.sym() * 0.7f}, d1);
            bark_tube(obj.bark, mid, mid + d2 * (len * 0.28f), d2, rr * 0.7f, 0.0f, 3,
                      bark_uv, at.pos.y + len, TAU * rr, w, w);
        }
    }

    // --- THE FOLIAGE SHEETS. Same bent 3x3 patch the first iteration proved
    // (a curved sheet has no edge-on bearing and no flat symmetry axis), with
    // three v2 changes:
    //   * THE ROW is chosen per sheet: sheets deep inside the crown draw the
    //     PackV2Deep tile, sheets on the rim PackV2Mid. The crown therefore has
    //     a dark interior without a single extra triangle or light.
    //   * THE NORMAL is bent toward the sheet's own LOBE CENTRE, so a lobe
    //     shades as one ball (Airborn's projected normals) instead of as a
    //     handful of independently facing planes.
    //   * THE SHEET IS BIG — 0.42-0.66 of its lobe's radius, i.e. four to six
    //     sheets cover a lobe. Fewer and bigger is the whole doctrine of §1.6.
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    for (const Anchor& a : anchors) {
        const Lobe& L = lobes[static_cast<size_t>(std::clamp(a.lobe, 0, lobe_count - 1))];
        const int sheets = p.far_lod ? 1 : (rng.unit() < 0.55f ? 2 : 1);
        for (int i = 0; i < sheets; ++i) {
            const float half_w = L.r * (0.55f + rng.unit() * (SHEET_REACH_MAX - 0.55f))
                               * (i == 0 ? 1.0f : 0.68f);
            const glm::vec3 c = a.pos + glm::vec3{rng.sym(), rng.sym() * 0.6f, rng.sym()}
                                          * (L.r * 0.16f);
            // EXPOSURE: how far this sheet stands from the crown's own centre,
            // in crown radii. The rim row starts at 0.62 — measured off the
            // lobe field, that is where a sheet stops having another lobe
            // outside it and starts being the silhouette.
            const float expo = glm::length(glm::vec3{c.x - crown_off.x,
                                                     (c.y - crown_cy) * 0.75f,
                                                     c.z - crown_off.y})
                             / std::max(crown_r, 0.5f);
            const bool rim = expo > 0.62f && c.y > crown_base + (crown_top - crown_base) * 0.18f;
            const glm::vec4 uvr = leaf_tile_uv(p.card_shape,
                                               rim ? p.tone_rim : p.tone_core);
            // THE SHEET LIES TANGENT TO ITS LOBE, NOT FLAT. This is the
            // single change that turned the v2 crown from bare twigs into a
            // mass, and it was found on a frame, not reasoned out: with the
            // sheets built in the HORIZONTAL plane (the first iteration's
            // construction) a crown seen from the ground shows every sheet
            // nearly EDGE-ON — a patch with no thickness seen from its own
            // height is a line, which is the same defect that once left the
            // whole conifer row standing bare (TreeForge UPD 16.08 22:40).
            // The first iteration survives it by sheer count; a v2 crown of
            // ~110 big sheets does not.
            //
            // Tangent to the lobe means: on the crown's flanks the sheet
            // stands UP and faces the viewer, on the crown's top it lies flat
            // and faces the sky. That is also what a leaf does — it presents
            // itself to the light — so the fix is the honest shape, not a
            // billboard trick, and the sheet stays FIXED (never camera-facing).
            const glm::vec3 outward = safe_normalize(c - L.c, up);
            const float roll = rng.unit() * TAU;
            glm::vec3 e1 = safe_normalize(glm::cross(outward, up), 
                                          perp_of(outward));
            glm::vec3 e2 = safe_normalize(glm::cross(outward, e1), up);
            // Roll inside the tangent plane, so neighbouring sheets do not
            // share an axis (parallel edges are the loudest procedural tell).
            const glm::vec3 r1 = e1 * std::cos(roll) + e2 * std::sin(roll);
            const glm::vec3 r2 = e2 * std::cos(roll) - e1 * std::sin(roll);
            e1 = r1;
            e2 = r2;
            // A DEEP SHELL, NOT A PLATE. Measured on the close frame: a shallow
            // patch lying in its lobe's tangent plane is seen EDGE-ON from
            // roughly a third of the bearings around the crown, and edge-on a
            // zero-thickness patch is a bright hairline — the crown wore a
            // dozen pale straight streaks. Curving it to 0.55-0.85 of its own
            // half-width gives the sheet a real shell profile: from every
            // bearing SOME of it faces the eye, exactly the argument that made
            // the conifer frond a folded V rather than a flat ribbon.
            const float droop = half_w * (0.55f + rng.unit() * 0.30f);
            const float jit_v = 0.44f + rng.unit() * 0.20f;
            const glm::vec3 sway_origin{0.0f, crown_base, 0.0f};
            const float sway_span = std::max(crown_r * 1.8f, 1.0f);
            glm::vec3 vp[3][3];
            for (int gj = 0; gj < 3; ++gj) {
                for (int gi = 0; gi < 3; ++gi) {
                    const float fx = static_cast<float>(gi - 1);
                    const float fz = static_cast<float>(gj - 1);
                    glm::vec3 q = c + e1 * (fx * half_w) + e2 * (fz * half_w);
                    const float rr2 = (fx * fx + fz * fz) * 0.5f;
                    // The dome curves along the lobe's own outward axis, so a
                    // sheet on the flank bows like the flank does. Curving in
                    // world Y would have flattened every side sheet back into
                    // the horizontal plane this construction just left.
                    q -= outward * (droop * std::pow(rr2, 1.3f));
                    q += outward * ((gi == 1 && gj == 1 ? 0.22f : 0.0f) * half_w);
                    q += outward * (rng.sym() * 0.06f * half_w);
                    vp[gj][gi] = q;
                }
            }
            const auto base_v = static_cast<uint32_t>(obj.cards.vertices.size());
            for (int gj = 0; gj < 3; ++gj) {
                for (int gi = 0; gi < 3; ++gi) {
                    const glm::vec3 du = vp[gj][std::min(gi + 1, 2)]
                                       - vp[gj][std::max(gi - 1, 0)];
                    const glm::vec3 dv = vp[std::min(gj + 1, 2)][gi]
                                       - vp[std::max(gj - 1, 0)][gi];
                    glm::vec3 n = safe_normalize(glm::cross(dv, du), outward);
                    // Face OUTWARD, not up: "sheets face the sky" was right for
                    // a flat patch and wrong for a tangent one — it flipped
                    // every flank sheet's normal into the crown.
                    if (glm::dot(n, outward) < 0.0f) n = -n;
                    // ONE LIT DOME PER LOBE: 55 % of the normal comes from the
                    // lobe's own outward direction. Pure surface normals made
                    // every sheet its own little roof, which is the confetti
                    // shading §3.1 of the note is complaining about.
                    const glm::vec3 outn = safe_normalize(vp[gj][gi] - L.c, n);
                    n = safe_normalize(n * 0.45f + outn * 0.55f, n);
                    const float sway = std::clamp(
                        glm::length(vp[gj][gi] - sway_origin) / sway_span, 0.0f, 1.0f);
                    obj.cards.vertices.push_back(
                        {vp[gj][gi], n,
                         {uvr.x + (uvr.z - uvr.x) * (static_cast<float>(gi) * 0.5f),
                          uvr.y + (uvr.w - uvr.y) * (static_cast<float>(gj) * 0.5f)},
                         pack({sway, phase, jit_v})});
                }
            }
            for (int gj = 0; gj < 2; ++gj) {
                for (int gi = 0; gi < 2; ++gi) {
                    const uint32_t v00 = base_v + static_cast<uint32_t>(gj * 3 + gi);
                    obj.cards.indices.insert(obj.cards.indices.end(),
                                             {v00, v00 + 3u, v00 + 4u,
                                              v00, v00 + 4u, v00 + 1u});
                }
            }
        }
    }

    // Flat-coloured wood (nothing but ornaments left in this stream today) gets
    // the same furrow-and-moss value pass the first iteration uses: the two
    // builders must not disagree about what bark looks like up close.
    for (platform::Vertex& v : obj.wood.vertices) {
        if (v.color_rgba != bark_flat) continue;
        const float az = std::atan2(v.position.z, v.position.x);
        const float stripe = std::sin(az * 9.0f + v.position.y * 0.35f)
                           + 0.5f * std::sin(az * 23.0f - v.position.y * 0.2f);
        const float furrow = 0.86f + 0.17f * std::clamp(stripe, -1.0f, 1.0f);
        const float moss_h = std::clamp(1.0f - v.position.y / 4.0f, 0.0f, 1.0f);
        const float patch = 0.5f + 0.5f * std::sin(az * 3.0f + 1.7f);
        const float moss = moss_h * patch * 0.55f;
        glm::vec3 c = p.bark * furrow;
        c = c * (1.0f - moss) + glm::vec3{0.22f, 0.34f, 0.13f} * moss;
        v.color_rgba = pack(glm::clamp(c, glm::vec3{0.0f}, glm::vec3{1.0f}));
    }

    obj.content_hash = object_content_hash(obj);
    return obj;
}

} // namespace dfn::render
