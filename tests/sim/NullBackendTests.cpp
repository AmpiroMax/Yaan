/*
Created: 09:08:2026 - 00:45:08
Last updated: 27:08:2026 - 11:57:52
Module: tests
File: tests/sim/NullBackendTests.cpp

Responsibility:
- Verifies every sim-zone null backend honors its contract (Rule 3: runnable
  modes): null llm returns fallback_text instantly, null audio succeeds
  silently, null anim fills identity, null physics glides and never crashes.

Key items:
- Doctest cases per backend, driven only through the public interfaces.

Dependencies:
- Uses: doctest, dfn_platform_{physics,anim,audio,llm}, engine/physics
  CollisionLayers.h (header-only layer constants).
- Used by: ctest (sim_null_backends).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- These tests ARE the null contracts; change them only with the interface docs.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial null backend contract suite.
- 09:08:2026 - 15:08:24: Added the zero-mask rejection case; existing physics
                         cases now set explicit layers (the default-layer
                         mistake is now rejected by contract).
- 27:08:2026 - 11:57:52: контракт null для sphere_cast — промах, как у луча.
*/

#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/anim/sources/null/CreateNullAnim.h"
#include "engine/platform/audio/sources/null/CreateNullAudio.h"
#include "engine/platform/llm/sources/null/CreateNullLlm.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace platform = dfn::platform;
// Aliased: the test cases keep a local `physics` unique_ptr, which would
// otherwise shadow the dfn::physics namespace.
namespace physics_layer = dfn::physics;

TEST_CASE("null physics: zero-mask bodies are rejected, matching Jolt") {
    // The null backend is a runnable mode, so it must catch the same authoring
    // mistake the real backend does (IPhysics.h zero-mask rejection).
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());

    platform::TerrainDesc terrain; // layer defaults to 0
    CHECK_FALSE(physics->create_terrain(terrain).valid());
    CHECK_FALSE(physics->create_static_box(platform::StaticBoxDesc{}).valid());

    platform::CharacterDesc character; // layer 0
    character.collides_with = physics_layer::LAYER_STATIC;
    CHECK_FALSE(physics->create_character(character).valid());

    character.layer = physics_layer::LAYER_CHARACTER; // collides with nothing
    character.collides_with = 0;
    CHECK_FALSE(physics->create_character(character).valid());
    physics->shutdown();
}

TEST_CASE("null llm: instant scripted fallback (Q62/Q67)") {
    auto llm = platform::create_null_llm();
    REQUIRE(llm->init({}));

    platform::CompletionRequest request;
    request.prompt = "Greet the player as a tired gate guard.";
    request.fallback_text = "scripted.greeting.line"; // scripted words, verbatim
    const auto handle = llm->submit(request);
    REQUIRE(handle.valid());

    // Done immediately — no polling delay, no thread.
    CHECK(llm->status(handle) == platform::LlmRequestStatus::Done);

    platform::CompletionResult result;
    REQUIRE(llm->try_get_result(handle, result));
    CHECK(result.status == platform::LlmRequestStatus::Done);
    CHECK(result.text == request.fallback_text);
    CHECK(result.from_fallback);

    // Handle is stale after retrieval.
    CHECK_FALSE(llm->try_get_result(handle, result));
    CHECK(llm->status(handle) == platform::LlmRequestStatus::Invalid);

    llm->set_inference_allowed(false); // gate is a safe no-op
    llm->cancel(handle);               // stale cancel is a safe no-op
    CHECK_FALSE(llm->active_model().loaded);
    llm->shutdown();
}

TEST_CASE("null audio: everything succeeds, nothing plays") {
    auto audio = platform::create_null_audio();
    REQUIRE(audio->init());

    const auto sound = audio->load_sound("assets/sfx/does_not_exist.opus");
    CHECK(sound.valid());

    platform::PlayParams params;
    const auto voice = audio->play(sound, params);
    CHECK(voice.valid());
    CHECK_FALSE(audio->is_playing(voice)); // contract: silence

    const std::array<platform::SoundHandle, 2> takes{sound, sound};
    CHECK(audio->play_variation(takes, params).valid());

    const auto bus = audio->create_bus({});
    CHECK(bus.valid());
    audio->set_bus_volume(bus, 0.5f);
    audio->set_bus_reverb(bus, platform::ReverbParams{2.0f, 30.0f, 0.4f});

    const std::array<platform::SoundHandle, 2> layers{sound, sound};
    const auto music = audio->play_music(layers, bus);
    CHECK(music.valid());
    audio->set_music_layer(music, 1, 1.0f, 2.0f);
    audio->stop_music(music, 1.0f);

    audio->update(platform::ListenerPose{});
    audio->shutdown();
}

TEST_CASE("null anim: loads succeed, evaluate writes identity") {
    auto anim = platform::create_null_anim();
    REQUIRE(anim->init());

    const auto skeleton = anim->load_skeleton("assets/rigs/humanoid.ozz");
    REQUIRE(skeleton.valid());
    CHECK(anim->joint_count(skeleton) == 0); // contract: no joints in null

    const auto clip = anim->load_clip(skeleton, "assets/anim/walk.ozz");
    REQUIRE(clip.valid());
    CHECK(anim->clip_duration(clip) == 0.0f);

    const auto instance = anim->create_instance(skeleton);
    REQUIRE(instance.valid());

    std::vector<glm::mat4> palette(8, glm::mat4{0.0f});
    const std::array<platform::AnimLayer, 1> layers{{{clip, 0.5f, 1.0f}}};
    REQUIRE(anim->evaluate(instance, layers, palette));
    for (const auto& matrix : palette) {
        CHECK(matrix == glm::mat4{1.0f}); // bind pose
    }
    anim->shutdown();
}

TEST_CASE("null physics: horizontal glide, always grounded, rays miss") {
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());

    // Bodies are valid-but-inert. A non-zero layer is mandatory (zero-mask
    // rejection is contract for every backend, IPhysics.h).
    platform::TerrainDesc terrain;
    terrain.layer = physics_layer::LAYER_STATIC;
    CHECK(physics->create_terrain(terrain).valid());
    platform::StaticBoxDesc box;
    box.layer = physics_layer::LAYER_STATIC;
    CHECK(physics->create_static_box(box).valid());

    platform::CharacterDesc desc;
    desc.position = {1.0f, 2.0f, 3.0f};
    desc.layer = physics_layer::LAYER_CHARACTER;
    desc.collides_with = physics_layer::LAYER_STATIC;
    const auto character = physics->create_character(desc);
    REQUIRE(character.valid());

    physics->move_character(character, {1.0f, -5.0f, 0.5f});
    physics->step(1.0f / 60.0f);
    const glm::vec3 position = physics->character_position(character);
    CHECK(position.x == doctest::Approx(2.0f)); // horizontal applied
    CHECK(position.y == doctest::Approx(2.0f)); // vertical ignored (Q31)
    CHECK(position.z == doctest::Approx(3.5f));
    CHECK(physics->character_grounded(character));

    const auto hit = physics->raycast({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f);
    CHECK_FALSE(hit.hit); // contract: null rays always miss
    // И СВЁРНУТЫЙ ОБЪЁМ ТОЖЕ (27.08). Запрос добавлен ради коллизии камеры;
    // бэкенд, у которого сфера «попадает во что-нибудь», прижал бы камеру к
    // голове во всех тестах, идущих на null, — то есть молча поменял бы вид
    // там, где по контракту не должно быть НИЧЕГО.
    const auto sweep = physics->sphere_cast({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                                            0.25f, 100.0f);
    CHECK_FALSE(sweep.hit);

    physics->teleport_character(character, {0.0f, 0.0f, 0.0f});
    CHECK(physics->character_position(character) == glm::vec3{0.0f});
    physics->shutdown();
}

} // namespace
