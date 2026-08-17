/*
Created: 17:08:2026 - 17:16:17
Last updated: 17:08:2026 - 17:16:17
Module: tests
File: tests/core/SceneStairRuleTests.cpp

Responsibility:
- ЛЕСТНИЦА И ПРОЁМ (HOUSES.md §9): the five red hands the user's order named,
  each beside the green control that differs from it by ONE number, plus the
  test that pins the CALCULATOR against the JUDGE without either being allowed
  to define the other.

Key items:
- The five: (1) марш висит верхом в воздухе; (2) сплошная панель вместо панели
  с проёмом; (3) проём короче выведенного — 8u против 9u; (4) пустота в полу
  не объявлена, то есть просто не положенная панель; (5) проём правильной
  длины, сдвинутый на 1u.
- The calculator/judge pin: opening_length_m() answers 2.045 m, the grid rounds
  it UP to 9u = 2.25, and the judge — which never calls the formula — passes 9u
  and fails 8u. Two roads, one number.

Dependencies:
- Uses: engine/world (Scene, SceneStairRules), doctest.
- Used by: ctest (core_scene_stair_rules).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЕСЛИ КАЛЬКУЛЯТОР И СУДЬЯ РАЗОШЛИСЬ — ПРАВ СУДЬЯ (§9.3). Правь формулу, а не
  правило. Заход, который подгонит правило под формулу, получит зелёный отчёт
  и игрока с шишкой, и это ровно тот обмен, ради запрета которого свод пишут.
- ИМЕНА ДЕТАЛЕЙ ЗДЕСЬ — НАСТОЯЩИЕ, они есть на полке (dfn_kit --list). Красная
  рука, собранная из выдуманного имени, проверяет то, чего в мире нет.
*/
/*
UPD:
- 17:08:2026 - 17:16:17: Создан вместе с правилами StairSeat/StairHeadroom.
*/

#include "engine/world/sources/Scene.h"
#include "engine/world/sources/SceneStairRules.h"

#include <cmath>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace dfn::world;

namespace {

constexpr float GRID = 0.25f;
constexpr float STOREY = 3.25f;     ///< стена 13u (HOUSES.md §6)
constexpr float DECK_T = 0.25f;     ///< настил 1u
constexpr float SLEEPER_R = 0.25f;  ///< d50 across-flats -> r_in

/// Настоящие имена с полки: марш 45° шириной 1.0 м на 13 ступеней, настилы
/// 4 x 4 м с объявленным проёмом 1.0 м шириной и лежень 16u круглый.
constexpr const char* FLIGHT = "stair-steep-timber-1x4x13-w03";
constexpr const char* DECK_SOLID = "deck-timber-16x16x1-w03";
constexpr const char* DECK_HOLE9 = "deck-timber-16x16x1-hole2x6x9x4-w03";
constexpr const char* DECK_HOLE8 = "deck-timber-16x16x1-hole2x6x8x4-w03";
constexpr const char* DECK_SHIFT = "deck-timber-16x16x1-hole3x6x9x4-w03";
constexpr const char* SLEEPER = "sleeper-timber-d50-nr-16u-w03";

float flat_ground(void*, glm::vec2) { return 0.0f; }

bool extent_of(void*, const std::string&, float& r, float& b) {
    r = 2.0f;
    b = 0.0f;
    return true;
}

bool top_of(void*, const std::string& name, float& t) {
    if (name.rfind("deck-", 0) == 0) {
        t = DECK_T;
        return true;
    }
    if (name.rfind("stair-", 0) == 0) {
        t = 13.0f * GRID;
        return true;
    }
    t = 0.5f;
    return true;
}

bool box_of(void*, const std::string& name, glm::vec2& lo, glm::vec2& hi) {
    if (name.rfind("deck-", 0) == 0) {
        lo = {0.0f, 0.0f};
        hi = {4.0f, 4.0f};
        return true;
    }
    if (name.rfind("stair-", 0) == 0) {
        lo = {0.0f, 0.0f};
        hi = {13.0f * GRID, 1.0f};
        return true;
    }
    lo = {-0.25f, -0.25f};
    hi = {0.25f, 0.25f};
    return true;
}

SceneWorld kit_world() {
    SceneWorld w;
    w.ground_at = &flat_ground;
    w.object_extent = &extent_of;
    w.object_top = &top_of;
    w.object_box = &box_of;
    return w;
}

Placement place(const std::string& obj, glm::vec3 pos, float yaw = 0.0f) {
    Placement p;
    p.object = obj;
    p.position = pos;
    p.yaw = yaw;
    p.group = "house";
    return p;
}

/// A lying joint whose AXIS passes through (x, y, z): a sleeper beds on its
/// underside, so its origin sits r_in below its axis.
Placement joist(float x, float y, float z, float yaw = 0.0f) {
    return place(SLEEPER, {x, y - SLEEPER_R, z}, yaw);
}

int count_rule(const std::vector<SceneFinding>& fs, SceneRule r) {
    int n = 0;
    for (const auto& f : fs) {
        if (f.rule == r) {
            ++n;
        }
    }
    return n;
}

/// THE STANDARD STAIRWELL, built once so that every red hand below can differ
/// from it by exactly one thing.
///
/// The flight's foot is at (100, 0, 100) and it climbs +X at 45 degrees, 13
/// steps of 0.25 = 3.25 m, so the head lands at x = 103.25, y = 3.25.
/// The upper deck's top is that same 3.25, so its UNDERSIDE — the thing the
/// player's head meets — is at 3.00, and the panel's origin is placed there.
///
/// WHERE THE VOID HAS TO BE, and it is derived, not chosen: it must cover
/// [d0, d1] measured from the flight's foot, where d1 = 3.25 (the horizontal at
/// which the flight reaches the upper level) and d0 = d1 - L = 1.055. On the
/// 0.25 grid that is 101.00 .. 103.25 in world x, which is 9u = 2.25 long —
/// exactly what the calculator's 2.195 rounds up to. The deck's own corner
/// therefore sits at x = 100.50, because the shelf's panel declares its void
/// starting 2u in. The z is the same story: the void is 4u = 1.0 m wide and the
/// flight is 1.0 m wide, so the panel's corner at z = 98.50 puts the void over
/// z 100.0..101.0 — the flight's own width, and the player's 0.35 radius has
/// 0.15 m to spare on each side.
SceneDoc stairwell(const char* deck, float deck_dx = 0.5f, bool head_joist = true) {
    SceneDoc doc;
    doc.world_span_m = 256.0f;
    doc.placements.push_back(place(FLIGHT, {100.0f, 0.0f, 100.0f}));
    // Пол-потолок, к которым марш и крепится (пользователь: «надо лестницы
    // крепить к пол-потолок»): лежень у подножия и лежень у верхней площадки.
    doc.placements.push_back(joist(100.0f, 0.0f, 99.0f, -1.5707963f));
    if (head_joist) {
        doc.placements.push_back(joist(103.25f, 3.25f, 99.0f, -1.5707963f));
    }
    if (deck != nullptr) {
        doc.placements.push_back(place(deck, {100.0f + deck_dx, 3.0f, 98.5f}));
    }
    return doc;
}

} // namespace

// ---------------------------------------------------------------------------
// КАЛЬКУЛЯТОР — и его сверка с судьёй.
// ---------------------------------------------------------------------------

TEST_CASE("КАЛЬКУЛЯТОР: выведенная длина проёма для марша 45, стены 13u, настила 1u") {
    // t = 1 (45 deg), thick = 0.25, H = 1.8, R = 0.35.
    const float L = opening_length_m(1.0f, DECK_T, 1.8f, 0.35f);
    CHECK(L == doctest::Approx(2.1950f).epsilon(0.001));
    // ВТОРОЙ ЧЛЕН — НЕ УКРАШЕНИЕ. Без него ответ 2.05, и это ровно та ошибка,
    // ради которой член выведен: макушка игрока — полусфера, она задевает УГОЛ
    // плиты раньше, чем ось дойдёт до края.
    const float naive = (1.8f + DECK_T) / 1.0f;
    CHECK(L - naive == doctest::Approx(0.1450f).epsilon(0.01));
    // ТОЖДЕСТВО, которым вывод проверяется сам собой: d1 - L = d0, где
    // d1 = H_верх/t — горизонталь, на которой марш доходит до верха.
    const float d0 = opening_start_m(1.0f, DECK_T, 1.8f, 0.35f, STOREY);
    CHECK(STOREY / 1.0f - L == doctest::Approx(d0).epsilon(0.001));
}

TEST_CASE("СУДЬЯ И КАЛЬКУЛЯТОР СХОДЯТСЯ, придя разными дорогами") {
    // Округление ВВЕРХ по сетке 0.25: 2.195 -> 9u = 2.25. Судья не зовёт
    // формулу ни разу (см. шапку SceneStairRules.cpp) — он ставит капсулу на
    // каждую ступень. Оба ответа — 9u, и это единственная проверка, способная
    // поймать ошибку в любом из них.
    const float L = opening_length_m(1.0f, DECK_T, 1.8f, 0.35f);
    const int units = static_cast<int>(std::ceil(L / GRID - 1e-4f));
    CHECK(units == 9);
    for (const char* deck : {DECK_HOLE8, DECK_HOLE9}) {
        const SceneDoc doc = stairwell(deck);
        const auto found = check_scene(doc, kit_world());
        CAPTURE(deck);
        const bool short_hole = deck == DECK_HOLE8;
        CHECK(count_rule(found, SceneRule::StairHeadroom) == (short_hole ? 1 : 0));
    }
}

// ---------------------------------------------------------------------------
// ПЯТЬ КРАСНЫХ РУК.
// ---------------------------------------------------------------------------

TEST_CASE("(1) марш висит верхом в воздухе") {
    // Одно отличие: есть ли лежень у верхней площадки. Лестница — ТРЕТИЙ
    // клиент горизонтальных шарниров, и висящий верх её отдельное правило:
    // проём над таким маршем может быть безупречен, а подниматься не по чему.
    for (const bool head : {false, true}) {
        const SceneDoc doc = stairwell(DECK_HOLE9, 0.5f, head);
        const auto found = check_scene(doc, kit_world());
        CAPTURE(head);
        CHECK(count_rule(found, SceneRule::StairSeat) == (head ? 0 : 1));
        // И проём при этом зелен в обоих плечах — правила разные, находки тоже.
        CHECK(count_rule(found, SceneRule::StairHeadroom) == 0);
    }
}

TEST_CASE("(2) сплошная панель вместо панели с проёмом") {
    for (const char* deck : {DECK_SOLID, DECK_HOLE9}) {
        const SceneDoc doc = stairwell(deck);
        const auto found = check_scene(doc, kit_world());
        CAPTURE(deck);
        CHECK(count_rule(found, SceneRule::StairHeadroom) == (deck == DECK_SOLID ? 1 : 0));
    }
}

TEST_CASE("(3) проём короче выведенного: 8u против 9u") {
    // 8u = 2.00 м — это НЕ небрежность, а тот же расчёт без второго члена:
    // взять рост 1.80, округлить вверх по сетке, получить 2.00. Выглядит
    // аккуратно и не хватает 19.5 см. Ровно за этим член и выведен.
    const SceneDoc doc = stairwell(DECK_HOLE8);
    const auto found = check_scene(doc, kit_world());
    REQUIRE(count_rule(found, SceneRule::StairHeadroom) == 1);
    for (const auto& f : found) {
        if (f.rule == SceneRule::StairHeadroom) {
            CHECK(f.amount_m > 0.0f);   // ЧИСЛО, а не вердикт
            CHECK(f.amount_m < 0.35f);  // и это укус капсулы, а не полный радиус
        }
    }
}

TEST_CASE("(4) пустота в полу не объявлена — просто не положенная панель") {
    // ОБА ПЛЕЧА ОСТАВЛЯЮТ НАД МАРШЕМ ВОЗДУХ. Отличие одно: в красном настил
    // СДВИНУТ так, что над лестницей его нет вовсе — дырка в полу есть, а
    // объявления нет. Без этого правила такая сцена зелена, и «здесь по
    // проекту проём» становится неотличимо от «здесь панель забыли», то есть
    // от того самого дефекта, ради которого весь свод и написан.
    for (const float dx : {6.5f, 0.5f}) {
        const SceneDoc doc = stairwell(DECK_HOLE9, dx);
        const auto found = check_scene(doc, kit_world());
        CAPTURE(dx);
        const bool gap = dx > 1.0f;
        CHECK(count_rule(found, SceneRule::StairHeadroom) == (gap ? 1 : 0));
    }
}

TEST_CASE("(5) проём ПРАВИЛЬНОЙ ДЛИНЫ, но сдвинутый на 1u") {
    // Длина сходится, покрытие нет. Правило, которое проверяет только длину,
    // прошло бы мимо: обе панели объявляют по 9u = 2.25 м пустоты, и обе
    // объявляют её честно. Разница в том, ГДЕ она.
    for (const char* deck : {DECK_SHIFT, DECK_HOLE9}) {
        const SceneDoc doc = stairwell(deck);
        const auto found = check_scene(doc, kit_world());
        CAPTURE(deck);
        CHECK(count_rule(found, SceneRule::StairHeadroom) == (deck == DECK_SHIFT ? 1 : 0));
    }
}

TEST_CASE("describe names the stair rules for the report") {
    SceneFinding f;
    f.object = FLIGHT;
    f.detail = "the head of the flight is on no lying joint";
    f.rule = SceneRule::StairSeat;
    CHECK(describe(f).find("stair-seat") != std::string::npos);
    f.rule = SceneRule::StairHeadroom;
    CHECK(describe(f).find("stair-headroom") != std::string::npos);
}
