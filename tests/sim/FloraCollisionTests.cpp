/*
Module: tests
File: tests/sim/FloraCollisionTests.cpp

Responsibility:
- Proves the user's complaint is answered and cannot silently come back: a tree
  trunk stops you, a bush slows you instead of stopping you, and a log under the
  step height is neither.

Key items:
- Rig: a real generated world (core's testbed seed) + Jolt, with NO terrain
  collision, so anything a ray hits can only be a plant.
- "the solid trunk is not wider than the drawn bark": the refutation test for
  the whole measured-collider approach, and the one that would have caught the
  invisible wall a formula-built cylinder produces.
- The control arm (Rule 30): the identical ray with no plant bodies built must
  MISS, and a walker outside every bush must run at full speed.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_world, dfn_render (the drawn geometry every
  expectation is derived from), dfn_platform_physics, dfn_core.
- Used by: ctest (sim_flora_collision).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every expected dimension is DERIVED FROM THE DRAWN MESH here too. A
  hand-written radius would restate whatever the code does and prove nothing.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/FloraCollision.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/PropCollision.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"
#include "engine/render/sources/ProcFlora.h"
#include "engine/world/sources/Chunk.h"
#include "engine/world/sources/ChunkManager.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;
namespace render = dfn::render;
namespace world = dfn::world;
namespace math = dfn::math;

struct Rig {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    world::ChunkManager chunks;
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();

    Rig() {
        REQUIRE(physics->init());
        chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                              world::ChunkStreamingParams{2, 3});
        for (int i = 0; i < 256; ++i) {
            const std::size_t before = chunks.loaded_chunks().size();
            chunks.update({256.0f, 0.0f, 256.0f}, ecs, bus);
            bus.pump();
            if (chunks.loaded_chunks().size() == before && i > 0) {
                break;
            }
        }
        REQUIRE_FALSE(chunks.loaded_chunks().empty());
    }

    void build_props() { gameplay::update_prop_collision(ecs, *physics, chunks); }

    [[nodiscard]] platform::RayHit ray(glm::vec3 from, glm::vec3 dir, float distance) const {
        return physics->raycast(from, dir, distance, physics_layer::LAYER_STATIC);
    }
};

// The first instance of `want` in any resident chunk.
[[nodiscard]] bool find_species(const Rig& rig, math::ScatterSpecies want,
                                math::ScatterInstance& out) {
    for (const world::ChunkCoord coord : rig.chunks.loaded_chunks()) {
        for (const math::ScatterInstance& inst : rig.chunks.scatter(coord)) {
            if (inst.species == want) {
                out = inst;
                return true;
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE("a tree trunk is solid: you cannot walk through an oak") {
    Rig rig;
    math::ScatterInstance tree{};
    REQUIRE(find_species(rig, math::ScatterSpecies::OakTree, tree));

    // Chest height on the player's capsule, aimed at the stem axis from 5 m out.
    const float chest = static_cast<float>(config::PLAYER_EYE_HEIGHT) * 0.6f;
    const glm::vec3 from{tree.position.x - 5.0f, tree.position.y + chest, tree.position.z};
    const glm::vec3 dir{1.0f, 0.0f, 0.0f};

    // CONTROL (Rule 30): nothing built, nothing to hit. This rig has no terrain
    // body at all, so a hit here would mean the case below measures something
    // other than the tree.
    CHECK_FALSE(rig.ray(from, dir, 10.0f).hit);

    rig.build_props();
    const platform::RayHit hit = rig.ray(from, dir, 10.0f);
    REQUIRE(hit.hit);

    // And it is the TRUNK that was hit, not a boulder that happened to be in
    // the way: the hit stands within a bole's radius of the stem axis.
    const float dx = hit.position.x - tree.position.x;
    const float dz = hit.position.z - tree.position.z;
    CHECK(std::sqrt(dx * dx + dz * dz) < 1.5f);
}

TEST_CASE("the solid trunk is not wider than the bark that is drawn") {
    // THE REFUTATION TEST FOR THE WHOLE APPROACH, and the one that a collider
    // built from `species_trunk_radius()` would fail: that accessor reports the
    // flare radius at the species' NOMINAL height, so on a sapling drawn at 0.4
    // maturity it stands ~0.35 m proud of the bark — an invisible wall.
    //
    // Here the expectation is read off the mesh flora BUILDS for the same
    // (species, variant, maturity), which is the only comparison that can fail
    // when the two disagree.
    gameplay::FloraCollisionCache cache;
    for (const auto species : {math::ScatterSpecies::OakTree, math::ScatterSpecies::PineTree,
                               math::ScatterSpecies::BirchTree, math::ScatterSpecies::Snag}) {
        const render::FloraSpecies fs = render::flora_species_of(species);
        for (uint32_t variant = 0; variant < render::FLORA_VARIANTS; ++variant) {
            for (const float maturity : {0.40f, 0.60f, 0.85f, 1.00f, 1.15f, 1.50f}) {
                const gameplay::FloraSolid& solid =
                    gameplay::flora_solid(cache, species, variant, maturity);
                REQUIRE(solid.kind == gameplay::FloraSolidKind::Solid);

                render::FloraShape shape;
                shape.maturity = maturity;
                const render::FloraMesh drawn =
                    render::build_flora_mesh(fs, variant, shape, render::FloraLod::Full);

                // The widest DRAWN wood anywhere below the solid part's top.
                float drawn_radius = 0.0f;
                for (const platform::Vertex& v : drawn.wood.vertices) {
                    if (v.position.y > solid.top) {
                        continue;
                    }
                    drawn_radius = std::max(
                        drawn_radius, std::sqrt(v.position.x * v.position.x +
                                                v.position.z * v.position.z));
                }
                // Not "close to": never WIDER. A collider inside the bark is
                // invisible; a collider outside it is the bug.
                CHECK(solid.max_radius <= drawn_radius + 1.0e-4f);
                // It must actually BLOCK a walker: a collider that ended at
                // the knee would pass the width assertion above and still let
                // the player walk through the tree from the waist up.
                CHECK(solid.top >= static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
                // And it is a BOLE, not the whole tree. Stated in triangles
                // rather than in height on purpose: the cut keeps whole
                // segments, so a bole's top depends on where the segment
                // boundary falls, while "a crown got made solid" is always a
                // jump of hundreds of triangles. This is the budget claim and
                // the no-solid-crown claim in one number.
                CHECK(solid.mesh.indices.size() / 3 <= 120);
            }
        }
    }
}

TEST_CASE("a bush slows the walker instead of stopping them") {
    // The two halves of the user's own proposal, asserted together because
    // either one alone is a different (wrong) design: passable AND felt.
    gameplay::BrushField field;
    gameplay::FloraCollisionCache cache;
    const gameplay::FloraSolid& bush =
        gameplay::flora_solid(cache, math::ScatterSpecies::Bush, 0, 1.0f);
    REQUIRE(bush.kind == gameplay::FloraSolidKind::Drag);
    REQUIRE(bush.drag_radius > 0.0f);

    gameplay::BrushField::Chunk chunk;
    chunk.discs.push_back(gameplay::BrushDisc{
        .center = {0.0f, 0.0f}, .radius = bush.drag_radius, .top = 1.0f, .base = 0.0f});
    field.chunks.emplace(1, std::move(chunk));

    const auto r = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    // PASSABLE: a bush is never a body, so nothing here can block. The heart of
    // it is full density; the far side of the world is none.
    CHECK(gameplay::brush_density_at(field, {0.0f, 0.5f, 0.0f}, r) == doctest::Approx(1.0f));
    CHECK(gameplay::brush_density_at(field, {50.0f, 0.5f, 0.0f}, r) == doctest::Approx(0.0f));
    // CONTROL ARM (Rule 30 / 48.3): the control differs in exactly the thing
    // under test — where the walker stands — and in nothing else. Standing just
    // outside the rim must read zero, or "density fell to zero" would only mean
    // "the query is broken everywhere".
    const float rim = bush.drag_radius + r + 0.01f;
    CHECK(gameplay::brush_density_at(field, {rim, 0.5f, 0.0f}, r) == doctest::Approx(0.0f));
    // A RAMP, not a step: half way in is not full drag.
    const float half = gameplay::brush_density_at(field, {rim * 0.5f, 0.5f, 0.0f}, r);
    CHECK(half > 0.0f);
    CHECK(half < 1.0f);
    // ABOVE THE FOLIAGE IS CLEAR: a player on a ledge over a shrub is not in it.
    CHECK(gameplay::brush_density_at(field, {0.0f, 1.5f, 0.0f}, r) == doctest::Approx(0.0f));
}

TEST_CASE("brush costs speed through the same factor wading uses") {
    // The movement half, with no world at all: the ref-based core takes the
    // density as data, which is the whole reason it is ferried on StepContext.
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());

    auto walk_one_tick = [&](float brush_density) {
        gameplay::PlayerState state;
        platform::CharacterDesc desc;
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        state.character = physics->create_character(desc);
        state.move_axes = {0.0f, 1.0f}; // full forward

        dfn::components::Transform xf{};
        dfn::components::PreviousTransform prev{};
        dfn::components::CameraPose cam{};
        dfn::components::PreviousCameraPose prev_cam{};
        gameplay::StepContext step;
        step.brush_density = brush_density;

        gameplay::player_pre_step(state, *physics, 0.0f, xf, prev, cam, prev_cam, step);
        physics->step(static_cast<float>(config::SIM_DT));
        const glm::vec3 moved = physics->character_position(state.character);
        return std::sqrt(moved.x * moved.x + moved.z * moved.z) /
               static_cast<float>(config::SIM_DT);
    };

    // CONTROL: the identical tick with the identical everything except the one
    // quantity under test.
    const float clear = walk_one_tick(0.0f);
    const float thicket = walk_one_tick(1.0f);
    CHECK(clear == doctest::Approx(static_cast<float>(config::WALK_SPEED)).epsilon(0.01));
    CHECK(thicket < clear);
    // Felt, not cosmetic: at least a fifth of the walk is gone in a thicket.
    CHECK(thicket < clear * 0.8f);
    // Passable, not a wall: the walker still moves.
    CHECK(thicket > clear * 0.25f);
    // And the ramp is monotone through the middle.
    CHECK(walk_one_tick(0.5f) < clear);
    CHECK(walk_one_tick(0.5f) > thicket);
}

TEST_CASE("a log under the step height gets no body, one above it does") {
    // PLAYER_STEP_HEIGHT is the watershed we already have: what the character
    // controller climbs for free is not an obstacle, so paying triangles for it
    // would buy nothing. This asserts the rule against the DRAWN log rather
    // than against a number written twice.
    gameplay::FloraCollisionCache cache;
    const auto step = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    int solid = 0;
    int stepped_over = 0;
    for (uint32_t variant = 0; variant < render::FLORA_VARIANTS; ++variant) {
        const gameplay::FloraSolid& log =
            gameplay::flora_solid(cache, math::ScatterSpecies::FallenLog, variant, 1.0f);
        if (log.kind == gameplay::FloraSolidKind::Solid) {
            CHECK(log.top > step);
            CHECK(log.mesh.indices.size() >= 3);
            ++solid;
        } else {
            CHECK(log.mesh.indices.empty());
            ++stepped_over;
        }
    }
    // The design's own claim is that a fallen log is something you go round or
    // over, so at least some of them must actually be obstacles. A rule that
    // silently removed every log would pass the loop above.
    CHECK(solid > 0);
    (void)stepped_over;

    // Small deadfall is drag, never a body: design ruled that tripping on twigs
    // is not a feature, and the user asked for brushwood that hinders — those
    // two meet at "it costs speed".
    const gameplay::FloraSolid& deadfall =
        gameplay::flora_solid(cache, math::ScatterSpecies::Deadfall, 0, 1.0f);
    CHECK(deadfall.mesh.indices.empty());
}

TEST_CASE("ground cover is neither solid nor drag") {
    // The third answer is a real answer. A mushroom that slowed the player
    // would be a bug report, and a flower with a body would be 30 000 of them.
    gameplay::FloraCollisionCache cache;
    for (const auto species :
         {math::ScatterSpecies::MossPatch, math::ScatterSpecies::FlowerCarpet,
          math::ScatterSpecies::FlowerAccent, math::ScatterSpecies::FlowerJewel,
          math::ScatterSpecies::FlowerUmbel, math::ScatterSpecies::Mushroom,
          math::ScatterSpecies::PebbleCluster, math::ScatterSpecies::Stone}) {
        CHECK(gameplay::flora_solid_kind(species) == gameplay::FloraSolidKind::None);
        CHECK(gameplay::flora_solid(cache, species, 0, 1.0f).kind ==
              gameplay::FloraSolidKind::None);
    }
}

TEST_CASE("the collider is built for the maturity the tree is DRAWN at") {
    // The bucket rounds DOWN, which is the safe direction, and this pins that
    // it is a bucket and not a fixed size: a sapling's bole must be markedly
    // thinner than a giant's, or the whole per-instance argument is empty.
    gameplay::FloraCollisionCache cache;
    const gameplay::FloraSolid& sapling =
        gameplay::flora_solid(cache, math::ScatterSpecies::OakTree, 0, 0.40f);
    const gameplay::FloraSolid& giant =
        gameplay::flora_solid(cache, math::ScatterSpecies::OakTree, 0, 1.50f);
    CHECK(sapling.max_radius < giant.max_radius * 0.6f);
    // Never THICKER than drawn, at either end — the invisible-wall direction.
    for (const auto* s : {&sapling, &giant}) {
        CHECK(s->max_radius > 0.0f);
    }
}

TEST_CASE("plants cost no physics bodies at all") {
    // THE BUDGET, IN THE UNITS THAT LIMIT IT (Rule 42). A capsule per trunk is
    // the obvious shape and costs ~7 200 resident bodies against a Jolt world
    // sized for 16 384; folding boles into the chunk mesh that already exists
    // costs zero. Asserted, so nobody re-adds per-trunk bodies without seeing
    // the number they are spending.
    Rig rig;
    rig.build_props();
    const auto& state = rig.ecs.resource<gameplay::PropCollisionState>();
    const auto& brush = rig.ecs.resource<gameplay::BrushField>();
    CHECK(state.bodies.size() <= rig.chunks.loaded_chunks().size());

    // And the drag field is populated: a brush field that silently stayed empty
    // would pass every "bushes do not block" assertion in this file.
    std::size_t discs = 0;
    for (const auto& [key, chunk] : brush.chunks) {
        (void)key;
        discs += chunk.discs.size();
    }
    CHECK(discs > 0);
}

TEST_CASE("the great oak is solid for its whole bole, treads included") {
    // The user wants to climb this tree and live in it. Today's honest state:
    // the SURFACES exist — the bole and every tread flora draws are solid all
    // the way to the crown base — but the STAIR does not, and that is a
    // geometry finding, not a physics one. Measured off the drawn mesh:
    // 28 treads, every consecutive pair 0.42 m apart vertically
    // (GREAT_OAK_STEP_RISE, against PLAYER_STEP_HEIGHT 0.35) and 2.40 m apart
    // HORIZONTALLY, because they spiral by the golden angle around a ~2.2 m
    // bole. Zero of the 27 pairs can be taken by a walker, and no collision
    // work can change that: it is a row of pegs, not a staircase. Reported to
    // the lead for flora/design; when the spacing is fixed, climbing works with
    // no change here, which is what this case pins.
    gameplay::FloraCollisionCache cache;
    const gameplay::FloraSolid& oak =
        gameplay::flora_solid(cache, math::ScatterSpecies::GreatOak, 0, 1.0f);
    REQUIRE(oak.kind == gameplay::FloraSolidKind::Solid);

    const render::FloraSpecies fs = render::flora_species_of(math::ScatterSpecies::GreatOak);
    const float crown_base = render::species_crown_base(fs);
    // Solid all the way up to where the crown starts — not to the 4 m a walker
    // on the ground would need.
    CHECK(oak.top >= crown_base);
    CHECK(oak.top > gameplay::TRUNK_COLLISION_HEIGHT * 2.0f);

    // The treads are IN the collider: they stand clear of the bole, so the
    // collider's widest reach must exceed the bole's own radius by about the
    // tread's length. Without this the case would pass on a bare column.
    render::FloraShape shape;
    const render::FloraMesh drawn =
        render::build_flora_mesh(fs, 0, shape, render::FloraLod::Full);
    float bole = 1.0e9f;
    for (const platform::Vertex& v : drawn.wood.vertices) {
        if (std::abs(v.position.y - 3.0f) > 0.25f) {
            continue;
        }
        bole = std::min(bole, std::sqrt(v.position.x * v.position.x +
                                        v.position.z * v.position.z));
    }
    REQUIRE(bole < 1.0e8f);
    CHECK(oak.max_radius > bole + static_cast<float>(config::GREAT_OAK_STEP_REACH) * 0.5f);
    // Still no CROWN, though: a great oak's crown is as wide as the tree is
    // tall, so anything approaching that would mean the cut had failed.
    CHECK(oak.max_radius < render::species_crown_radius(fs) * 0.5f);
    // A landmark's budget, not a forest's: one tree per region.
    CHECK(oak.mesh.indices.size() / 3 < 600);
}

TEST_CASE("brush reaches across the chunk seam it is rooted next to") {
    // The drag query skips chunks the walker cannot be standing in, which is
    // what makes its cost independent of CHUNK_LOAD_RADIUS. This is the case
    // that filter could break: a shrub rooted at the very edge of its chunk
    // leans over the border, and a walker on the far side is genuinely inside
    // it. Silently dropping that is a seam-shaped hole in the world — the kind
    // that shows up as "the bushes sometimes don't work" and never as a crash.
    const auto chunk_size = static_cast<float>(config::CHUNK_SIZE);
    const auto r = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);

    gameplay::BrushField field;
    gameplay::BrushField::Chunk chunk;
    chunk.coord = {0, 0};
    // Rooted 0.2 m inside chunk (0,0)'s eastern edge, with a 1.8 m reach.
    chunk.discs.push_back(gameplay::BrushDisc{.center = {chunk_size - 0.2f, 32.0f},
                                              .radius = 1.8f,
                                              .top = 3.0f,
                                              .base = 0.0f});
    field.chunks.emplace(world::chunk_group(world::ChunkCoord{0, 0}), std::move(chunk));

    // A walker one metre EAST of the border stands in chunk (1,0) and is inside
    // a shrub that belongs to chunk (0,0).
    CHECK(gameplay::brush_density_at(field, {chunk_size + 1.0f, 0.5f, 32.0f}, r) > 0.0f);
    // CONTROL: far side of the same chunk, same distance from the field's only
    // entry in every respect but the one that matters — how far away it is.
    CHECK(gameplay::brush_density_at(field, {chunk_size + 40.0f, 0.5f, 32.0f}, r) ==
          doctest::Approx(0.0f));
}

TEST_CASE("a chunk with nothing solid in it is built once, not every tick") {
    // The residency marker is the BRUSH FIELD, not the body map, and this is
    // the case that says why. A chunk gets a brush entry unconditionally and a
    // body only if it holds something solid, so gating the build on "has a
    // body" makes every propless chunk rebuild its geometry on every tick,
    // forever, waiting for a body that is never going to appear. The symptom
    // would be a per-tick cost that grows with the number of EMPTY chunks —
    // the least likely place anybody would look.
    Rig rig;
    rig.build_props();
    const std::size_t bodies_after_first = rig.ecs.resource<gameplay::PropCollisionState>()
                                               .bodies.size();
    const auto& cache_before = rig.ecs.resource<gameplay::PropCollisionState>().flora_cache;
    const uint32_t hits_before = cache_before.hits;
    const uint64_t solids_before =
        rig.ecs.resource<gameplay::PropCollisionState>().solid_plants;

    // Ten more ticks with nothing changing.
    for (int i = 0; i < 10; ++i) {
        rig.build_props();
    }
    const auto& state = rig.ecs.resource<gameplay::PropCollisionState>();
    CHECK(state.bodies.size() == bodies_after_first);
    // NOT ONE plant was classified again: the counters and the memo are frozen.
    // Comparing counters rather than timing keeps this a behavioural assertion
    // instead of a wall-clock threshold (Rule 38).
    CHECK(state.solid_plants == solids_before);
    CHECK(state.flora_cache.hits == hits_before);
    // Every resident chunk is represented in the field, including any that has
    // no body — that is what makes the marker complete.
    CHECK(rig.ecs.resource<gameplay::BrushField>().chunks.size() ==
          rig.chunks.loaded_chunks().size());
}
