/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: engine/gameplay
File: engine/gameplay/sources/StepAudio.cpp

Responsibility:
- Implementation of the step/ambient audio consumers (see StepAudio.h).

Key items:
- load_step_sound_bank / wire_step_audio / start_wind_loop / update_wind_loop.
- Look-dev volume constants (WindModel tradition; migrate to NUMBERS when a
  second zone must agree — Rule 35).

Dependencies:
- Uses: StepAudio.h, StepEvents.h, EventBus, generated constants.
- Used by: engine/app wiring, tests.

Notes:
- The hard-landing threshold is DERIVED from JUMP_HEIGHT: a standing jump
  lands at sqrt(2·G·H) and must sound soft; 1.25x that speed (a fall from
  ~1.6 m) turns hard. Deriving keeps it true when the jump is retuned.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Handlers must stay cheap: they run inside the tick's pump.
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Created for the landscape stage (в3+в12 audio).
*/

#include "engine/gameplay/sources/StepAudio.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/StepEvents.h"

namespace dfn::gameplay {

namespace {

// Look-dev mix constants (see header note).
constexpr float STEP_VOLUME_MAX = 0.9f;
constexpr float STEP_VOLUME_MIN = 0.35f; // a creeping step still whispers
constexpr float LAND_VOLUME = 1.0f;
constexpr float JUMP_VOLUME = 0.5f;
constexpr float SPLASH_VOLUME = 1.0f;
constexpr float WIND_VOLUME_SCALE = 0.6f;
// Spatial envelope for feet: full volume within arm's reach (the player's own
// feet are at distance ~0), silent past earshot. Serves NPC feet unchanged.
constexpr float STEP_MIN_DISTANCE = 2.0f;
constexpr float STEP_MAX_DISTANCE = 30.0f;

constexpr float RUN = static_cast<float>(config::RUN_SPEED);

// Derived hard-landing threshold (header note): 1.25x the standing-jump
// impact speed sqrt(2·G·JUMP_HEIGHT).
const float HARD_LANDING_SPEED =
    1.25f * std::sqrt(2.0f * static_cast<float>(config::GRAVITY)
                      * static_cast<float>(config::JUMP_HEIGHT));

[[nodiscard]] platform::PlayParams spatial_at(const glm::vec3& position,
                                              platform::BusHandle bus, float volume) {
    platform::PlayParams params;
    params.bus = bus;
    params.volume = volume;
    params.spatial = true;
    params.spatial_params.position = position;
    params.spatial_params.min_distance = STEP_MIN_DISTANCE;
    params.spatial_params.max_distance = STEP_MAX_DISTANCE;
    return params;
}

} // namespace

StepSoundBank load_step_sound_bank(platform::IAudio& audio, std::string_view dir,
                                   platform::BusHandle bus) {
    StepSoundBank bank;
    bank.bus = bus;
    const std::string base(dir);
    // Index = math::SurfaceClass value (Grass, GrassRockBlend, Rock, Sand,
    // WaterBed). The blend class sounds like gravel: half grass, half rock is
    // exactly what gravel over soil is.
    const char* names[5] = {"grass", "gravel", "rock", "sand", "water"};
    static_assert(static_cast<int>(math::SurfaceClass::Grass) == 0
                      && static_cast<int>(math::SurfaceClass::GrassRockBlend) == 1
                      && static_cast<int>(math::SurfaceClass::Rock) == 2
                      && static_cast<int>(math::SurfaceClass::Sand) == 3
                      && static_cast<int>(math::SurfaceClass::WaterBed) == 4,
                  "surface-class order changed: re-map the take table");
    for (int s = 0; s < 5; ++s) {
        for (int take = 1; take <= 4; ++take) {
            const platform::SoundHandle h = audio.load_sound(
                base + "/footstep_" + names[s] + "_" + std::to_string(take) + ".wav");
            if (h.valid()) {
                bank.takes[s].push_back(h);
            }
        }
    }
    bank.jump = audio.load_sound(base + "/jump_takeoff.wav");
    bank.land_soft = audio.load_sound(base + "/land_soft.wav");
    bank.land_hard = audio.load_sound(base + "/land_hard.wav");
    bank.splash = audio.load_sound(base + "/splash_enter.wav");
    bank.wind_loop = audio.load_sound(base + "/wind_loop.wav");
    return bank;
}

void wire_step_audio(events::EventBus& bus, platform::IAudio& audio,
                     const StepSoundBank& bank) {
    // The bank is captured BY VALUE: handles are PODs and the copy unties the
    // subscriptions from the caller's storage lifetime.
    bus.subscribe<FootfallEvent>([&audio, bank](const FootfallEvent& e) {
        // A wet step splashes whatever the bed is (wading override).
        const int surface = e.wading ? static_cast<int>(math::SurfaceClass::WaterBed)
                                     : static_cast<int>(e.surface);
        const auto& takes = bank.takes[surface];
        if (takes.empty()) {
            return; // missing assets: silent, not broken (Rule 3 spirit)
        }
        const float volume = std::clamp(e.speed / RUN, STEP_VOLUME_MIN, STEP_VOLUME_MAX);
        (void)audio.play_variation(takes, spatial_at(e.position, bank.bus, volume));
    });

    bus.subscribe<Jumped>([&audio, bank](const Jumped& e) {
        if (bank.jump.valid()) {
            (void)audio.play(bank.jump, spatial_at(e.position, bank.bus, JUMP_VOLUME));
        }
    });

    bus.subscribe<Landed>([&audio, bank](const Landed& e) {
        if (e.in_water) {
            if (bank.splash.valid()) {
                (void)audio.play(bank.splash,
                                 spatial_at(e.position, bank.bus, SPLASH_VOLUME));
            }
            return;
        }
        const platform::SoundHandle sound =
            e.impact_speed > HARD_LANDING_SPEED ? bank.land_hard : bank.land_soft;
        if (sound.valid()) {
            // Louder the harder: scale within [0.5, 1] across the soft range.
            const float volume =
                LAND_VOLUME * std::clamp(0.5f + 0.5f * e.impact_speed / HARD_LANDING_SPEED,
                                         0.5f, 1.0f);
            (void)audio.play(sound, spatial_at(e.position, bank.bus, volume));
        }
    });

    bus.subscribe<WaterEntered>([&audio, bank](const WaterEntered& e) {
        if (bank.splash.valid()) {
            (void)audio.play(bank.splash, spatial_at(e.position, bank.bus, SPLASH_VOLUME));
        }
    });
}

WindLoop start_wind_loop(platform::IAudio& audio, const StepSoundBank& bank) {
    WindLoop loop;
    if (!bank.wind_loop.valid()) {
        return loop;
    }
    platform::PlayParams params;
    params.bus = bank.bus;
    params.volume = 0.0f; // update_wind_loop sets the real volume each frame
    params.loop = true;
    params.spatial = false; // the wind is everywhere, not a point
    loop.voice = audio.play(bank.wind_loop, params);
    return loop;
}

void update_wind_loop(platform::IAudio& audio, const WindLoop& loop,
                      float wind_strength) {
    if (loop.voice.valid()) {
        audio.set_voice_volume(loop.voice,
                               WIND_VOLUME_SCALE * std::clamp(wind_strength, 0.0f, 1.0f));
    }
}

} // namespace dfn::gameplay
