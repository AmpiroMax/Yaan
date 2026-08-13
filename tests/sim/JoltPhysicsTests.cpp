/*
Created: 09:08:2026 - 00:45:08
Last updated: 13:08:2026 - 18:30:00
Module: tests
File: tests/sim/JoltPhysicsTests.cpp

Responsibility:
- Jolt backend integration tests: terrain body from a HeightFieldView (the
  frozen uint16 decode), character falls to ground and stands, walks on
  terrain, slides against walls, masked raycasts resolve user_data.

Key items:
- make_flat_view(): a synthetic 129x129 HeightFieldView at a known height.
- Doctest cases through IPhysics only (no Jolt types — Rule 1 holds in tests too).

Dependencies:
- Uses: doctest, dfn_platform_physics (jolt factory), dfn_physics, constants,
  dfn_render (the diagonal case reads render's REAL mesh, never a copy of its
  triangulation — the defect it guards was two zones holding two copies).
- Used by: ctest (sim_jolt_physics).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never include Jolt headers here; the public contract must be enough.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial Jolt backend suite.
- 09:08:2026 - 01:02:15: Added the real-ChunkManager heightfield smoke test
  (core's suggestion: catches decode/contract drift between zones early).
- 09:08:2026 - 15:08:24: Added the zero-mask rejection case per body kind
  (regression guard for the fall-through-the-world bug, app fix 37f1e1c).
- 09:08:2026 - 18:56:32: Added the crosshair-targeting case: a real ray against
  a real interactable body, proving user_data -> EntityId and the reach limit.
- 10:08:2026 - 21:24:32: THE DRAWN GROUND AND THE SOLID GROUND. Added the
  cross-zone diagonal case: render's mesh and Jolt's collision mesh split every
  quad on the same diagonal, measured on REAL generated terrain. The pre-existing
  terrain coverage used a flat chunk, on which the two splits are identically
  equal — so it could not have failed. That flat-chunk blindness is now its own
  executable case rather than a remark.
- 13:08:2026 - 18:30:00: The spawn desc uses designated initialisers; see the
  note in InteractionTests.cpp on why a positional aggregate is a trap.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/math/sources/HeightField.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/render/sources/TerrainMesher.h"
#include "engine/world/sources/ChunkManager.h"

namespace {

namespace config = dfn::config;
namespace physics_layer = dfn::physics;
namespace platform = dfn::platform;

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float GROUND_Y = 10.0f;
constexpr uint64_t TERRAIN_USER_DATA = 0xC0FFEEull;

// Flat 129x129 chunk at GROUND_Y meters: raw = 0 everywhere, offset carries
// the height (the frozen formula: height = offset + raw * scale).
struct FlatChunk {
    std::vector<uint16_t> raw;
    dfn::math::HeightFieldView view;

    FlatChunk() {
        const auto resolution = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
        raw.assign(static_cast<size_t>(resolution) * resolution, 0);
        view.chunk_coord = {0, 0};
        view.origin = {0.0f, 0.0f};
        view.resolution = resolution;
        view.step = static_cast<float>(config::HEIGHTMAP_STEP);
        view.heights = raw;
        view.height_scale = 0.0f; // flat: every sample decodes to the offset
        view.height_offset = GROUND_Y;
    }
};

struct JoltRig {
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    FlatChunk chunk;
    std::vector<float> scratch;
    platform::PhysicsBodyHandle terrain;
    platform::CharacterHandle character;
    float vertical_velocity = 0.0f;

    JoltRig() {
        REQUIRE(physics->init());
        terrain = physics_layer::create_terrain_body(*physics, chunk.view,
                                                     TERRAIN_USER_DATA, scratch);
        REQUIRE(terrain.valid());

        platform::CharacterDesc desc;
        desc.position = {128.0f, GROUND_Y + 2.0f, 128.0f}; // chunk center, 2 m up
        desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
        desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
        desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
        desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
        desc.layer = physics_layer::LAYER_CHARACTER;
        desc.collides_with = physics_layer::LAYER_STATIC;
        desc.user_data = 0xAB1EEull;
        character = physics->create_character(desc);
        REQUIRE(character.valid());
    }

    // One fixed tick with gravity, optionally with a horizontal intent.
    void tick(const glm::vec3& horizontal = {0.0f, 0.0f, 0.0f}) {
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        physics->move_character(character,
                                horizontal + glm::vec3{0.0f, vertical_velocity * DT, 0.0f});
        physics->step(DT);
        if (physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }
    }
};

TEST_CASE("character falls onto heightfield terrain and stands on it") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) { // 3 seconds is plenty for a 2 m drop
        rig.tick();
    }
    CHECK(rig.physics->character_grounded(rig.character));
    CHECK(rig.physics->character_position(rig.character).y ==
          doctest::Approx(GROUND_Y).epsilon(0.02));
}

TEST_CASE("grounded character walks without sinking or launching") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    REQUIRE(rig.physics->character_grounded(rig.character));

    const glm::vec3 start = rig.physics->character_position(rig.character);
    const float step = static_cast<float>(config::WALK_SPEED) * DT;
    for (int i = 0; i < 60; ++i) { // one second east
        rig.tick({step, 0.0f, 0.0f});
    }
    const glm::vec3 end = rig.physics->character_position(rig.character);
    CHECK(end.x - start.x ==
          doctest::Approx(static_cast<float>(config::WALK_SPEED)).epsilon(0.05));
    CHECK(end.y == doctest::Approx(GROUND_Y).epsilon(0.02));
    CHECK(rig.physics->character_grounded(rig.character));
}

TEST_CASE("walls block and slide the character") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    const glm::vec3 start = rig.physics->character_position(rig.character);

    platform::StaticBoxDesc wall;
    wall.center = {start.x + 1.5f, GROUND_Y + 2.0f, start.z};
    wall.half_extents = {0.25f, 2.0f, 10.0f}; // wall across +X, long in Z
    wall.layer = physics_layer::LAYER_STATIC;
    wall.user_data = 0xBA11ull;
    REQUIRE(rig.physics->create_static_box(wall).valid());

    const float step = static_cast<float>(config::WALK_SPEED) * DT;
    for (int i = 0; i < 120; ++i) { // two seconds straight into the wall
        rig.tick({step, 0.0f, 0.0f});
    }
    const glm::vec3 end = rig.physics->character_position(rig.character);
    // Blocked before the wall face (allow the capsule radius + padding).
    CHECK(end.x < wall.center.x);
    CHECK(end.x - start.x < 1.5f);
    // And walking diagonally along the wall still slides in Z.
    for (int i = 0; i < 60; ++i) {
        rig.tick({step, 0.0f, step});
    }
    CHECK(rig.physics->character_position(rig.character).z > end.z + 0.5f);
}

TEST_CASE("raycast hits terrain with user_data, respects the mask") {
    JoltRig rig;
    const glm::vec3 origin{64.0f, GROUND_Y + 20.0f, 64.0f};
    const glm::vec3 down{0.0f, -1.0f, 0.0f};

    const auto hit =
        rig.physics->raycast(origin, down, 100.0f, physics_layer::LAYER_STATIC);
    REQUIRE(hit.hit);
    CHECK(hit.position.y == doctest::Approx(GROUND_Y).epsilon(0.01));
    CHECK(hit.distance == doctest::Approx(20.0f).epsilon(0.01));
    CHECK(hit.normal.y == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(hit.user_data == TERRAIN_USER_DATA);

    // A mask excluding static geometry must miss the terrain.
    const auto miss =
        rig.physics->raycast(origin, down, 100.0f, physics_layer::LAYER_CHARACTER);
    CHECK_FALSE(miss.hit);
}

TEST_CASE("raycast finds the character's body via its mask and user_data") {
    JoltRig rig;
    for (int i = 0; i < 180; ++i) {
        rig.tick();
    }
    const glm::vec3 position = rig.physics->character_position(rig.character);
    const glm::vec3 origin = position + glm::vec3{0.0f, 0.9f, -3.0f};
    const auto hit = rig.physics->raycast(origin, {0.0f, 0.0f, 1.0f}, 10.0f,
                                          physics_layer::LAYER_CHARACTER);
    REQUIRE(hit.hit);
    CHECK(hit.user_data == 0xAB1EEull);
}

TEST_CASE("real generated heightfield: terrain body + raycast agree with the view") {
    // Cross-zone smoke test (core's suggestion): a genuine ChunkManager view,
    // not a synthetic one — catches decode-formula drift between zones.
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    dfn::world::ChunkManager chunks;
    chunks.open_generated({.seed = 7, .min_chunk = {-1, -1}, .max_chunk = {1, 1}},
                          {.load_radius = 1, .unload_radius = 2});
    chunks.update({0.0f, 0.0f, 0.0f}, ecs, bus);

    const auto view = chunks.heightfield({0, 0});
    REQUIRE(view.has_value());
    REQUIRE(view->resolution ==
            static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION));

    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    std::vector<float> scratch;
    const auto terrain =
        dfn::physics::create_terrain_body(*physics, *view, 0xC1A55ull, scratch);
    REQUIRE(terrain.valid());

    // Ray down over the chunk center must hit exactly the decoded height.
    const uint32_t center = view->resolution / 2;
    const float expected = view->height_at(center, center);
    const glm::vec3 over{view->origin.x + static_cast<float>(center) * view->step,
                         expected + 50.0f,
                         view->origin.y + static_cast<float>(center) * view->step};
    const auto hit = physics->raycast(over, {0.0f, -1.0f, 0.0f}, 100.0f,
                                      physics_layer::LAYER_STATIC);
    REQUIRE(hit.hit);
    CHECK(hit.position.y == doctest::Approx(expected).epsilon(0.01));
    CHECK(hit.user_data == 0xC1A55ull);
    physics->shutdown();
}

TEST_CASE("zero-mask bodies are rejected per body kind") {
    // Regression guard: a hand-filled TerrainDesc left `layer` at 0, the body
    // collided with nothing, and the player fell through the world. A collider
    // no mask can select is never intentional (IPhysics.h contract).
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());

    FlatChunk chunk;
    std::vector<float> heights(chunk.raw.size(), GROUND_Y);
    platform::TerrainDesc terrain;
    terrain.sample_count_x = chunk.view.resolution;
    terrain.sample_count_z = chunk.view.resolution;
    terrain.sample_spacing = chunk.view.step;
    terrain.heights = heights;
    terrain.user_data = 1; // layer left at 0 — the exact bug
    CHECK_FALSE(physics->create_terrain(terrain).valid());
    terrain.layer = physics_layer::LAYER_STATIC; // same desc, now well-formed
    CHECK(physics->create_terrain(terrain).valid());

    platform::StaticBoxDesc box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    CHECK_FALSE(physics->create_static_box(box).valid()); // layer 0
    box.layer = physics_layer::LAYER_STATIC;
    CHECK(physics->create_static_box(box).valid());

    platform::CharacterDesc character;
    character.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    character.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    character.collides_with = physics_layer::LAYER_STATIC;
    CHECK_FALSE(physics->create_character(character).valid()); // layer 0
    character.layer = physics_layer::LAYER_CHARACTER;
    character.collides_with = 0; // would walk through the world
    CHECK_FALSE(physics->create_character(character).valid());
    character.collides_with = physics_layer::LAYER_STATIC;
    CHECK(physics->create_character(character).valid());
    physics->shutdown();
}

TEST_CASE("LOOK: the crosshair ray finds an interactable and resolves its entity") {
    // The half of LOOK that null physics cannot prove: a real ray, a real
    // prop body, and user_data surviving the round trip back to an EntityId.
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;
    world.add_resource(dfn::components::HoverTarget{});

    const auto player = world.spawn();
    world.add(player, dfn::gameplay::PlayerState{});
    // Eye at origin, yaw 0 => looking down -Z (PlayerMovement's convention).
    world.add(player, dfn::components::CameraPose{{0.0f, 1.7f, 0.0f}, 0.0f, 0.0f});

    // A chest 2 m ahead, inside INTERACT_DISTANCE.
    const auto chest = dfn::gameplay::spawn_interactable(
        world, *physics,
        {.kind = dfn::gameplay::InteractableKind::Openable,
         .position = {0.0f, 1.7f, -2.0f},
         .half_extents = {0.5f, 0.35f, 0.35f},
         .prompt_key = "interact.open"});

    dfn::gameplay::update_hover(world, *physics);
    const auto& hover = world.resource<dfn::components::HoverTarget>();
    CHECK(hover.entity == chest); // user_data -> EntityId survived the backend
    CHECK(hover.verb == static_cast<uint8_t>(dfn::gameplay::InteractionVerb::Open));
    CHECK(hover.prompt_key ==
          dfn::serialization::fnv1a64("interact.open"));

    // Out of reach: same prop pushed past INTERACT_DISTANCE is not offered.
    auto far_physics = platform::create_jolt_physics();
    REQUIRE(far_physics->init());
    dfn::ecs::World far_world;
    far_world.add_resource(dfn::components::HoverTarget{});
    const auto far_player = far_world.spawn();
    far_world.add(far_player, dfn::gameplay::PlayerState{});
    far_world.add(far_player,
                  dfn::components::CameraPose{{0.0f, 1.7f, 0.0f}, 0.0f, 0.0f});
    (void)dfn::gameplay::spawn_interactable(
        far_world, *far_physics,
        {dfn::gameplay::InteractableKind::Openable,
         {0.0f, 1.7f, -(static_cast<float>(config::INTERACT_DISTANCE) + 2.0f)},
         {0.5f, 0.35f, 0.35f}, "interact.open"});
    dfn::gameplay::update_hover(far_world, *far_physics);
    CHECK(far_world.resource<dfn::components::HoverTarget>().entity.is_null());

    physics->shutdown();
    far_physics->shutdown();
}

TEST_CASE("teleport relocates without residual velocity") {
    JoltRig rig;
    rig.physics->teleport_character(rig.character, {10.0f, GROUND_Y, 10.0f});
    rig.tick();
    const glm::vec3 position = rig.physics->character_position(rig.character);
    CHECK(position.x == doctest::Approx(10.0f).epsilon(0.01));
    CHECK(position.z == doctest::Approx(10.0f).epsilon(0.01));
}

} // namespace

// --- The drawn ground and the solid ground are ONE surface ------------------
//
// Rule 35 in test form. Which diagonal a quad is split on is arbitrary; that
// render and physics pick THE SAME one is a number two zones must agree about,
// and until this case existed the agreement lived in a comment in JoltPhysics.h
// saying the two "cannot disagree" while they in fact disagreed on every quad.
//
// The reason it survived: the only case covering terrain collision used a FLAT
// chunk, and on a flat chunk the two diagonals are identically equal. That is
// the vantage-that-cannot-fail in test form (Rule 27 + 30a) — an instrument
// pointed at the one input on which the defect is invisible.
//
// So this case measures against a NON-FLAT field, and it does not hardcode
// render's diagonal: it reads the triangles render actually emits and asks
// whether a downward ray hits the collision mesh at the same height.

namespace {

// Height where a vertical line through (px, pz) meets the triangle (a, b, c),
// or nullopt if the line misses it. Barycentric in the XZ plane.
[[nodiscard]] std::optional<float> vertical_hit(const glm::vec3& a, const glm::vec3& b,
                                                const glm::vec3& c, float px, float pz) {
    const float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
    if (std::abs(d) < 1e-12f) {
        return std::nullopt; // degenerate in plan view
    }
    const float w0 = ((b.z - c.z) * (px - c.x) + (c.x - b.x) * (pz - c.z)) / d;
    const float w1 = ((c.z - a.z) * (px - c.x) + (a.x - c.x) * (pz - c.z)) / d;
    const float w2 = 1.0f - w0 - w1;
    constexpr float EDGE = -1e-4f; // a point on a shared edge belongs to both
    if (w0 < EDGE || w1 < EDGE || w2 < EDGE) {
        return std::nullopt;
    }
    return w0 * a.y + w1 * b.y + w2 * c.y;
}

// Surface height of one grid cell at (px, pz), for a given choice of diagonal.
// `flip` swaps to the OTHER diagonal, which is how this case carries its own
// counterfactual: no code has to be reverted to see the instrument discriminate.
[[nodiscard]] std::optional<float> cell_height(const dfn::math::HeightFieldView& field,
                                               const std::vector<float>& heights,
                                               uint32_t cx, uint32_t cz, float px,
                                               float pz, bool flip) {
    const auto at = [&](uint32_t x, uint32_t z) {
        return glm::vec3{field.origin.x + static_cast<float>(x) * field.step,
                         heights[static_cast<size_t>(z) * field.resolution + x],
                         field.origin.y + static_cast<float>(z) * field.step};
    };
    const glm::vec3 v00 = at(cx, cz);
    const glm::vec3 v10 = at(cx + 1, cz);
    const glm::vec3 v01 = at(cx, cz + 1);
    const glm::vec3 v11 = at(cx + 1, cz + 1);
    if (!flip) { // diagonal v00-v11
        if (const auto h = vertical_hit(v00, v11, v10, px, pz)) {
            return h;
        }
        return vertical_hit(v00, v01, v11, px, pz);
    }
    if (const auto h = vertical_hit(v00, v01, v10, px, pz)) { // diagonal v01-v10
        return h;
    }
    return vertical_hit(v10, v01, v11, px, pz);
}

// Which diagonal does the RENDER mesher actually use? Read it out of the mesh
// rather than asserting a literal — the whole defect was two zones holding
// independent copies of this choice (Rule 35), and a test with a third copy
// would be the same mistake wearing a lab coat.
[[nodiscard]] bool render_splits_on_v00_v11(const dfn::render::TerrainMeshData& mesh,
                                            uint32_t resolution) {
    const uint32_t cells = resolution - 1;
    REQUIRE(mesh.indices.size() == static_cast<size_t>(cells) * cells * 6);
    const uint32_t i00 = 0;
    const uint32_t i10 = 1;
    const uint32_t i01 = resolution;
    const uint32_t i11 = resolution + 1;
    // The diagonal is the vertex pair shared by both triangles of cell (0,0).
    std::array<uint32_t, 6> cell{};
    std::copy_n(mesh.indices.begin(), 6, cell.begin());
    const auto uses = [&](uint32_t a, uint32_t b) {
        int first = 0;
        int second = 0;
        for (int t = 0; t < 2; ++t) {
            bool has_a = false;
            bool has_b = false;
            for (int k = 0; k < 3; ++k) {
                has_a = has_a || cell[t * 3 + k] == a;
                has_b = has_b || cell[t * 3 + k] == b;
            }
            (t == 0 ? first : second) = (has_a && has_b) ? 1 : 0;
        }
        return first == 1 && second == 1;
    };
    const bool main_diagonal = uses(i00, i11);
    REQUIRE(main_diagonal != uses(i01, i10)); // exactly one of the two, always
    return main_diagonal;
}

} // namespace


namespace {

/// One measurement of the two surfaces against each other, over the cell
/// CENTRES of `field` — the point of a quad furthest from both diagonals and so
/// where two triangulations differ most.
struct DiagonalAgreement {
    float worst_agreement_m = 0.0f; ///< drawn surface vs the solid one.
    float worst_flipped_m = 0.0f;   ///< the same against the OTHER diagonal.
    int samples = 0;
};

[[nodiscard]] DiagonalAgreement measure_agreement(const dfn::math::HeightFieldView& field,
                                                  uint32_t stride) {
    const uint32_t resolution = field.resolution;
    std::vector<float> heights(static_cast<size_t>(resolution) * resolution, 0.0f);
    float max_h = -1e30f;
    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const float h = field.height_at(x, z);
            heights[static_cast<size_t>(z) * resolution + x] = h;
            max_h = std::max(max_h, h);
        }
    }

    // Which diagonal does RENDER actually use? Read it out of render's mesh
    // rather than writing a literal here: the defect this case guards was two
    // zones holding two copies of that choice, and a test holding a third copy
    // would be the same mistake wearing a lab coat (Rule 35).
    const dfn::render::TerrainMeshData mesh = dfn::render::build_terrain_mesh(field);
    const bool render_main_diagonal = render_splits_on_v00_v11(mesh, resolution);

    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    std::vector<float> scratch;
    const auto body =
        dfn::physics::create_terrain_body(*physics, field, 0xD1A6ull, scratch);
    REQUIRE(body.valid());

    DiagonalAgreement out;
    for (uint32_t cz = 1; cz + 2 < resolution; cz += stride) {
        for (uint32_t cx = 1; cx + 2 < resolution; cx += stride) {
            const float px = field.origin.x + (static_cast<float>(cx) + 0.5f) * field.step;
            const float pz = field.origin.y + (static_cast<float>(cz) + 0.5f) * field.step;
            const auto drawn =
                cell_height(field, heights, cx, cz, px, pz, /*flip=*/!render_main_diagonal);
            const auto other =
                cell_height(field, heights, cx, cz, px, pz, /*flip=*/render_main_diagonal);
            REQUIRE(drawn.has_value());
            REQUIRE(other.has_value());

            const auto hit = physics->raycast({px, max_h + 100.0f, pz}, {0.0f, -1.0f, 0.0f},
                                              400.0f, physics_layer::LAYER_STATIC);
            REQUIRE(hit.hit);
            out.worst_agreement_m =
                std::max(out.worst_agreement_m, std::abs(hit.position.y - *drawn));
            out.worst_flipped_m =
                std::max(out.worst_flipped_m, std::abs(hit.position.y - *other));
            ++out.samples;
        }
    }
    physics->shutdown();
    return out;
}

// A field engineered so that every quad's corners are maximally non-coplanar:
// low, high, low, high in checkerboard. The centre of every cell sits on one
// diagonal at CHECKER_RELIEF_M and on the other at 0, so the two triangulations
// disagree by exactly CHECKER_RELIEF_M everywhere. This is the instrument's
// range check, not the acceptance — the acceptance runs on real terrain.
constexpr float CHECKER_RELIEF_M = 20.0f;

struct CheckerChunk {
    std::vector<uint16_t> raw;
    dfn::math::HeightFieldView view;

    CheckerChunk() {
        const auto resolution = static_cast<uint32_t>(config::HEIGHTMAP_RESOLUTION);
        raw.assign(static_cast<size_t>(resolution) * resolution, 0);
        for (uint32_t z = 0; z < resolution; ++z) {
            for (uint32_t x = 0; x < resolution; ++x) {
                raw[static_cast<size_t>(z) * resolution + x] = ((x + z) % 2 == 0) ? 0 : 1000;
            }
        }
        view.chunk_coord = {0, 0};
        view.origin = {0.0f, 0.0f};
        view.resolution = resolution;
        view.step = static_cast<float>(config::HEIGHTMAP_STEP);
        view.heights = raw;
        view.height_scale = CHECKER_RELIEF_M / 1000.0f;
        view.height_offset = GROUND_Y;
    }
};

} // namespace

TEST_CASE("render and collision split every quad on the same diagonal") {
    // Rule 35 in test form. WHICH diagonal a quad is split on is arbitrary;
    // that render and physics pick THE SAME one is a fact two zones must agree
    // about, and until this case existed the agreement lived only in a comment
    // in JoltPhysics.h asserting the two "cannot disagree" — while they in fact
    // disagreed on every quad in the world.
    //
    // Real generated terrain, because that is what the claim is about.
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    dfn::world::ChunkManager chunks;
    chunks.open_generated({.seed = 7, .min_chunk = {-1, -1}, .max_chunk = {1, 1}},
                          {.load_radius = 1, .unload_radius = 2});
    chunks.update({0.0f, 0.0f, 0.0f}, ecs, bus);
    const auto field = chunks.heightfield({0, 0});
    REQUIRE(field.has_value());

    const DiagonalAgreement real = measure_agreement(*field, 3);
    MESSAGE("real terrain: " << real.samples << " samples, drawn-vs-solid "
                             << real.worst_agreement_m << " m, against the opposite "
                             << "diagonal " << real.worst_flipped_m << " m");
    REQUIRE(real.samples > 1000);

    // THE CLAIM: the solid ground IS the drawn ground. The tolerance is ray
    // precision rather than a fudge factor — the two meshes share every vertex,
    // so a correct pair agrees to within float noise. Measured: 1.5e-5 m.
    CHECK(real.worst_agreement_m < 0.001f);

    // THE CONTROL, and it needs no code reverted: the same instrument, pointed
    // at the other diagonal, must measure a disagreement it can actually
    // resolve. The floor is derived from the instrument, not fitted to the
    // result — two orders of magnitude above the agreement noise above. If this
    // ever fails, the field has gone flat and the assertion above has quietly
    // stopped being able to fail.
    CHECK(real.worst_flipped_m > 100.0f * real.worst_agreement_m);
    CHECK(real.worst_flipped_m > 0.004f);
}

TEST_CASE("the diagonal instrument has range: a maximally non-coplanar field") {
    // Rule 30's other half. The real-terrain case above accepts, but on that
    // terrain the wrong diagonal costs only millimetres — the audit that found
    // this defect quoted 18.56 m, and on the HEIGHTFIELD collision path that
    // number is not reachable: the worst per-cell divergence over all 81 chunks
    // of seed 7 measures 0.0122 m, because a smooth field sampled every 2 m has
    // very little corner asymmetry to express. Reported to the lead rather than
    // repeated (Rule 34: a number from another zone's report is a premise).
    //
    // So the magnitude is a property of the TERRAIN, not a bound on the bug,
    // and this case proves it by handing the same instrument a field where the
    // corners are as non-coplanar as a grid allows.
    CheckerChunk checker;
    const DiagonalAgreement synthetic = measure_agreement(checker.view, 8);
    MESSAGE("checkerboard: " << synthetic.samples << " samples, drawn-vs-solid "
                             << synthetic.worst_agreement_m << " m, against the opposite "
                             << "diagonal " << synthetic.worst_flipped_m << " m");
    REQUIRE(synthetic.samples > 100);
    // The agreement tolerance is RELATIVE here, unlike the real-terrain case:
    // a ray's hit precision scales with the magnitudes it is solving against,
    // and a 20 m step every 2 m is a far harsher intersection than real ground.
    // 1e-4 of the relief, measured at 5.3e-5 of it.
    CHECK(synthetic.worst_agreement_m < 1.0e-4f * CHECKER_RELIEF_M);
    // Every cell centre sits on one diagonal's edge and CHECKER_RELIEF_M above
    // or below the other's, so the wrong choice is wrong by the full relief.
    CHECK(synthetic.worst_flipped_m == doctest::Approx(CHECKER_RELIEF_M).epsilon(0.01));
}

TEST_CASE("on a flat chunk the two diagonals are identical — which is why the old case could not fail") {
    // Not redundant: this is the DIAGNOSIS made executable. The pre-existing
    // terrain-collision coverage used exactly this input, and on it a mesher
    // splitting quads the wrong way is indistinguishable from a correct one.
    // The choice of sample decided the result (Rule 36's shape), and a test
    // that cannot fail is a description (Rule 30).
    FlatChunk chunk;
    const uint32_t resolution = chunk.view.resolution;
    const std::vector<float> heights(static_cast<size_t>(resolution) * resolution, GROUND_Y);

    float worst_between_diagonals = 0.0f;
    for (uint32_t cz = 1; cz + 2 < resolution; cz += 8) {
        for (uint32_t cx = 1; cx + 2 < resolution; cx += 8) {
            const float px = chunk.view.origin.x + (static_cast<float>(cx) + 0.5f) * chunk.view.step;
            const float pz = chunk.view.origin.y + (static_cast<float>(cz) + 0.5f) * chunk.view.step;
            const auto a = cell_height(chunk.view, heights, cx, cz, px, pz, false);
            const auto b = cell_height(chunk.view, heights, cx, cz, px, pz, true);
            REQUIRE(a.has_value());
            REQUIRE(b.has_value());
            worst_between_diagonals = std::max(worst_between_diagonals, std::abs(*a - *b));
        }
    }
    CHECK(worst_between_diagonals == 0.0f);
}
