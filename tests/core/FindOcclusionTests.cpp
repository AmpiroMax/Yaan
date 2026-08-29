/*
Module: tests
File: tests/core/FindOcclusionTests.cpp

Responsibility:
- BR-5's COMPOSED-SCENE instrument (LANDSCAPE §1.7, design's ruling of
  10.08.2026): find occlusion measured against terrain + real placed oak
  trunks + real placed Bush/BigBush, aggregated PER RING DISTANCE, with the
  bare-terrain reading kept as the permanent must-fail control.

Dependencies:
- Uses: doctest, dfn_world (Worldgen, WorldgenFinds, WorldgenScatter,
  WorldgenForest), dfn_render (species geometry — see the note below).
- Used by: ctest (test_find_occlusion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS TEST LINKS dfn_render ON PURPOSE, exactly as test_lod_seam does, and
  for the same reason: it is checking that two zones agree about one geometry.
  The occluder discs are sized from render's OWN species_trunk_radius() /
  species_crown_radius() / species_crown_base(), not from literals copied into
  this file. A literal here would be a third copy of a number that already has
  two consumers (render's mesh, sim's collision capsule — species_trunk_radius
  says so in its own comment), which is precisely the Rule 35 defect this
  project spent 10.08.2026 paying for in the height pipeline.
- The SHIPPING instrument (engine/world) cannot see render — DAG siblings,
  Rule 1 — so OccluderGeometry is a required input with no defaults. When the
  NUMBERS.md rows land, this file changes and the instrument does not.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenFinds.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/WorldgenScatter.h"
#include "engine/render/sources/FloraSpecies.h"
#include "engine/render/sources/ProcFlora.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <vector>

using namespace dfn;

namespace {

float median(std::vector<float> v) {
    REQUIRE(!v.empty());
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

/// The stand, built once — build_world_context is the expensive part and every
/// case below wants the same world.
const world::WorldGenContext& stand() {
    static const world::WorldGenContext ctx = [] {
        world::WorldGenParams p{1, {0, 0}, {3, 3}};
        p.layout = world::forest_stand_layout();
        return world::build_world_context(p);
    }();
    return ctx;
}

/// Disc geometry taken from RENDER'S OWN species table (see the file notice).
world::OccluderGeometry geometry_from_render() {
    using render::FloraSpecies;
    world::OccluderGeometry g;
    const float oak_h = render::species_nominal_height(FloraSpecies::DaleOak);
    g.oak_height_m = oak_h;
    // NOT species_trunk_radius(): that accessor returns the ROOT FLARE at
    // y=0, which is 0.986 m and 1.6x the bole above it (flora measured the
    // built mesh, 10.08.2026). This ray runs from an eye at 1.7 m down to a
    // find top at 0.5 m, so it never leaves the 0.3-1.7 m band and never meets
    // the clear bole at all; at its mid-path height of ~1.0 m the oak measures
    // 0.62-0.65 m over 12 variants. Using the accessor overstated the oak's
    // occluding width by up to 1.67x and flattered this gate.
    //
    // A LITERAL HERE, DELIBERATELY AND TEMPORARILY, because the honest value is
    // not exposed: flora is renaming the accessor or giving it a height
    // argument (Rule 35's "a thing gains a dimension" firing early -- the next
    // consumer is placement, where a 1.67x trunk is a collision hull rather
    // than a rounding error). This line becomes a call again when it lands.
    g.oak_trunk_radius_m = 0.65f;
    // Only the CLEAR TRUNK occludes. Above crown_base the ray is in foliage,
    // which is C1's transmittance model and explicitly not this one.
    g.oak_trunk_top_frac = render::species_crown_base(FloraSpecies::DaleOak) / oak_h;
    g.bush_radius_m = render::species_crown_radius(FloraSpecies::Bush);
    g.bush_height_m = render::species_nominal_height(FloraSpecies::Bush);
    g.big_bush_radius_m = render::species_crown_radius(FloraSpecies::BigBush);
    g.big_bush_height_m = render::species_nominal_height(FloraSpecies::BigBush);
    return g;
}

std::vector<math::ScatterInstance> stand_scatter() {
    const world::WorldGenContext& c = stand();
    const auto CH = static_cast<float>(config::CHUNK_SIZE);
    std::vector<math::ScatterInstance> all;
    for (int cz = 0; cz < 4; ++cz) {
        for (int cx = 0; cx < 4; ++cx) {
            const auto s = world::build_scatter(c, {static_cast<float>(cx) * CH, static_cast<float>(cz) * CH}, {static_cast<float>(cx + 1) * CH, static_cast<float>(cz + 1) * CH});
            all.insert(all.end(), s.begin(), s.end());
        }
    }
    return all;
}

} // namespace

TEST_CASE("BR-5's occluder set is the three ruled classes and nothing else") {
    // EXCLUDED BY CAUSE, NOT BY SIZE (Rule 36). Dead wood measures near zero at
    // 60 m, but that is not why it is out: it is out because those classes are
    // sized for the user's brief («поваленные деревья... сухие мертвые
    // деревья»), so a gate leaning on them could be passed by making scenery
    // bigger. Excluding by the measured contribution would have been the filter
    // choosing the result.
    const auto scatter = stand_scatter();
    REQUIRE(scatter.size() > 5000);
    const auto discs = world::build_find_occluders(scatter, geometry_from_render(),
                                                   [](glm::vec2 p) {
                                                       return world::terrain_height(stand(), p);
                                                   });
    std::size_t ruled = 0;
    for (const math::ScatterInstance& i : scatter) {
        if (i.species == math::ScatterSpecies::OakTree || i.species == math::ScatterSpecies::Bush
            || i.species == math::ScatterSpecies::BigBush) {
            ++ruled;
        }
    }
    CHECK(discs.size() == ruled);
    CHECK(discs.size() > 1000);
    // And it is a STRICT subset — the control that this filter is doing work at
    // all rather than passing everything through.
    CHECK(discs.size() < scatter.size());
    for (const world::OccluderDisc& d : discs) {
        CHECK(d.radius > 0.0f);
    }
}

TEST_CASE("BR-5: the composed scene occludes, and bare terrain is the must-fail control") {
    const world::WorldGenContext& c = stand();
    REQUIRE(c.finds.size() > 50);
    const auto height = [&](glm::vec2 p) { return world::terrain_height(c, p); };
    const auto discs = world::build_find_occluders(stand_scatter(), geometry_from_render(), height);

    // PER-DISTANCE, NEVER POOLED (design sharpened this on flora's find): BR-5
    // models a walker CROSSING the band, so a strong reading at 80 m must not
    // buy cover for a weak one at 40 m. The instrument itself takes one radius
    // and has no way to pool, which is the point.
    struct Ring {
        float radius;
        float composed;
        float bare;
        float closed;   ///< the scatter's share of the openness the GROUND LEFT
    };
    std::vector<Ring> rings;
    for (const float r : {40.0f, 60.0f, 80.0f}) {
        std::vector<float> composed;
        std::vector<float> bare;
        std::vector<float> closed;
        composed.reserve(c.finds.size());
        bare.reserve(c.finds.size());
        closed.reserve(c.finds.size());
        for (const world::Find& f : c.finds) {
            const float cf = world::occluded_fraction_at(height, discs, f.position, r, 24);
            const float bf = world::occluded_fraction_at(height, {}, f.position, r, 24);
            composed.push_back(cf);
            bare.push_back(bf);
            // NORMALISED PER FIND, not as a ratio of the two medians: both
            // readings are taken at the SAME find, so the pairing is real and
            // throwing it away to divide two summaries would be a coarser
            // instrument for no reason.
            closed.push_back(bf >= 0.999f ? 1.0f : (cf - bf) / (1.0f - bf));
        }
        rings.push_back({r, median(composed), median(bare), median(closed)});
    }

    const auto BAR = static_cast<float>(config::FIND_OCCLUSION_FRAC);
    for (const Ring& r : rings) {
        INFO("ring ", r.radius, " m: composed median ", r.composed, ", bare-terrain ", r.bare,
             ", scatter closes ", r.closed, " of what the ground left, bar ", BAR);
        MESSAGE("ring " << r.radius << " m: composed " << r.composed << ", bare " << r.bare
                        << ", scatter closes " << r.closed << " of the remaining openness");
        // THE BARE-TERRAIN CONTROL IS RETIRED HERE, AND THIS NOTE IS ITS
        // HEADSTONE — it is not being weakened, it FIRED and its work is done.
        //
        // It read: "if the bare-terrain reading ever climbs toward the bar, the
        // terrain has quietly grown a job it was not given, and the ruling's
        // premise needs re-checking rather than a shrug." On 13.08.2026 it
        // climbed — 0.000/0.167/0.333 to 0.167/0.375/0.500 — because §10.1.3's
        // forms landed and the ground genuinely began to hide things. That is
        // the premise changing, not a regression, and the sentinel caught it
        // days after it was written, which is the whole reason a control is
        // kept "forever". The premise it guarded ("bare ground hides nothing")
        // was TRUE ONLY BECAUSE THE GROUND WAS FLAT.
        //
        // The reading stays REPORTED (above) because it is now a fact about the
        // terrain worth watching; it is no longer an assertion about the finds.
        //
        // ALSO WORTH RECORDING, because it arrived as the good half of the same
        // event: the 40 m ring, pinned below as KNOWN-OPEN with "flip when the
        // lever lands" and an expected cure of density-aware find placement,
        // CLOSED — 0.375 -> 0.500, and it was the GROUND that closed it. The
        // density lever does not have to be spent.
        // THE SITING CLAIM, THIRD FORM, AND EACH FORM DIED OF THE SAME DISEASE
        // ONE LEVEL FURTHER IN. It was a RATIO (siting beats bare ground 3-4x)
        // until the denominator went to exactly 0.0000 at the near ring. It
        // became a DIFFERENCE (composed - bare > 0.3) until the ground started
        // hiding things: an occluded FRACTION is capped at 1, so the more the
        // ground hides, the less there is LEFT for the scatter to hide, and a
        // threshold on a difference with a moving base slides on its own. It
        // read 0.333 / 0.292 / 0.250 and failed two rings while the scatter had
        // not changed at all.
        //
        // Now it is the share of the openness THE GROUND LEFT that the scatter
        // closes: (composed - bare) / (1 - bare), per find. Two properties earn
        // it the place: at bare = 0 it is IDENTICAL to the old clause (so the
        // world it was accepted on is still described by it), and it is nearly
        // INVARIANT to the terrain change that broke the other two — measured
        // across worlds whose bare-ground occlusion differs threefold:
        //
        //   ring   forms OFF (the world BR-5 was accepted on)   forms ON
        //   40 m   0.3043                                       0.3158
        //   60 m   0.4545                                       0.4375
        //   80 m   0.5333                                       0.5385
        //
        // The scatter's own contribution moves by at most 0.017 across two
        // worlds whose BARE-GROUND occlusion differs by 0.25, 0.25 and 0.17 at
        // the three rings — i.e. the base tripled and this quantity did not
        // move. That invariance is the evidence that the ratio and the
        // difference were failing on the BASE and never on the scatter.
        //
        // THE FLOOR AND WHAT IT STANDS ON (proposed by core, approved by the
        // lead in design's absence and flagged re-openable by design):
        //   * 0.25 is below every ring of the world the clause was ACCEPTED on
        //     (lowest 0.3043), not below today's build — a bar fitted to today
        //     would read 0.30, and the point of quoting the accepted world is
        //     that it is the one arm nobody can have tuned after the fact.
        //   * It is failed by the retired control's OWN must-fail artefact,
        //     which carries over exactly: "the forest with the forest deleted"
        //     reads 0.000 here BY CONSTRUCTION, since composed == bare with no
        //     discs. The rejected case survives the change of quantity, and
        //     that is what makes this a re-definition rather than a retreat.
        //   * It does NOT inherit FIND_OCCLUSION_FRAC's 0.5. On flat ground the
        //     two clauses coincide, and the near ring has never met 0.5 on
        //     either — it was pinned known-open at 0.375 in the accepted world.
        //     Setting the floor at 0.5 would retroactively fail the build the
        //     clause was written against, which is a different claim from the
        //     one being made here.
        CHECK(r.closed > 0.25f);
    }

    // THE GATE ITSELF, REPORTED PER RING RATHER THAN ASSERTED AS ONE NUMBER.
    // Measured 10.08.2026 at seed 1, with flora's corrected bole radius:
    // 40 m -> 0.3333, 60 m -> 0.5417, 80 m -> 0.6250 against a bar of 0.5.
    // (With the flare radius they read 0.4167 / 0.6250 / 0.7083 -- the
    // correction moved every ring DOWN, i.e. against this instrument's own
    // result, which is the direction an error in one's favour should move when
    // it is fixed.)
    //
    // THE NEAR RING FAILS AND THAT IS NOT ASSERTED AWAY. It is the same cell
    // flora found marginal from the other side (their ruled-MIN/40 m missed by
    // 0.021; this instrument reads 0.4167 there), and design ruled the fix is
    // density-aware find placement, sized only after core re-measures on this
    // instrument — which is what this case exists to make possible. Asserting
    // the bar at 40 m today would turn a known-open design item into a red
    // suite that the next reader weakens (Rule 38's expensive failure mode).
    const Ring& near_ring = rings.front();
    INFO("near ring ", near_ring.radius, " m reads ", near_ring.composed, " against bar ", BAR);
    // FLIPPED 13.08.2026 — "flip when the lever lands", and it landed from a
    // direction nobody had listed. The near ring was pinned known-open at 0.375
    // with an expected cure of density-aware find placement; §10.1.3's ground
    // forms took it to 0.500 without a single find moving. The cure was not
    // needed and the density lever is unspent. Now asserted the right way up,
    // so a regression in it goes red instead of quietly passing a "< bar".
    CHECK(near_ring.composed >= BAR);
    for (std::size_t i = 1; i < rings.size(); ++i) {
        INFO("ring ", rings[i].radius, " m reads ", rings[i].composed);
        CHECK(rings[i].composed > BAR);
    }
    // Occlusion must RISE with distance on this stand — the ordering is the
    // structural claim, and it is what a broken march would scramble.
    CHECK(rings[1].composed > rings[0].composed);
    CHECK(rings[2].composed > rings[1].composed);
}

TEST_CASE("the disc test is exact, not sampled: a trunk between eye and find blocks it") {
    // 30a — A CASE THAT CAN PASS, built by hand so the geometry is knowable.
    // Flat ground, one occluder, one bearing's worth of reasoning.
    const auto flat = [](glm::vec2) { return 0.0f; };
    const glm::vec2 find{0.0f, 0.0f};
    const glm::vec2 eye{40.0f, 0.0f};

    world::OccluderDisc trunk;
    trunk.center = {20.0f, 0.0f}; // dead centre of the segment
    trunk.radius = 0.5f;
    trunk.top_y = 10.0f;
    // A SINGLE BEARING aimed straight down +x: bearing 0 of 4 is exactly this
    // ray, so the fraction is 1/4 when it blocks and 0 when it does not.
    CHECK(world::occluded_fraction_at(flat, std::span{&trunk, 1}, find, 40.0f, 4)
          == doctest::Approx(0.25f));

    // THE CONTROL, AND IT IS THE ONE THAT MATTERS: nudge the trunk off the line
    // by more than its radius and the ray is clear. A march that sampled every
    // 4 m instead of testing the segment would give the same answer for both,
    // because a 4 m stride steps over a 0.5 m trunk either way.
    world::OccluderDisc beside = trunk;
    beside.center = {20.0f, 2.0f};
    CHECK(world::occluded_fraction_at(flat, std::span{&beside, 1}, find, 40.0f, 4)
          == doctest::Approx(0.0f));
    // Half a metre off centre is still a hit: the disc has width and the test
    // must use it. This is the assertion that fails if someone "simplifies"
    // the segment-vs-disc test back to point sampling.
    world::OccluderDisc grazing = trunk;
    grazing.center = {20.0f, 0.4f};
    CHECK(world::occluded_fraction_at(flat, std::span{&grazing, 1}, find, 40.0f, 4)
          == doctest::Approx(0.25f));

    // HEIGHT IS PART OF THE TEST. A bush the ray sails over does not occlude,
    // and this is the arm that separates a stem-level model from a
    // "there is vegetation somewhere along the line" model.
    world::OccluderDisc low = trunk;
    low.top_y = 0.2f; // ray runs 1.7 m -> 0.5 m, so it passes well above
    CHECK(world::occluded_fraction_at(flat, std::span{&low, 1}, find, 40.0f, 4)
          == doctest::Approx(0.0f));
}
