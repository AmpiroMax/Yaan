/*
Module: tests
File: tests/core/EcsTests.cpp

Responsibility:
- ECS test suite: entity lifecycle, generational safety, component CRUD,
  swap-and-pop integrity, views, deferred destroy, batch ops, group index.

Dependencies:
- Uses: doctest, dfn_core (ecs::World).
- Used by: ctest (test_ecs).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/ecs/sources/World.h"

#include <array>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};
struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};
struct Label {
    std::string text; // exercises non-trivial members (sim's NpcActionQueue case)
};

} // namespace

using dfn::ecs::EntityId;
using dfn::ecs::GroupId;
using dfn::ecs::NO_GROUP;
using dfn::ecs::World;

TEST_CASE("spawn/alive/destroy basics") {
    World w;
    const EntityId a = w.spawn();
    const EntityId b = w.spawn();
    CHECK(w.alive(a));
    CHECK(w.alive(b));
    CHECK(w.entity_count() == 2);
    CHECK(a != b);

    w.destroy(a);
    CHECK_FALSE(w.alive(a));
    CHECK(w.alive(b));
    CHECK(w.entity_count() == 1);

    CHECK_FALSE(w.alive(EntityId::null()));
    CHECK_FALSE(w.alive(EntityId{9999, 0}));
}

TEST_CASE("generational ids catch use-after-destroy") {
    World w;
    const EntityId stale = w.spawn();
    w.add(stale, Position{1.0f, 2.0f});
    w.destroy(stale);

    // Slot is reused; the stale id must stay dead.
    const EntityId fresh = w.spawn();
    CHECK(fresh.index == stale.index);
    CHECK(fresh.generation != stale.generation);
    CHECK(w.alive(fresh));
    CHECK_FALSE(w.alive(stale));

    // Stale accessors are inert.
    CHECK(w.get<Position>(stale) == nullptr);
    CHECK_FALSE(w.has<Position>(stale));
    w.destroy(stale); // no-op, must not kill `fresh`
    CHECK(w.alive(fresh));
}

TEST_CASE("component add/get/remove and swap-and-pop integrity") {
    World w;
    std::vector<EntityId> ids;
    for (int i = 0; i < 5; ++i) {
        const EntityId id = w.spawn();
        w.add(id, Position{static_cast<float>(i), 0.0f});
        ids.push_back(id);
    }

    // Remove the middle entity's component: swap-and-pop must keep the rest.
    w.remove<Position>(ids[2]);
    CHECK_FALSE(w.has<Position>(ids[2]));
    for (const int i : {0, 1, 3, 4}) {
        REQUIRE(w.get<Position>(ids[static_cast<std::size_t>(i)]) != nullptr);
        CHECK(w.get<Position>(ids[static_cast<std::size_t>(i)])->x
              == doctest::Approx(static_cast<float>(i)));
    }

    // Non-trivial member type round-trip.
    w.add(ids[0], Label{"chunk_3_-2"});
    CHECK(w.get<Label>(ids[0])->text == "chunk_3_-2");
}

TEST_CASE("view iterates the intersection only") {
    World w;
    const EntityId both_a = w.spawn();
    const EntityId both_b = w.spawn();
    const EntityId pos_only = w.spawn();
    w.add(both_a, Position{1.0f, 0.0f});
    w.add(both_a, Velocity{10.0f, 0.0f});
    w.add(both_b, Position{2.0f, 0.0f});
    w.add(both_b, Velocity{20.0f, 0.0f});
    w.add(pos_only, Position{3.0f, 0.0f});

    int rows = 0;
    float vel_sum = 0.0f;
    for (auto [id, pos, vel] : w.view<Position, Velocity>()) {
        CHECK((id == both_a || id == both_b));
        pos.x += vel.dx;
        vel_sum += vel.dx;
        ++rows;
    }
    CHECK(rows == 2);
    CHECK(vel_sum == doctest::Approx(30.0f));
    CHECK(w.get<Position>(both_a)->x == doctest::Approx(11.0f)); // writes stick
    CHECK(w.get<Position>(pos_only)->x == doctest::Approx(3.0f));

    int single = 0;
    w.view<Position>().each([&](EntityId, Position&) { ++single; });
    CHECK(single == 3);
}

TEST_CASE("deferred destruction is safe during iteration") {
    World w;
    for (int i = 0; i < 10; ++i) {
        w.add(w.spawn(), Position{static_cast<float>(i), 0.0f});
    }
    for (auto [id, pos] : w.view<Position>()) {
        if (pos.x >= 5.0f) {
            w.destroy_deferred(id);
            w.destroy_deferred(id); // double-queue must be harmless
        }
    }
    CHECK(w.entity_count() == 10); // nothing dead until flush
    w.flush_destroyed();
    CHECK(w.entity_count() == 5);
    int remaining = 0;
    for (auto [id, pos] : w.view<Position>()) {
        CHECK(pos.x < 5.0f);
        ++remaining;
    }
    CHECK(remaining == 5);
}

TEST_CASE("batch spawn/destroy with group index (Rule 11 / Q22)") {
    World w;
    constexpr GroupId CHUNK_A = 0x8000000000000001ull;
    constexpr GroupId CHUNK_B = 0x8000000000000002ull;

    std::array<EntityId, 32> a_ids{};
    std::array<EntityId, 8> b_ids{};
    w.spawn_batch(a_ids, CHUNK_A);
    w.spawn_batch(b_ids, CHUNK_B);
    CHECK(w.entity_count() == 40);
    CHECK(w.entities_in_group(CHUNK_A).size() == 32);
    CHECK(w.entities_in_group(CHUNK_B).size() == 8);
    CHECK(w.group_of(a_ids[0]) == CHUNK_A);

    // Batch component attach: prototype form + per-entity values form.
    w.add_batch(a_ids, Position{7.0f, 7.0f});
    CHECK(w.get<Position>(a_ids[31])->x == doctest::Approx(7.0f));
    std::array<Velocity, 8> vels{};
    for (std::size_t i = 0; i < vels.size(); ++i) {
        vels[i].dx = static_cast<float>(i);
    }
    w.add_batch<Velocity>(b_ids, vels);
    CHECK(w.get<Velocity>(b_ids[7])->dx == doctest::Approx(7.0f));

    // Entity crossing a chunk boundary.
    w.set_group(a_ids[0], CHUNK_B);
    CHECK(w.entities_in_group(CHUNK_A).size() == 31);
    CHECK(w.entities_in_group(CHUNK_B).size() == 9);

    // Chunk unload: one batch destroy per group.
    CHECK(w.destroy_group(CHUNK_A) == 31);
    CHECK(w.entity_count() == 9);
    CHECK(w.entities_in_group(CHUNK_A).empty());
    for (std::size_t i = 1; i < a_ids.size(); ++i) {
        CHECK_FALSE(w.alive(a_ids[i]));
    }
    CHECK(w.alive(a_ids[0])); // moved to CHUNK_B before the unload

    // destroy_batch skips dead ids silently.
    w.destroy_batch(a_ids);
    CHECK(w.entity_count() == 8);
    CHECK(w.destroy_group(CHUNK_A) == 0);
}

TEST_CASE("group of dead entity is NO_GROUP; groups survive spawn reuse") {
    World w;
    constexpr GroupId G = 0x8000000000000010ull;
    std::array<EntityId, 4> ids{};
    w.spawn_batch(ids, G);
    w.destroy(ids[1]);
    CHECK(w.group_of(ids[1]) == NO_GROUP);
    CHECK(w.entities_in_group(G).size() == 3);

    // Reused slot must not inherit the old group.
    const EntityId reused = w.spawn();
    CHECK(reused.index == ids[1].index);
    CHECK(w.group_of(reused) == NO_GROUP);
    CHECK(w.entities_in_group(G).size() == 3);
}

TEST_CASE("resources are type-keyed singletons") {
    World w;
    struct Config {
        int value = 0;
    };
    CHECK_FALSE(w.has_resource<Config>());
    w.add_resource(Config{41});
    REQUIRE(w.has_resource<Config>());
    w.resource<Config>().value += 1;
    CHECK(w.resource<Config>().value == 42);
    w.add_resource(Config{7}); // replace
    CHECK(w.resource<Config>().value == 7);
}

TEST_CASE("clear resets everything") {
    World w;
    std::array<EntityId, 16> ids{};
    w.spawn_batch(ids, GroupId{0x8000000000000042ull});
    w.add_batch(ids, Position{1.0f, 1.0f});
    w.add_resource(int{5});
    w.clear();
    CHECK(w.entity_count() == 0);
    CHECK_FALSE(w.alive(ids[0]));
    CHECK_FALSE(w.has_resource<int>());
    CHECK(w.entities_in_group(0x8000000000000042ull).empty());
    const EntityId fresh = w.spawn(); // world stays usable
    CHECK(w.alive(fresh));
}
