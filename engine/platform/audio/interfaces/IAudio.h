/*
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
Module: engine/platform/audio
File: engine/platform/audio/interfaces/IAudio.h

Responsibility:
- The platform audio contract (Rule 0). Sound loading and playback, 3D
  spatialization, buses with reverb, layered music; miniaudio lives only behind it.

Key items:
- IAudio: init/shutdown/update(listener), load/play/stop, buses, reverb params,
  variation playback (footsteps, Q68), layered music.
- PlayParams / Spatial3d / ReverbParams: plain-data descriptors.
- SoundHandle / AudioVoiceHandle / BusHandle / MusicHandle: opaque POD handles
  (0 = invalid).

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else.
- Used by: engine/gameplay (footsteps, dialogue voice, interaction sounds),
  engine/app (music, bus setup), tests (null backend).

Notes:
- Q68 priorities are served by two hooks, both backend-side:
  * Footsteps by surface material: gameplay maps material -> a set of takes and
    calls play_variation(); the backend picks a take (round-robin/random with
    pitch jitter) so repeated steps do not machine-gun. Audio randomness never
    feeds back into simulation, so determinism (Rule 13) is unaffected.
  * Reverb by room volume: gameplay/world computes ReverbParams from the current
    space and applies them to a bus via set_bus_reverb(); "dungeon sounds like a
    dungeon" is a bus property, not per-voice work.
- Buses form a tree under an implicit master; the engine's fixed bus set (sfx /
  music / voice / ambient) is created by app at startup — names are content, not
  contract.
- Layered music (adaptive): play_music() starts all layers sample-synchronized;
  set_music_layer() fades individual layers, re-mixing intensity without
  restarting the track.
- Units: meters and seconds (Rule 14); volumes are linear multipliers (1 =
  neutral), pitch is a playback-rate multiplier (1 = original).
- Handles for finished one-shot voices are recycled by the backend; calls on a
  stale AudioVoiceHandle are safe no-ops (is_playing() returns false).
- Null backend (Rule 3 — a runnable mode): everything succeeds silently;
  handles are valid-but-inert; is_playing() returns false. The game is fully
  playable and testable with no audio device.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add miniaudio types, includes, or assumptions to this header.
- Contract frozen for stage 1 (Rule 26); changes only via group sync.
*/
/*
UPD:
- 09:08:2026 - 00:18:26: Initial stage-1 contract (playback, 3D, buses+reverb,
                         variation sets, layered music).
*/

#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <string_view>

namespace dfn::platform {

// Opaque resource handles. id == 0 means "invalid / none".
struct SoundHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct AudioVoiceHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct BusHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct MusicHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

// Listener pose for 3D spatialization, updated once per frame.
struct ListenerPose {
    glm::vec3 position{0.0f}; // meters, world space
    glm::vec3 forward{0.0f};  // unit
    glm::vec3 up{0.0f};       // unit
};

// 3D emitter parameters. Attenuation: full volume inside min_distance, rolloff
// to silence at max_distance (curve is a backend detail).
struct Spatial3d {
    glm::vec3 position{0.0f}; // meters, world space
    float min_distance = 0.0f;
    float max_distance = 0.0f;
};

struct PlayParams {
    BusHandle bus;              // invalid = master
    float volume = 1.0f;        // linear, 1 = neutral
    float pitch = 1.0f;         // playback-rate multiplier, 1 = original
    bool loop = false;
    bool spatial = false;       // false = 2D (UI, music stingers)
    Spatial3d spatial_params;   // used only when spatial == true
};

// Bus reverb driven by room volume (Q68). Computed by the engine from the
// current space; applied by the backend. wet == 0 disables.
struct ReverbParams {
    float decay_seconds = 0.0f;   // RT60-style tail length
    float room_size_meters = 0.0f;// characteristic dimension of the space
    float wet = 0.0f;             // 0..1 reverb mix
};

class IAudio {
public:
    virtual ~IAudio() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Per-frame: advances fades and updates 3D spatialization for the listener.
    virtual void update(const ListenerPose& listener) = 0;

    // Assets -------------------------------------------------------------------
    // Whether a sound is fully decoded or streamed (music) is a backend decision.
    [[nodiscard]] virtual SoundHandle load_sound(std::string_view path) = 0;
    virtual void unload_sound(SoundHandle sound) = 0;

    // Playback -----------------------------------------------------------------
    [[nodiscard]] virtual AudioVoiceHandle play(SoundHandle sound,
                                                const PlayParams& params) = 0;

    // Footstep/impact hook (Q68): backend picks one take from the set with
    // variation (rotation + slight pitch jitter). takes must be non-empty.
    [[nodiscard]] virtual AudioVoiceHandle play_variation(std::span<const SoundHandle> takes,
                                                          const PlayParams& params) = 0;

    virtual void stop(AudioVoiceHandle voice) = 0;
    virtual void set_voice_position(AudioVoiceHandle voice, const glm::vec3& position) = 0;
    virtual void set_voice_volume(AudioVoiceHandle voice, float volume) = 0;
    [[nodiscard]] virtual bool is_playing(AudioVoiceHandle voice) const = 0;

    // Buses --------------------------------------------------------------------
    // parent invalid = child of master. The engine creates its fixed bus set at
    // startup; backends impose no bus-count semantics of their own.
    [[nodiscard]] virtual BusHandle create_bus(BusHandle parent) = 0;
    virtual void set_bus_volume(BusHandle bus, float volume) = 0;
    virtual void set_bus_reverb(BusHandle bus, const ReverbParams& params) = 0;

    // Music layers (adaptive music) --------------------------------------------
    // Starts all layers sample-synchronized on the given bus; every layer begins
    // at volume 0 except layer 0 at volume 1. Layers loop together.
    [[nodiscard]] virtual MusicHandle play_music(std::span<const SoundHandle> layers,
                                                 BusHandle bus) = 0;
    virtual void set_music_layer(MusicHandle music, uint32_t layer,
                                 float target_volume, float fade_seconds) = 0;
    virtual void stop_music(MusicHandle music, float fade_seconds) = 0;
};

} // namespace dfn::platform
