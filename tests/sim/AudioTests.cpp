/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: tests (sim zone)
File: tests/sim/AudioTests.cpp

Responsibility:
- The miniaudio backend contract: loading real (generated) wavs, one-shot and
  loop playback, variation sets, buses, and the failure paths — plus the
  StepAudio bank loader against the shipped placeholder assets.

Key items:
- Device-gated: if the machine has no audio device, init() returning false is
  itself the tested contract and the playback cases are skipped (a headless
  CI box is a legitimate environment, not a failure).
- Rule 30 controls: a nonexistent file must NOT load; a stale voice handle
  must report not-playing.

Dependencies:
- Uses: doctest, platform audio (miniaudio + null), gameplay StepAudio,
  DFN_REPO_ROOT (compile definition from tests/sim.cmake) for asset paths.
- Used by: ctest (sim_audio).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Created with the audio bring-up.
*/

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

#include "engine/gameplay/sources/StepAudio.h"
#include "engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h"
#include "engine/platform/audio/sources/null/CreateNullAudio.h"

namespace {

namespace platform = dfn::platform;
namespace gameplay = dfn::gameplay;

const std::string ASSETS = std::string(DFN_REPO_ROOT) + "/games/daggerfall_n/assets/audio";

// One backend per suite run; miniaudio device init is not free.
platform::IAudio* audio_or_skip() {
    static std::unique_ptr<platform::IAudio> audio = [] {
        auto a = platform::create_miniaudio_audio();
        if (!a->init()) {
            a.reset(); // no device: the null-fallback contract, tested below
        }
        return a;
    }();
    return audio.get();
}

} // namespace

TEST_CASE("miniaudio: load, play, finish; broken path refuses (control)") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        MESSAGE("no audio device: init()==false is the contract; playback skipped");
        return;
    }

    // CONTROL (Rule 30): a path that does not exist must not produce a handle.
    CHECK(!audio->load_sound(ASSETS + "/no_such_file.wav").valid());

    const platform::SoundHandle sound =
        audio->load_sound(ASSETS + "/footstep_rock_1.wav");
    REQUIRE(sound.valid());

    platform::PlayParams params;
    params.volume = 0.0f; // silent for CI ears; volume does not gate playback
    const platform::AudioVoiceHandle voice = audio->play(sound, params);
    REQUIRE(voice.valid());
    CHECK(audio->is_playing(voice));

    // The rock take is ~0.11 s; after waiting past it the voice must be done
    // and its handle recycled to a safe no-op (the sweep runs in update()).
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    audio->update({});
    CHECK(!audio->is_playing(voice));
    audio->set_voice_volume(voice, 1.0f); // stale handle: must be a safe no-op
    audio->stop(voice);
}

TEST_CASE("miniaudio: variation sets and loops") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        return;
    }

    platform::SoundHandle takes[3];
    for (int i = 0; i < 3; ++i) {
        takes[i] = audio->load_sound(ASSETS + "/footstep_grass_" + std::to_string(i + 1)
                                     + ".wav");
        REQUIRE(takes[i].valid());
    }
    platform::PlayParams params;
    params.volume = 0.0f;
    for (int i = 0; i < 8; ++i) {
        CHECK(audio->play_variation({takes, 3}, params).valid());
    }

    // A loop keeps playing past its own length until stopped.
    params.loop = true;
    const platform::AudioVoiceHandle loop = audio->play(takes[0], params);
    REQUIRE(loop.valid());
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    audio->update({});
    CHECK(audio->is_playing(loop)); // control vs the one-shot case above
    audio->stop(loop);
    CHECK(!audio->is_playing(loop));
}

TEST_CASE("step sound bank loads the full placeholder set") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        // The bank must also survive a null backend (Rule 3: runnable mode).
        auto null_audio = platform::create_null_audio();
        REQUIRE(null_audio->init());
        const auto bank = gameplay::load_step_sound_bank(*null_audio, ASSETS, {});
        CHECK(bank.jump.valid()); // null loads always succeed by contract
        return;
    }
    const auto bank = gameplay::load_step_sound_bank(*audio, ASSETS, {});
    for (int s = 0; s < 5; ++s) {
        CHECK(bank.takes[s].size() == 4); // four takes per surface class
    }
    CHECK(bank.jump.valid());
    CHECK(bank.land_soft.valid());
    CHECK(bank.land_hard.valid());
    CHECK(bank.splash.valid());
    CHECK(bank.wind_loop.valid());

    // The wind loop starts silent and follows the strength it is fed.
    const gameplay::WindLoop wind = gameplay::start_wind_loop(*audio, bank);
    CHECK(wind.voice.valid());
    gameplay::update_wind_loop(*audio, wind, 0.5f);
    CHECK(audio->is_playing(wind.voice));
    audio->stop(wind.voice);
}
