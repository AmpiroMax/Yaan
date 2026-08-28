/*
Created: 28:08:2026 - 15:10:00
Last updated: 28:08:2026 - 15:10:00
Module: tests (sim zone)
File: tests/sim/WorldAmbienceTests.cpp

Responsibility:
- ПРИЁМКА ЗВУКА ОТ ИСТОЧНИКА (заказ владельца 28.08). Меряется МОДЕЛЬ, а не
  звуковая карта: кривая громкости шелеста по расстоянию от рощи, ноль в
  чистом поле, пара «в доме против улицы», окклюзия лучом и бюджет голосов.

Key items:
- Кривая расстояния печатается числами (MESSAGE) — её и цитирует отчёт.
- У каждого утверждения контрольная рука (Rule 30): «тихо» предъявляется рядом
  с «слышно», и оба — из одной и той же функции.

Dependencies:
- Uses: doctest, gameplay WorldAmbience, платформенный НУЛЕВОЙ бэкенд звука
  (голоса нужны, устройство — нет).
- Used by: ctest (sim_world_ambience).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НЕ ЗАМЕНЯТЬ ЧИСЛА НА КОНСТАНТЫ МОДУЛЯ там, где проверяется САМО ЧИСЛО:
  тест, написанный через ту же константу, что и код, проверяет равенство
  переменной самой себе и молчит при любой правке.
*/
/*
UPD:
- 28:08:2026 - 15:10:00: Создан вместе с зоной.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <string>
#include <vector>

#include "engine/gameplay/sources/WorldAmbience.h"
#include "engine/platform/audio/sources/null/CreateNullAudio.h"

namespace {

namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;

/// Взрослое лиственное дерево: крона радиусом 4 м, верхушка на 9 м.
gameplay::CrownSource oak(float x, float z, float r = 4.0f) {
    gameplay::CrownSource c;
    c.position = {x, 6.0f, z};
    c.radius_m = r;
    c.top_m = 9.0f;
    return c;
}

/// Средний ветер карты (render::apply_wind: база 0.35).
constexpr float WIND_TYPICAL = 0.35f;

[[nodiscard]] float db(float gain) {
    return gain <= 0.0f ? -120.0f : 20.0f * std::log10(gain);
}

} // namespace

// ---------------------------------------------------------------------------
// 1. КРИВАЯ ПО РАССТОЯНИЮ И ЧИСТОЕ ПОЛЕ
// ---------------------------------------------------------------------------

TEST_CASE("шелест затихает с удалением от рощи, и в чистом поле его нет") {
    // Роща: девять дубов в одной ячейке 24 м.
    std::vector<gameplay::CrownSource> crowns;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            crowns.push_back(oak(4.0f + 6.0f * static_cast<float>(i),
                                 4.0f + 6.0f * static_cast<float>(j)));
        }
    }
    const auto clusters = gameplay::cluster_crowns(crowns);
    REQUIRE(clusters.size() == 1);
    const gameplay::AmbienceCluster& grove = clusters.front();
    CHECK(grove.crowns == 9);

    const float near_m = gameplay::cluster_near_m(grove);
    const float far_m = gameplay::cluster_far_m(grove);
    MESSAGE("роща 9 крон: полка " << near_m << " м, предел " << far_m << " м");

    std::string curve;
    float previous = 1e9f;
    for (const float d : {0.0f, 5.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 55.0f,
                          60.0f, 80.0f}) {
        const float g = gameplay::leaves_base_gain(grove, d, WIND_TYPICAL);
        curve += "  d=" + std::to_string(static_cast<int>(d)) + " м g="
                 + std::to_string(g) + " (" + std::to_string(db(g)) + " дБ)\n";
        CHECK(g <= previous); // МОНОТОННОСТЬ: дальше никогда не громче
        previous = g;
    }
    MESSAGE("кривая шелеста по расстоянию (ветер 0.35):\n" << curve);

    // ЗА ПРЕДЕЛОМ СЛЫШИМОСТИ — РОВНЫЙ НОЛЬ, а не «очень тихо».
    CHECK(gameplay::leaves_base_gain(grove, far_m, WIND_TYPICAL) == 0.0f);
    CHECK(gameplay::leaves_base_gain(grove, far_m + 50.0f, WIND_TYPICAL) == 0.0f);
    // КОНТРОЛЬ (Rule 30): у той же рощи вблизи звук ЕСТЬ, иначе «ноль вдали»
    // означал бы только «функция всегда ноль».
    CHECK(gameplay::leaves_base_gain(grove, 5.0f, WIND_TYPICAL) > 0.05f);

    // ЗАКОН 1/r ВНУТРИ ПОЛОСЫ: удвоение расстояния — примерно −6 дБ. Проверка
    // идёт вдали от окна затухания у предела, где окно вмешивается нарочно.
    const float g20 = gameplay::leaves_base_gain(grove, 20.0f, WIND_TYPICAL);
    const float g40 = gameplay::leaves_base_gain(grove, 40.0f, WIND_TYPICAL);
    CHECK(db(g20) - db(g40) > 5.5f);

    // ЧИСТОЕ ПОЛЕ: карта без деревьев не даёт ни одной рощи, и ни на каком
    // расстоянии ничего не звучит.
    const auto empty = gameplay::cluster_crowns({});
    CHECK(empty.empty());
}

TEST_CASE("трава и цветы не шелестят: крона ниже порога — не источник") {
    gameplay::CrownSource flower;
    flower.position = {10.0f, 0.2f, 10.0f};
    flower.radius_m = 0.3f;
    flower.top_m = 0.4f;
    const gameplay::CrownSource takes[] = {flower};
    CHECK(gameplay::cluster_crowns(takes).empty());
    // КОНТРОЛЬ: тот же вызов с настоящим деревом даёт рощу.
    const gameplay::CrownSource tree[] = {oak(10.0f, 10.0f)};
    CHECK(gameplay::cluster_crowns(tree).size() == 1);
}

TEST_CASE("в штиль листва молчит, на ветру звучит") {
    const gameplay::CrownSource tree[] = {oak(0.0f, 0.0f)};
    const auto cl = gameplay::cluster_crowns(tree);
    REQUIRE(cl.size() == 1);
    CHECK(gameplay::leaves_base_gain(cl.front(), 5.0f, 0.0f) == 0.0f);
    CHECK(gameplay::leaves_base_gain(cl.front(), 5.0f, WIND_TYPICAL) > 0.0f);
    // Выше полной силы ветер громкости не добавляет.
    CHECK(gameplay::leaves_base_gain(cl.front(), 5.0f, 0.60f)
          == doctest::Approx(gameplay::leaves_base_gain(cl.front(), 5.0f, 0.80f)));
}

// ---------------------------------------------------------------------------
// 2. РОЩИ: КЛАСТЕРИЗАЦИЯ И ТОЧКА ИЗЛУЧЕНИЯ
// ---------------------------------------------------------------------------

TEST_CASE("сотни деревьев складываются в десятки рощ, а не в сотни голосов") {
    std::vector<gameplay::CrownSource> crowns;
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            crowns.push_back(oak(8.0f * static_cast<float>(i),
                                 8.0f * static_cast<float>(j)));
        }
    }
    REQUIRE(crowns.size() == 256);
    const auto clusters = gameplay::cluster_crowns(crowns);
    MESSAGE("256 крон -> " << clusters.size() << " рощ");
    CHECK(clusters.size() <= 36); // сетка 128 м стороной при ячейке 24 м
    std::uint32_t total = 0;
    for (const auto& c : clusters) {
        total += c.crowns;
        CHECK(c.amplitude > 0.0f);
    }
    CHECK(total == 256); // НИ ОДНА КРОНА НЕ ПОТЕРЯНА при группировке

    // РОЩА ЗВУЧИТ БЛИЖНИМ КРАЕМ. Точка излучения обязана быть ближе центроида,
    // когда слушатель стоит у опушки.
    const gameplay::AmbienceCluster& c0 = clusters.front();
    const glm::vec3 listener{-30.0f, 1.7f, -30.0f};
    const glm::vec3 at = gameplay::cluster_emitter_point(c0, listener);
    CHECK(glm::length(at - listener) <= glm::length(c0.centroid - listener));
}

TEST_CASE("роща громче одного дерева, но не во столько раз, сколько в ней крон") {
    const gameplay::CrownSource one[] = {oak(0.0f, 0.0f)};
    std::vector<gameplay::CrownSource> nine;
    for (int i = 0; i < 9; ++i) {
        nine.push_back(oak(static_cast<float>(i) * 2.0f, 0.0f));
    }
    const auto c1 = gameplay::cluster_crowns(one);
    const auto c9 = gameplay::cluster_crowns(nine);
    REQUIRE(c1.size() == 1);
    REQUIRE(c9.size() == 1);
    // Сложение по мощности: sqrt(9) = 3 раза, а не 9.
    CHECK(c9.front().amplitude == doctest::Approx(3.0f * c1.front().amplitude));
}

// ---------------------------------------------------------------------------
// 3. ОККЛЮЗИЯ И ДОМ ПРОТИВ УЛИЦЫ
// ---------------------------------------------------------------------------

TEST_CASE("стена глушит и заваливает верх; открытое небо не трогает ни того ни другого") {
    const gameplay::AmbienceMix open = gameplay::occluded_mix(0.0f, false, 0.0f);
    const gameplay::AmbienceMix half = gameplay::occluded_mix(0.5f, false, 0.0f);
    const gameplay::AmbienceMix wall = gameplay::occluded_mix(1.0f, false, 0.0f);
    MESSAGE("окклюзия: свободно g=" << open.gain << " срез=" << open.cutoff_hz
            << " | половина g=" << half.gain << " срез=" << half.cutoff_hz
            << " | стена g=" << wall.gain << " срез=" << wall.cutoff_hz);
    CHECK(open.gain == doctest::Approx(1.0f));
    CHECK(open.cutoff_hz == 0.0f); // фильтра НЕТ В ГРАФЕ: звук как до волны
    CHECK(wall.gain == doctest::Approx(0.30f));
    CHECK(wall.cutoff_hz == doctest::Approx(800.0f));
    CHECK(half.gain < open.gain);
    CHECK(half.gain > wall.gain);
    CHECK(half.cutoff_hz > wall.cutoff_hz);
    CHECK(half.cutoff_hz < 20000.0f);
}

TEST_CASE("в доме улица глухая, у открытой двери — слышна") {
    const gameplay::CrownSource tree[] = {oak(0.0f, 0.0f)};
    const auto cl = gameplay::cluster_crowns(tree);
    REQUIRE(cl.size() == 1);
    const float base = gameplay::leaves_base_gain(cl.front(), 8.0f, WIND_TYPICAL);

    const gameplay::AmbienceMix street = gameplay::occluded_mix(0.0f, false, 0.0f);
    const gameplay::AmbienceMix room = gameplay::occluded_mix(0.0f, true, 0.0f);
    const gameplay::AmbienceMix door = gameplay::occluded_mix(0.0f, true, 1.0f);

    const float g_street = base * street.gain;
    const float g_room = base * room.gain;
    const float g_door = base * door.gain;
    MESSAGE("одно дерево в 8 м, ветер 0.35: улица g=" << g_street << " ("
            << db(g_street) << " дБ), у открытой двери g=" << g_door << " ("
            << db(g_door) << " дБ), вглубь комнаты g=" << g_room << " ("
            << db(g_room) << " дБ)");
    // Заказ дословно: «внутри дома шелест почти не слышен, у открытой
    // двери — слышен». В числах это ровно два неравенства.
    CHECK(db(g_street) - db(g_room) > 20.0f);  // комната тише улицы на 22 дБ
    CHECK(db(g_door) - db(g_room) > 10.0f);    // дверь слышнее комнаты
    CHECK(g_door < g_street);                  // но улицы не громче
    CHECK(room.cutoff_hz < door.cutoff_hz);    // и глубже в доме глуше по тембру
}

// ---------------------------------------------------------------------------
// 4. ВОДА
// ---------------------------------------------------------------------------

TEST_CASE("река звучит с ближайшей точки русла и затихает от него") {
    const std::vector<glm::vec3> bed = {{0.0f, 0.0f, 0.0f},
                                        {50.0f, 0.0f, 0.0f},
                                        {50.0f, 0.0f, 50.0f}};
    // Ближайшая точка — на ОТРЕЗКЕ, а не в вершине: слушатель напротив
    // середины первого колена.
    const glm::vec3 p = gameplay::nearest_point_on_course(bed, {25.0f, 0.0f, 9.0f});
    CHECK(p.x == doctest::Approx(25.0f));
    CHECK(p.z == doctest::Approx(0.0f));

    const float near_g = gameplay::water_base_gain(7.2f, 4.0f);
    const float far_g = gameplay::water_base_gain(7.2f, 40.0f);
    MESSAGE("река 7.2 м: в 4 м g=" << near_g << ", в 40 м g=" << far_g
            << ", предел " << gameplay::water_far_m(7.2f) << " м");
    CHECK(near_g > far_g);
    CHECK(gameplay::water_base_gain(7.2f, gameplay::water_far_m(7.2f)) == 0.0f);
    // Ручей тише реки на том же расстоянии — размер входит в модель.
    CHECK(gameplay::water_base_gain(2.0f, 10.0f)
          < gameplay::water_base_gain(7.2f, 10.0f));
    // ВОДЕ ВЕТЕР БЕЗРАЗЛИЧЕН: у water_base_gain его нет в параметрах вовсе —
    // это контроль формой сигнатуры, и он сильнее любого числа.
}

// ---------------------------------------------------------------------------
// 5. БЮДЖЕТ ГОЛОСОВ (на нулевом бэкенде: голоса настоящие, устройство не нужно)
// ---------------------------------------------------------------------------

TEST_CASE("бюджет голосов держится, сколько бы деревьев ни стояло") {
    auto audio = platform::create_null_audio();
    REQUIRE(audio->init());

    std::vector<gameplay::CrownSource> crowns;
    for (int i = 0; i < 20; ++i) {
        for (int j = 0; j < 20; ++j) {
            crowns.push_back(oak(6.0f * static_cast<float>(i),
                                 6.0f * static_cast<float>(j)));
        }
    }
    std::vector<gameplay::WaterCourse> courses;
    gameplay::WaterCourse river;
    river.width_m = 7.2f;
    river.points = {{0.0f, 0.0f, 60.0f}, {120.0f, 0.0f, 60.0f}};
    courses.push_back(river);

    gameplay::WorldAmbience amb;
    gameplay::WorldAmbience::Bank bank;
    // Ручки нулевого бэкенда действительны по контракту: голоса заведутся.
    for (int s = 0; s < 2; ++s) {
        for (int st = 0; st < 3; ++st) {
            bank.leaves[s][st] = audio->load_sound("leaves.ogg");
        }
    }
    bank.stream_small = audio->load_sound("stream.ogg");
    bank.river_wide = audio->load_sound("river.ogg");
    amb.set_bank(bank);
    amb.set_sources(*audio, crowns, courses);
    MESSAGE("400 крон -> " << amb.clusters().size() << " рощ");

    gameplay::WorldAmbience::Listener l;
    // Прогулка по карте: место в бюджете не имеет права утечь ни на шаге, ни
    // на перекличке ветровых ступеней. Ходок начинает в лесу (лес 0..114 м) и
    // выходит из него — на последних кадрах он ОБЯЗАН оказаться в тишине, и
    // проверять «звучит» на последнем кадре было бы проверкой обратного.
    std::size_t peak_voices = 0;
    std::size_t peak_emitters = 0;
    for (int frame = 0; frame < 200; ++frame) {
        l.position = {static_cast<float>(frame), 1.7f, static_cast<float>(frame)};
        const float wind = 0.1f + 0.6f * std::fabs(std::sin(frame * 0.1f));
        amb.update(*audio, l, wind, 1.0f / 60.0f, {});
        peak_voices = std::max(peak_voices, amb.live_voices());
        peak_emitters = std::max(peak_emitters, amb.emitters().size());
        CHECK(amb.live_voices() <= gameplay::AMBIENCE_MAX_VOICES);
        CHECK(amb.emitters().size() <= 8);
    }
    MESSAGE("прогулка по лесу из 400 крон: пик голосов " << peak_voices << "/"
            << gameplay::AMBIENCE_MAX_VOICES << ", пик излучателей "
            << peak_emitters);
    CHECK(peak_emitters > 0); // КОНТРОЛЬ: в лесу он всё-таки что-то играл
    // И ВЫЙДЯ ИЗ ЛЕСА (199 м от ближней кроны при пределе 110) — тишина.
    CHECK(amb.emitters().empty());

    // ХОЗЯИН ЗАМОЛК — ЗАМОЛКЛИ ВСЕ. Ни одного живого голоса после выгрузки.
    amb.silence(*audio);
    CHECK(amb.live_voices() == 0);
    CHECK(amb.emitters().empty());

    // И КАРТА БЕЗ ДЕРЕВЬЕВ МОЛЧИТ ЦЕЛИКОМ: ни рощ, ни голосов, ни излучателей.
    amb.set_sources(*audio, {}, {});
    for (int frame = 0; frame < 10; ++frame) {
        amb.update(*audio, l, WIND_TYPICAL, 1.0f / 60.0f, {});
    }
    CHECK(amb.live_voices() == 0);
    CHECK(amb.emitters().empty());
}

TEST_CASE("ветровая ступень переключается с гистерезисом") {
    // Вверх через порог 0.28 и обратно: порог сдвигается навстречу текущей
    // ступени, поэтому дыхание ветра вокруг границы не перекликает файлы.
    CHECK(gameplay::wind_step(0.10f, 0) == 0);
    CHECK(gameplay::wind_step(0.30f, 0) == 0); // порог сдвинут вверх: ещё рано
    CHECK(gameplay::wind_step(0.34f, 0) == 1); // а вот теперь настоящий подъём
    CHECK(gameplay::wind_step(0.26f, 1) == 1); // уже поднялись — держимся
    CHECK(gameplay::wind_step(0.20f, 1) == 0); // а вот это уже настоящий спад
    CHECK(gameplay::wind_step(0.70f, 1) == 2);
}

TEST_CASE("порода определяется именем объекта, и это соглашение содержимого") {
    CHECK(gameplay::species_is_conifer("spruce-forge-a"));
    CHECK(gameplay::species_is_conifer("pine-forge-a"));
    CHECK(gameplay::species_is_conifer("juniper-forge-a"));
    CHECK(!gameplay::species_is_conifer("oak-forge-a"));
    CHECK(!gameplay::species_is_conifer("birch-forge-b"));
    CHECK(!gameplay::species_is_conifer("great-forge-oak"));
}
