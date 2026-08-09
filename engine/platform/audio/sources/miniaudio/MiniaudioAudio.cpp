/*
Created: 10:08:2026 - 01:53:17
Last updated: 10:08:2026 - 01:53:17
Module: engine/platform/audio
File: engine/platform/audio/sources/miniaudio/MiniaudioAudio.cpp

Responsibility:
- The miniaudio IAudio backend: real sound on the platform's default device.
  Buffer loading through miniaudio's resource manager (decode-once cache),
  one-shot and looping voices with volume/pitch, take-variation playback with
  rotation + pitch jitter, a bus tree of sound groups, 3D listener/emitter
  attenuation, and sample-synchronized layered music.

Key items:
- MiniaudioAudio (file-local) + create_miniaudio_audio() factory.
- Voice sweep in update(): finished one-shots are uninited and their handles
  become safe no-ops (the IAudio recycling contract).

Dependencies:
- Uses: interfaces/IAudio.h, miniaudio 0.11.22 (FetchContent, pinned; the
  implementation macro lives HERE and nowhere else).
- Used by: engine/app wiring (create_miniaudio_audio), audio tests.

Notes:
- ma_engine owns the device thread; every IAudio call happens on the sim
  thread and miniaudio's public API is safe for that split.
- PITCH JITTER in play_variation uses a backend-local RNG seeded from the
  clock. Audio randomness NEVER feeds back into simulation, so determinism
  (Rule 13.2) is untouched — this is the same ruling recorded in IAudio.h.
- KNOWN v1 GAP: set_bus_reverb is a documented no-op. miniaudio ships no
  reverb node; a hand-rolled DSP node arrives with the dungeon-audio stage.
  The factory header carries the same note so no consumer is surprised.
- Attenuation model: linear between min_distance and max_distance — the
  simple rolloff the interface promises ("curve is a backend detail").
- unload_sound stops any voices still playing that sound before dropping the
  cached buffer: a voice outliving its buffer would read freed memory, and
  "your one-shot dies when you unload its sound" is the least surprising rule.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- miniaudio types must never leak out of this directory (Rule 1).
- Contract semantics live in IAudio.h; change them only via group sync.
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Stage 3 audio bring-up — full backend v1 (playback,
                         variation, buses, 3D, layered music; reverb no-op).
*/

#include "engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h"

#define MA_NO_GENERATION // no waveform/noise generators; we decode files only
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace dfn::platform {
namespace {

// Take-variation pitch jitter half-range (playback-rate multiplier). Look-dev
// constant in the WindModel tradition: backend presentation detail, migrates
// to NUMBERS the moment a second zone needs to agree with it (Rule 35).
constexpr float VARIATION_PITCH_JITTER = 0.05f;

class MiniaudioAudio final : public IAudio {
public:
    bool init() override {
        engine_ = std::make_unique<ma_engine>();
        if (ma_engine_init(nullptr, engine_.get()) != MA_SUCCESS) {
            engine_.reset();
            return false; // no device: app falls back to the null backend (Rule 3)
        }
        rng_.seed(static_cast<uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        return true;
    }

    void shutdown() override {
        if (!engine_) {
            return;
        }
        for (auto& [id, voice] : voices_) {
            ma_sound_uninit(voice->sound.get());
        }
        voices_.clear();
        for (auto& [id, m] : music_owned_) {
            for (auto& layer : m->layers) {
                ma_sound_uninit(layer.get());
            }
        }
        music_owned_.clear();
        for (auto& [id, s] : sounds_) {
            ma_sound_uninit(s->prototype.get());
        }
        sounds_.clear();
        for (auto& [id, g] : buses_) {
            ma_sound_group_uninit(g.get());
        }
        buses_.clear();
        ma_engine_uninit(engine_.get());
        engine_.reset();
    }

    void update(const ListenerPose& listener) override {
        if (!engine_) {
            return;
        }
        ma_engine_listener_set_position(engine_.get(), 0, listener.position.x,
                                        listener.position.y, listener.position.z);
        ma_engine_listener_set_direction(engine_.get(), 0, listener.forward.x,
                                         listener.forward.y, listener.forward.z);
        ma_engine_listener_set_world_up(engine_.get(), 0, listener.up.x,
                                        listener.up.y, listener.up.z);

        // Voice sweep: finished one-shots are freed and their handles become
        // safe no-ops (the recycling contract in IAudio.h). Looping voices
        // only leave through stop()/unload_sound().
        for (auto it = voices_.begin(); it != voices_.end();) {
            ma_sound* s = it->second->sound.get();
            if (!ma_sound_is_looping(s) && !ma_sound_is_playing(s)) {
                ma_sound_uninit(s);
                it = voices_.erase(it);
            } else {
                ++it;
            }
        }
        // Music sweep: a stop_music fade that has fully faded is finished.
        for (auto it = music_owned_.begin(); it != music_owned_.end();) {
            Music& m = *it->second;
            if (m.stopping && engine_time_ms() >= m.stop_deadline_ms) {
                for (auto& layer : m.layers) {
                    ma_sound_uninit(layer.get());
                }
                it = music_owned_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Assets -------------------------------------------------------------------
    SoundHandle load_sound(std::string_view path) override {
        if (!engine_) {
            return {};
        }
        auto rec = std::make_unique<Sound>();
        rec->path.assign(path);
        rec->prototype = std::make_unique<ma_sound>();
        // DECODE up front: gameplay one-shots (footsteps) must start on the
        // tick they are asked for, not after an async decode.
        if (ma_sound_init_from_file(engine_.get(), rec->path.c_str(),
                                    MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                    rec->prototype.get()) != MA_SUCCESS) {
            return {};
        }
        const SoundHandle handle{next_id_++};
        sounds_.emplace(handle.id, std::move(rec));
        return handle;
    }

    void unload_sound(SoundHandle sound) override {
        auto it = sounds_.find(sound.id);
        if (it == sounds_.end()) {
            return;
        }
        // Voices still playing this sound die with it (header note).
        for (auto vit = voices_.begin(); vit != voices_.end();) {
            if (vit->second->source.id == sound.id) {
                ma_sound_uninit(vit->second->sound.get());
                vit = voices_.erase(vit);
            } else {
                ++vit;
            }
        }
        ma_sound_uninit(it->second->prototype.get());
        sounds_.erase(it);
    }

    // Playback -----------------------------------------------------------------
    AudioVoiceHandle play(SoundHandle sound, const PlayParams& params) override {
        ma_sound* voice = spawn_voice(sound, params);
        if (voice == nullptr) {
            return {};
        }
        ma_sound_start(voice);
        return AudioVoiceHandle{last_voice_id_};
    }

    AudioVoiceHandle play_variation(std::span<const SoundHandle> takes,
                                    const PlayParams& params) override {
        if (takes.empty()) {
            return {};
        }
        // Rotation: never the same take twice in a row (per take-set, keyed by
        // the first handle — the set identity gameplay hands us each call).
        size_t pick = 0;
        if (takes.size() > 1) {
            std::uniform_int_distribution<size_t> dist(0, takes.size() - 2);
            const size_t offset = dist(rng_);
            const size_t last = last_take_[takes[0].id];
            pick = (last + 1 + offset) % takes.size();
            if (pick == last) {
                pick = (pick + 1) % takes.size();
            }
        }
        last_take_[takes[0].id] = pick;

        PlayParams jittered = params;
        std::uniform_real_distribution<float> jitter(1.0f - VARIATION_PITCH_JITTER,
                                                     1.0f + VARIATION_PITCH_JITTER);
        jittered.pitch = params.pitch * jitter(rng_);
        return play(takes[pick], jittered);
    }

    void stop(AudioVoiceHandle voice) override {
        auto it = voices_.find(voice.id);
        if (it == voices_.end()) {
            return; // stale handle: safe no-op (contract)
        }
        ma_sound_uninit(it->second->sound.get());
        voices_.erase(it);
    }

    void set_voice_position(AudioVoiceHandle voice, const glm::vec3& position) override {
        if (auto it = voices_.find(voice.id); it != voices_.end()) {
            ma_sound_set_position(it->second->sound.get(), position.x, position.y,
                                  position.z);
        }
    }

    void set_voice_volume(AudioVoiceHandle voice, float volume) override {
        if (auto it = voices_.find(voice.id); it != voices_.end()) {
            ma_sound_set_volume(it->second->sound.get(), volume);
        }
    }

    bool is_playing(AudioVoiceHandle voice) const override {
        const auto it = voices_.find(voice.id);
        return it != voices_.end()
               && ma_sound_is_playing(it->second->sound.get()) == MA_TRUE;
    }

    // Buses --------------------------------------------------------------------
    BusHandle create_bus(BusHandle parent) override {
        if (!engine_) {
            return {};
        }
        ma_sound_group* parent_group = find_bus(parent);
        auto group = std::make_unique<ma_sound_group>();
        if (ma_sound_group_init(engine_.get(), 0, parent_group, group.get())
            != MA_SUCCESS) {
            return {};
        }
        const BusHandle handle{next_id_++};
        buses_.emplace(handle.id, std::move(group));
        return handle;
    }

    void set_bus_volume(BusHandle bus, float volume) override {
        if (ma_sound_group* g = find_bus(bus)) {
            ma_sound_group_set_volume(g, volume);
        }
    }

    void set_bus_reverb(BusHandle bus, const ReverbParams& params) override {
        // v1 no-op, DOCUMENTED in the factory header and the module README:
        // miniaudio has no reverb node; the room-reverb pass is a hand-rolled
        // DSP node scheduled with the dungeon-audio stage. Silently pretending
        // would be worse than admitting it.
        (void)bus;
        (void)params;
    }

    // Music layers -------------------------------------------------------------
    MusicHandle play_music(std::span<const SoundHandle> layers, BusHandle bus) override {
        if (!engine_ || layers.empty()) {
            return {};
        }
        auto music = std::make_unique<Music>();
        ma_sound_group* group = find_bus(bus);
        for (const SoundHandle layer : layers) {
            const auto sit = sounds_.find(layer.id);
            if (sit == sounds_.end()) {
                continue;
            }
            auto s = std::make_unique<ma_sound>();
            if (ma_sound_init_copy(engine_.get(), sit->second->prototype.get(),
                                   MA_SOUND_FLAG_DECODE, group, s.get())
                != MA_SUCCESS) {
                continue;
            }
            ma_sound_set_looping(s.get(), MA_TRUE);
            ma_sound_set_volume(s.get(), music->layers.empty() ? 1.0f : 0.0f);
            music->layers.push_back(std::move(s));
        }
        if (music->layers.empty()) {
            return {};
        }
        // Sample-synchronized start: every layer is scheduled to the same
        // engine clock frame slightly in the future, so layer N never starts a
        // buffer-length behind layer 0.
        const ma_uint64 start = ma_engine_get_time_in_pcm_frames(engine_.get())
                                + ma_engine_get_sample_rate(engine_.get()) / 20; // +50 ms
        for (auto& layer : music->layers) {
            ma_sound_set_start_time_in_pcm_frames(layer.get(), start);
            ma_sound_start(layer.get());
        }
        const MusicHandle handle{next_id_++};
        music_owned_.emplace(handle.id, std::move(music));
        return handle;
    }

    void set_music_layer(MusicHandle music, uint32_t layer, float target_volume,
                         float fade_seconds) override {
        auto it = music_owned_.find(music.id);
        if (it == music_owned_.end() || layer >= it->second->layers.size()) {
            return;
        }
        ma_sound_set_fade_in_milliseconds(
            it->second->layers[layer].get(), -1.0f, target_volume,
            static_cast<ma_uint64>(fade_seconds * 1000.0f));
    }

    void stop_music(MusicHandle music, float fade_seconds) override {
        auto it = music_owned_.find(music.id);
        if (it == music_owned_.end()) {
            return;
        }
        const auto fade_ms = static_cast<ma_uint64>(fade_seconds * 1000.0f);
        for (auto& layer : it->second->layers) {
            ma_sound_stop_with_fade_in_milliseconds(layer.get(), fade_ms);
        }
        it->second->stopping = true;
        it->second->stop_deadline_ms = engine_time_ms() + fade_ms;
    }

private:
    struct Sound {
        std::string path;
        std::unique_ptr<ma_sound> prototype; // never started; owns the decode cache ref
    };
    struct Voice {
        std::unique_ptr<ma_sound> sound;
        SoundHandle source; // which Sound this voice was copied from
    };
    struct Music {
        std::vector<std::unique_ptr<ma_sound>> layers;
        bool stopping = false;
        ma_uint64 stop_deadline_ms = 0;
    };

    [[nodiscard]] ma_uint64 engine_time_ms() const {
        return ma_engine_get_time_in_milliseconds(engine_.get());
    }

    [[nodiscard]] ma_sound_group* find_bus(BusHandle bus) {
        const auto it = buses_.find(bus.id);
        return it == buses_.end() ? nullptr : it->second.get(); // null = master
    }

    // Creates (but does not start) a voice as a copy of the loaded sound's
    // prototype, applying every PlayParams field. Returns null on any failure;
    // on success last_voice_id_ names the new handle.
    ma_sound* spawn_voice(SoundHandle sound, const PlayParams& params) {
        if (!engine_) {
            return nullptr;
        }
        const auto sit = sounds_.find(sound.id);
        if (sit == sounds_.end()) {
            return nullptr;
        }
        auto voice = std::make_unique<Voice>();
        voice->source = sound;
        voice->sound = std::make_unique<ma_sound>();
        if (ma_sound_init_copy(engine_.get(), sit->second->prototype.get(),
                               MA_SOUND_FLAG_DECODE, find_bus(params.bus),
                               voice->sound.get()) != MA_SUCCESS) {
            return nullptr;
        }
        ma_sound* s = voice->sound.get();
        ma_sound_set_volume(s, params.volume);
        ma_sound_set_pitch(s, params.pitch);
        ma_sound_set_looping(s, params.loop ? MA_TRUE : MA_FALSE);
        if (params.spatial) {
            ma_sound_set_spatialization_enabled(s, MA_TRUE);
            ma_sound_set_attenuation_model(s, ma_attenuation_model_linear);
            ma_sound_set_position(s, params.spatial_params.position.x,
                                  params.spatial_params.position.y,
                                  params.spatial_params.position.z);
            ma_sound_set_min_distance(s, params.spatial_params.min_distance);
            ma_sound_set_max_distance(s, params.spatial_params.max_distance);
        } else {
            ma_sound_set_spatialization_enabled(s, MA_FALSE);
        }
        last_voice_id_ = next_id_++;
        ma_sound* raw = voice->sound.get();
        voices_.emplace(last_voice_id_, std::move(voice));
        return raw;
    }

    std::unique_ptr<ma_engine> engine_;
    std::unordered_map<uint32_t, std::unique_ptr<Sound>> sounds_;
    std::unordered_map<uint32_t, std::unique_ptr<Voice>> voices_;
    std::unordered_map<uint32_t, std::unique_ptr<ma_sound_group>> buses_;
    std::unordered_map<uint32_t, std::unique_ptr<Music>> music_owned_;
    std::unordered_map<uint32_t, size_t> last_take_; // take-set id -> last index
    std::mt19937 rng_;
    uint32_t next_id_ = 1; // 0 is the invalid handle
    uint32_t last_voice_id_ = 0;
};

} // namespace

std::unique_ptr<IAudio> create_miniaudio_audio() {
    return std::make_unique<MiniaudioAudio>();
}

} // namespace dfn::platform
