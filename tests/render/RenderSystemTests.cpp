/*
Created: 09:08:2026 - 11:13:00
Last updated: 10:08:2026 - 02:30:08
Module: tests
File: tests/render/RenderSystemTests.cpp

Responsibility:
- Headless RenderSystem tests over the null renderer (Rule 3): procedural
  texture upload/caching at init, the water plane lifecycle (set/clear +
  DFN_WATER debug env parsing), environment defaults, clean shutdown.

Key items:
- doctest cases; NullRenderer introspection counters.

Dependencies:
- Uses: doctest, engine/render RenderSystem/Materials, null render backend,
  engine/core ecs World.
- Used by: ctest (render_system).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- GPU-free: null backend only.
*/
/*
UPD:
- 09:08:2026 - 11:13:00: Stage 3 — initial tests.
- 09:08:2026 - 11:57:20: Stage 3b — scatter upload/drop, per-body water,
  site placeholder mesh registry (blessed ids 1..7).
- 09:08:2026 - 20:52:00: Interior lighting: carried lights land at the HAND
  (offset rotated by body yaw) and only MAX_SHADOW_POINT_LIGHTS of them get a
  shadow map. Two counts re-baselined for flora's foliage stream: init now
  uploads three textures (leaf mask atlas) and a scattered chunk builds three
  meshes (branches + leaf cards + micro tile).
- 10:08:2026 - 02:30:08: register_mesh cases (character zone seam): blessed
  body range accepted, collisions and foreign ranges refused, registered id
  resolves in the ECS pass.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/ProcMesh.h"
#include "engine/render/sources/SkyModel.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/geometric.hpp>

#include <cstdlib>
#include <utility>
#include <vector>

using dfn::platform::NullRenderer;
using dfn::render::RenderSystem;

TEST_CASE("init uploads the procedural textures once and shutdown releases all") {
    NullRenderer renderer;
    dfn::platform::RendererInitParams params;
    REQUIRE(renderer.init(params));

    RenderSystem system;
    REQUIRE(system.init(renderer));
    // Terrain atlas + water texture + the leaf mask atlas + the §8.1 path
    // surface atlas (all cached by params — exactly four since the path splat
    // landed).
    CHECK(renderer.live_textures() == 4);
    CHECK_FALSE(system.water_enabled());

    system.shutdown(renderer);
    CHECK(renderer.live_textures() == 0);
    CHECK(renderer.live_meshes() == 0);
}

TEST_CASE("water plane lifecycle: set, replace, clear; sand line follows") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    const uint32_t base_meshes = renderer.live_meshes();

    system.set_water(renderer, 15.0f, {512.0f, 512.0f}, 1024.0f);
    CHECK(system.water_enabled());
    CHECK(renderer.live_meshes() == base_meshes + 1);
    CHECK(system.environment().sand_height_m > 15.0f); // beach just above water

    system.set_water(renderer, 18.0f, {512.0f, 512.0f}, 1024.0f); // replace
    CHECK(renderer.live_meshes() == base_meshes + 1);

    system.clear_water(renderer);
    CHECK_FALSE(system.water_enabled());
    CHECK(renderer.live_meshes() == base_meshes);

    system.shutdown(renderer);
}

TEST_CASE("DFN_WATER debug env enables the plane; malformed value does not") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));

    ::setenv("DFN_WATER", "12.5", 1);
    {
        RenderSystem system;
        REQUIRE(system.init(renderer));
        CHECK(system.water_enabled());
        system.shutdown(renderer);
    }
    ::setenv("DFN_WATER", "wet", 1);
    {
        RenderSystem system;
        REQUIRE(system.init(renderer));
        CHECK_FALSE(system.water_enabled());
        system.shutdown(renderer);
    }
    ::unsetenv("DFN_WATER");
    CHECK(renderer.live_meshes() == 0);
    CHECK(renderer.live_textures() == 0);
}

TEST_CASE("render frame submits terrain atlas + water and stays null-safe") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    system.set_water(renderer, 10.0f, {128.0f, 128.0f}, 512.0f);

    dfn::ecs::World world;
    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    system.render(world, renderer, camera, 0.5f);
    CHECK(renderer.frame_submits() == 1); // water only (no terrain uploaded)

    system.shutdown(renderer);
}

TEST_CASE("environment defaults come from Materials.h look-dev constants") {
    const auto env = dfn::render::make_default_environment(3.0f);
    CHECK(env.sand_height_m == doctest::Approx(3.0f));
    CHECK(env.fog_end_m > env.fog_start_m);
    CHECK(env.fog_color.r == doctest::Approx(env.sky_horizon_color.r));
    CHECK(env.fog_color.g == doctest::Approx(env.sky_horizon_color.g));
    CHECK(env.fog_color.b == doctest::Approx(env.sky_horizon_color.b));
    CHECK(glm::length(env.sun_direction) == doctest::Approx(1.0f));
}

TEST_CASE("scatter upload/drop lifecycle is idempotent per chunk") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    const uint32_t base_meshes = renderer.live_meshes();

    const std::vector<dfn::math::ScatterInstance> instances{
        {{10.0f, 20.0f, 10.0f}, 0.0f, 1.0f, dfn::math::ScatterSpecies::OakTree},
        {{30.0f, 20.0f, 30.0f}, 0.0f, 1.0f, dfn::math::ScatterSpecies::Bush},
    };
    system.upload_scatter(renderer, {0, 0}, instances);
    const uint32_t after_upload = renderer.live_meshes();
    // One wood batch + one card batch, and NO micro tile: routing now asks
    // flora_owns(), and flora owns the bush, so both instances bake into the
    // two flora streams. The foliage stream is a SECOND mesh per chunk because
    // cards need the alpha-cutout program and wood does not.
    CHECK(after_upload == base_meshes + 2);

    system.upload_scatter(renderer, {0, 0}, instances); // replace, no leak
    CHECK(renderer.live_meshes() == after_upload);

    system.drop_scatter(renderer, {0, 0});
    CHECK(renderer.live_meshes() == base_meshes);
    system.drop_scatter(renderer, {0, 0}); // double drop is safe

    system.upload_scatter(renderer, {1, 0}, {}); // empty span -> no meshes
    CHECK(renderer.live_meshes() == base_meshes);
    system.shutdown(renderer);
    CHECK(renderer.live_meshes() == 0);
}

TEST_CASE("water bodies build one mesh per lake and river segment") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    const uint32_t base_meshes = renderer.live_meshes();

    const std::vector<dfn::math::LakePlane> lakes{
        {{230.0f, 520.0f}, {45.0f, 70.0f}, 15.0f}};
    const std::vector<dfn::math::RiverStation> stations{
        {{700.0f, 300.0f}, 30.0f, 2.0f}, {{696.0f, 304.0f}, 29.0f, 2.5f},
        {{692.0f, 308.0f}, 28.5f, 3.0f}, // segment 0
        {{300.0f, 600.0f}, 14.0f, 3.0f}, {{300.0f, 610.0f}, 13.5f, 3.5f},
    };
    const std::vector<uint32_t> offsets{0, 3}; // two segments
    system.set_water_bodies(renderer, lakes, stations, offsets);
    CHECK(renderer.live_meshes() == base_meshes + 3); // 1 lake + 2 ribbons
    CHECK_FALSE(system.water_enabled()); // the debug plane is separate

    system.set_water_bodies(renderer, lakes, stations, offsets); // replace
    CHECK(renderer.live_meshes() == base_meshes + 3);

    system.clear_water_bodies(renderer);
    CHECK(renderer.live_meshes() == base_meshes);
    system.shutdown(renderer);
}

TEST_CASE("a carried light becomes a point light at the HAND, not at the origin") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));

    dfn::ecs::World world;
    const auto carrier = world.spawn();
    // Facing +X: a quarter turn about Y from the identity heading. The carrier
    // origin is the FEET (sim's convention), so the hand offset is +1.45 up.
    const glm::quat yaw = glm::angleAxis(glm::radians(90.0f),
                                         glm::vec3{0.0f, 1.0f, 0.0f});
    world.add(carrier, dfn::components::Transform{
                           {50.0f, 10.0f, 50.0f}, yaw, glm::vec3{1.0f}});
    world.add(carrier, dfn::components::PreviousTransform{
                           {50.0f, 10.0f, 50.0f}, yaw, glm::vec3{1.0f}});
    dfn::components::CarriedLight light;
    light.active = true;
    light.offset = {0.35f, 1.45f, 0.0f}; // right hand, hand height above feet
    world.add(carrier, std::move(light));

    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    system.render(world, renderer, camera, 1.0f);

    const auto& env = system.environment();
    REQUIRE(env.point_light_count == 1);
    // Defaults applied for radius 0 / colour 0.
    CHECK(env.point_lights[0].radius_m == doctest::Approx(dfn::render::TORCH_RADIUS_M));
    CHECK(env.point_lights[0].color.r == doctest::Approx(dfn::render::TORCH_COLOR.r));
    // THE POINT OF THE TEST: the flame is above the feet and displaced
    // sideways, and the sideways part ROTATED with the body — a light left at
    // the carrier origin (or at the eye) casts no visible shadow at all, which
    // is the bug this whole feature exists to avoid.
    CHECK(env.point_lights[0].position.y == doctest::Approx(11.45f));
    CHECK(env.point_lights[0].position.y > 10.0f);
    CHECK(glm::distance(env.point_lights[0].position,
                        glm::vec3{50.0f, 11.45f, 50.0f}) == doctest::Approx(0.35f));
    // Facing +X, the right hand points toward -Z (right-handed, Y up).
    CHECK(env.point_lights[0].position.z == doctest::Approx(49.65f).epsilon(0.01));
    // The first lights are the ones that get a cube map; that is render's
    // decision, never gameplay's.
    CHECK(env.point_lights[0].casts_shadow);

    // Doused: the component stays, the light does not.
    world.get<dfn::components::CarriedLight>(carrier)->active = false;
    system.render(world, renderer, camera, 1.0f);
    CHECK(system.environment().point_light_count == 0);

    system.shutdown(renderer);
}

TEST_CASE("only MAX_SHADOW_POINT_LIGHTS carried lights get a shadow map") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));

    dfn::ecs::World world;
    const uint32_t total = dfn::platform::MAX_SHADOW_POINT_LIGHTS + 2;
    for (uint32_t i = 0; i < total; ++i) {
        const auto e = world.spawn();
        const glm::vec3 p{static_cast<float>(i) * 5.0f, 10.0f, 0.0f};
        world.add(e, dfn::components::Transform{
                         p, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, glm::vec3{1.0f}});
        dfn::components::CarriedLight light;
        light.active = true;
        light.radius_m = 6.0f;
        light.color_rgb = 0x00FF8040u;
        world.add(e, std::move(light));
    }

    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    system.render(world, renderer, camera, 1.0f);

    const auto& env = system.environment();
    REQUIRE(env.point_light_count == total);
    uint32_t shadowed = 0;
    for (uint32_t i = 0; i < env.point_light_count; ++i) {
        CHECK(env.point_lights[i].radius_m == doctest::Approx(6.0f));
        // Explicit colour is honoured (0x00RRGGBB), the default is not.
        CHECK(env.point_lights[i].color.r == doctest::Approx(1.0f));
        shadowed += env.point_lights[i].casts_shadow ? 1u : 0u;
    }
    CHECK(shadowed == dfn::platform::MAX_SHADOW_POINT_LIGHTS);

    system.shutdown(renderer);
}

TEST_CASE("site entities with blessed mesh ids 1..7 are submitted") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));

    dfn::ecs::World world;
    const auto e = world.spawn();
    world.add(e, dfn::components::Transform{{100.0f, 20.0f, 100.0f},
                                            glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
                                            glm::vec3{1.0f}});
    world.add(e, dfn::components::PreviousTransform{{100.0f, 20.0f, 100.0f},
                                                    glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
                                                    glm::vec3{1.0f}});
    world.add(e, dfn::components::RenderMesh{3, 0}); // tavern placeholder

    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    system.render(world, renderer, camera, 0.0f);
    CHECK(renderer.frame_submits() == 1); // the site mesh resolved and drew

    world.get<dfn::components::RenderMesh>(e)->mesh_asset = 99; // unknown: skipped
    system.render(world, renderer, camera, 0.0f);
    CHECK(renderer.frame_submits() == 0);

    system.shutdown(renderer);
}

TEST_CASE("register_mesh accepts the blessed body range and refuses everything foreign") {
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));

    const std::vector<dfn::platform::Vertex> verts{
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu},
        {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu},
    };
    const std::vector<uint32_t> idx{0, 1, 2};

    // The character zone's blessed range registers (34 = pelvis in RIG.md).
    CHECK(system.register_mesh(renderer, dfn::render::BODY_MESH_ID_FIRST, verts, idx));
    CHECK(system.register_mesh(renderer, dfn::render::BODY_MESH_ID_LAST, verts, idx));

    // COLLISION IS REFUSED, not replaced: two zones disagreeing about an id
    // must be heard, and a silent replace is how the drift would hide.
    CHECK_FALSE(system.register_mesh(renderer, dfn::render::BODY_MESH_ID_FIRST,
                                     verts, idx));

    // Foreign ranges are refused even where unoccupied — a typo must not
    // shadow a blessed site id (13..31 are free today), the view model, or
    // an item id.
    CHECK_FALSE(system.register_mesh(renderer, 5, verts, idx));   // site table
    CHECK_FALSE(system.register_mesh(renderer, 13, verts, idx));  // site growth
    CHECK_FALSE(system.register_mesh(renderer, 32, verts, idx));  // view model
    CHECK_FALSE(system.register_mesh(renderer, 64, verts, idx));  // items
    CHECK_FALSE(system.register_mesh(renderer, 0, verts, idx));   // not an id
    CHECK_FALSE(system.register_mesh(renderer, dfn::render::BODY_MESH_ID_LAST + 1,
                                     verts, {}));                 // empty geometry

    // A registered id resolves in the ECS pass exactly like a built-in mesh.
    dfn::ecs::World world;
    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    const auto e = world.spawn();
    world.add(e, dfn::components::Transform{{100.0f, 20.0f, 100.0f},
                                            glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
                                            glm::vec3{1.0f}});
    world.add(e, dfn::components::PreviousTransform{{100.0f, 20.0f, 100.0f},
                                                    glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
                                                    glm::vec3{1.0f}});
    world.add(e, dfn::components::RenderMesh{dfn::render::BODY_MESH_ID_FIRST, 0});
    system.render(world, renderer, camera, 0.0f);
    CHECK(renderer.frame_submits() >= 1);

    system.shutdown(renderer);
    CHECK(renderer.live_meshes() == 0);
}
