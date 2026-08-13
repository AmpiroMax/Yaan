/*
Created: 09:08:2026 - 23:44:12
Last updated: 13:08:2026 - 21:15:00
Module: engine/render
File: engine/render/sources/FloraNeighbours.cpp

Responsibility:
- Neighbour analysis: derives a per-instance FloraShape (crown shyness, lean
  away from crowding, understory suppression, wind phase) from the scatter
  instance array a chunk already has.

Key items:
- analyse_neighbourhood().

Dependencies:
- Uses: ProcFlora.h, FloraSpecies.h, core math ScatterInstance, glm.
- Used by: ScatterBatcher (render), ProcFloraTests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; zone contract docs/specs/flora.md §3.3.
- PURE AND DETERMINISTIC. No globals, no wall-clock, no IO.
- math::ScatterInstance IS FROZEN (Rule 26) and gains no fields: everything here
  is DERIVED from the array core already sends, which is the whole reason this
  analysis is cheap enough to exist.
- Split out of ProcFlora.cpp on 09:08:2026 for Rule 21 (800-line limit) when the
  space-colonization rewrite landed. It was always a separate responsibility.
*/
/*
UPD:
- 09:08:2026 - 23:44:12: Split out of ProcFlora.cpp unchanged (Rule 21).
- 12:08:2026 - 00:0x: THE MATURITY TIERS AND THE WIND LEAN ARE BUILT.
  (a) maturity for canopy trees now comes from math::flora_maturity_for(), the
      25/60/12/3 tier draw that had been sitting in core/math with no consumer
      but the test suite — every tree in the world was standing at core's flat
      0.8-1.2 scatter scale;
  (b) tree lean gained a WIND AZIMUTH (LANDSCAPE §10.3.1: every tilt has an
      azimuth source; only boulders may use a free one), so a stand leans
      TOGETHER, and the magnitude band opened to what reference frame 16 shows.
- 12:08:2026 - 00:24:00: (see the entry above — the maturity tiers and the wind
  lean; this line exists because the previous one carried a placeholder time.)
- 12:08:2026 - 00:38:00: The lean band reads TREE_LEAN_WIND_MIN/MAX from the
  generated constants; the local copies are gone (Rule 14).
- 12:08:2026 - 23:20:00: The great oak is exempt from the maturity tier draw
  (lead's carve, core's hand): that species IS the giant, so the 0.4-1.5
  multiplier would be the same multiplier twice — and on the low tiers it would
  shrink a landmark the world's occlusion envelope has already promised at 48 m.
- 13:08:2026 - 19:45:00: The crowding neighbours are kept INDIVIDUALLY (up to
  eight) instead of being collapsed into one worst bearing, and each carries the
  radius at which this tree's wood must stop: the two crowns split the gap in
  proportion to their radii and both back off by half a channel. The channel
  width is derived from the read rule (a gap under distance/30 is one grey
  pixel, Rule 33) at the 30 m the near-canopy vantage looks from.
- 13:08:2026 - 21:40:00: FloraShape::crowding, and crown shyness now opens
  its channel only where the canopy is actually closing. Both are the same
  closeness curve on purpose: two numbers for "how closed is it here" that could
  disagree are two numbers that eventually will.
- 13:08:2026 - 21:15:00: DFN_FLORA_CROWNBASE -- a verification hook for the
  EDGE-EFFECT hypothesis, and the hypothesis is REFUTED, which is why the hook
  is worth keeping. Real forests read as canopy from outside because their edge
  trees carry foliage nearly to the ground; ours are built the same at the edge
  as in the middle, so the guess was that a lower crown would close the view.
  Measured on the frame, per material (composite against wood-only and
  cards-only), share of the treeline band each material stands IN FRONT of:
        crown base            sky     WOOD    CARD
        today (0.45-0.53)    23.7 %  58.6 %   4.8 %
        forced 0.30          26.1 %  57.1 %   3.4 %
        forced 0.18          28.7 %  55.5 %   2.7 %
  It goes the WRONG WAY. The foliage budget is a fixed cluster count, so
  lowering the base only spreads the same leaf over a taller crown and thins
  it -- more sky, not less. Eighth hypothesis about this crown, eighth
  measurement, and the first seven are in the files above.
  WHAT THE MEASUREMENT FOUND INSTEAD IS ARITHMETIC AND NEEDS NO HYPOTHESIS.
  Trunk coverage of a view through a stand of depth D at spacing s with boles d
  across is 1-(1-d/s)^(D/s):
        15 m spacing, 250 m of forest -> 49 %      8 m -> 91 %
        10 m                          -> 79 %      6 m -> 99 %
  At 6 m the boles ALONE tile the view within about forty rows, whatever the
  crowns do, and the per-row profile agrees: wood outweighs foliage at every
  height of the frame, 35 % against 15 % even in the middle of the canopy mass.
  So the colonnade is not a defect in the tree and cannot be fixed by moving a
  budget inside it.
*/

#include "engine/render/sources/ProcFlora.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dfn::render {

namespace {

/// splitmix64 — local, deterministic, no shared state.
uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

/// THE WIND FIELD'S AZIMUTH AT A POSITION — the source of every tree's lean.
///
/// LANDSCAPE §10.3.1 is the rule and it is the whole reason this is a field and
/// not a per-instance random: *"a field of independently tilted objects reads
/// as debris; a field of objects that agree about a direction reads as a place
/// with a history."* Reference frame 16 is the evidence — a leaning canopy
/// whose trees agree which way the wind blows.
///
/// A PREVAILING BEARING PLUS A SLOW WANDER. The wander's wavelength is
/// WIND_FIELD_WAVELENGTH (600 m), which is the number render already drives
/// cloud shadow with, so the canopy and the sky agree about the weather instead
/// of each inventing it; the swing is +/- 25 deg, which is design's own figure
/// for the snag row of the tilt table. Over a 40 m stand the azimuth is
/// effectively constant — trees a crown apart cannot disagree.
///
/// It is DERIVED FROM POSITION rather than read from the live wind so that the
/// geometry stays deterministic and cacheable: a mesh that changed shape when
/// the gust changed would rebuild the chunk every frame. The live wind animates
/// the FOLIAGE (vertex red/green); the standing lean is the climate, not the
/// weather.
float wind_lean_azimuth(glm::vec2 p) {
    constexpr float PREVAILING = 2.16f; ///< rad; the valley's down-wind bearing
    constexpr float WANDER = 0.436f;    ///< rad, 25 deg (LANDSCAPE §10.3.2)
    constexpr float LAMBDA = 600.0f;    ///< m, WIND_FIELD_WAVELENGTH
    const float a = std::sin(p.x * (6.2831853f / LAMBDA))
        + std::cos(p.y * (6.2831853f / (LAMBDA * 1.37f)));
    return PREVAILING + WANDER * (a * 0.5f);
}

} // namespace

std::vector<FloraShape> analyse_neighbourhood(std::span<const math::ScatterInstance> all,
                                              size_t count) {
    std::vector<FloraShape> out(std::min(count, all.size()));
    for (size_t i = 0; i < out.size(); ++i) {
        const math::ScatterInstance& a = all[i];
        const FloraSpecies fs = flora_species_of(a.species);
        FloraShape& sh = out[i];
        sh.maturity = a.scale;
        // THE MATURITY TIERS, FINALLY WIRED (design §5.8: 25/60/12/3 with
        // multipliers 1.15-1.50 / 0.85-1.15 / 0.50-0.70 / 0.40-0.60).
        //
        // Sixteen NUMBERS rows and a distribution test have existed for this
        // since 09.08.2026, all of them marked "НЕ ПОСТРОЕНО: у константы нет
        // ни одного потребителя", while every tree in the world stood at core's
        // flat scatter scale of 0.8-1.2 — a +/-20 % band around one size, which
        // is the plantation design ruled against, with the cure written down
        // and not connected. It is connected here rather than in placement
        // because the tier is a property of the TREE, and the same draw is what
        // core's canopy occlusion envelope is defined from (Rule 35: one home).
        //
        // REPLACING a.scale, not multiplying it: core's 0.8-1.2 is an unnamed
        // second spread on the same quantity, and composing them would put a
        // giant at 1.5 x 1.2 = 1.8 and break the occlusion envelope that reads
        // SPECIES_HEIGHT_MAX x TREE_MATURITY_GIANT_MULT_MAX. Told to lead for
        // core: the scale core writes for trees is now ignored by flora and
        // should either become this draw or stop being drawn.
        const glm::vec2 pos_xz{a.position.x, a.position.z};
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
        // The great oak takes NO tier draw: that species IS the giant (its own
        // row says so), so a 0.4-1.5 multiplier on top would be the same
        // multiplier twice — and downward it would shrink core's landmark.
        if (!flora_control_arm() && fs != FloraSpecies::GreatOak) {
            sh.maturity = math::flora_maturity_for(pos_xz);
        }
        // WIDTH GETS ITS OWN DRAW, on a different key from the height tier.
        // Two trees that agreed on height would otherwise agree on width too,
        // and a pair of identical crowns is exactly what "reads as copies"
        // means. Position-keyed, so it survives chunk borders and reloads.
        sh.crown_width_mult = 0.86f
            + 0.30f
                * (static_cast<float>(
                       mix64(static_cast<uint64_t>(
                                 static_cast<int64_t>(std::lround(a.position.x * 4.0f)))
                                 * 0xD1B54A32D192ED03ull
                             ^ mix64(static_cast<uint64_t>(static_cast<int64_t>(
                                   std::lround(a.position.z * 4.0f))) + 77ull))
                       >> 40)
                   / 16777216.0f);

        const float r_a = species_crown_radius(fs) * sh.maturity;
        const glm::vec2 pa{a.position.x, a.position.z};
        glm::vec2 pressure{0.0f};
        glm::vec2 worst_dir{0.0f};
        float worst_overlap = 0.0f;
        float tallest_neighbour = 0.0f;
        // The crowding neighbours, kept individually rather than summed. See
        // FloraShape::crowd: a tree with three neighbours grows three FLATS,
        // and the flats are where the sky channels between crowns are.
        struct Crowder {
            glm::vec2 dir;
            float limit;
            float overlap;
            float gap_ratio; ///< dist / (r_a + r_b): 1 = crowns just touch
        };
        Crowder crowders[FloraShape::CROWD_MAX]{};
        int crowder_count = 0;

        for (size_t j = 0; j < all.size(); ++j) {
            if (j == i) continue;
            const math::ScatterInstance& b = all[j];
            const FloraSpecies fb = flora_species_of(b.species);
            if (!is_canopy_tree(fb)) continue;
            const glm::vec2 pb{b.position.x, b.position.z};
            const glm::vec2 d = pb - pa;
            const float dist = glm::length(d);
            if (dist < 1e-3f || dist > 40.0f) continue;
            const float r_b = species_crown_radius(fb)
                * math::flora_maturity_for({b.position.x, b.position.z});
            const float overlap = r_a + r_b - dist;
            if (overlap <= 0.0f) continue;
            const glm::vec2 dir = d / dist;
            pressure += dir / (dist * dist);
            // WHERE THIS TREE'S WOOD MUST STOP along that bearing. The two
            // crowns meet at the point that splits the gap in proportion to
            // their radii — a big tree is not pushed back by a sapling as far
            // as the sapling is pushed by it — and then BOTH back off by half
            // the shy gap, which is what leaves a channel rather than a seam.
            //
            // THE GAP IS DERIVED FROM THE READ RULE, not chosen. A channel
            // narrower than distance/30 is not a channel at that distance, it
            // is one grey pixel (Rule 33, SILHOUETTE_MIN_PX). The near-canopy
            // vantage this is judged from stands 5-30 m under the crowns, so
            // the gap has to survive 30 m: 30/30 = 1.0 m. Expressed against the
            // crown so it scales with the tree instead of being a metre that
            // is right for an oak and absurd for a krummholz.
            // AND THE CHANNEL OPENS ONLY WHERE THE CANOPY IS ACTUALLY CLOSING.
            // Crown shyness is a phenomenon of CLOSED canopies — trees whose
            // crowns are nowhere near each other do not hold back, and neither
            // should ours. Without this the rule fires at any spacing where the
            // NOMINAL crowns overlap, which at our 12-18 m brief they do while
            // the BUILT ones barely touch: measured, canopy cover fell 0.692 ->
            // 0.468 at 12 m, i.e. a third of the forest given up to answer a
            // question about dense stands.
            //
            // Same closeness curve as `crowding` below, and deliberately the
            // same: two numbers describing "how closed is it here" that could
            // disagree are two numbers that eventually will.
            const float gap_ratio = dist / std::max(r_a + r_b, 0.01f);
            const float closeness = std::clamp((0.70f - gap_ratio) / 0.45f, 0.0f, 1.0f);
            const float gap = std::max(1.0f, r_a * 0.12f) * closeness;
            const float share = r_a / std::max(r_a + r_b, 0.01f);
            const float limit = std::max(dist * share - gap * 0.5f, r_a * 0.25f);
            if (crowder_count < FloraShape::CROWD_MAX) {
                crowders[crowder_count++] =
                    Crowder{dir, limit, overlap, gap_ratio};
            } else {
                // Keep the WORST four: a fifth neighbour that crowds less than
                // the four already held cannot be the one that decides the
                // silhouette.
                int weakest = 0;
                for (int k = 1; k < FloraShape::CROWD_MAX; ++k) {
                    if (crowders[k].overlap < crowders[weakest].overlap) weakest = k;
                }
                if (overlap > crowders[weakest].overlap) {
                    crowders[weakest] =
                        Crowder{dir, limit, overlap, gap_ratio};
                }
            }
            if (overlap > worst_overlap) {
                worst_overlap = overlap;
                worst_dir = dir;
            }
            const float h_b = species_nominal_height(fb)
                * math::flora_maturity_for({b.position.x, b.position.z});
            tallest_neighbour = std::max(tallest_neighbour, h_b);
        }

        const SpeciesParams& sp = species_params(fs);
        // HOW CROWDED, as a number this tree can be BUILT from. Derived from
        // the same neighbour sweep that produces the shyness boundaries, so it
        // costs nothing extra and cannot disagree with them: the share of this
        // tree's own crown circle that its neighbours' crowns reach into,
        // saturating at one crown's worth. Two neighbours pressing halfway in
        // is a closed-forest tree; none is an open-grown one.
        // The first form of this SATURATED EVERYWHERE and is recorded because
        // it looked reasonable: the summed overlap over eight neighbours,
        // divided by a crown. At any spacing we ship it came out at 1, so every
        // tree in the world narrowed by the full amount and the canopy fell
        // from 0.538 cover to 0.177 — the whole forest thinned to answer a
        // question about dense stands. The quantity has to be a RATIO of
        // spacing to crown, not a sum of overlaps, or it stops discriminating
        // exactly where it is supposed to.
        //
        // CALIBRATED AGAINST THE TWO ENDS WE HAVE MEASUREMENTS FOR, which is
        // the only honest way to place a curve with two free numbers: at the
        // shipped 12-18 m spacing the canopy is not closed and a tree there
        // should keep the width the user asked for (crowding ~0.2), and at the
        // 5-6 m the user is asking for the stand sweep says width must fall to
        // 0.65 (crowding ~0.9). Between them it is linear because nothing we
        // measured says otherwise, and a curve invented past the evidence is
        // the thing Rule 31 is about.
        {
            float sum = 0.0f;
            for (int k = 0; k < crowder_count; ++k) {
                sum += std::clamp((0.70f - crowders[k].gap_ratio) / 0.45f, 0.0f, 1.0f);
            }
            sh.crowding = (crowder_count > 0)
                ? sum / static_cast<float>(crowder_count)
                : 0.0f;
        }
        // A species that does not hold back gets no boundaries at all: nothing
        // crowds a great oak (its own row says shyness 0), and a conifer's
        // narrow crown rarely touches its neighbour's.
        if (sp.shyness > 0.0f) {
            for (int k = 0; k < crowder_count; ++k) {
                sh.crowd[sh.crowd_count].dir = crowders[k].dir;
                sh.crowd[sh.crowd_count].limit = crowders[k].limit;
                ++sh.crowd_count;
            }
        }
        if (worst_overlap > 0.0f) {
            sh.shy_dir = worst_dir;
            sh.shyness = std::min(sp.shyness, worst_overlap / (2.0f * std::max(r_a, 0.1f)));
        }
        // --- THE LEAN. «Деревья не должны расти чётко вверх» -------------
        // WHAT CHANGED AND WHY IT IS NOT A BIGGER NUMBER. The old lean was
        // crowding only: its DIRECTION was "away from my neighbours", i.e. a
        // different bearing for every tree, so raising its cap would have
        // produced exactly the noise LANDSCAPE §10.3.1 forbids. The lean now
        // has two parts with two different characters:
        //   - a WIND lean, shared by everything within a few hundred metres,
        //     which is what makes a canopy read as weathered rather than as
        //     wonky. Band 8-20 deg, from reference frame 16's measured 15-25
        //     deg for its most exposed trees, taken at the conservative end
        //     because frame 16 is a coastal canopy and our valley is not;
        //   - the old crowding lean, kept, but now a MODULATION of the shared
        //     one rather than a competing bearing: a crowded tree leans further
        //     into the gap the wind is already pushing it toward.
        // Magnitude varies per tree (exposure), direction does not.
        // The rows landed 12.08.2026 (NUMBERS.md, «Деревья: ширина, наклон,
        // гигантский дуб»), so the numbers are read, never re-typed (Rule 14).
        constexpr auto LEAN_MIN = static_cast<float>(config::TREE_LEAN_WIND_MIN);
        constexpr auto LEAN_MAX = static_cast<float>(config::TREE_LEAN_WIND_MAX);
        const float az = wind_lean_azimuth(pos_xz);
        const glm::vec2 wind_dir{std::cos(az), std::sin(az)};
        const float exposure = static_cast<float>(
                                   mix64(static_cast<uint64_t>(static_cast<int64_t>(
                                             std::lround(a.position.x * 2.0f)))
                                             * 0x9E3779B1ull
                                         ^ mix64(static_cast<uint64_t>(
                                             static_cast<int64_t>(
                                                 std::lround(a.position.z * 2.0f)))
                                             + 313ull))
                                   >> 40)
            / 16777216.0f;
        float lean = LEAN_MIN + (LEAN_MAX - LEAN_MIN) * exposure;
        glm::vec2 lean_dir = wind_dir;
        const float press = glm::length(pressure);
        if (press > 1e-5f) {
            // Crowding bends the shared bearing rather than replacing it, and
            // adds to the magnitude: the tree still goes with the wind, it just
            // goes further where there is room.
            const glm::vec2 open = -pressure / press;
            lean_dir = glm::normalize(wind_dir * 0.72f + open * 0.28f);
            lean += std::min(sp.lean_response * press * 40.0f, 0.10f);
        }
        sh.lean_dir = lean_dir;
        sh.lean = std::min(lean, LEAN_MAX + 0.10f);
        if (flora_control_arm()) {
            // The pre-change lean: crowding only, own bearing per tree, capped
            // at 0.12 rad.
            if (press > 1e-5f) {
                sh.lean_dir = -pressure / press;
                sh.lean = std::min(sp.lean_response * press * 40.0f, 0.12f);
            } else {
                sh.lean = 0.0f;
            }
            sh.crown_width_mult = 1.0f;
        }
        // Understory: a small tree under a much taller crowded canopy is drawn
        // out and reaching, not a scale model of a mature one.
        const float own_h = species_nominal_height(fs) * sh.maturity;
        sh.understory = sh.maturity < 0.75f && tallest_neighbour > own_h * 1.5f;
        // VERIFICATION HOOK, NEVER A SHIPPING PATH (same standing as
        // DFN_FLORA_NODES): DFN_FLORA_CROWNBASE=<fraction> puts every crown's
        // base at that fraction of height, so the question "would a forest
        // whose crowns come lower stop reading as a colonnade" can be answered
        // by a frame instead of by argument. It is the counterfactual for the
        // EDGE-EFFECT hypothesis: measured, wood stands in front of foliage in
        // the treeline band by twelve to one, while a single tree is 80 % crown
        // by outline — so what the eye meets is the 8-11 m of clear bole that
        // CANOPY_CLEARANCE_MIN and the crown-base fraction jointly buy, seen
        // through forty rows of it. Real forests read as canopy from outside
        // because their EDGE trees carry foliage nearly to the ground; ours are
        // built the same at the edge as in the middle.
        if (const char* e = std::getenv("DFN_FLORA_CROWNBASE")) {
            const float f = static_cast<float>(std::atof(e));
            if (f > 0.0f && f < 1.0f) sh.crown_base_override = f;
        }
    }
    return out;
}
} // namespace dfn::render
