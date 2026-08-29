/*
Module: engine/platform/audio
File: engine/platform/audio/sources/null/NullAudio.cpp

Responsibility:
- Null IAudio backend (Rule 3): every call succeeds and plays nothing;
  handles are valid-but-inert; is_playing() is always false.

Key items:
- NullAudio (file-local) + create_null_audio() factory.

Dependencies:
- Uses: interfaces/IAudio.h, C++ stdlib.
- Used by: engine/app wiring, tests, headless tours.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics here are contract (IAudio.h notes); keep them in sync.
*/

#include "engine/platform/audio/sources/null/CreateNullAudio.h"

namespace dfn::platform {
namespace {

class NullAudio final : public IAudio {
public:
    bool init() override { return true; }
    void shutdown() override {}
    void update(const ListenerPose& listener) override { (void)listener; }

    SoundHandle load_sound(std::string_view path) override {
        (void)path;
        return SoundHandle{next_id_++};
    }
    void unload_sound(SoundHandle sound) override { (void)sound; }

    AudioVoiceHandle play(SoundHandle sound, const PlayParams& params) override {
        (void)sound;
        (void)params;
        return AudioVoiceHandle{next_id_++};
    }
    AudioVoiceHandle play_variation(std::span<const SoundHandle> takes,
                                    const PlayParams& params) override {
        (void)takes;
        (void)params;
        return AudioVoiceHandle{next_id_++};
    }

    void stop(AudioVoiceHandle voice) override { (void)voice; }
    void set_voice_position(AudioVoiceHandle voice, const glm::vec3& position) override {
        (void)voice;
        (void)position;
    }
    void set_voice_volume(AudioVoiceHandle voice, float volume) override {
        (void)voice;
        (void)volume;
    }
    void set_voice_lowpass(AudioVoiceHandle voice, float cutoff_hz) override {
        (void)voice;
        (void)cutoff_hz;
    }
    bool is_playing(AudioVoiceHandle voice) const override {
        (void)voice;
        return false; // contract: nothing ever plays in null
    }

    BusHandle create_bus(BusHandle parent) override {
        (void)parent;
        return BusHandle{next_id_++};
    }
    void set_bus_volume(BusHandle bus, float volume) override {
        (void)bus;
        (void)volume;
    }
    void set_bus_reverb(BusHandle bus, const ReverbParams& params) override {
        (void)bus;
        (void)params;
    }

    MusicHandle play_music(std::span<const SoundHandle> layers, BusHandle bus) override {
        (void)layers;
        (void)bus;
        return MusicHandle{next_id_++};
    }
    void set_music_layer(MusicHandle music, uint32_t layer, float target_volume,
                         float fade_seconds) override {
        (void)music;
        (void)layer;
        (void)target_volume;
        (void)fade_seconds;
    }
    void stop_music(MusicHandle music, float fade_seconds) override {
        (void)music;
        (void)fade_seconds;
    }

private:
    uint32_t next_id_ = 1; // 0 is the invalid handle
};

} // namespace

std::unique_ptr<IAudio> create_null_audio() {
    return std::make_unique<NullAudio>();
}

} // namespace dfn::platform
