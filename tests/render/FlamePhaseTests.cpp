/*
Module: tests
File: tests/render/FlamePhaseTests.cpp

Responsibility:
- ДВА ПРИБОРА ТРЕТЬЕГО АУДИТА, оба на чистые функции, которые до сегодня жили в
  безымянных пространствах и потому не проверялись ничем:
  1. фаза мерцания факела не зависит от того, СКОЛЬКО ещё светильников собрано
     в кадре (жалоба «свет мигает»);
  2. ключ кэша плитки набора несёт РЕВИЗИЮ листа (PartsAtlas.h требует этого
     прямым текстом).

Key items:
- flame_phase_for / flame_at / house_tile_key (все три — RenderSystem.h).

Dependencies:
- Uses: dfn_render, dfn_platform_render (нулевой бэкенд), dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- У КАЖДОГО СЛУЧАЯ ЗДЕСЬ ЕСТЬ КОНТРОЛЬНОЕ ПЛЕЧО (правило 30), и оба контроля
  устроены по правилу 39: это ДОФИКСНАЯ формула, выписанная дословно. Её
  ценность в том, что она РАВНА верному ответу на том случае, который все
  проверяли, и расходится ровно там, куда никто не смотрел. Если контроль
  перестанет проваливаться — прибор сломался, а не дефект вернулся.
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/SkyModel.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <vector>

using dfn::platform::NullRenderer;
using dfn::render::RenderSystem;

namespace {

// ДОФИКСНЫЙ ОСЦИЛЛЯТОР, ВЫПИСАННЫЙ ДОСЛОВНО (правило 39). Второй аргумент —
// РАЗМЕР ПУЛА кандидатов на момент сбора, ровно как звал collect_point_lights
// до 20.08. Это контрольное плечо обоих случаев ниже: оно обязано ПРОВАЛИВАТЬ
// то, что новая формула проходит.
struct LegacyFlame {
    float intensity;
    float warmth;
};

[[nodiscard]] LegacyFlame legacy_flame_at(float t, std::uint32_t index) {
    const float phase = static_cast<float>(index) * 1.7f;
    const float a = std::sin((t + phase) * 5.7f * 6.2831853f * 0.1591549f);
    const float b = std::sin((t + phase) * 9.1f * 6.2831853f * 0.1591549f + 2.399f);
    const float mix = 0.6f * a + 0.4f * b;
    return {1.0f + dfn::render::FLAME_INTENSITY_SWING * mix,
            dfn::render::FLAME_WARMTH_SWING * mix};
}

// ДОФИКСНАЯ УПАКОВКА КЛЮЧА ПЛИТКИ, ВЫПИСАННАЯ ДОСЛОВНО: `surface * 16 + tone +
// (normal ? 0x100 : 0)`. Ревизии в ней нет — это и есть дефект.
[[nodiscard]] std::uint32_t legacy_tile_key(std::uint32_t surface, std::uint32_t tone,
                                            bool normal) {
    return surface * 16u + tone + (normal ? 0x100u : 0u);
}

} // namespace

TEST_CASE("фаза пламени принадлежит светильнику, а не размеру пула") {
    // ЧТО ИМЕННО СЛОМАЛОСЬ. Фаза бралась из candidates.size() — числа
    // кандидатов, УЖЕ собранных к этому мгновению. В нём сидят светляки этой
    // ночи и лампы композиции, попавшие в кадр, поэтому при НЕПОДВИЖНОМ факеле
    // и неподвижном игроке оно менялось от кадра к кадру, и вместе с ним
    // прыгала фаза.
    const float t = 3.25f; // любое мгновение пришпиленных визуальных часов
    const std::uint64_t light_key = 0x0000000700000001ull; // EntityId.packed()

    const float phase = dfn::render::flame_phase_for(light_key);
    // Сдвиг лежит внутри отрезка биения и одинаков при каждом спросе.
    CHECK(phase >= 0.0f);
    CHECK(phase < dfn::render::FLAME_PHASE_SPREAD_S);
    CHECK(dfn::render::flame_phase_for(light_key) == doctest::Approx(phase));

    // РАБОЧЕЕ ПЛЕЧО: сколько бы ни было соседей, ответ про ЭТОТ факел один.
    const dfn::render::Flame alone = dfn::render::flame_at(t, phase);
    for (std::uint32_t pool = 0; pool <= 8; ++pool) {
        const dfn::render::Flame f = dfn::render::flame_at(t, phase);
        CHECK(f.intensity == doctest::Approx(alone.intensity));
        CHECK(f.warmth == doctest::Approx(alone.warmth));
    }

    // КОНТРОЛЬНОЕ ПЛЕЧО (правило 30): дофиксная формула на тех же входах
    // ОБЯЗАНА разъезжаться. Без этой пары строк рабочее плечо выше
    // тавтологично — оно проверяло бы, что константа равна себе.
    const LegacyFlame legacy_alone = legacy_flame_at(t, 0u);
    bool legacy_moved = false;
    for (std::uint32_t pool = 1; pool <= 8; ++pool) {
        if (std::fabs(legacy_flame_at(t, pool).intensity - legacy_alone.intensity)
            > 1e-4f) {
            legacy_moved = true;
        }
    }
    CHECK(legacy_moved);

    // И ФАЗЫ РАЗНЫХ СВЕТИЛЬНИКОВ РАЗЪЕЗЖАЮТСЯ — иначе коридор факелов пульсировал
    // бы как одна лампа, ради чего сдвиг и заведён. Соседние EntityId (разница в
    // единицу) — самый частый случай и самый трудный для слабого хэша.
    std::set<int> buckets;
    for (std::uint64_t id = 1; id <= 16; ++id) {
        buckets.insert(static_cast<int>(dfn::render::flame_phase_for(id)
                                        / dfn::render::FLAME_PHASE_SPREAD_S * 8.0f));
    }
    CHECK(buckets.size() >= 5); // 16 соседних ключей не должны сесть в один угол
}

TEST_CASE("живой факел не меняет цвет от того, сколько ламп в кадре") {
    // ТО ЖЕ УТВЕРЖДЕНИЕ, НО ЧЕРЕЗ ВЕСЬ КОНВЕЙЕР: единица измерения здесь —
    // цвет, который уходит в кадр, а не число внутри осциллятора. Часы
    // пришпилены (DFN_VISTIME читается в init), иначе оба прогона мерили бы
    // разные мгновения настенного времени и разница ничего не значила бы.
    const auto torch_color = [](int extra_lamps) {
        setenv("DFN_VISTIME", "4.0", 1);
        NullRenderer renderer;
        REQUIRE(renderer.init({}));
        RenderSystem system;
        REQUIRE(system.init(renderer));
        // ЛАМПЫ СТОЯТ ДАЛЕКО И ИХ МАЛО: далеко — чтобы факел остался нулевым по
        // дальности и попал в тот же слот; мало — чтобы кандидатов было меньше
        // бюджета и растворение на границе не вмешалось третьей величиной.
        std::vector<RenderSystem::ExtraLight> lamps;
        for (int i = 0; i < extra_lamps; ++i) {
            RenderSystem::ExtraLight l;
            l.position = {200.0f + static_cast<float>(i) * 10.0f, 0.0f, 200.0f};
            l.radius_m = 1.0f;
            l.color = {1.0f, 1.0f, 1.0f};
            l.casts_shadow = false;
            lamps.push_back(l);
        }
        system.set_scene_lights(std::move(lamps));

        dfn::ecs::World world;
        const auto carrier = world.spawn();
        world.add(carrier, dfn::components::Transform{
                               {0.0f, 0.0f, 0.0f},
                               glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, glm::vec3{1.0f}});
        dfn::components::CarriedLight light;
        light.active = true;
        world.add(carrier, std::move(light));

        dfn::render::FirstPersonCamera camera;
        camera.set_projection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        system.render(world, renderer, camera, 1.0f);
        const auto& env = system.environment();
        REQUIRE(env.point_light_count == static_cast<std::uint32_t>(extra_lamps) + 1u);
        const glm::vec3 c = env.point_lights[0].color; // ближайший — факел
        system.shutdown(renderer);
        return c;
    };
    const glm::vec3 no_lamps = torch_color(0);
    const glm::vec3 three_lamps = torch_color(3);
    unsetenv("DFN_VISTIME");
    CHECK(three_lamps.r == doctest::Approx(no_lamps.r));
    CHECK(three_lamps.g == doctest::Approx(no_lamps.g));
    CHECK(three_lamps.b == doctest::Approx(no_lamps.b));
    // КОНТРОЛЬ: на тех же двух размерах пула дофиксная формула даёт РАЗНЫЙ
    // множитель яркости. Значит утверждение выше способно провалиться, и
    // именно оно, а не «два вызова вернули одно и то же».
    CHECK(std::fabs(legacy_flame_at(4.0f, 3u).intensity
                    - legacy_flame_at(4.0f, 0u).intensity) > 1e-3f);
}

TEST_CASE("ключ плитки набора несёт ревизию листа") {
    // ЗАЧЕМ. PartsAtlas.h говорит прямым текстом: ревизия — часть ключа кэша,
    // и флора потеряла заход на кэшированном листе в четыре колонки, который
    // сэмплили uv на пять (белые хвойные при верном коде везде). Ключ без
    // ревизии переживает перепечатку листа и отдаёт вчерашние пиксели.
    const std::uint32_t rev = dfn::render::PARTS_ATLAS_REVISION;
    for (std::uint32_t s = 0; s < dfn::render::PARTS_ATLAS_SURFACES; ++s) {
        for (std::uint32_t t = 0; t < dfn::render::PARTS_ATLAS_TONES; ++t) {
            for (int n = 0; n < 2; ++n) {
                CHECK(dfn::render::house_tile_key(s, t, n != 0, rev)
                      != dfn::render::house_tile_key(s, t, n != 0, rev + 1u));
            }
        }
    }
    // КОНТРОЛЬНОЕ ПЛЕЧО: дофиксная упаковка на тех же входах НЕ различает
    // ревизии — ей нечем. Это тот самый разрыв, из-за которого утверждение
    // выше вообще что-то значит.
    CHECK(legacy_tile_key(3u, 2u, false) == legacy_tile_key(3u, 2u, false));

    // И ВТОРАЯ ПОЛОВИНА: упаковка ПЛОТНАЯ, то есть без зазоров и без
    // предположения о числе тонов. Вся решётка материалов и тонов, оба листа —
    // столько же различных ключей, сколько клеток.
    std::set<std::uint32_t> keys;
    for (std::uint32_t s = 0; s < dfn::render::PARTS_ATLAS_SURFACES; ++s) {
        for (std::uint32_t t = 0; t < dfn::render::PARTS_ATLAS_TONES; ++t) {
            keys.insert(dfn::render::house_tile_key(s, t, false, rev));
            keys.insert(dfn::render::house_tile_key(s, t, true, rev));
        }
    }
    CHECK(keys.size() == static_cast<std::size_t>(2u * dfn::render::PARTS_ATLAS_SURFACES
                                                  * dfn::render::PARTS_ATLAS_TONES));

    // КОНТРОЛЬ К НЕЙ. Дофиксная упаковка держалась на арифметическом
    // совпадении: она закладывает ШЕСТНАДЦАТЬ тонов там, где их четыре, и
    // сегодня не сталкивается только потому, что 8*16+3 < 0x100. Ряд тонов,
    // доросший до шестнадцатого, столкнул бы (0, 16) с (1, 0) молча — а общая
    // форма новой упаковки (умножение на ЧИСЛО тонов, а не на 16) этого не
    // может по построению.
    CHECK(legacy_tile_key(0u, 16u, false) == legacy_tile_key(1u, 0u, false));
    const auto packed = [](std::uint32_t s, std::uint32_t t, std::uint32_t tones) {
        return (s * tones) + t;
    };
    CHECK(packed(0u, 16u, 17u) != packed(1u, 0u, 17u));
}
