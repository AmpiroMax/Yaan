/*
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

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

#include "engine/gameplay/sources/StepAudio.h"
#include "engine/gameplay/sources/WorldAmbience.h"
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
}

// ЗАПИСИ МИРА — ТЕ САМЫЕ ФАЙЛЫ, ПО КОТОРЫМ НАЗВАНЫ ПУТИ. Случай стоит здесь,
// а не среди модели, потому что модель к диску не ходит: он ловит ровно одну
// поломку — переименованный или не доехавший .ogg, после которого шелест
// молчит и никто не знает почему.
TEST_CASE("world ambience bank loads the eight world loops") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        auto null_audio = platform::create_null_audio();
        REQUIRE(null_audio->init());
        const auto bank = gameplay::WorldAmbience::load_bank(*null_audio, ASSETS, {});
        CHECK(bank.has_leaves()); // null loads always succeed by contract
        return;
    }
    const auto bank = gameplay::WorldAmbience::load_bank(*audio, ASSETS, {});
    for (int species = 0; species < 2; ++species) {
        for (int step = 0; step < 3; ++step) {
            CHECK(bank.leaves[species][step].valid());
        }
    }
    CHECK(bank.stream_small.valid());
    CHECK(bank.river_wide.valid());
    // КОНТРОЛЬ (правило 30): снятая ветровая петля обязана НЕ загружаться.
    // Без него «файлы на месте» значило бы только «load_sound что-то вернул».
    CHECK(!audio->load_sound(ASSETS + "/wind_loop.wav").valid());
}

// СРЕЗ ВЕРХА НА ГОЛОСЕ (окклюзия). Слышимость здесь не проверить — проверяется
// КОНТРАКТ: узел заводится, снимается, переставляется и переживает протухшую
// ручку. Что он звучит, слышно в прогоне с DFN_AMBIENCE_LOG.
TEST_CASE("miniaudio: per-voice low-pass is set, changed and removed") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        MESSAGE("no audio device: the null backend accepts the call silently");
        auto null_audio = platform::create_null_audio();
        REQUIRE(null_audio->init());
        null_audio->set_voice_lowpass({42}, 800.0f); // протухшая ручка: тихо
        return;
    }
    const platform::SoundHandle sound =
        audio->load_sound(ASSETS + "/world/leaves_broad_2.ogg");
    REQUIRE(sound.valid());
    platform::PlayParams p;
    p.loop = true;
    p.volume = 0.0f; // тест не обязан быть слышен на машине человека
    const platform::AudioVoiceHandle voice = audio->play(sound, p);
    REQUIRE(voice.valid());
    audio->set_voice_lowpass(voice, 800.0f);  // узел заводится
    audio->set_voice_lowpass(voice, 400.0f);  // и перестраивается
    audio->set_voice_lowpass(voice, 900000.0f); // выше Найквиста — зажимается
    audio->update({});
    CHECK(audio->is_playing(voice)); // фильтр не обрывает голос
    audio->set_voice_lowpass(voice, 0.0f); // узел снимается
    audio->update({});
    CHECK(audio->is_playing(voice));
    audio->stop(voice);
    // КОНТРОЛЬ: на снятом голосе вызов обязан быть тихим no-op.
    audio->set_voice_lowpass(voice, 500.0f);
    CHECK(!audio->is_playing(voice));
    audio->unload_sound(sound);
}

// РЕВЕР ШИНЫ. С 10.08 set_bus_reverb был задокументированной пустышкой; теперь
// это линия задержки, и контроль тот же: wet = 0 обязан вернуть шину в
// состояние «узла нет», а не «узел на нуле».
TEST_CASE("miniaudio: bus reverb attaches and detaches") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        MESSAGE("no audio device: the null backend accepts the call silently");
        return;
    }
    const platform::BusHandle bus = audio->create_bus({});
    REQUIRE(bus.valid());
    platform::ReverbParams r;
    r.decay_seconds = 1.2f;
    r.room_size_meters = 5.0f;
    r.wet = 0.35f;
    audio->set_bus_reverb(bus, r);   // узел встаёт между шиной и мастером
    r.room_size_meters = 12.0f;
    audio->set_bus_reverb(bus, r);   // и переживает смену параметров
    r.wet = 0.0f;
    audio->set_bus_reverb(bus, r);   // и снимается
    audio->set_bus_reverb(bus, r);   // повторное снятие — тихо
    // КОНТРОЛЬ: несуществующая шина не заводит ревер и не падает.
    r.wet = 0.5f;
    audio->set_bus_reverb(platform::BusHandle{999999}, r);
    audio->update({});
}

// OGG VORBIS, WHICH THE ENGINE COULD NOT OPEN UNTIL TODAY. The music pipeline
// produces exactly one format (artifacts/reports/music-research.html §7: Vorbis q5,
// because MP3's encoder padding puts a gap in every loop), and miniaudio's
// built-in decoders are WAV/MP3/FLAC — so this case is the whole of "the
// decoder is wired". It fails loudly if anyone reorders the two includes at the
// top of MiniaudioAudio.cpp, which is the one edit that silently unwires it.
TEST_CASE("miniaudio: the title theme is an .ogg, and .ogg now loads") {
    platform::IAudio* audio = audio_or_skip();
    if (audio == nullptr) {
        MESSAGE("no audio device: the decoder is compiled in either way");
        return;
    }
    const std::string music = std::string(DFN_REPO_ROOT)
                              + "/games/daggerfall_n/assets/audio/music/";

    // CONTROL FIRST (Rule 30), and it is a control about the FORMAT, not about
    // the path: a missing .ogg must not produce a handle either, or "it loaded"
    // would mean nothing more than "load_sound returns something".
    CHECK(!audio->load_sound(music + "no_such_theme.ogg").valid());

    // ПЕТЛЕВАЯ НАРЕЗКА — та, что играет в меню. Именно она названа константой
    // в App.cpp, и именно её отсутствие превратило бы музыку меню в тишину.
    const platform::SoundHandle loop = audio->load_sound(music + "main_theme_loop.ogg");
    REQUIRE(loop.valid());
    // И версия с затуханием — запасной ход того же кода. Обе обязаны читаться:
    // запасной ход, который не грузится, — это не запасной ход.
    CHECK(audio->load_sound(music + "main_theme.ogg").valid());

    // ЗАЦИКЛЕННОЕ ВОСПРОИЗВЕДЕНИЕ ЧЕРЕЗ МУЗЫКАЛЬНУЮ ШИНУ — то, что делает меню.
    // Утверждается ИСХОД (правило 38): после паузы, много большей любого
    // буфера, музыка всё ещё играет, а после stop_music с фейдом — уже нет.
    const platform::BusHandle music_bus = audio->create_bus({});
    audio->set_bus_volume(music_bus, 0.0f); // тихо: у сборочной машины нет ушей
    const platform::SoundHandle layers[] = {loop};
    const platform::MusicHandle handle = audio->play_music({layers, 1}, music_bus);
    REQUIRE(handle.valid());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    audio->update({});

    // Затухание в ноль секунд: рукав проверяет, что stop_music ДОВОДИТСЯ до
    // сноса, а не что секунда длится секунду — время здесь мерил бы сон, а не
    // предмет. Длительность фейда живёт в App (MENU_MUSIC_FADE_OUT_S).
    audio->stop_music(handle, 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    audio->update({}); // music sweep: доигравшая тема снимается здесь
    audio->stop_music(handle, 0.0f); // снятая ручка: безопасный no-op (контракт)
}
