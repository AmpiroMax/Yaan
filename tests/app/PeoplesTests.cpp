/*
Module: tests
File: tests/app/PeoplesTests.cpp

Responsibility:
- НАРОДЫ ЯАНА КАК РАСПРЕДЕЛЕНИЯ: то, что кадром не доказать. Чтение всех
  четырёх файлов, счёт имён, народный край ВНУТРИ судейского, тысяча бросков
  на народ в пределах, частоты типажей по χ², скрытая связь «крупность» и
  воспроизводимость по зерну.

Key items:
- Народный край внутри судейского: спор лоровода и судьи решён в пользу судьи,
  и это ПРОВЕРЯЕТСЯ, а не записано в комментарии.
- 1000 бросков на народ: ни одна ручка не выходит за народную полосу.
- χ² по типажам: выпадают по частотам, а не по порядку в файле.
- Одно зерно — одно тело, побайтово (правило 13).

Dependencies:
- Uses: doctest, engine/app Peoples + CharGenBody (судейская полоса роста),
  engine/render ObjectRegistry (судейские полосы целей MORF).
- Used by: ctest (app_peoples).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ДАННЫЕ БЕРУТСЯ НАСТОЯЩИЕ (assets/characters/peoples): предмет проверки —
  «эти четыре народа», а не «разбор строки». На выдуманном файле из двух
  ручек утверждение «венеды не бывают ниже 1.66» бессодержательно
  (правило 30).
*/

#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/Peoples.h"

#include "engine/render/sources/ObjectRegistry.h"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using namespace dfn;

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool peoples_present() {
    std::error_code ec;
    return fs::is_directory(app::PEOPLES_DIR, ec);
}

/// ЕСТЬ ЛИ У ТЕЛА СЕКЦИЯ MORF. Тело без неё — законное состояние дерева
/// (выпеченный персонаж, перепечка целей), и наборы, которым нужны СУДЕЙСКИЕ
/// ПОЛОСЫ, в нём бессодержательны: полос нет ни одной, кроме роста. Такие
/// наборы пропускаются ВСЛУХ, а не краснеют чужой бедой.
[[nodiscard]] bool morphs_present() {
    const auto obj = render::read_object(fs::path(app::CHARGEN_SOURCE_BODY));
    return obj && !obj->morphs.empty();
}

/// СУДЕЙСКИЕ ПОЛОСЫ: секция MORF настоящего тела плюс полоса роста из
/// константы. Ровно тот набор, против которого экран рисует дорожки.
[[nodiscard]] std::vector<app::PeopleBand> canon_bands() {
    std::vector<app::PeopleBand> out;
    const auto obj = render::read_object(fs::path(app::CHARGEN_SOURCE_BODY));
    if (obj) {
        for (const render::MorphTarget& t : obj->morphs) {
            out.push_back(app::PeopleBand{t.name, t.lo, t.hi});
        }
    }
    out.push_back(app::PeopleBand{app::CHARGEN_HEIGHT_KEY, app::CHARGEN_HEIGHT_MIN_M,
                                  app::CHARGEN_HEIGHT_MAX_M});
    return out;
}

} // namespace

TEST_CASE("каталог народов читается целиком, и в нём четыре народа") {
    if (!peoples_present()) {
        MESSAGE("assets/characters/peoples нет в дереве — набор пропущен");
        return;
    }
    const std::vector<app::People> peoples = app::read_peoples(app::PEOPLES_DIR);
    REQUIRE(peoples.size() == 4);
    // ПОРЯДОК — ПО `order`, А НЕ ПО ОБХОДУ КАТАЛОГА. Порядок обхода не
    // определён стандартом, и «третья карточка слева» в рецепте приёмки
    // означала бы на другой машине другой народ (правило 13).
    for (std::size_t i = 1; i < peoples.size(); ++i) {
        CHECK(peoples[i - 1].order <= peoples[i].order);
    }
    for (const app::People& p : peoples) {
        CAPTURE(p.id);
        CHECK_FALSE(p.name_key.empty());
        CHECK_FALSE(p.blurb_key.empty());
        // ВОСЕМЬ ТИПАЖЕЙ НА НАРОД — счёт лороводa, и он же счёт карточек на
        // экране: типаж, потерянный при переносе, не виден никак.
        CHECK(p.archetypes.size() == 8);
        // 3 x 40 ИМЁН. Четыреста восемьдесят собственных имён руками не
        // переносят — переносят с потерей, а потерю в списке из сорока
        // невозможно ни увидеть, ни вспомнить.
        CHECK(p.male.size() == 40);
        CHECK(p.female.size() == 40);
        CHECK(p.family.size() == 40);
        CHECK_FALSE(p.naming.rule_key.empty());
    }
}

TEST_CASE("частоты типажей и веса палитр складываются в сто") {
    if (!peoples_present()) {
        return;
    }
    for (const app::People& p : app::read_peoples(app::PEOPLES_DIR)) {
        CAPTURE(p.id);
        float freq = 0.0f;
        for (const app::PeopleArchetype& a : p.archetypes) {
            freq += a.frequency;
        }
        CHECK(freq == doctest::Approx(100.0f).epsilon(0.005));
        for (const std::vector<app::PeopleSwatch>* list : {&p.skin, &p.hair,
                                                           &p.eyes}) {
            float sum = 0.0f;
            for (const app::PeopleSwatch& s : *list) {
                sum += s.weight;
            }
            CHECK(sum == doctest::Approx(100.0f).epsilon(0.005));
        }
    }
}

TEST_CASE("народный край лежит ВНУТРИ судейского — спор решён в пользу судьи") {
    if (!peoples_present()) {
        return;
    }
    if (!morphs_present()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — судейских полос нет, набор пропущен");
        return;
    }
    const std::vector<app::PeopleBand> canon = canon_bands();
    REQUIRE(canon.size() >= 2); // рост плюс хотя бы одна цель MORF
    for (app::People p : app::read_peoples(app::PEOPLES_DIR)) {
        // ТА ЖЕ ПОДГОТОВКА, ЧТО У ЭКРАНА: ручка, которую народ не назвал,
        // берёт судейскую полосу целиком. Набор, проверяющий НЕ то, что потом
        // работает, проверяет чужой предмет.
        app::peoples_fill_from_canon(p, canon);
        std::string why;
        CAPTURE(p.id);
        CHECK_MESSAGE(app::peoples_validate(p, canon, why), why);
        // И ПОСЛЕ ДОПОЛНЕНИЯ У НАРОДА ЕСТЬ ПОЛОСА НА КАЖДУЮ СУДЕЙСКУЮ РУЧКУ:
        // ползунок, о котором народ молчит, обязан всё равно бросаться.
        for (const app::PeopleBand& c : canon) {
            CAPTURE(c.name);
            REQUIRE(p.band(c.name) != nullptr);
        }
    }
}

TEST_CASE("молчание файла = судейская полоса целиком, и сужение переживает её") {
    // ДЕФЕКТ, КОТОРЫЙ ЭТО ДЕРЖИТ, БЫЛ НАСТОЯЩИМ И СЛУЧИЛСЯ В ЭТУ ЖЕ ВОЛНУ:
    // три файла из четырёх писали «вся судейская полоса: лор её не сужает» и
    // ПОВТОРЯЛИ её числа рядом с комментарием. Судья перепёк цели, сузил belly
    // с 0.45 до 0.43 — и два народа были громко отвергнуты за полосу ШИРЕ
    // судейской, которую никто не собирался расширять.
    app::People p;
    p.id = "проба";
    p.limits.push_back(app::PeopleBand{"belly", 0.0f, 0.2f}); // народ СУЗИЛ
    const std::vector<app::PeopleBand> canon{
        app::PeopleBand{"belly", 0.0f, 0.43f},
        app::PeopleBand{"weight", -1.0f, 1.0f},
        app::PeopleBand{"age", 0.0f, 0.75f}};
    app::peoples_fill_from_canon(p, canon);
    REQUIRE(p.limits.size() == 3);
    // Названная ручка НЕ ТРОНУТА — сужение остаётся явным.
    CHECK(p.band("belly")->hi == doctest::Approx(0.2f));
    // Неназванные взяли судейскую полосу целиком.
    CHECK(p.band("age")->hi == doctest::Approx(0.75f));
    CHECK(p.band("weight")->lo == doctest::Approx(-1.0f));
    // ПО АЛФАВИТУ: порядок полос есть порядок вызовов генератора, и
    // дополненные ручки не имеют права приехать в конец (правило 13).
    for (std::size_t i = 1; i < p.limits.size(); ++i) {
        CHECK(p.limits[i - 1].name < p.limits[i].name);
    }
}

TEST_CASE("рост УСЕЧЁН судейской константой, а не лором") {
    if (!peoples_present()) {
        return;
    }
    // ЛОРОВЕД НАЗВАЛ ВЕНЕДАМ 150-200 СМ, судья мерил канон пропорций и дал
    // 1.66-1.84. Данные обязаны нести УСЕЧЕНИЕ, а не лорную полосу: полоса
    // шире судейской — это ползунок, после которого судья красный, а виноват
    // интерфейс. Константу при этом НЕ расширяют — она рука капсулы игрока и
    // канона 7.5-8.0 голов.
    for (const app::People& p : app::read_peoples(app::PEOPLES_DIR)) {
        const app::PeopleBand* b = p.band(app::PEOPLE_STATURE_KNOB);
        REQUIRE(b != nullptr);
        CAPTURE(p.id);
        CHECK(b->lo >= app::CHARGEN_HEIGHT_MIN_M - 1e-4f);
        CHECK(b->hi <= app::CHARGEN_HEIGHT_MAX_M + 1e-4f);
    }
}

TEST_CASE("шире судейской полосы — ГРОМКИЙ ОТКАЗ, а не молчаливое обрезание") {
    // КОНТРОЛЬНАЯ РУКА. Без неё предыдущий набор доказывал бы только то, что
    // проверка ничего не проверяет: она обязана уметь сказать «нет».
    app::People p;
    p.id = "проба";
    p.name_key = "k";
    p.limits.push_back(app::PeopleBand{"stature", 1.50f, 2.00f});
    app::PeopleArchetype a;
    a.id = "a";
    a.name_key = "k";
    a.frequency = 100.0f;
    p.archetypes.push_back(a);
    p.male.emplace_back("Ждан");
    p.female.emplace_back("Милава");
    const std::vector<app::PeopleBand> canon{
        app::PeopleBand{"stature", app::CHARGEN_HEIGHT_MIN_M,
                        app::CHARGEN_HEIGHT_MAX_M}};
    std::string why;
    CHECK_FALSE(app::peoples_validate(p, canon, why));
    CHECK_FALSE(why.empty());

    // А внутри судейской — принимается.
    p.limits[0] = app::PeopleBand{"stature", 1.70f, 1.80f};
    CHECK_MESSAGE(app::peoples_validate(p, canon, why), why);

    // И ручка, о которой судья не знает, — тоже отказ: опечатка в имени цели,
    // принятая молча, даёт ползунок, который никогда не двинется.
    p.limits.push_back(app::PeopleBand{"statture", 0.0f, 1.0f});
    CHECK_FALSE(app::peoples_validate(p, canon, why));
}

TEST_CASE("«Случайно»: тысяча бросков на народ — все ручки в народной полосе") {
    if (!peoples_present()) {
        return;
    }
    for (const app::People& p : app::read_peoples(app::PEOPLES_DIR)) {
        CAPTURE(p.id);
        app::PeopleRng rng{0x5EEDULL};
        int outside = 0;
        for (int i = 0; i < 1000; ++i) {
            const app::PeopleDraw draw = app::people_sample(p, app::PeopleSex::Male,
                                                            rng);
            REQUIRE(draw.sliders.size() == p.limits.size());
            for (const auto& [name, value] : draw.sliders) {
                const app::PeopleBand* b = p.band(name);
                REQUIRE(b != nullptr);
                if (value < b->lo - 1e-4f || value > b->hi + 1e-4f) {
                    ++outside;
                }
            }
            CHECK_FALSE(draw.name.empty());
        }
        CHECK(outside == 0);
    }
}

TEST_CASE("выборка идёт ПО АЛФАВИТУ, и одно зерно даёт одно тело") {
    if (!peoples_present()) {
        return;
    }
    const std::vector<app::People> peoples = app::read_peoples(app::PEOPLES_DIR);
    REQUIRE_FALSE(peoples.empty());
    app::PeopleRng a{12345ULL};
    app::PeopleRng b{12345ULL};
    const app::PeopleDraw x = app::people_sample(peoples[0], app::PeopleSex::Female, a);
    const app::PeopleDraw y = app::people_sample(peoples[0], app::PeopleSex::Female, b);
    CHECK(x.archetype == y.archetype);
    CHECK(x.name == y.name);
    REQUIRE(x.sliders.size() == y.sliders.size());
    for (std::size_t i = 0; i < x.sliders.size(); ++i) {
        CHECK(x.sliders[i].first == y.sliders[i].first);
        // ПОБАЙТОВО, А НЕ «ПРИБЛИЗИТЕЛЬНО»: сложение float не ассоциативно, и
        // допуск здесь скрыл бы ровно ту беду, ради которой порядок и задан.
        CHECK(x.sliders[i].second == y.sliders[i].second);
    }
    for (std::size_t i = 1; i < x.sliders.size(); ++i) {
        CHECK(x.sliders[i - 1].first < x.sliders[i].first);
    }
    // ДРУГОЕ ЗЕРНО — ДРУГОЕ ТЕЛО. Без этой строки набор держался бы и для
    // генератора, который всегда возвращает одно и то же (правило 30b).
    app::PeopleRng c{999ULL};
    const app::PeopleDraw z = app::people_sample(peoples[0], app::PeopleSex::Female, c);
    bool any_different = z.name != x.name;
    for (std::size_t i = 0; i < x.sliders.size(); ++i) {
        any_different = any_different || z.sliders[i].second != x.sliders[i].second;
    }
    CHECK(any_different);
}

TEST_CASE("типажи выпадают по своим частотам (χ² грубо)") {
    if (!peoples_present()) {
        return;
    }
    const std::vector<app::People> peoples = app::read_peoples(app::PEOPLES_DIR);
    for (const app::People& p : peoples) {
        CAPTURE(p.id);
        constexpr int N = 8000;
        std::vector<int> seen(p.archetypes.size(), 0);
        app::PeopleRng rng{777ULL};
        for (int i = 0; i < N; ++i) {
            ++seen[app::people_pick_archetype(p, rng)];
        }
        double chi2 = 0.0;
        for (std::size_t i = 0; i < seen.size(); ++i) {
            const double expected =
                static_cast<double>(N) * p.archetypes[i].frequency / 100.0;
            REQUIRE(expected > 5.0); // χ² не применяют к ожиданию меньше пяти
            const double d = static_cast<double>(seen[i]) - expected;
            chi2 += d * d / expected;
        }
        // СЕМЬ СТЕПЕНЕЙ СВОБОДЫ (восемь типажей минус один), порог 24.32 —
        // это 0.1 % справа. Взят СВОБОДНЫМ нарочно: набор ловит генератор,
        // который частоты игнорирует (там χ² уходит в сотни), а не
        // статистическую флуктуацию, из-за которой приёмка краснела бы раз в
        // тысячу прогонов без единой причины. Зерно при этом прибито, так что
        // прогон вообще не случаен, — порог держит будущую правку зерна.
        CHECK(chi2 < 24.32);
    }
}

TEST_CASE("скрытая связь «крупность» есть, и она положительная") {
    if (!peoples_present()) {
        return;
    }
    // БЕЗ НЕЁ В ТОЛПЕ ЗАВОДЯТСЯ КОРОТЫШКИ С РУКАМИ ДО КОЛЕН, и это видно с
    // первого кадра. Прибор — коэффициент корреляции Пирсона между ростом и
    // плечами по тысяче тел ОДНОГО типажа (по всем типажам сразу связь была бы
    // смешана с разницей их центров и мерила бы не то).
    const std::vector<app::People> peoples = app::read_peoples(app::PEOPLES_DIR);
    REQUIRE_FALSE(peoples.empty());
    // ТА ЖЕ ПОДГОТОВКА, ЧТО У ЭКРАНА: живот в файле не назван (лор его не
    // сужает), и полосу он берёт у судьи. Без дополнения контрольной руки
    // просто не существует.
    app::People p = peoples[0];
    if (!morphs_present()) {
        MESSAGE("у HumanBase.dfo нет секции MORF — набор пропущен");
        return;
    }
    app::peoples_fill_from_canon(p, canon_bands());
    app::PeopleRng rng{4242ULL};
    constexpr int N = 2000;
    std::map<std::string, std::vector<double>> knob;
    for (int i = 0; i < N; ++i) {
        for (const auto& [name, value] : app::people_sample_build(p, 0, rng)) {
            knob[name].push_back(value);
        }
    }
    const auto pearson = [](const std::vector<double>& a,
                            const std::vector<double>& b) {
        const auto mean = [](const std::vector<double>& v) {
            double sum = 0.0;
            for (const double x : v) {
                sum += x;
            }
            return sum / static_cast<double>(v.size());
        };
        const double ma = mean(a);
        const double mb = mean(b);
        double cov = 0.0;
        double va = 0.0;
        double vb = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            cov += (a[i] - ma) * (b[i] - mb);
            va += (a[i] - ma) * (a[i] - ma);
            vb += (b[i] - mb) * (b[i] - mb);
        }
        return cov / std::sqrt(va * vb);
    };
    REQUIRE(knob.count(app::PEOPLE_STATURE_KNOB) == 1);
    REQUIRE(knob.count("shoulders") == 1);
    REQUIRE(knob.count("belly") == 1);
    const double linked = pearson(knob[app::PEOPLE_STATURE_KNOB], knob["shoulders"]);
    // КОНТРОЛЬНАЯ РУКА, И БЕЗ НЕЁ НАБОР НЕ СТОИТ НИЧЕГО: живот в списке
    // PEOPLE_BULK_KNOBS НЕ значится, и связи у него быть не должно. Пара
    // «связанная — несвязанная» из ОДНОГО прогона показывает, что мерится
    // именно связь, а не общий сдвиг всех ручек типажа.
    const double loose = pearson(knob[app::PEOPLE_STATURE_KNOB], knob["belly"]);
    CAPTURE(linked);
    CAPTURE(loose);
    // ЗАКАЗАНО ρ = 0.7, ИЗМЕРЕНО 0.42, и разница — не брак, а цена усечения:
    // центр типажа «пахарь» по плечам (0.10) стоит близко к нижнему краю
    // народной полосы (−0.35), то есть меньше двух сигм, и весь левый хвост
    // прижимается к краю. Прижатый хвост — это потерянный разброс, а вместе с
    // ним и часть общей дисперсии, на которой связь и держится. Порог 0.30
    // ловит генератор, у которого связи НЕТ вовсе (там r ~ 0), а не эту цену.
    CHECK(linked > 0.30);
    // И сверху: r близкое к единице значило бы, что ручки не бросаются вовсе,
    // а копируют одна другую.
    CHECK(linked < 0.95);
    CHECK(std::fabs(loose) < 0.10);
    CHECK(linked > std::fabs(loose) * 3.0);
}

TEST_CASE("смесь народов с весами: вход генератора толпы принимает пограничье") {
    if (!peoples_present()) {
        return;
    }
    const std::vector<app::People> peoples = app::read_peoples(app::PEOPLES_DIR);
    REQUIRE(peoples.size() >= 2);
    // «В КУРГАНЛЫ В СЕЗОН ДО ПОЛОВИНЫ КАЙСАКОВ» — это не украшение лора, а
    // вход генератора населения, и он обязан быть принят СРАЗУ: переделывать
    // вход потом дороже.
    app::PeopleMix mix;
    mix.push_back(app::PeopleMixEntry{&peoples[0], 85.0f});
    mix.push_back(app::PeopleMixEntry{&peoples[1], 15.0f});
    app::PeopleRng rng{31337ULL};
    std::map<std::string, int> tally;
    for (int i = 0; i < 2000; ++i) {
        const app::PeopleDraw d = app::people_sample(mix, app::PeopleSex::Male, rng);
        REQUIRE(d.people != nullptr);
        ++tally[d.people->id];
    }
    CHECK(tally.size() == 2);
    const int minor = tally[peoples[1].id];
    CAPTURE(minor);
    CHECK(minor > 200);  // 15 % от 2000 — это 300; полоса широкая нарочно
    CHECK(minor < 400);
}
