/*
Created: 09:08:2026 - 11:13:00
Last updated: 09:08:2026 - 11:13:00
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
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/ecs/sources/World.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/Materials.h"

#include <doctest/doctest.h>

#include <cstdlib>

using dfn::platform::NullRenderer;
using dfn::render::RenderSystem;

TEST_CASE("init uploads the procedural textures once and shutdown releases all") {
    NullRenderer renderer;
    dfn::platform::RendererInitParams params;
    REQUIRE(renderer.init(params));

    RenderSystem system;
    REQUIRE(system.init(renderer));
    // Terrain atlas + water texture (cached by params — exactly two).
    CHECK(renderer.live_textures() == 2);
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
