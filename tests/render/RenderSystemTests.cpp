/*
Created: 09:08:2026 - 11:13:00
Last updated: 19:08:2026 - 04:05:50
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
- 10:08:2026 - 21:12:53: Rule 40 sweep (code audit). The carried-light DIRECTION
  assertion used Approx(49.65f).epsilon(0.01), which admits
  0.01 * (1 + 49.65) = +-0.5065 m -- wider than the whole 0.35 m
  displacement it guards, so z = 50.0 (a light left at the carrier
  origin, the exact bug the case's own comment names) PASSED it. Now an
  explicit-metre residual bound, with two Rule 30 controls that run
  through the real render path: the origin light and the right distance
  in the wrong direction must both FAIL, and the second also PASSES the
  magnitude assertion, which is why direction needed its own line.
- 13:08:2026 - 19:11:13: THE FLAME BREATHES, so two assertions that read a torch's
  colour as a VALUE were hidden clock reads -- green only while the sine sat
  near zero, and red at the wall-clock instant the suite happened to reach
  them. Both are bands now (width from SkyModel.h, not a second copy of 0.12),
  each with the control the band alone cannot fail: b/r for the warm default,
  g/r for the explicit colour. Plus a new case for the thing a band CANNOT
  catch -- that the flame moves at all -- swept over 2 s of PINNED visual clock
  (DFN_VISTIME) with the same-second repeat as its determinism control. Its
  first version sampled two instants 0.145 s apart, from the oscillator's own
  claim of '5.7/9.1 Hz, beat 0.29 s': the rates are not in hertz (the code
  multiplies by tau and by 1/tau, which cancel), the real rates are 0.91 and
  1.45 Hz, and the pair measured 0.0057 where it needed 0.01.
  Shadow casters back to MAX_SHADOW_POINT_LIGHTS (the backend's double-append
  is fixed), so that assertion stands as written.
- 13:08:2026 - 20:37:12: set_visual_time's two cases — what is told is what is used, the same
  time gives the same cloud field back, and an untold system still runs on the
  wall clock (the additive half). The control is the +30 s arm: without it the
  reproducibility assertion would pass on a clock that was never read.
- 15:08:2026 - 16:10:00: счёт процедурных текстур 4 -> 5: добавился лист нормалей коры.
- 19:08:2026 - 02:48:10: Счёт текстур 5 -> 7: две плитки постройки (брус и штукатурка) из листа набора.
- 19:08:2026 - 04:05:50: Счёт текстур на init обратно 5: плитки постройки стали ленивыми.
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

#include <algorithm>
#include <cmath>
#include <cstdio>
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
    // surface atlas + the BARK NORMAL sheet (flora's). Плитки постройки на
    // init НЕ печутся: они ленивые, по первому выбранному материалу — платим
    // только за то, что реально носится. Точное число — и есть прибор: он
    // ловит текстуру, залитую дважды или не освобождённую.
    CHECK(renderer.live_textures() == 5);
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
    // THE COLOUR IS A BAND, NOT A VALUE, since the flame breathes: intensity
    // swings by FLAME_INTENSITY_SWING about the look-dev colour and warmth
    // trims green and blue. `color.r == Approx(TORCH_COLOR.r)` was written
    // before that and failed on a correct flame at a wall-clock time it never
    // controlled — the assertion was a HIDDEN CLOCK READ, green only while
    // the sine happened to be near zero.
    //
    // What survives the flicker is the band on r (warmth does not touch it)...
    CHECK(std::fabs(env.point_lights[0].color.r - dfn::render::TORCH_COLOR.r)
          <= dfn::render::FLAME_INTENSITY_SWING * dfn::render::TORCH_COLOR.r);
    // ...and the HUE, which is the thing the default is actually for: a torch
    // is warm. b/r is the torch's ratio up to the warmth swing, where a
    // white default (the failure this line guards) would read 1.0.
    const float torch_br = dfn::render::TORCH_COLOR.b / dfn::render::TORCH_COLOR.r;
    const float lit_br = env.point_lights[0].color.b / env.point_lights[0].color.r;
    CHECK(std::fabs(lit_br - torch_br) <= dfn::render::FLAME_WARMTH_SWING * torch_br);
    CHECK(lit_br < 0.5f); // the control the band alone cannot fail: not white
    // THE POINT OF THE TEST: the flame is above the feet and displaced
    // sideways, and the sideways part ROTATED with the body — a light left at
    // the carrier origin (or at the eye) casts no visible shadow at all, which
    // is the bug this whole feature exists to avoid.
    CHECK(env.point_lights[0].position.y == doctest::Approx(11.45f));
    CHECK(env.point_lights[0].position.y > 10.0f);
    CHECK(glm::distance(env.point_lights[0].position,
                        glm::vec3{50.0f, 11.45f, 50.0f}) == doctest::Approx(0.35f));
    // Facing +X, the right hand points toward -Z (right-handed, Y up). This is
    // the ONLY assertion of DIRECTION in the case; :243 pins the magnitude.
    //
    // EXPLICIT METRES, not doctest::Approx(...).epsilon() (Rule 40, and the
    // pattern is ClipTests.cpp:170-180): epsilon(e) admits e * (scale +
    // max(|lhs|,|rhs|)) with scale defaulting to 1, so the `.epsilon(0.01)`
    // that stood here admitted 0.01 * (1 + 49.65) = +-0.5065 m -- WIDER THAN
    // THE ENTIRE 0.35 m DISPLACEMENT IT GUARDS. z = 50.0, a light left at the
    // carrier origin with no lateral offset at all, satisfied it
    // (|50.0 - 49.65| = 0.35 < 0.5065): the assertion admitted the exact bug
    // the comment eight lines up says this test exists to catch.
    //
    // The quantity is a RESIDUAL against an analytically exact value
    // (origin + yaw * offset, a quarter turn), whose correct value is zero, so
    // the bound is float rounding and nothing else -- not a tolerance anyone
    // chose about the world.
    constexpr float HAND_POS_EXACT_M = 1e-4f;
    CHECK(std::fabs(env.point_lights[0].position.z - 49.65f) < HAND_POS_EXACT_M);
    // The first lights are the ones that get a cube map; that is render's
    // decision, never gameplay's.
    CHECK(env.point_lights[0].casts_shadow);

    // CONTROL (Rule 30), and it runs through the real code rather than
    // restating arithmetic: the named bug -- a light left at the carrier
    // ORIGIN, offset stripped of its lateral component -- must FAIL the
    // assertion above. It passed the old band, which is the whole finding.
    world.get<dfn::components::CarriedLight>(carrier)->offset = {0.0f, 1.45f, 0.0f};
    system.render(world, renderer, camera, 1.0f);
    REQUIRE(system.environment().point_light_count == 1);
    const float origin_z = system.environment().point_lights[0].position.z;
    CHECK(origin_z == doctest::Approx(50.0f)); // it really is at the origin
    CHECK_FALSE(std::fabs(origin_z - 49.65f) < HAND_POS_EXACT_M);
    // ...and the OTHER failure the magnitude assertion cannot see: the right
    // distance in the wrong direction. Offset +0.35 along body-forward, facing
    // +X, displaces along +X and leaves z at 50.0 -- so it satisfies :243's
    // 0.35 m magnitude exactly while pointing the torch nowhere near the hand.
    world.get<dfn::components::CarriedLight>(carrier)->offset = {0.0f, 1.45f, -0.35f};
    system.render(world, renderer, camera, 1.0f);
    REQUIRE(system.environment().point_light_count == 1);
    const glm::vec3 wrong_dir = system.environment().point_lights[0].position;
    CHECK(glm::distance(wrong_dir, glm::vec3{50.0f, 11.45f, 50.0f})
          == doctest::Approx(0.35f)); // passes the magnitude check...
    CHECK_FALSE(std::fabs(wrong_dir.z - 49.65f) < HAND_POS_EXACT_M); // ...fails this
    world.get<dfn::components::CarriedLight>(carrier)->offset = {0.35f, 1.45f, 0.0f};

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
        // Explicit colour is honoured (0x00RRGGBB), the default is not — and
        // the assertion is a band because the flame breathes (see the hand
        // case above). 0xFF8040 is r 1.000 / g 0.502 / b 0.251.
        CHECK(std::fabs(env.point_lights[i].color.r - 1.0f)
              <= dfn::render::FLAME_INTENSITY_SWING);
        // g/r separates the REQUESTED colour (128/255) from the torch default
        // (0.620) by 0.118, which is 9.4x the widest the warmth trim can move
        // it (g *= 1 - warmth*0.5, so +-0.0126) — so this line fails if the
        // explicit colour is dropped, and the default cannot satisfy it at any
        // phase of the flicker.
        const float asked_gr = 128.0f / 255.0f;
        const float gr = env.point_lights[i].color.g / env.point_lights[i].color.r;
        CHECK(std::fabs(gr - asked_gr)
              <= 0.5f * dfn::render::FLAME_WARMTH_SWING * asked_gr);
        shadowed += env.point_lights[i].casts_shadow ? 1u : 0u;
    }
    CHECK(shadowed == dfn::platform::MAX_SHADOW_POINT_LIGHTS);

    system.shutdown(renderer);
}

TEST_CASE("the flame breathes, and a dead flame is what the band cannot catch") {
    // THE COMPANION TO THE TWO BAND ASSERTIONS ABOVE. A band admits a flame
    // that has stopped moving — it is the widest possible pass — so the fact
    // that the light MOVES needs its own case, and it needs a pinned clock:
    // the flicker is a function of env.time_seconds, which is wall time unless
    // DFN_VISTIME says otherwise. Same hook the cloud-drift pair uses; it is
    // read in init(), so it is set before each system is built.
    const auto light_at = [](const char* seconds) {
        setenv("DFN_VISTIME", seconds, 1);
        NullRenderer renderer;
        REQUIRE(renderer.init({}));
        RenderSystem system;
        REQUIRE(system.init(renderer));
        dfn::ecs::World world;
        const auto e = world.spawn();
        world.add(e, dfn::components::Transform{
                         {0.0f, 0.0f, 0.0f},
                         glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, glm::vec3{1.0f}});
        dfn::components::CarriedLight light;
        light.active = true;
        world.add(e, std::move(light));
        dfn::render::FirstPersonCamera camera;
        camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        system.render(world, renderer, camera, 1.0f);
        REQUIRE(system.environment().point_light_count == 1);
        const glm::vec3 c = system.environment().point_lights[0].color;
        system.shutdown(renderer);
        return c;
    };
    // SWEPT, NOT SAMPLED TWICE, and the first version of this case is why: two
    // instants 0.145 s apart differed by 0.0057, which I had predicted would
    // clear 0.01 from the oscillator's stated "5.7/9.1 Hz, beat period 0.29 s".
    // The rates are NOT in hertz — `5.7f * 6.2831853f * 0.1591549f` multiplies
    // by tau and by 1/tau, which cancel — so the flame breathes at 0.91 and
    // 1.45 Hz and beats every 1.85 s, and a pair chosen from the documented
    // period lands wherever it happens to land. A sweep over two seconds
    // cannot be defeated by that; it also states the real claim, which is
    // about the SPAN the flame covers and not about any two instants.
    float lo = 2.0f;
    float hi = 0.0f;
    for (int i = 0; i <= 10; ++i) {
        char t[16];
        std::snprintf(t, sizeof(t), "%.2f", static_cast<float>(i) * 0.2f);
        const float r = light_at(t).r;
        lo = std::min(lo, r);
        hi = std::max(hi, r);
        // Every sample is inside the band the other two cases assert.
        CHECK(std::fabs(r - dfn::render::TORCH_COLOR.r)
              <= dfn::render::FLAME_INTENSITY_SWING * dfn::render::TORCH_COLOR.r);
    }
    unsetenv("DFN_VISTIME");
    // Half the band is the claim: a flame that had stopped would give 0.
    CHECK(hi - lo > 0.5f * dfn::render::FLAME_INTENSITY_SWING
                        * dfn::render::TORCH_COLOR.r);
    const glm::vec3 a = light_at("0.0");
    // THE CLOCK IS PINNED, NOT IGNORED: the same pinned second gives the same
    // colour twice. Without this a flicker driven by an unpinned wall clock
    // would pass the line above and break every screenshot recipe in the
    // project, which is the reason the oscillator reads the visual clock.
    const glm::vec3 a2 = light_at("0.0");
    unsetenv("DFN_VISTIME");
    CHECK(a2.r == doctest::Approx(a.r));
}

TEST_CASE("the visual clock can be TOLD, and a told clock is reproducible") {
    // THE SKY HAD TWO CLOCKS. The sun and moon run off the app's game clock,
    // which a tour advances by a fixed step per frame so the world is a pure
    // function of the frame index; the cloud drift and the wind envelope ran
    // off a steady_clock read in here. Measured on the sky probe, two runs of
    // one binary on one recipe: 67.466 % of pixels differed with the clock
    // free and 0.000 % — byte for byte — with it pinned. This case is the unit
    // half of that: what is told is what is used, and telling the same thing
    // twice gives the same frame environment.
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    dfn::ecs::World world;
    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);

    system.set_visual_time(1234.5);
    system.render(world, renderer, camera, 1.0f);
    CHECK(system.environment().time_seconds == doctest::Approx(1234.5f));
    const glm::vec2 drift_a = system.environment().cloud_offset_m;

    // A DIFFERENT time must move the cloud field — otherwise the assertion
    // below would pass on a clock that was never read at all (Rule 30).
    system.set_visual_time(1264.5); // +30 s, the drift pair's own interval
    system.render(world, renderer, camera, 1.0f);
    const glm::vec2 drift_b = system.environment().cloud_offset_m;
    CHECK(glm::distance(drift_a, drift_b) > 1.0f);

    // ...and the same time gives the same field back, which is the property
    // the whole project's acceptance method rests on.
    system.set_visual_time(1234.5);
    system.render(world, renderer, camera, 1.0f);
    CHECK(system.environment().time_seconds == doctest::Approx(1234.5f));
    CHECK(system.environment().cloud_offset_m.x == doctest::Approx(drift_a.x));
    CHECK(system.environment().cloud_offset_m.y == doctest::Approx(drift_a.y));

    system.shutdown(renderer);
}

TEST_CASE("a RenderSystem nobody tells the time still reads the wall clock") {
    // The additive half: until a caller says anything, behaviour is what
    // shipped. Two frames of an untold system advance the visual clock on
    // their own, which is exactly the property that made the recipe
    // irreproducible — and it stays, because a caller that never opted in must
    // not have its behaviour changed underneath it.
    NullRenderer renderer;
    REQUIRE(renderer.init({}));
    RenderSystem system;
    REQUIRE(system.init(renderer));
    dfn::ecs::World world;
    dfn::render::FirstPersonCamera camera;
    camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    system.render(world, renderer, camera, 1.0f);
    const float first = system.environment().time_seconds;
    CHECK(first >= 0.0f);
    CHECK(first < 60.0f); // seconds since this object was constructed
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
