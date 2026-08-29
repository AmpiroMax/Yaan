/*
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
- Per-voice low-pass (ma_lpf_node), created LAZILY — the occlusion half that
  is not volume.
- Per-bus reverb (ma_delay_node), created LAZILY — set_bus_reverb stopped
  being a no-op today.
- Voice sweep in update(): finished one-shots are uninited and their handles
  become safe no-ops (the IAudio recycling contract).

Dependencies:
- Uses: interfaces/IAudio.h, miniaudio 0.11.22 (FetchContent, pinned; the
  implementation macro lives HERE and nowhere else), stb_vorbis v1.22 from the
  same pinned checkout (compiled in sources/miniaudio/StbVorbis.cpp; see that
  file for provenance and licence).
- Used by: engine/app wiring (create_miniaudio_audio), audio tests.

Notes:
- ma_engine owns the device thread; every IAudio call happens on the sim
  thread and miniaudio's public API is safe for that split.
- PITCH JITTER in play_variation uses a backend-local RNG seeded from the
  clock. Audio randomness NEVER feeds back into simulation, so determinism
  (Rule 13.2) is untouched — this is the same ruling recorded in IAudio.h.
- ЗАКРЫТЫЙ ПРОБЕЛ v1: set_bus_reverb БОЛЬШЕ НЕ ПУСТЫШКА. Настоящего
  ревербератора у miniaudio по-прежнему нет (записка docs/reports/audio-engines:
  «Реверберация — НЕТ, 0 упоминаний»), но ЛИНИЯ ЗАДЕРЖКИ есть — ma_delay_node,
  — и одна линия с обратной связью даёт ХВОСТ. Это не сеть Шрёдера и не
  свёрточный ревер: у одиночной линии слышна периодичность, и на больших
  комнатах она отдаёт эхом. Названо вслух, потому что «реверб есть» и «комната
  звучит комнатой» — разные утверждения, и второе эта волна не заявляет.
  Зачем всё равно сделано: объявленный в контракте метод, который ничего не
  делает, — это интерфейс, который врёт, и следующий его читатель поверит.
- УЗЛЫ СОЗДАЮТСЯ ЛЕНИВО, И ЭТО КОНТРОЛЬНАЯ РУКА, А НЕ ЭКОНОМИЯ. Ни один узел
  (ни фильтр голоса, ни ревер шины) не появляется в графе, пока его не
  попросили: прогон, где окклюзия и реверб не включались, гонит те же самые
  сэмплы через тот же самый граф, что и до этой волны. «Стало иначе» и «стало
  так же» предъявляются одним бинарником (правило 30).
- Attenuation model: linear between min_distance and max_distance — the
  simple rolloff the interface promises ("curve is a backend detail").
- unload_sound stops any voices still playing that sound before dropping the
  cached buffer: a voice outliving its buffer would read freed memory, and
  "your one-shot dies when you unload its sound" is the least surprising rule.
- OGG VORBIS: miniaudio decodes WAV/MP3/FLAC by itself and carries a full
  ma_stbvorbis data source that only switches on when stb_vorbis has been
  included BEFORE the implementation — which the include order below does.
  Nothing else about this file changes: .ogg becomes a format load_sound
  simply opens. The decoder's own TU is sources/miniaudio/StbVorbis.cpp, and
  its licence and provenance are written there.
- MUSIC IS DECODED WHOLE, ON PURPOSE AND FOR NOW (owner's order, relayed
  through the music session; the
  streaming path is defect #1 of docs/reports/music-research.html §8 and is a
  contract change, so it waits for a group sync). The main theme is 1:36, which
  is ~37 MB of f32 stereo at the device rate — the price of a single decoded
  track, paid once, against a music path that would have to exist in two
  versions today. Four four-minute LAYERS would be ~370 MB and that is exactly
  when streaming stops being optional.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- miniaudio types must never leak out of this directory (Rule 1).
- Contract semantics live in IAudio.h; change them only via group sync.
*/

#include "engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h"

// ORDER IS THE FEATURE, NOT A STYLE CHOICE. miniaudio enables its Vorbis data
// source on `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H`, which stb_vorbis defines
// when it is included — so this include MUST precede MINIAUDIO_IMPLEMENTATION.
// Move it below and .ogg stops loading, silently and with no compile error.
// HEADER_ONLY: the implementation half is compiled once, in StbVorbis.cpp.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c" // NOLINT — a .c include IS the documented stb usage

#define MA_NO_GENERATION // no waveform/noise generators; we decode files only
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

// AND THE ORDER IS CHECKED, not trusted. miniaudio defines MA_HAS_VORBIS only
// if it saw stb_vorbis first; without this line the failure mode is a game that
// builds, runs, and has no music, which is the kind of defect that costs a day.
#ifndef MA_HAS_VORBIS
#error "stb_vorbis must be included BEFORE MINIAUDIO_IMPLEMENTATION, or .ogg will not load"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
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

// ПОРЯДОК ФИЛЬТРА ОККЛЮЗИИ. 2 — это 12 дБ на октаву: слышно как «за стеной»,
// а не как «под одеялом» (order 1 почти не меняет тембр, order 4 съедает и то,
// по чему ухо узнаёт источник).
constexpr ma_uint32 LOWPASS_ORDER = 2;

// ЗВУК В ВОЗДУХЕ, м/с — им длина комнаты превращается в задержку эха.
constexpr double SPEED_OF_SOUND_MPS = 343.0;
// Границы линии задержки: короче 20 мс отдаёт гребёнкой (звучит как фильтр, а
// не как комната), длиннее 200 мс распадается на отдельные шлепки эха.
constexpr double REVERB_DELAY_MIN_S = 0.020;
constexpr double REVERB_DELAY_MAX_S = 0.200;
// Предел обратной связи: 0.95 — это уже почти самовозбуждение, выше нельзя.
constexpr float REVERB_FEEDBACK_MAX = 0.95f;

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
            destroy_voice(*voice);
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
        for (auto& [id, b] : buses_) {
            ma_sound_group_uninit(b->group.get());
            if (b->reverb) {
                ma_delay_node_uninit(b->reverb.get(), nullptr);
                b->reverb.reset();
            }
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
                destroy_voice(*it->second);
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
                destroy_voice(*vit->second);
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
        destroy_voice(*it->second);
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

    // СРЕЗ ВЕРХА У ОДНОГО ГОЛОСА (окклюзия). Узел заводится ЛЕНИВО и снимается
    // при cutoff <= 0, поэтому голос, которому никто не резал верх, идёт по
    // графу, не изменившемуся ни на узел (контрольная рука в заметках файла).
    void set_voice_lowpass(AudioVoiceHandle voice, float cutoff_hz) override {
        auto it = voices_.find(voice.id);
        if (it == voices_.end() || !engine_) {
            return; // stale handle: safe no-op (contract)
        }
        Voice& v = *it->second;
        const ma_uint32 rate = ma_engine_get_sample_rate(engine_.get());
        const ma_uint32 channels = ma_engine_get_channels(engine_.get());
        if (cutoff_hz <= 0.0f) {
            if (v.lpf) {
                // Обратно на шину, и только потом уничтожать узел.
                ma_node_attach_output_bus(reinterpret_cast<ma_node*>(v.sound.get()), 0,
                                          bus_node_or_endpoint(v.bus), 0);
                ma_lpf_node_uninit(v.lpf.get(), nullptr);
                v.lpf.reset();
            }
            v.cutoff_hz = 0.0f;
            return;
        }
        // Найквист — не совет: срез выше него молча превращает фильтр в шум.
        const double cutoff = static_cast<double>(
            std::clamp(cutoff_hz, 30.0f, 0.45f * static_cast<float>(rate)));
        if (!v.lpf) {
            auto node = std::make_unique<ma_lpf_node>();
            const ma_lpf_node_config cfg =
                ma_lpf_node_config_init(channels, rate, cutoff, LOWPASS_ORDER);
            if (ma_lpf_node_init(ma_engine_get_node_graph(engine_.get()), &cfg, nullptr,
                                 node.get())
                != MA_SUCCESS) {
                return; // без узла: остаётся одна громкость, и это честнее тишины
            }
            // Порядок втыкания: сначала выход фильтра на шину, потом голос на
            // фильтр. Наоборот — и один буфер уходит в никуда.
            ma_node_attach_output_bus(reinterpret_cast<ma_node*>(node.get()), 0,
                                      bus_node_or_endpoint(v.bus), 0);
            ma_node_attach_output_bus(reinterpret_cast<ma_node*>(v.sound.get()), 0,
                                      reinterpret_cast<ma_node*>(node.get()), 0);
            v.lpf = std::move(node);
            v.cutoff_hz = static_cast<float>(cutoff);
            return;
        }
        // Пересборка коэффициентов стоит денег, а зовут это каждый кадр:
        // мимо уха проходит всё, что меняет срез меньше чем на десятую.
        if (std::fabs(cutoff - static_cast<double>(v.cutoff_hz))
            <= 0.10 * static_cast<double>(v.cutoff_hz)) {
            return;
        }
        const ma_lpf_config lc =
            ma_lpf_config_init(ma_format_f32, channels, rate, cutoff, LOWPASS_ORDER);
        if (ma_lpf_node_reinit(&lc, v.lpf.get()) == MA_SUCCESS) {
            v.cutoff_hz = static_cast<float>(cutoff);
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
        auto rec = std::make_unique<Bus>();
        rec->group = std::move(group);
        rec->parent = parent;
        buses_.emplace(handle.id, std::move(rec));
        return handle;
    }

    void set_bus_volume(BusHandle bus, float volume) override {
        if (ma_sound_group* g = find_bus(bus)) {
            ma_sound_group_set_volume(g, volume);
        }
    }

    // РЕВЕР ШИНЫ ОДНОЙ ЛИНИЕЙ ЗАДЕРЖКИ. Узел встаёт МЕЖДУ шиной и её
    // родителем, поэтому «мокрым» становится всё, что играет на шине, и ничего
    // сверх неё — ровно то, что обещает контракт («реверб — свойство шины, а
    // не работа на голос»).
    //
    // ДВА ЧИСЛА ИЗ ФИЗИКИ, А НЕ ИЗ ВКУСА: задержка — время, за которое звук
    // пересекает комнату (room_size / 343 м/с), обратная связь — та, при
    // которой хвост падает на 60 дБ ровно за decay_seconds, то есть
    // g = 10^(-3·delay/RT60). Комната 5 м с RT60 = 1.2 с даёт 20 мс и 0.89.
    void set_bus_reverb(BusHandle bus, const ReverbParams& params) override {
        const auto it = buses_.find(bus.id);
        if (it == buses_.end() || !engine_) {
            return; // мастер-шина ревера не носит: некуда воткнуть узел
        }
        Bus& b = *it->second;
        if (params.wet <= 0.0f) {
            if (b.reverb) {
                ma_node_attach_output_bus(reinterpret_cast<ma_node*>(b.group.get()), 0,
                                          bus_node_or_endpoint(b.parent), 0);
                ma_delay_node_uninit(b.reverb.get(), nullptr);
                b.reverb.reset();
            }
            return; // ВЫКЛЮЧЕНО = УЗЛА НЕТ В ГРАФЕ (контрольная рука)
        }
        const ma_uint32 rate = ma_engine_get_sample_rate(engine_.get());
        const ma_uint32 channels = ma_engine_get_channels(engine_.get());
        const double delay_s =
            std::clamp(static_cast<double>(std::max(0.0f, params.room_size_meters))
                           / SPEED_OF_SOUND_MPS,
                       REVERB_DELAY_MIN_S, REVERB_DELAY_MAX_S);
        const float feedback =
            params.decay_seconds > 0.0f
                ? std::clamp(static_cast<float>(
                                 std::pow(10.0, -3.0 * delay_s
                                                    / static_cast<double>(params.decay_seconds))),
                             0.0f, REVERB_FEEDBACK_MAX)
                : 0.0f;
        const ma_uint32 frames =
            static_cast<ma_uint32>(delay_s * static_cast<double>(rate));
        if (!b.reverb) {
            auto node = std::make_unique<ma_delay_node>();
            const ma_delay_node_config cfg =
                ma_delay_node_config_init(channels, rate, frames, feedback);
            if (ma_delay_node_init(ma_engine_get_node_graph(engine_.get()), &cfg, nullptr,
                                   node.get())
                != MA_SUCCESS) {
                return; // без ревера, но и без вранья: сухая шина как прежде
            }
            ma_node_attach_output_bus(reinterpret_cast<ma_node*>(node.get()), 0,
                                      bus_node_or_endpoint(b.parent), 0);
            ma_node_attach_output_bus(reinterpret_cast<ma_node*>(b.group.get()), 0,
                                      reinterpret_cast<ma_node*>(node.get()), 0);
            b.reverb = std::move(node);
        }
        // ДЛИНА ЛИНИИ У ma_delay_node НЕИЗМЕНЯЕМА (буфер выделен при init), и
        // это ограничение названо вслух: смена РАЗМЕРА комнаты на живой шине
        // меняет только хвост и баланс, а не время пробега. Комната меняется
        // на переходе, где ревер и так снимается в ноль, поэтому цена нулевая.
        ma_delay_node_set_decay(b.reverb.get(), feedback);
        ma_delay_node_set_wet(b.reverb.get(), std::clamp(params.wet, 0.0f, 1.0f));
        ma_delay_node_set_dry(b.reverb.get(), 1.0f);
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
            // 2D, ALWAYS. miniaudio spatializes by default, so without this a
            // music layer is an emitter standing at the world origin: panned,
            // attenuated, and quieter the further the player walks from (0,0,0).
            // The contract has said "music is 2D" since day one; the backend
            // simply never said it to miniaudio.
            ma_sound_set_spatialization_enabled(s.get(), MA_FALSE);
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
        BusHandle bus;      // куда голос воткнут, когда фильтра нет
        std::unique_ptr<ma_lpf_node> lpf; // null = фильтра нет В ГРАФЕ
        float cutoff_hz = 0.0f;           // 0 = прозрачно
    };
    // ШИНА — ЭТО ГРУППА ПЛЮС (может быть) ЕЁ РЕВЕР. Родитель хранится потому,
    // что снятие ревера обязано вернуть группу ровно туда, откуда её увели, а
    // miniaudio обратной ссылки «чей я ребёнок» не держит.
    struct Bus {
        std::unique_ptr<ma_sound_group> group;
        std::unique_ptr<ma_delay_node> reverb; // null = ревера нет В ГРАФЕ
        BusHandle parent;
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
        return it == buses_.end() ? nullptr : it->second->group.get(); // null = master
    }

    // КУДА ВТЫКАТЬСЯ, если шины нет: конечная точка графа (мастер).
    [[nodiscard]] ma_node* bus_node_or_endpoint(BusHandle bus) {
        if (ma_sound_group* g = find_bus(bus)) {
            return reinterpret_cast<ma_node*>(g);
        }
        return ma_node_graph_get_endpoint(ma_engine_get_node_graph(engine_.get()));
    }

    // СНЯТИЕ ГОЛОСА ЦЕЛИКОМ. Порядок обязателен: сначала звук (он висит на
    // фильтре), потом фильтр — иначе узел уничтожается под живым входом.
    void destroy_voice(Voice& v) {
        ma_sound_uninit(v.sound.get());
        if (v.lpf) {
            ma_lpf_node_uninit(v.lpf.get(), nullptr);
            v.lpf.reset();
        }
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
        voice->bus = params.bus; // куда возвращать голос, когда снимут фильтр
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
    std::unordered_map<uint32_t, std::unique_ptr<Bus>> buses_;
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
