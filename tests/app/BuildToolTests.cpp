/*
Module: tests/app
File: tests/app/BuildToolTests.cpp

Responsibility:
- Holds the build hand to its one promise: the colour the builder sees is the
  JUDGE'S answer about HIS ghost, and nobody else's.

Dependencies:
- Uses: engine/app/sources/BuildTool, engine/world (Scene), doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE ARMS DIFFER BY ONE THING. A test where the allowed and the refused case
  differ in two ways proves neither (Rule 30).
*/

#include <doctest/doctest.h>

#include "engine/app/sources/BuildTool.h"

using namespace dfn;
using namespace dfn::app;

TEST_CASE("the ghost is coloured by ITS OWN findings, not the scene's") {
    // THE DEFECT THIS GUARDS AGAINST is the one that makes a build tool
    // unusable rather than merely wrong: a composition with an old problem
    // three houses away paints every new ghost red, and the builder can place
    // nothing until somebody cleans a map he did not come to clean.
    std::vector<world::SceneFinding> findings;
    world::SceneFinding other;
    other.rule = world::SceneRule::OffPath;
    other.placement_index = 7; // somebody else's problem
    other.object = "tree-oak";
    findings.push_back(other);

    const BuildVerdict clean = verdict_from_findings(findings, /*candidate=*/12);
    CHECK(clean.allowed);
    CHECK(clean.reason.empty());

    // ONE THING CHANGES: the finding now names the ghost.
    findings[0].placement_index = 12;
    const BuildVerdict red = verdict_from_findings(findings, 12);
    CHECK_FALSE(red.allowed);
    CHECK(red.reason == "build.no.path");
}

TEST_CASE("every rule the judge can raise has a sentence for the builder") {
    // A verdict with no reason is a red square with no explanation, which
    // teaches the builder to move the mouse until it goes green — the exact
    // opposite of what the colour is for. Walking the enum here means a rule
    // added to the judge without a sentence fails in CI, not on his screen.
        // THE BOUND IS THE LAST VALUE, NOT A NAMED ONE. Written as RoofSeat, this
    // loop stopped one short the day the judge grew StairSeat and StairHeadroom
    // — and an enum walk that ends before the end is exactly the instrument
    // that cannot fail. The two new rules fell through to "build.no.other" and
    // the ghost went red WITHOUT A REASON on every stair, which is the failure
    // this case exists to prevent. Found by the palette agent, not by this test.
    constexpr uint8_t LAST = static_cast<uint8_t>(world::SceneRule::StairHeadroom);
    for (uint8_t r = 0; r <= LAST; ++r) {
        std::vector<world::SceneFinding> f(1);
        f[0].rule = static_cast<world::SceneRule>(r);
        f[0].placement_index = 0;
        const BuildVerdict v = verdict_from_findings(f, 0);
        CHECK_FALSE(v.allowed);
        REQUIRE_FALSE(v.reason.empty());
        // Not the fallback: every rule must be named, not swept into "other".
        CHECK(v.reason != "build.no.other");
    }
}

TEST_CASE("the grid catches x and z and leaves the ground alone") {
    const glm::vec3 snapped = snap_to_grid({12.34f, 25.51f, -7.60f});
    CHECK(snapped.x == doctest::Approx(12.25f));
    CHECK(snapped.z == doctest::Approx(-7.50f));
    // HEIGHT IS NOT SNAPPED, and this is the arm that says so out loud: a part
    // pushed to the nearest 25 cm would hover or sink on sloping ground, and
    // the judge would then call the builder's own tool wrong.
    CHECK(snapped.y == doctest::Approx(25.51f));
}

TEST_CASE("the palette comes from the shelf, and a missing shelf is empty, not a crash") {
    // A shelf that is not there yet (first run, before the bake) must give an
    // empty menu rather than take the editor down with it.
    const std::vector<BuildGroup> none = build_palette("assets/objects/does-not-exist");
    CHECK(none.empty());

    const std::vector<BuildGroup> parts = build_palette("assets/objects/parts");
    if (parts.empty()) {
        MESSAGE("полка деталей не испечена — пропускаю рукав каталога");
        return;
    }
    // Grouped by the family the kit spells into the name, and the far LOD form
    // is not a second thing to place.
    bool saw_wall = false;
    for (const BuildGroup& g : parts) {
        CHECK_FALSE(g.names.empty());
        if (g.title == "wall") {
            saw_wall = true;
        }
        for (const std::string& n : g.names) {
            CHECK(n.find("-far") == std::string::npos);
            CHECK(n.rfind(g.title, 0) == 0); // the family really is the prefix
        }
    }
    CHECK(saw_wall);
}

// ============================ ШТАБЕЛИРОВАНИЕ =================================
// «сейчас я могу ставить блок строительные только на землю, из-за проверок
// постановки на пол я не могу ставить объекты на другие объекты» — user, 18.08.
//
// РАЗБОР, И ОН ВАЖНЕЕ САМОГО ТЕСТА: судья это разрешает. OnGround (Scene.cpp)
// ищет среди расстановок ту, чей верх подходит к низу ставимой детали, и любое
// пересечение следов считает опорой. Отказывал ИНСТРУМЕНТ: призрак садился на
// грунт БЕЗУСЛОВНО, то есть внутрь того, во что целились, и судья честно
// отвечал «buried in». Ниже — обе половины: опора выбирается верно, и судья
// принимает результат.
namespace {

struct StackCtx {
    float ground_m = 0.0f;
    float top_m = 1.0f;    ///< how tall the test object is above its origin
    float radius_m = 1.0f; ///< its footprint radius
};

float stack_ground(void* ctx, glm::vec2) {
    return static_cast<StackCtx*>(ctx)->ground_m;
}
bool stack_extent(void* ctx, const std::string&, float& radius, float& bottom) {
    radius = static_cast<StackCtx*>(ctx)->radius_m;
    bottom = 0.0f;
    return true;
}
bool stack_top(void* ctx, const std::string&, float& top) {
    top = static_cast<StackCtx*>(ctx)->top_m;
    return true;
}

world::SceneWorld stack_world(StackCtx& ctx) {
    world::SceneWorld w;
    w.ground_at = &stack_ground;
    w.object_extent = &stack_extent;
    w.object_top = &stack_top;
    w.ctx = &ctx;
    return w;
}

} // namespace

TEST_CASE("деталь садится на ВЕРХ той, в которую целятся, а мимо — на землю") {
    StackCtx ctx;
    ctx.ground_m = 0.0f;
    ctx.top_m = 1.0f;
    ctx.radius_m = 1.0f;
    const world::SceneWorld w = stack_world(ctx);

    world::SceneDoc doc;
    world::Placement first;
    first.object = "beam";
    first.position = {0.0f, 0.0f, 0.0f};
    first.group = "house";
    doc.placements.push_back(first);

    // РУКА ЦЕЛИТСЯ В ВЕРХ ПЕРВОЙ ДЕТАЛИ: луч останавливается на её крышке, то
    // есть на высоте 1 м над её началом.
    std::string on;
    const float support = place_support_y(doc, glm::vec3{0.2f, 1.0f, 0.1f},
                                          ctx.ground_m, w, &on);
    CHECK(support == doctest::Approx(1.0f));
    CHECK(on == "beam");
    MESSAGE("прицел в верх детали: опора " << support << " м (" << on << ")");

    // КОНТРОЛЬ, БЕЗ КОТОРОГО УТВЕРЖДЕНИЕ ПУСТОЕ: тот же прицел МИМО детали
    // садится на землю. Земля здесь 0 и верх детали 1 — числа разные, поэтому
    // «сел на землю» и «сел на деталь» различимы.
    std::string on_miss;
    const float miss = place_support_y(doc, glm::vec3{5.0f, 0.0f, 5.0f}, ctx.ground_m,
                                       w, &on_miss);
    CHECK(miss == doctest::Approx(0.0f));
    CHECK(on_miss == "the ground");
    MESSAGE("прицел мимо: опора " << miss << " м (" << on_miss << ")");

    // И ВТОРОЙ КОНТРОЛЬ — ПОТОЛОК ПРИЦЕЛА: целясь в ЗЕМЛЮ рядом со стеной (в
    // пределах её следа, но НИЖЕ её верха), деталь не должна улетать на крышу.
    std::string on_low;
    const float low = place_support_y(doc, glm::vec3{0.2f, 0.0f, 0.1f}, ctx.ground_m,
                                      w, &on_low);
    CHECK(low == doctest::Approx(0.0f));
    CHECK(on_low == "the ground");

    // ТЕПЕРЬ СУДЬЯ. Вторая деталь встаёт на верх первой — обе в одной постройке,
    // как их и ставит рука (build_place: p.group = build_group_name_).
    world::Placement second;
    second.object = "beam";
    second.position = {0.2f, support, 0.1f};
    second.group = "house";
    doc.placements.push_back(second);
    std::size_t stacked_findings = 0;
    for (const world::SceneFinding& f : world::check_scene(doc, w)) {
        ++stacked_findings;
        MESSAGE("судья о штабеле: правило " << static_cast<int>(f.rule) << " "
                << f.object << " " << f.detail);
    }
    CHECK(stacked_findings == 0);

    // КОНТРОЛЬ, ОТЛИЧАЮЩИЙСЯ РОВНО ОДНИМ: та же деталь на том же месте, но БЕЗ
    // ПОСТРОЙКИ. Судья отказывает — и это его правило, а не отказ инструмента
    // (см. следующий рукав). Без этого плеча «судья принял» проходило бы и на
    // судье, который принимает вообще всё.
    doc.placements.back().group.clear();
    std::size_t lone_findings = 0;
    for (const world::SceneFinding& f : world::check_scene(doc, w)) {
        ++lone_findings;
    }
    CHECK(lone_findings > 0);
    MESSAGE("контроль (та же деталь без постройки): находок " << lone_findings);
}

TEST_CASE("без группы судья штабель НЕ принимает — и это его правило, не наше") {
    // НАЙДЕНО ЭТИМ РУКАВОМ И СКАЗАНО ВСЛУХ: OnGround ищет опору только СРЕДИ
    // ЧЛЕНОВ ОДНОЙ ПОСТРОЙКИ (Scene.cpp: `if (!p.group.empty())` и
    // `doc.placements[j].group != p.group`). То есть штабель из двух одиночек
    // судья отвергнет, сколько бы инструмент ни целился правильно. Рука ставит
    // детали с группой, поэтому обычный путь работает; но если пользователь
    // строит БЕЗ постройки, отказ придёт — и придёт от судьи.
    StackCtx ctx;
    const world::SceneWorld w = stack_world(ctx);
    world::SceneDoc doc;
    world::Placement first;
    first.object = "beam";
    first.position = {0.0f, 0.0f, 0.0f};
    doc.placements.push_back(first);
    world::Placement second;
    second.object = "beam";
    second.position = {0.0f, 1.0f, 0.0f}; // на верху первой, но БЕЗ группы
    doc.placements.push_back(second);

    std::size_t hovering = 0;
    for (const world::SceneFinding& f : world::check_scene(doc, w)) {
        if (f.rule == world::SceneRule::OnGround) {
            ++hovering;
        }
    }
    CHECK(hovering == 1);
    MESSAGE("без группы: находок OnGround " << hovering
            << " — правило судьи, а не отказ инструмента");
}
