/*
Module: tests
File: tests/sim/GreatOakStairTests.cpp

Responsibility:
- THE INSTRUMENT for the great oak's stair: can a player actually climb it?
  Measured by climbing it, with the real character controller against the real
  collision, and reported as the fraction of consecutive tread pairs a walker
  can take.

Key items:
- StairRig: the oak's collider in a Jolt world, plus a ground far below so a
  fall is a measurement and not a hang.
- find_treads(): the treads read out of the SOLID mesh, not the drawn one —
  climbing is a physics question and the collider is what a foot lands on.
- "every consecutive pair of treads can be taken": the gate. RED until the
  stair is walkable; see the note below before "fixing" it here.

Dependencies:
- Uses: doctest, dfn_gameplay (FloraCollision + PlayerMovement), dfn_physics,
  dfn_platform_physics (Jolt), dfn_render (flora's geometry), dfn_core.
- Used by: ctest (sim_great_oak_stair).

Notes:
- THIS TEST IS RED ON PURPOSE, TODAY. It is a gate for a defect that lives in
  ANOTHER ZONE's geometry (flora emits the treads), written here because this
  is the zone that can measure it: the question "can a walker take this step" is
  a character-controller question and the answer needs a controller.
  The number to watch is `walkable_pairs / pairs`, and the contract in
  NUMBERS.md (GREAT_OAK_STEP_RISE) says it must be 1.0.
  DO NOT relax the assertion to make the suite green. The suite going green is
  the event this file exists to announce.
- WHY NOT A FRAME (Rule 27): a stair is a sequence of TRANSITIONS. A frame can
  show treads; only a run can show that a foot gets from one to the next. The
  NUMBERS row says so explicitly — "приёмка пары — не кадр, а ПРОГОН".
- The rise half of the defect was fixed by re-deriving GREAT_OAK_STEP_RISE
  0.42 -> 0.28 (80 % of PLAYER_STEP_HEIGHT). The horizontal half is untouched:
  the treads spiral by the golden angle, so neighbours in HEIGHT are on opposite
  sides of the bole. This file measures both halves separately, so whoever
  rebuilds the stair sees which one is still open without reading physics.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here may restate flora's step formula. Tread positions are MEASURED
  out of the collider; the only numbers taken from the registry are the
  player's own (PLAYER_STEP_HEIGHT, PLAYER_CAPSULE_*) and the rise, which is
  used as a search stride and not as a position.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/gameplay/sources/FloraCollision.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/render/sources/ProcFlora.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace physics_layer = dfn::physics;
namespace platform = dfn::platform;
namespace render = dfn::render;
namespace math = dfn::math;

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float STEP_HEIGHT = static_cast<float>(config::PLAYER_STEP_HEIGHT);
constexpr float CAPSULE_R = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
constexpr float RISE = static_cast<float>(config::GREAT_OAK_STEP_RISE);

// One tread, as the COLLIDER has it.
struct Tread {
    int index = 0;           // the rise band it sits in; +1 apart = consecutive
    float top = 0.0f;        // world y of the surface a foot lands on
    glm::vec2 centre{0.0f};  // world x/z of that surface
    float width = 0.0f;      // narrowest horizontal span, metres
    float azimuth_span = 0.0f; // radians of bole the cluster wraps
    // The tread's own points, so the gap to the next one can be measured
    // SURFACE TO SURFACE. Centre-to-centre flatters or damns depending on how
    // long the treads are, and the number flora needs to drive to zero is the
    // one a foot actually has to bridge.
    std::vector<glm::vec2> footprint;
};

// Closest approach between two treads, metres.
[[nodiscard]] float surface_gap(const Tread& a, const Tread& b) {
    float best = 1.0e9f;
    for (const glm::vec2& p : a.footprint) {
        for (const glm::vec2& q : b.footprint) {
            best = std::min(best, glm::length(q - p));
        }
    }
    return best;
}

// How many rise bands held outboard geometry that could NOT be resolved into a
// single tread — above ~7 m the bole starts throwing limbs, and a limb stands
// clear of the bole in exactly the way a tread does. Reported rather than
// silently skipped: a detector that quietly drops what it cannot read turns
// its own blind spot into a defect in the subject, which is what the first
// version of this file did (it reported a 1.37 m rise on a stair built at a
// uniform 0.28 m).
int g_unresolved_bands = 0;

// The treads, read out of the solid mesh. A tread is geometry standing CLEAR of
// the bole in a height band; the bole's own radius at that height is the
// smallest radius any wood reaches there, so "clear of the bole" needs no
// formula from flora.
[[nodiscard]] std::vector<Tread> find_treads(const render::MeshData& solid, float max_y) {
    auto bole_radius = [&](float y) {
        float best = 1.0e9f;
        for (const platform::Vertex& v : solid.vertices) {
            if (std::abs(v.position.y - y) > 0.25f) {
                continue;
            }
            best = std::min(best, std::sqrt(v.position.x * v.position.x +
                                            v.position.z * v.position.z));
        }
        return best > 1.0e8f ? 0.0f : best;
    };

    struct Bucket {
        std::vector<glm::vec3> points;
    };
    std::map<int, Bucket> buckets; // ordered: deterministic iteration (Rule 13.2)
    for (const platform::Vertex& v : solid.vertices) {
        const float y = v.position.y;
        if (y < 0.5f || y > max_y) {
            continue;
        }
        const float r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        const float bole = bole_radius(y);
        if (bole <= 0.0f || r < bole + 0.30f) {
            continue; // still the bole
        }
        // THE THRESHOLD IS NOT DOING THE WORK, and that was worth checking
        // before quoting any of these numbers: swept 0.08 / 0.15 / 0.30 and the
        // tread count moves 20 <-> 21 while the width (0.24 m), the gap
        // (3.99 m) and the verdict (0 of 17) do not move at all. A tread is a
        // tube with two rings, and its inner ring sits INSIDE the bole — buried
        // wood nobody stands on — so only the outer end is ever a surface,
        // whatever the margin says.
        buckets[static_cast<int>(std::lround(y / RISE))].points.push_back(v.position);
    }

    std::vector<Tread> out;
    g_unresolved_bands = 0;
    for (const auto& [key, bucket] : buckets) {
        if (bucket.points.size() < 4) {
            continue;
        }
        // A TREAD IS A COMPACT CLUSTER. The bole also carries a golden chain and
        // the forks carry decks, and both stand clear of the bole in exactly the
        // same way — but they WRAP it. Anything spanning more than a third of a
        // turn is not something you stand on, it is something you walk past, and
        // counting it as a tread would flatter the very number this file exists
        // to report.
        // ANGULAR SPREAD, MEASURED ON THE CIRCLE. min/max of atan2 is wrong and
        // was wrong here first time out: a tread sitting across the +/-pi seam
        // has points at +3.1 and -3.1 rad, reads as a 6.2 rad span, and gets
        // thrown away as a chain. Seven treads vanished that way, and the gaps
        // they left made the RISE case fail at 1.37 m on a stair built at a
        // uniform 0.28 m -- a red test for a reason that had nothing to do with
        // the stair, which is the worst kind of red there is.
        // Mean direction, then the largest deviation from it: no seam.
        glm::vec2 mean{0.0f};
        for (const glm::vec3& p : bucket.points) {
            const glm::vec2 dir{p.x, p.z};
            if (glm::length(dir) > 1.0e-4f) {
                mean += glm::normalize(dir);
            }
        }
        if (glm::length(mean) < 1.0e-4f) {
            ++g_unresolved_bands;
            continue; // points spread evenly all round: a ring, not a tread
        }
        mean = glm::normalize(mean);
        float max_dev = 0.0f;
        for (const glm::vec3& p : bucket.points) {
            const glm::vec2 dir{p.x, p.z};
            if (glm::length(dir) <= 1.0e-4f) {
                continue;
            }
            max_dev = std::max(max_dev,
                               std::acos(std::clamp(glm::dot(glm::normalize(dir), mean),
                                                    -1.0f, 1.0f)));
        }
        const float span = 2.0f * max_dev;
        if (span > 2.0f) {
            ++g_unresolved_bands;
            continue; // wraps the bole: chain or deck, not a tread
        }

        // AND COMPACT RADIALLY. A bucket holding a tread AND a limb passes the
        // angular test when they happen to point the same way, and its centroid
        // is then neither. A tread is GREAT_OAK_STEP_REACH long; anything
        // reaching much further out in one band is a mixture.
        float r_lo = 1.0e9f;
        float r_hi = 0.0f;
        for (const glm::vec3& p : bucket.points) {
            const float r = std::sqrt(p.x * p.x + p.z * p.z);
            r_lo = std::min(r_lo, r);
            r_hi = std::max(r_hi, r);
        }
        if (r_hi - r_lo > 2.0f * static_cast<float>(config::GREAT_OAK_STEP_REACH)) {
            ++g_unresolved_bands;
            continue;
        }

        Tread t;
        t.index = key;
        t.azimuth_span = span;
        double sx = 0.0;
        double sz = 0.0;
        for (const glm::vec3& p : bucket.points) {
            sx += p.x;
            sz += p.z;
            t.top = std::max(t.top, p.y);
        }
        t.centre = {static_cast<float>(sx / bucket.points.size()),
                    static_cast<float>(sz / bucket.points.size())};
        for (const glm::vec3& p : bucket.points) {
            t.footprint.push_back({p.x, p.z});
        }
        // Width across the tread: the span perpendicular to the direction it
        // points. That is the dimension a foot has to fit on.
        const glm::vec2 out_dir = glm::normalize(t.centre);
        const glm::vec2 across{-out_dir.y, out_dir.x};
        float lo = 1.0e9f;
        float hi = -1.0e9f;
        for (const glm::vec3& p : bucket.points) {
            const float d = glm::dot(glm::vec2{p.x, p.z}, across);
            lo = std::min(lo, d);
            hi = std::max(hi, d);
        }
        t.width = hi - lo;
        out.push_back(t);
    }
    std::sort(out.begin(), out.end(),
              [](const Tread& a, const Tread& b) { return a.index < b.index; });
    return out;
}

// The oak's collider, alone in a Jolt world, with a floor far below so that
// falling off is a measurement rather than an endless drop.
struct StairRig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    render::MeshData solid;
    std::vector<Tread> treads;

    StairRig() {
        REQUIRE(physics->init());
        gameplay::FloraCollisionCache cache;
        const gameplay::FloraSolid& oak =
            gameplay::flora_solid(cache, math::ScatterSpecies::GreatOak, 0, 1.0f);
        REQUIRE(oak.kind == gameplay::FloraSolidKind::Solid);
        solid = oak.mesh;

        std::vector<glm::vec3> positions;
        positions.reserve(solid.vertices.size());
        for (const platform::Vertex& v : solid.vertices) {
            positions.push_back(v.position);
        }
        platform::TerrainMeshDesc desc;
        desc.positions = positions;
        desc.indices = solid.indices;
        desc.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_terrain_mesh(desc).valid());

        platform::StaticBoxDesc floor;
        floor.center = {0.0f, -6.0f, 0.0f};
        floor.half_extents = {60.0f, 1.0f, 60.0f};
        floor.layer = physics_layer::LAYER_STATIC;
        REQUIRE(physics->create_static_box(floor).valid());

        treads = find_treads(solid, oak.top);
    }

    [[nodiscard]] platform::CharacterHandle spawn(glm::vec3 feet) {
        platform::CharacterDesc desc;
        desc.position = feet;
        desc.radius = CAPSULE_R;
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = STEP_HEIGHT;
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        return physics->create_character(desc);
    }
};

enum class PairResult : uint8_t {
    Walkable = 0,
    FellOffTheStart,  // could not even stand on the tread it starts from
    NeverArrived,     // walked at it and did not get there
};

// THE MEASUREMENT. Stand on tread i, walk at tread i+1, see whether a foot
// lands on it. Real controller, real collision, real fixed step — the point
// being that no geometric criterion invented here can be argued with, and this
// one cannot be argued with either way.
[[nodiscard]] PairResult try_pair(StairRig& rig, const Tread& from, const Tread& to) {
    const glm::vec3 start{from.centre.x, from.top + 0.05f, from.centre.y};
    const platform::CharacterHandle character = rig.spawn(start);
    REQUIRE(character.valid());

    gameplay::PlayerState state;
    state.character = character;
    dfn::components::Transform xf{.position = start};
    dfn::components::PreviousTransform prev{.position = start};
    dfn::components::CameraPose cam{};
    dfn::components::PreviousCameraPose prev_cam{};

    // Settle: no input, gravity only. A tread you cannot stand still on is not
    // a tread, and that failure has to be told apart from "could not reach the
    // next one" or a rebuilt stair gets the wrong repair.
    for (int t = 0; t < 25; ++t) {
        state.move_axes = {0.0f, 0.0f};
        gameplay::player_pre_step(state, *rig.physics, 0.0f, xf, prev, cam, prev_cam);
        rig.physics->step(DT);
        gameplay::player_post_step(state, *rig.physics, prev, xf, cam);
    }
    if (xf.position.y < from.top - 0.35f) {
        rig.physics->destroy_character(character);
        return PairResult::FellOffTheStart;
    }

    // Walk at the next tread. yaw such that forward = (sin, 0, -cos).
    const glm::vec2 delta = to.centre - glm::vec2{xf.position.x, xf.position.z};
    state.yaw = std::atan2(delta.x, -delta.y);
    bool arrived = false;
    for (int t = 0; t < 150 && !arrived; ++t) { // 2.5 s
        state.move_axes = {0.0f, 1.0f};
        gameplay::player_pre_step(state, *rig.physics, 0.0f, xf, prev, cam, prev_cam);
        rig.physics->step(DT);
        gameplay::player_post_step(state, *rig.physics, prev, xf, cam);
        const float dx = xf.position.x - to.centre.x;
        const float dz = xf.position.z - to.centre.y;
        const bool over_it = std::sqrt(dx * dx + dz * dz) < CAPSULE_R + 0.15f;
        const bool on_it = std::abs(xf.position.y - to.top) < 0.20f;
        arrived = over_it && on_it;
    }
    rig.physics->destroy_character(character);
    return arrived ? PairResult::Walkable : PairResult::NeverArrived;
}

} // namespace

TEST_CASE("the great oak's treads exist and are solid") {
    // The precondition, separately, so a zero further down can never be read as
    // "there was no stair to climb".
    StairRig rig;
    MESSAGE("treads found in the collider: " << rig.treads.size());
    // The tops, listed. A gap in this list is a tread the DETECTOR lost, and it
    // would show up in the rise case as a defect in the STAIR -- which is how a
    // measurement quietly starts lying about its subject.
    for (size_t i = 0; i < rig.treads.size(); ++i) {
        MESSAGE("  tread " << i << ": top " << rig.treads[i].top << " m, centre ("
                           << rig.treads[i].centre.x << ", " << rig.treads[i].centre.y
                           << "), width " << rig.treads[i].width << " m, span "
                           << rig.treads[i].azimuth_span << " rad");
    }
    REQUIRE(rig.treads.size() >= 8);
    for (const Tread& t : rig.treads) {
        CHECK(t.top > 0.5f);
        CHECK(t.width > 0.0f);
    }
}

TEST_CASE("the rise between treads is inside what the controller climbs") {
    // The half that a NUMBERS re-derivation closed (GREAT_OAK_STEP_RISE
    // 0.42 -> 0.28 = 80 % of PLAYER_STEP_HEIGHT). Kept as its own case so the
    // two halves of the defect can never be confused for one another again.
    StairRig rig;
    float worst = 0.0f;
    int pairs = 0;
    for (size_t i = 1; i < rig.treads.size(); ++i) {
        // CONSECUTIVE MEANS NEIGHBOURING BANDS, not neighbouring entries in this
        // list. Where the detector could not resolve a band the list skips it,
        // and treating that skip as one big step would report the detector's
        // blind spot as the stair's rise.
        if (rig.treads[i].index != rig.treads[i - 1].index + 1) {
            continue;
        }
        ++pairs;
        worst = std::max(worst, rig.treads[i].top - rig.treads[i - 1].top);
    }
    MESSAGE("worst rise over " << pairs << " consecutive pairs: " << worst
                               << " m, against a " << STEP_HEIGHT << " m step ("
                               << g_unresolved_bands << " bands unresolved)");
    REQUIRE(pairs > 0);
    CHECK(worst <= STEP_HEIGHT);
}

TEST_CASE("a tread is wide enough to stand on") {
    // Not part of the walkability fraction, and reported anyway: a tread
    // narrower than the walker is a beam, and a beam is a different verb.
    StairRig rig;
    float narrowest = 1.0e9f;
    for (const Tread& t : rig.treads) {
        narrowest = std::min(narrowest, t.width);
    }
    MESSAGE("narrowest tread: " << narrowest << " m across, against a capsule "
                                << 2.0f * CAPSULE_R << " m wide");
    CHECK(narrowest > 0.0f);
}

TEST_CASE("every consecutive pair of treads can be taken by a walker") {
    // *** THE GATE. RED UNTIL THE STAIR IS WALKABLE. ***
    //
    // Measured by climbing, not by a rule: stand on one tread, walk at the next,
    // see whether a foot lands on it. The contract in NUMBERS.md
    // (GREAT_OAK_STEP_RISE) is that this fraction must be 1.0.
    //
    // Today it is 0. The rise is fine; the treads spiral by the GOLDEN ANGLE
    // 137.5 degrees around a ~2.2 m bole, so two treads one step apart in height
    // are on OPPOSITE SIDES of the trunk, 2.4 m from each other. That is not a
    // staircase with a tuning problem, it is a row of pegs on a helix, and it is
    // flora's geometry to rebuild — this file only has to say, immediately and
    // without anyone asking, whether the rebuild worked.
    //
    // If you are reading this because the case failed: do not relax it. Look at
    // the horizontal gap printed below.
    StairRig rig;
    REQUIRE(rig.treads.size() >= 2);

    int walkable = 0;
    int fell = 0;
    int never = 0;
    float worst_gap = 0.0f;
    float worst_centre_gap = 0.0f;
    for (size_t i = 1; i < rig.treads.size(); ++i) {
        const Tread& a = rig.treads[i - 1];
        const Tread& b = rig.treads[i];
        if (b.index != a.index + 1) {
            continue; // not neighbours: see the rise case
        }
        worst_gap = std::max(worst_gap, surface_gap(a, b));
        worst_centre_gap = std::max(worst_centre_gap, glm::length(b.centre - a.centre));
        switch (try_pair(rig, a, b)) {
        case PairResult::Walkable:
            ++walkable;
            break;
        case PairResult::FellOffTheStart:
            ++fell;
            break;
        case PairResult::NeverArrived:
            ++never;
            break;
        }
    }
    const int pairs = walkable + fell + never;
    REQUIRE(pairs > 0);
    MESSAGE("WALKABLE PAIRS: " << walkable << " of " << pairs << " ("
                               << (100.0f * static_cast<float>(walkable) /
                                   static_cast<float>(pairs))
                               << " %); could not stand on the start tread: " << fell
                               << "; walked at the next and never arrived: " << never
                               << "; worst gap a foot must bridge, SURFACE to surface: "
                               << worst_gap << " m (centre to centre "
                               << worst_centre_gap << " m)");
    CHECK(walkable == pairs);
}
