/*
Created: 09:08:2026 - 16:51:22
Last updated: 10:08:2026 - 21:33:30
Module: tests
File: tests/sim/TunnelWalkTests.cpp

Responsibility:
- THE acceptance test for voxel terrain collision: a character walks INTO the
  crag tunnel mouth and is still inside the mountain 100 m later, and stands on
  the tunnel floor with rock overhead. A heightfield-derived body passes
  neither — it has no overhangs, so the capsule would ride the surface over the
  crag instead of through it.

Key items:
- TunnelRig: real ChunkManager (seed 1, testbed extent) + Jolt + one static
  mesh body per resident chunk via physics::create_terrain_mesh_body.
- Collision-resolution measurement (triangle counts + shape build time) that
  justifies the chosen extraction resolution.

Dependencies:
- Uses: doctest, dfn_physics, dfn_platform_physics, dfn_world, dfn_core.
- Used by: ctest (sim_tunnel_walk).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This test is the stage's acceptance line: it must drive the capsule through
  the real generated world, never a synthetic stand-in.
*/
/*
UPD:
- 09:08:2026 - 16:51:22: Created with the voxel terrain collision swap.
- 09:08:2026 - 17:08:40: DEBUG sprint (30 m/s) risk coverage: per-tick
                         tunnelling detector against tunnel walls and the
                         castle curtain wall, plus a streaming fall-through
                         run across chunk boundaries.
- 09:08:2026 - 18:21:53: Follow core's CHUNK_LOAD_BUDGET (one chunk per
                         update): drive streaming to SETTLED before walking
                         instead of assuming a ring loads in one update.
                         Assertions unchanged.
- 09:08:2026 - 19:17:00: Steer along the corridor CENTERLINE with a look-ahead
                         instead of beelining at each waypoint. Diagnosed after
                         a worldgen change stalled the walk at 91.6 m: the
                         corridor was open (nudged to the centerline the capsule
                         walked freely), but a beeline cuts the corner into the
                         outer wall at a switchback and wedges on voxel-wall
                         bumps. Assertions unchanged; the walk now completes the
                         full 8/8 waypoints, ~149 m.
- 09:08:2026 - 20:56:45: Split the timing: `shape_ms` (my Jolt MeshShape
                         build) is now measured separately from `stream_ms`
                         (core's generate + extract inside ChunkManager::update).
                         The old single timer wrapped both and reported
                         worldgen's cost as physics' cost. The per-chunk ceiling
                         now covers only this zone's cost and is an
                         order-of-magnitude guard, not a budget — the measured
                         figure moves 2x with machine load alone.
- 10:08:2026 - 02:44:55: Walk budget 3600 -> 5400 (core's verified fix, landed by the lead with sim resting; the walk is longer since the massif reshape, completing at 4251).
- 10:08:2026 - 21:33:30: THREE METRICS THAT FAILED OPEN, fixed together
  because they are one disease: a measurement whose EMPTY state is
  indistinguishable from its SUCCESS state.
  (1) count_tunnelling_ticks scored a perfect zero for a capsule that never
      moved. It is now probe_tunnelling, which reports its own coverage and a
      verdict word, and the callers assert the coverage before the result.
  (2) The coverage quantity itself was wrong on the first attempt and the run
      said so: "moving ticks whose segment met a wall" reads ZERO on a clean
      pass, because a capsule that does not tunnel never crosses geometry. No
      threshold on that quantity separates the accepted case from the rejected
      one, so the QUANTITY was wrong rather than the number (Rule 30). Replaced
      by IMPEDED ticks — geometry took at least half the intended step — which
      counts being BLOCKED as the strongest evidence a wall is there.
  (3) The castle curtain-wall case had no curtain wall: TunnelRig builds terrain
      collision only, and the wall is a prop. Eight charges from 60 m out were
      running through open ground and reporting no tunnelling. Impeded ticks
      went 0 -> 501 once the props are built; it still does not tunnel, so the
      claim now stands on evidence instead of on absence.
  Also the floor band at the deep waypoint: Approx(deep.y).epsilon(0.35) is
  0.35 * (1 + |deep.y|) = a +/-14.67 m tolerance on a ray only 6 m long, which
  NO RESULT COULD HAVE FAILED (Rule 40 — and it is a DIFFERENCE, so a relative
  band was the wrong instrument at any scale). Now absolute metres, derived from
  the voxel lattice the floor is quantised on. Measured: 0.067 m against a
  1.6 m band.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/gameplay/sources/PropCollision.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/world/sources/ChunkManager.h"

namespace {

namespace config = dfn::config;
namespace platform = dfn::platform;
namespace physics_layer = dfn::physics;
namespace world = dfn::world;

constexpr float DT = static_cast<float>(config::SIM_DT);

// The generated world the whole stage is verified against (core's testbed:
// seed 1, 4x4 chunks). The crag tunnel lives in the layout defaults.
struct TunnelRig {
    dfn::ecs::World ecs;
    dfn::events::EventBus bus;
    world::ChunkManager chunks;
    std::unique_ptr<platform::IPhysics> physics = platform::create_jolt_physics();
    std::vector<platform::PhysicsBodyHandle> terrain_bodies;
    std::unordered_map<uint64_t, platform::PhysicsBodyHandle> bodies_by_chunk;

    size_t total_triangles = 0;
    size_t chunk_bodies = 0;
    // Two SEPARATE costs, deliberately not merged: `stream_ms` is core's chunk
    // generation + surface extraction inside ChunkManager::update, `shape_ms`
    // is MY Jolt MeshShape build. Timing them together (as this rig first did)
    // reports worldgen's cost as physics' cost and misdirects optimisation.
    double stream_ms = 0.0;
    double shape_ms = 0.0;
    [[nodiscard]] double settle_ms() const { return stream_ms + shape_ms; }

    explicit TunnelRig(const glm::vec3& focus) {
        REQUIRE(physics->init());
        chunks.open_generated(world::WorldGenParams{1, {0, 0}, {3, 3}},
                              world::ChunkStreamingParams{2, 3});
        settle(focus);
        REQUIRE(chunk_bodies > 0);
    }

    /// Streaming is rate-limited to CHUNK_LOAD_BUDGET chunks per update (core's
    /// fix for multi-second load freezes), so a ring fills over many updates.
    /// Tests that walk on the terrain need the SETTLED world: drive updates —
    /// each building collision for whatever became resident — until residency
    /// stops changing. Same pattern as core's Fixture::settle.
    /// Buildings and boulders. The terrain path above does NOT include them:
    /// the castle's curtain wall is a prop, so a sweep meant to charge it needs
    /// this call or it charges open ground (see the case that found this).
    void build_props() { dfn::gameplay::update_prop_collision(ecs, *physics, chunks); }

    void settle(const glm::vec3& focus) {
        for (int i = 0; i < 256; ++i) {
            const std::size_t before = chunks.loaded_chunks().size();
            restream(focus);
            if (chunks.loaded_chunks().size() == before) {
                return;
            }
        }
        FAIL("streaming never settled");
    }

    // Re-streams around `focus` exactly as the app ferry does: update the
    // resident set, then rebuild collision so newly resident chunks have bodies
    // and departed ones do not. Keyed by chunk coord so repeats are cheap.
    void restream(const glm::vec3& focus) {
        const auto stream_start = std::chrono::steady_clock::now();
        chunks.update(focus, ecs, bus);
        stream_ms += std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - stream_start)
                         .count();
        for (const world::ChunkCoord coord : chunks.loaded_chunks()) {
            const uint64_t key = chunk_key(coord);
            if (bodies_by_chunk.contains(key)) {
                continue; // already collidable
            }
            const auto mesh = chunks.voxel_mesh(coord);
            if (!mesh.has_value()) {
                continue;
            }
            total_triangles += mesh->triangle_count();
            const auto shape_start = std::chrono::steady_clock::now();
            const auto body =
                dfn::physics::create_terrain_mesh_body(*physics, *mesh, key);
            shape_ms += std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - shape_start)
                            .count();
            if (body.valid()) {
                bodies_by_chunk.emplace(key, body);
                ++chunk_bodies;
            }
        }
    }

    static uint64_t chunk_key(world::ChunkCoord coord) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(coord.x)) << 32) |
               static_cast<uint32_t>(coord.z);
    }

    // Ground height from the heightfield — the SURFACE of the mountain. Being
    // below it with rock overhead is what "inside the mountain" means.
    [[nodiscard]] float surface_height(glm::vec2 world_xz) const {
        const world::ChunkCoord coord = world::chunk_at_position(world_xz);
        const auto view = chunks.heightfield(coord);
        if (!view.has_value()) {
            return 0.0f;
        }
        const float fx = (world_xz.x - view->origin.x) / view->step;
        const float fz = (world_xz.y - view->origin.y) / view->step;
        const auto x = static_cast<uint32_t>(
            std::clamp(fx, 0.0f, static_cast<float>(view->resolution - 1)));
        const auto z = static_cast<uint32_t>(
            std::clamp(fz, 0.0f, static_cast<float>(view->resolution - 1)));
        return view->height_at(x, z);
    }
};

platform::CharacterDesc player_desc(const glm::vec3& position) {
    platform::CharacterDesc desc;
    desc.position = position;
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.max_slope_radians = static_cast<float>(config::PLAYER_MAX_SLOPE);
    desc.step_height = static_cast<float>(config::PLAYER_STEP_HEIGHT);
    desc.layer = physics_layer::LAYER_CHARACTER;
    desc.collides_with = physics_layer::LAYER_STATIC;
    desc.user_data = 0x9A1Full; // stand-in EntityId bits for the walker
    return desc;
}

TEST_CASE("voxel terrain collision: the extraction resolution is affordable") {
    // Measured, not assumed: the render-resolution mesh is what we collide
    // against, so its cost has to be stated in numbers.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    TunnelRig rig({tunnel.points[0].x, tunnel.points[0].y, tunnel.points[0].z});

    MESSAGE("collision chunks: "
            << rig.chunk_bodies << ", triangles: " << rig.total_triangles
            << " | MY cost (Jolt MeshShape): " << rig.shape_ms << " ms ("
            << (rig.shape_ms / static_cast<double>(rig.chunk_bodies))
            << " ms/chunk) | core's cost (generate + extract): " << rig.stream_ms
            << " ms | settle total: " << rig.settle_ms() << " ms");
    CHECK(rig.total_triangles > 0);
    // The cost that is MINE to defend: Jolt shape building per chunk. Kept
    // separate from streaming on purpose — a ceiling that also covers worldgen
    // fails for reasons this zone cannot fix, and hides the cost it should
    // expose.
    // This is a CATASTROPHE GUARD, not a budget. Measured ~58 ms/chunk on an
    // idle machine and ~105 ms/chunk while parallel builds compete for cores,
    // so anything near the real figure would flake in CI for reasons unrelated
    // to the code. The MESSAGE above carries the actual number for a human to
    // read; this only catches an order-of-magnitude regression.
    CHECK(rig.shape_ms / static_cast<double>(rig.chunk_bodies) < 250.0);
}

// The carved floor is an isosurface on a VOXEL_SIZE lattice, so the only
// meaningful band around a waypoint is that lattice plus the SDF band the
// surface can shift within, plus a tick of ray precision. Derived from the
// representation, not fitted to the measurement — a tolerance chosen after
// seeing the number is a description of today's build.
constexpr float FLOOR_TOLERANCE_M =
    static_cast<float>(config::VOXEL_SIZE + config::VOXEL_SDF_BAND) * 0.5f + 0.1f;

// The one property that makes the assertion an assertion: the ray searches from
// 1 m above the waypoint to 5 m below it, so a tolerance of 5 m or more admits
// every result the instrument can produce. The old band was 14.67 m. This is
// the check that would have caught it without anyone measuring anything.
static_assert(FLOOR_TOLERANCE_M < 5.0f,
              "a tolerance wider than the ray's own reach cannot be failed");

TEST_CASE("voxel terrain collision: rock overhead inside the tunnel (overhang)") {
    // The geometric proof that a heightfield-derived body cannot pass: at a
    // deep waypoint there is collision ABOVE the floor, and the floor itself
    // sits below the mountain's surface height.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    const glm::vec3 deep = tunnel.points[3]; // interior leg, well inside the massif
    TunnelRig rig(deep);

    const float surface = rig.surface_height({deep.x, deep.z});
    CHECK(surface > deep.y + tunnel.height); // mountain closes over the corridor

    // Floor beneath the waypoint.
    const auto down = rig.physics->raycast({deep.x, deep.y + 1.0f, deep.z},
                                           {0.0f, -1.0f, 0.0f}, 6.0f,
                                           physics_layer::LAYER_STATIC);
    REQUIRE(down.hit);
    // ABSOLUTE metres, because this is a DIFFERENCE — the distance between the
    // carved floor and the waypoint it was carved around, whose correct value
    // is zero (Rule 40). It was written as Approx(deep.y).epsilon(0.35), which
    // doctest expands to 0.35 * (1 + |deep.y|): at this waypoint's height that
    // is a +/-14.67 m band on a ray only 6 m long. NO RESULT THE RAY CAN
    // PRODUCE COULD HAVE FAILED IT — the assertion was unfalsifiable by
    // arithmetic, not merely loose.
    //
    // The floor is a voxel surface, so the band that means something is the
    // lattice it is quantised on: one VOXEL_SIZE either way, plus the SDF band
    // the isosurface can shift within. Measured below so the number in the
    // report is the real one and not this bound.
    const float floor_error_m = std::abs(down.position.y - deep.y);
    MESSAGE("carved floor sits " << floor_error_m << " m from the waypoint");
    CHECK(floor_error_m < FLOOR_TOLERANCE_M);

    // Ceiling above it — this is the overhang a heightfield cannot represent.
    const auto up = rig.physics->raycast({deep.x, deep.y + 0.5f, deep.z},
                                         {0.0f, 1.0f, 0.0f}, 40.0f,
                                         physics_layer::LAYER_STATIC);
    REQUIRE(up.hit);
    CHECK(up.distance < 20.0f); // rock, not open sky
    // Headroom must clear a standing player.
    CHECK(up.position.y - down.position.y >
          static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
}

TEST_CASE("voxel terrain collision: the player walks THROUGH the crag, not over it") {
    // The acceptance walk: enter at the mouth, steer waypoint to waypoint, and
    // stay under the mountain. With heightfield-derived collision the capsule
    // rides the surface and this test cannot pass.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    REQUIRE(tunnel.point_count >= 4);

    const glm::vec3 mouth = tunnel.points[0];
    TunnelRig rig(mouth);
    const auto character =
        rig.physics->create_character(player_desc({mouth.x, mouth.y + 0.5f, mouth.z}));
    REQUIRE(character.valid());

    int waypoint = 1;
    float travelled = 0.0f;
    float deepest_cover = 0.0f; // most rock ever recorded over the player's head
    int ticks_under_rock = 0;
    float vertical_velocity = 0.0f;
    glm::vec3 previous = rig.physics->character_position(character);

    // Steering: follow the corridor CENTERLINE with a look-ahead, the way a
    // player walks a passage. Beelining at the next waypoint instead pushes the
    // capsule into the outer wall at a switchback, where it can wedge on the
    // voxel wall's bumps — measured, and the reason this loop is written this
    // way (see the report: nudged back to the centerline it walks freely).
    // 5400: the walk is genuinely longer since the §2.8 massif reshape (the
    // instrumented probe completes at tick 4251); at 3600 the budget expired
    // mid-climb with a transient airborne frame. Verified at 7200 by core.
    for (int tick = 0; tick < 5400 && waypoint < tunnel.point_count; ++tick) {
        const glm::vec3 position = rig.physics->character_position(character);
        const glm::vec3 leg_start = tunnel.points[waypoint - 1];
        const glm::vec3 leg_end = tunnel.points[waypoint];
        const glm::vec3 leg = leg_end - leg_start;
        const float leg_length_sq = glm::dot(leg, leg);

        // How far along this leg the capsule currently is.
        const float t = leg_length_sq > 0.0f
                            ? glm::clamp(glm::dot(position - leg_start, leg) /
                                             leg_length_sq,
                                         0.0f, 1.0f)
                            : 1.0f;
        if (glm::length(glm::vec2(leg_end.x - position.x, leg_end.z - position.z)) <
            1.5f) {
            ++waypoint; // reached this leg's end, steer down the next
            continue;
        }

        // Aim 2 m further along the centerline, so the capsule stays centred
        // instead of cutting the corner into the wall.
        const float look_ahead = leg_length_sq > 0.0f
                                     ? std::min(1.0f, t + 2.0f / std::sqrt(leg_length_sq))
                                     : 1.0f;
        const glm::vec3 aim = leg_start + leg * look_ahead;
        const glm::vec2 to_target{aim.x - position.x, aim.z - position.z};
        if (glm::length(to_target) < 0.01f) {
            ++waypoint;
            continue;
        }

        const glm::vec2 direction = glm::normalize(to_target);
        const float speed = static_cast<float>(config::WALK_SPEED);
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(character,
                                    {direction.x * speed * DT,
                                     vertical_velocity * DT,
                                     direction.y * speed * DT});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        const glm::vec3 moved = rig.physics->character_position(character);
        travelled += glm::length(glm::vec2(moved.x - previous.x, moved.z - previous.z));
        previous = moved;

        // "Inside the mountain": the surface is above the player's head.
        const float head = moved.y + static_cast<float>(config::PLAYER_EYE_HEIGHT);
        const float cover = rig.surface_height({moved.x, moved.z}) - head;
        if (cover > 0.0f) {
            ++ticks_under_rock;
            deepest_cover = std::max(deepest_cover, cover);
        }
    }

    MESSAGE("travelled " << travelled << " m, reached waypoint " << waypoint << "/"
                         << tunnel.point_count << ", ticks under rock "
                         << ticks_under_rock << ", deepest cover " << deepest_cover
                         << " m");

    CHECK(travelled > 100.0f);        // a traverse, not a doorway
    CHECK(ticks_under_rock > 600);    // most of the walk happens inside the massif
    CHECK(deepest_cover > 5.0f);      // genuinely under the crag, not in a cutting
    CHECK(rig.physics->character_grounded(character)); // on the floor, not falling
}

// Sprints a capsule from `start` along `direction` for `ticks`, returning the
// number of ticks in which it passed THROUGH static geometry. Detection is
// exact rather than heuristic: each tick, ray-cast the movement segment; if the
// segment hits a wall yet the capsule ended past that hit (beyond its own
// radius of penetration tolerance), collide-and-slide let it tunnel.
//
// IT REPORTS ITS OWN COVERAGE, and that is not decoration. A capsule that never
// moves — spawned inside solid rock, or wedged, or created against a body that
// failed to build — produces zero tunnelling ticks and reads exactly like a
// clean sweep. That is the same disease as the worst_foot_slip_mm 0 that every
// run printed while measuring nothing: A METRIC WHOSE EMPTY STATE IS
// INDISTINGUISHABLE FROM ITS SUCCESS STATE. So the probe returns how far it
// actually got, the caller asserts that, and the verdict says which of the two
// it is looking at.
struct TunnelProbe {
    int tunnelled = 0;     ///< ticks that passed THROUGH static geometry.
    int moving_ticks = 0;  ///< ticks in which the capsule actually advanced.
    int impeded_ticks = 0; ///< ticks in which geometry took away most of the
                           ///< intended displacement — blocked outright or
                           ///< slid along a wall. THE COVERAGE QUANTITY: it is
                           ///< the evidence that the sweep met the walls it
                           ///< claims to be testing.
    float travelled_m = 0.0f;

    /// One word a caller can print, so a run that measured nothing says so
    /// instead of reporting a perfect zero (see the note above the probe).
    [[nodiscard]] std::string verdict() const {
        if (moving_ticks == 0 && impeded_ticks == 0) {
            return "NOTHING HAPPENED - measured nothing";
        }
        if (impeded_ticks == 0) {
            return "MET NO GEOMETRY - measured nothing";
        }
        return tunnelled == 0 ? "held" : "TUNNELLED";
    }
};

[[nodiscard]] TunnelProbe probe_tunnelling(TunnelRig& rig, const glm::vec3& start,
                                           const glm::vec3& direction, int ticks) {
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());
    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    const float radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    // What an unobstructed tick would cover. Zero when the caller asked for no
    // movement at all, which must NOT read as "geometry stopped us" — that
    // distinction is the whole point of the coverage quantity.
    const float intended = glm::length(direction) * speed * DT;

    TunnelProbe probe;
    for (int tick = 0; tick < ticks; ++tick) {
        const glm::vec3 before = rig.physics->character_position(character);
        rig.physics->move_character(character, direction * speed * DT);
        rig.physics->step(DT);
        const glm::vec3 after = rig.physics->character_position(character);

        const glm::vec3 delta = after - before;
        const float distance = glm::length(delta);
        // Impeded = geometry took at least half the intended step away, whether
        // by stopping the capsule dead or by turning it along a wall. Being
        // BLOCKED is the strongest evidence a wall is there, so it counts as
        // coverage rather than being skipped as "nothing happened" — which is
        // how the old metric managed to confront a wall and record silence.
        if (intended > 1e-6f && distance < 0.5f * intended) {
            ++probe.impeded_ticks;
        }
        if (distance < 1e-4f) {
            continue; // blocked outright: the correct outcome
        }
        ++probe.moving_ticks;
        probe.travelled_m += distance;
        // Cast from chest height so floor contact is not mistaken for a wall.
        const glm::vec3 chest{0.0f, 0.9f, 0.0f};
        const auto hit = rig.physics->raycast(before + chest, delta / distance, distance,
                                              physics_layer::LAYER_STATIC);
        if (!hit.hit) {
            continue; // open corridor ahead: nothing to tunnel through
        }
        if (distance > hit.distance + radius) {
            ++probe.tunnelled;
        }
    }
    rig.physics->destroy_character(character);
    return probe;
}

TEST_CASE("DEBUG sprint: 30 m/s does not tunnel through tunnel walls") {
    // 0.5 m of travel per fixed tick against corridor walls only 2 m from the
    // centerline. If collide-and-slide misses, the sprinter leaves the mountain.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    const glm::vec3 deep = tunnel.points[3];
    TunnelRig rig(deep);

    const glm::vec3 start{deep.x, deep.y + 0.5f, deep.z};
    TunnelProbe total;
    // Sweep every horizontal direction: switchback legs stack, so some headings
    // face a thin wall with open corridor on the far side — the worst case.
    for (int i = 0; i < 16; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / 16.0f;
        const TunnelProbe leg = probe_tunnelling(
            rig, start, {std::cos(angle), 0.0f, std::sin(angle)}, 120);
        total.tunnelled += leg.tunnelled;
        total.moving_ticks += leg.moving_ticks;
        total.impeded_ticks += leg.impeded_ticks;
        total.travelled_m += leg.travelled_m;
    }
    MESSAGE("16 headings: " << total.verdict() << " - moved on " << total.moving_ticks
                            << " ticks over " << total.travelled_m << " m, impeded on "
                            << total.impeded_ticks);
    // COVERAGE FIRST, because zero tunnelling on a capsule that never left the
    // spawn point is not a result. Only these two lines separate "the walls
    // held" from "nothing was measured".
    CHECK(total.moving_ticks > 500);
    CHECK(total.impeded_ticks > 50);
    CHECK(total.tunnelled == 0);
}

TEST_CASE("DEBUG sprint: 30 m/s does not tunnel through the castle curtain wall") {
    // The other thin vertical geometry the user can run at head-on.
    const dfn::world::TestbedLayout layout{};
    const glm::vec2 castle = layout.castle.center;
    const glm::vec3 focus{castle.x, 0.0f, castle.y};
    TunnelRig rig(focus);

    rig.build_props(); // WITHOUT THIS THE CASE CHARGES OPEN GROUND — see below.
    const float ground = rig.surface_height(castle);
    TunnelProbe total;
    // Charge the pad from eight compass points, starting well outside it.
    for (int i = 0; i < 8; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / 8.0f;
        const glm::vec2 offset{std::cos(angle) * 60.0f, std::sin(angle) * 60.0f};
        const glm::vec2 from = castle + offset;
        const glm::vec3 start{from.x, rig.surface_height(from) + 1.0f, from.y};
        const glm::vec3 toward = glm::normalize(glm::vec3{-offset.x, 0.0f, -offset.y});
        const TunnelProbe leg = probe_tunnelling(rig, start, toward, 180);
        total.tunnelled += leg.tunnelled;
        total.moving_ticks += leg.moving_ticks;
        total.impeded_ticks += leg.impeded_ticks;
        total.travelled_m += leg.travelled_m;
    }
    MESSAGE("castle ground height " << ground << " m; 8 charges: " << total.verdict()
                                    << " - moved on " << total.moving_ticks
                                    << " ticks over " << total.travelled_m
                                    << " m, impeded on " << total.impeded_ticks);
    CHECK(total.moving_ticks > 500);
    CHECK(total.impeded_ticks > 20);
    CHECK(total.tunnelled == 0);
}

TEST_CASE("the tunnelling probe reports an empty run as empty, not as a pass") {
    // THE CONTROL for the two cases above, and the reason the metric was worth
    // changing at all. A probe that cannot move scores a perfect zero on
    // tunnelling — identical to a clean sweep — so the coverage assertions are
    // the only thing standing between "the walls held" and "nothing happened".
    // Here the empty run is manufactured on purpose and must be VISIBLE.
    const dfn::world::TestbedLayout layout{};
    const auto& tunnel = layout.carves.crag_tunnel;
    const glm::vec3 deep = tunnel.points[3];
    TunnelRig rig(deep);
    const glm::vec3 start{deep.x, deep.y + 0.5f, deep.z};

    // A zero heading: the capsule is asked to go nowhere and obeys.
    const TunnelProbe still = probe_tunnelling(rig, start, {0.0f, 0.0f, 0.0f}, 120);
    MESSAGE("standing still: " << still.verdict());
    CHECK(still.tunnelled == 0);       // the number the old metric reported...
    CHECK(still.moving_ticks == 0);    // ...on a run that measured nothing,
    CHECK(still.impeded_ticks == 0);   // which the probe now says out loud.
    CHECK(still.verdict() == "NOTHING HAPPENED - measured nothing");

    // And the control's control: a real heading from the same spawn DOES move
    // and DOES meet walls, so the guards above are satisfiable rather than
    // merely strict (Rule 30a).
    const TunnelProbe moving = probe_tunnelling(rig, start, {1.0f, 0.0f, 0.0f}, 120);
    MESSAGE("heading +X: " << moving.verdict() << " - " << moving.moving_ticks
                           << " moving ticks, impeded on " << moving.impeded_ticks);
    CHECK(moving.moving_ticks > 0);
    CHECK(moving.impeded_ticks > 0);
    CHECK(moving.verdict() == "held");
}

TEST_CASE("DEBUG sprint: no fall-through while chunks are still streaming") {
    // The failure mode that bit us before: at 30 m/s a chunk boundary arrives
    // every 8.5 s while terrain shape build takes ~68 ms/chunk. The player must
    // never end up below the terrain surface, mid-stream or not.
    // Route stays INSIDE the generated extent (4x4 chunks = 0..1024 m): from
    // x=200 east to x=800 crosses three chunk boundaries in 20 s. Running out
    // of the world is a separate concern (see the world-edge case below), not
    // a streaming failure.
    const glm::vec2 start_xz{200.0f, 430.0f};
    TunnelRig rig({start_xz.x, 0.0f, start_xz.y});

    const glm::vec3 start{start_xz.x, rig.surface_height(start_xz) + 1.0f, start_xz.y};
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());

    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    float vertical_velocity = 0.0f;
    int below_surface_ticks = 0;
    float worst_depth = 0.0f;

    // Sprint due east across chunk boundaries, restreaming as the app would.
    for (int tick = 0; tick < 1200; ++tick) {
        const glm::vec3 position = rig.physics->character_position(character);
        if (tick % 30 == 0) {
            rig.restream(position); // app-side ferry: load/unload + bodies
        }
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(
            character, {speed * DT, vertical_velocity * DT, 0.0f});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        const glm::vec3 moved = rig.physics->character_position(character);
        // Open valley: the capsule must stay at or above the ground surface.
        const float depth = rig.surface_height({moved.x, moved.z}) - moved.y;
        if (depth > 1.0f) { // 1 m tolerance for heightfield-vs-mesh sampling
            ++below_surface_ticks;
            worst_depth = std::max(worst_depth, depth);
        }
    }

    const glm::vec3 finish = rig.physics->character_position(character);
    MESSAGE("sprinted to x=" << finish.x << ", ticks below surface: "
                             << below_surface_ticks << ", worst depth " << worst_depth
                             << " m");
    CHECK(finish.x > start.x + 500.0f); // actually crossed three chunk boundaries
    CHECK(below_surface_ticks == 0);
    CHECK(finish.y > -50.0f); // not falling forever
}

TEST_CASE("DEBUG sprint: the world edge is reachable in seconds (known limit)") {
    // Consequence of the debug sprint, recorded rather than hidden: 30 m/s
    // reaches the edge of the 1024 m generated extent in well under a minute,
    // and past the last chunk there is no terrain, so the player falls. This is
    // a world-extent/app concern (a boundary or a bigger world), NOT a physics
    // or streaming defect — the test documents it so the grill can decide.
    const glm::vec2 start_xz{200.0f, 430.0f};
    TunnelRig rig({start_xz.x, 0.0f, start_xz.y});
    const glm::vec3 start{start_xz.x, rig.surface_height(start_xz) + 1.0f, start_xz.y};
    const auto character = rig.physics->create_character(player_desc(start));
    REQUIRE(character.valid());

    const float speed =
        static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);
    float vertical_velocity = 0.0f;
    for (int tick = 0; tick < 1200; ++tick) { // 20 s due WEST, off the extent
        if (tick % 30 == 0) {
            rig.restream(rig.physics->character_position(character));
        }
        vertical_velocity -= static_cast<float>(config::GRAVITY) * DT;
        rig.physics->move_character(character,
                                    {-speed * DT, vertical_velocity * DT, 0.0f});
        rig.physics->step(DT);
        if (rig.physics->character_grounded(character) && vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }
    }

    const glm::vec3 finish = rig.physics->character_position(character);
    MESSAGE("off-extent sprint ended at x=" << finish.x << ", y=" << finish.y);
    CHECK(finish.x < 0.0f);  // left the generated world
    CHECK(finish.y < -50.0f); // and fell, because there is nothing out there
}

} // namespace
