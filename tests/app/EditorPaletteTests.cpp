/*
Created: 17:08:2026 - 19:13:38
Last updated: 17:08:2026 - 19:41:13
Module: tests/app
File: tests/app/EditorPaletteTests.cpp

Responsibility:
- Holds the object menu's model to the promises a screenshot cannot check: that
  the name is read rather than guessed, that a chip count means what it says,
  that favourites belong to a map, and that the size the filter offers agrees
  with the size the mesh actually has.

Dependencies:
- Uses: engine/editor/sources/EditorPalette, engine/render (ObjectRegistry, for the
  shelf-wide reconciliation), doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY ASSERTION HERE SHIPS WITH THE CASE IT REJECTS (Rule 30). The shelf-wide
  arms are the ones to watch: a run with no baked shelf skips, and a skipped arm
  proves nothing, so each of them also carries a synthetic control that runs
  everywhere.
- IF THE NAME/MESH ARM GOES RED, DO NOT WIDEN THE BOUND AND DO NOT EDIT THE
  PARSER TO MATCH. Report the name, the number from the name and the number from
  the mesh: that disagreement is a finding about the KIT and belongs to the
  houses zone.
*/
/*
UPD:
- 17:08:2026 - 19:13:38: Создан — грамматика имени, фасетные счётчики, память по карте, сверка имени с мешем.
- 17:08:2026 - 19:22:54: Переезд в engine/editor. ARCHITECTURE.md разрешает Dear ImGui
  ТОЛЬКО в engine/editor, а слой editor не имеет права включать engine/app
  (LAYERS в tools/dag_check.py) — значит панель и её модель обязаны жить
  по одну сторону, и эта сторона — editor. Ни строки логики не тронуто.
- 17:08:2026 - 19:37:50: рукав на index_of. Держит не только ответ, но и ПРЕДПОСЫЛКУ двоичного
  поиска — что полка отсортирована по имени; иначе поиск начнёт тихо промахиваться,
  а не тихо тормозить. Контроль: отсутствующее имя даёт part_count(), а не 0.
- 17:08:2026 - 19:41:13: два рукава на ЦЕЛОЕ. Первый нажимает КАЖДУЮ фишку настоящей полки
  (52 на 2411) и сверяет обещанный счёт с полученным — это единственное число
  в меню, которое человек не может проверить сам. Второй проходит фразу
  пользователя целиком: открыл, набрал, нажал фишку, взял, отметил, ушёл на
  другую карту, вернулся, перезапустил — деталь в руке.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/BuildTool.h"
#include "engine/editor/sources/EditorPalette.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace dfn;
using namespace dfn::app;

namespace {

namespace fs = std::filesystem;

constexpr const char* SHELF = "assets/objects/parts";

/// The shelf's names, or an empty vector when nothing has been baked yet.
std::vector<std::string> shelf_names() {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(SHELF, ec)) {
        return out;
    }
    for (const auto& e : fs::directory_iterator(SHELF, ec)) {
        if (e.path().extension() != ".dfo") {
            continue;
        }
        std::string n = e.path().stem().string();
        if (n.size() > 4 && n.compare(n.size() - 4, 4, "-far") == 0) {
            continue;
        }
        out.push_back(std::move(n));
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// A small hand-made shelf. Every arm that does not need the real kit runs on
/// this one, so the suite discriminates on a machine that has never baked.
std::vector<std::string> toy_shelf() {
    return {
        "wall-log-timber-12x1x13-blind-w03",
        "wall-log-timber-12x1x13-door-w08",
        "wall-ashlar-stone-16x1x11-win1-w05",
        "joint-timber-d50-n4-h13-w03",
        "joint-stone-d75-n8-h13-cap-w05",
        "joint-timber-d35-nr-h11-w08",
        "beam-dark-4x1x1-w03",
        "stair-steep-timber-1x4x13-w03",
    };
}

} // namespace

// ---------------------------------------------------------------------------
// The grammar
// ---------------------------------------------------------------------------

TEST_CASE("the name is read, and a name outside the grammar is refused out loud") {
    // WHAT THE KIT SPELLS INTO A NAME (HOUSES.md §3.3): material, wear, working
    // sizes and — for a connector — the facet count that decides which angles
    // the next wall may leave at.
    const PartFacets j = parse_part_name("joint-stone-d75-n8-h13-cap-w05");
    REQUIRE(j.parsed);
    CHECK(j.family == "joint");
    CHECK(j.material == "stone");
    CHECK(j.style.empty());
    CHECK(j.diameter_m == doctest::Approx(0.75f));
    CHECK(j.height_m == doctest::Approx(3.25f));
    CHECK(j.faces == 8);
    CHECK(j.wear_pct == 50);
    REQUIRE(j.tags.size() == 1);
    CHECK(j.tags[0] == "cap");
    CHECK(j.span_m == doctest::Approx(3.25f));

    // A STYLED WALL CARRIES TWO WORDS BEFORE ITS SIZES, and the material is the
    // second one. Nothing in the parser knows what "ashlar" or "stone" mean —
    // it knows only which token is a size.
    const PartFacets w = parse_part_name("wall-ashlar-stone-16x1x11-win1-w05");
    REQUIRE(w.parsed);
    CHECK(w.style == "ashlar");
    CHECK(w.material == "stone");
    CHECK(w.box_u[0] == 16);
    CHECK(w.box_u[2] == 11);
    CHECK(w.span_m == doctest::Approx(4.0f));
    REQUIRE(w.tags.size() == 1);
    CHECK(w.tags[0] == "win1");
    CHECK(w.faces == -1); // a panel has no facet count, and -1 is not 0

    // ROUND IS NOT "NO FACETS". `nr` means any angle is legal; a missing token
    // means the part is not a connector at all. Collapsing the two would offer
    // the builder a free angle on a wall.
    CHECK(parse_part_name("joint-timber-d35-nr-h11-w08").faces == 0);

    // THE REFUSALS. Each differs from a legal name by ONE thing.
    CHECK_FALSE(parse_part_name("").parsed);
    CHECK_FALSE(parse_part_name("wall").parsed);
    // no material: the size follows the family directly
    CHECK_FALSE(parse_part_name("wall-12x1x13-w03").parsed);
    // no size token at all
    CHECK_FALSE(parse_part_name("wall-log-timber-blind").parsed);
    // no wear: every part the forge makes carries one
    CHECK_FALSE(parse_part_name("wall-log-timber-12x1x13-blind").parsed);
    // ...and the same name WITH wear is accepted, which is what makes the four
    // refusals above measurements rather than descriptions.
    CHECK(parse_part_name("wall-log-timber-12x1x13-blind-w03").parsed);
}

TEST_CASE("a stair's triple is not a box, and the parser refuses to read it as one") {
    // THE FORGE PINS THE FIRST NUMBER AT 1 for both pitches and lets the pair
    // carry (width, STEPS) — PartForgeCatalogue.cpp says so in as many words.
    // Read as a box, a 13-step flight would report a 0.25 m span, and the size
    // filter would file the largest part in the kit among the smallest.
    const PartFacets s = parse_part_name("stair-steep-timber-1x4x13-w03");
    REQUIRE(s.parsed);
    CHECK(s.family == "stair");
    CHECK(s.style == "steep");
    CHECK(s.material == "timber");
    CHECK(s.steps == 13);
    CHECK(s.span_m == doctest::Approx(0.0f)); // unstated, not zero-sized
    CHECK(s.box_u[0] == 0);

    // THE CONTROL: the same triple on a family whose triple IS a box keeps its
    // span. Without this the arm above would also pass on a parser that simply
    // never reads a box.
    const PartFacets b = parse_part_name("beam-dark-4x1x1-w03");
    REQUIRE(b.parsed);
    CHECK(b.span_m == doctest::Approx(1.0f));
    CHECK(b.steps == 0);
}

TEST_CASE("the kit's unit and the build grid are the same number") {
    // Rule 35: two zones must agree on this, so it is pinned by a test rather
    // than by a comment. The names are measured in the grid the parts seat on;
    // if these ever drift apart, every size in the menu is wrong by their ratio.
    CHECK(KIT_UNIT_M == doctest::Approx(BUILD_GRID_M));
}

TEST_CASE("every name on the shelf fits the grammar") {
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка деталей не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);
    CHECK(m.part_count() == names.size());
    // A KIT CHANGE THAT RENAMES A FAMILY GOES RED HERE rather than putting rows
    // with invented facets in front of the builder.
    if (m.unparsed_count() != 0) {
        for (std::size_t i = 0; i < m.part_count(); ++i) {
            if (!m.part(i).parsed) {
                MESSAGE("не разобрано: " << m.part(i).name);
            }
        }
    }
    CHECK(m.unparsed_count() == 0);
    MESSAGE("полка: " << names.size() << " деталей, семейств "
                      << m.facet_values(FacetKind::Family).size() << ", материалов "
                      << m.facet_values(FacetKind::Material).size());
}

// ---------------------------------------------------------------------------
// The name against the mesh
// ---------------------------------------------------------------------------

TEST_CASE("the name never promises more size than the mesh delivers") {
    // TWO DERIVATIONS OF ONE QUANTITY. The size FACET is read from the name
    // (it must answer over the whole shelf on every keystroke); the tooltip
    // shows render::measure_object. This arm is what keeps them one truth.
    //
    // The bound is DIRECTIONAL and that is the whole point. A mesh MAY exceed
    // its name — an eave overhangs, a doorframe's jambs stand outside the
    // opening they are named for — and clamping that would be a rule about
    // architecture. A mesh may NOT fall short: a part smaller than its name is
    // a gap in a wall, and the gap is invisible until somebody walks through it.
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка деталей не испечена — рукав пропущен");
        return;
    }
    // 0.05 m sits above the largest honest shortfall on today's shelf (the log
    // corner at h14 reaches 3.471 of its named 3.50 — the course rhythm is
    // 0.23, so 14 courses do not land exactly on 14 units) and far below the
    // control below, which misses by 2 m.
    constexpr float SHORTFALL_M = 0.05f;
    std::size_t checked = 0;
    std::size_t worst_at = 0;
    float worst = 0.0f;
    std::string worst_name;
    for (const std::string& n : names) {
        const PartFacets f = parse_part_name(n);
        if (!f.parsed || f.span_m <= 0.0f) {
            continue; // stairs state no span; nothing to reconcile
        }
        const auto obj = render::read_object(fs::path(SHELF) / (n + ".dfo"));
        if (!obj) {
            continue;
        }
        const render::ObjectExtent x = render::measure_object(*obj);
        const float measured = std::max({x.hi.x - x.lo.x, x.hi.y - x.lo.y, x.top - x.bottom});
        ++checked;
        if (f.span_m - measured > worst) {
            worst = f.span_m - measured;
            worst_name = n;
            worst_at = checked;
        }
    }
    REQUIRE(checked > 0);
    if (worst > SHORTFALL_M) {
        MESSAGE("ИМЯ ОБЕЩАЕТ БОЛЬШЕ МЕША: " << worst_name << " — из имени "
                                            << parse_part_name(worst_name).span_m << " м, с меша "
                                            << (parse_part_name(worst_name).span_m - worst)
                                            << " м (недобор " << worst << " м). Это находка о "
                                               "НАБОРЕ: не расширяй допуск и не правь разборщик.");
    }
    CHECK(worst <= SHORTFALL_M);
    MESSAGE("сверено деталей: " << checked << ", худший недобор " << worst << " м на "
                                << (worst_name.empty() ? "-" : worst_name) << " (строка "
                                << worst_at << ")");

    // THE CONTROL, and it runs on the same real meshes: give a part the name of
    // a longer one from its own family and the arm must reject it. Without this
    // the bound above is a floor nothing was ever measured against (Rule 45).
    const auto small = render::read_object(fs::path(SHELF) /
                                           "wall-log-timber-8x1x13-blind-w03.dfo");
    if (small) {
        const render::ObjectExtent x = render::measure_object(*small);
        const float measured =
            std::max({x.hi.x - x.lo.x, x.hi.y - x.lo.y, x.top - x.bottom});
        const float lying = parse_part_name("wall-log-timber-16x1x13-blind-w03").span_m;
        CHECK(lying - measured > SHORTFALL_M);
    }
}

// ---------------------------------------------------------------------------
// Search and facets
// ---------------------------------------------------------------------------

TEST_CASE("search narrows as you type, and the words are ANDed") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    CHECK(m.result_count() == 8);

    m.set_search("joint");
    CHECK(m.result_count() == 3);

    // TWO WORDS MEAN BOTH, NOT THE PAIR. "joint stone" finds the stone post
    // even though its name never contains that string; a substring search would
    // find nothing here, and a builder who types two words and gets an empty
    // list concludes the search is broken.
    m.set_search("joint stone");
    REQUIRE(m.result_count() == 1);
    CHECK(m.part(m.results()[0]).name == "joint-stone-d75-n8-h13-cap-w05");

    // THE CONTROL FOR "ANDed": the same two words in the other order give the
    // same one row, and adding a third word that no part has empties it.
    m.set_search("stone joint");
    CHECK(m.result_count() == 1);
    m.set_search("stone joint thatch");
    CHECK(m.result_count() == 0);
    CHECK(m.empty_result()); // a shelf that exists and a query that matched none

    m.set_search("");
    CHECK(m.result_count() == 8);
    CHECK_FALSE(m.empty_result());
}

TEST_CASE("an empty shelf and an empty result are different states") {
    // A menu that shows nothing must be able to say WHICH nothing. Told apart
    // wrongly, the builder either waits for a shelf that is already there or
    // hunts for a filter he never set.
    PaletteModel empty;
    CHECK(empty.result_count() == 0);
    CHECK_FALSE(empty.empty_result());

    PaletteModel full;
    full.set_parts(toy_shelf());
    full.set_search("нетакого");
    CHECK(full.result_count() == 0);
    CHECK(full.empty_result());
}

TEST_CASE("a chip's count says what clicking it would leave") {
    PaletteModel m;
    m.set_parts(toy_shelf());

    const auto count_of = [&m](FacetKind k, const char* v) {
        for (const FacetValue& f : m.facet_values(k)) {
            if (f.value == v) {
                return f.count;
            }
        }
        return static_cast<std::size_t>(0);
    };

    CHECK(count_of(FacetKind::Family, "wall") == 3);
    CHECK(count_of(FacetKind::Material, "timber") == 5); // both walls, both timber posts, the flight

    // A CHIP OF THE SAME KIND MUST NOT SHRINK ITS SIBLINGS. Selecting "wall"
    // and then reading "joint" as 0 would tell the builder that clicking joint
    // gives him nothing, when it gives him three — the count is computed with
    // its OWN kind's selection left out for exactly this reason.
    m.toggle_facet(FacetKind::Family, "wall");
    CHECK(m.result_count() == 3);
    CHECK(count_of(FacetKind::Family, "joint") == 3);

    // A chip of ANOTHER kind does narrow it, and that is the counterfactual
    // that proves the skip above is not simply "counts ignore the query".
    m.toggle_facet(FacetKind::Material, "timber");
    CHECK(m.result_count() == 2);
    CHECK(count_of(FacetKind::Family, "joint") == 2);
    CHECK(count_of(FacetKind::Family, "beam") == 0); // a dead end, honestly zero

    m.clear_facets();
    CHECK(m.result_count() == 8);
    CHECK_FALSE(m.any_facet_on());
}

TEST_CASE("the facet vocabulary comes from the shelf and holds nothing the shelf lacks") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    const std::vector<FacetValue>& mats = m.facet_values(FacetKind::Material);
    CHECK(mats.size() == 3); // timber, stone, dark, and nothing else
    CHECK(std::none_of(mats.begin(), mats.end(),
                       [](const FacetValue& f) { return f.value == "clay"; }));

    // THE CONTROL: bake one clay part into the shelf and the chip appears with
    // no code change. That is the property a hand-written material list cannot
    // have, and it is the reason there is no such list.
    std::vector<std::string> more = toy_shelf();
    more.push_back("wall-framex-clay-12x1x11-win2-w05");
    m.set_parts(more);
    const std::vector<FacetValue>& mats2 = m.facet_values(FacetKind::Material);
    CHECK(mats2.size() == 4);
    CHECK(std::any_of(mats2.begin(), mats2.end(),
                      [](const FacetValue& f) { return f.value == "clay"; }));
}

TEST_CASE("size sorting puts the unstated last in BOTH directions") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    m.set_sort(PaletteSort::Size);
    REQUIRE(m.result_count() == 8);
    CHECK(m.part(m.results().front()).name == "beam-dark-4x1x1-w03"); // 1.0 m
    CHECK(m.part(m.results().back()).family == "stair");              // span unstated

    m.set_sort(PaletteSort::SizeDesc);
    CHECK(m.part(m.results().front()).span_m == doctest::Approx(4.0f));
    // THE ARM THAT MATTERS: flipping the direction must NOT float the unknown
    // to the top. "Unknown" sorted as a number is how a stair ends up filed
    // among the screws, whichever way the arrow points.
    CHECK(m.part(m.results().back()).family == "stair");
}

// ---------------------------------------------------------------------------
// What the builder keeps
// ---------------------------------------------------------------------------

TEST_CASE("favourites and recents belong to a map, not to the editor") {
    // The user builds a town on one map and a flora stand on another; the ten
    // parts that matter are not the same ten, and one shared list is in both
    // builders' way.
    PaletteModel m;
    m.set_parts(toy_shelf());

    m.set_map_id("houses/demo");
    m.toggle_favourite("joint-timber-d50-n4-h13-w03");
    m.select("wall-log-timber-12x1x13-blind-w03");
    CHECK(m.is_favourite("joint-timber-d50-n4-h13-w03"));
    CHECK(m.recents().size() == 1);

    m.set_map_id("trees/glade");
    // ONE THING CHANGED — the map — and the kept lists changed with it.
    CHECK_FALSE(m.is_favourite("joint-timber-d50-n4-h13-w03"));
    CHECK(m.recents().empty());
    CHECK(m.selected().empty());

    m.set_map_id("houses/demo");
    CHECK(m.is_favourite("joint-timber-d50-n4-h13-w03"));
    CHECK(m.selected() == "wall-log-timber-12x1x13-blind-w03");
}

TEST_CASE("recents are most-recent-first, unique, and capped") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    m.set_map_id("houses/demo");
    m.select("beam-dark-4x1x1-w03");
    m.select("joint-timber-d50-n4-h13-w03");
    m.select("beam-dark-4x1x1-w03"); // used again: it moves up, it does not duplicate
    REQUIRE(m.recents().size() == 2);
    CHECK(m.recents()[0] == "beam-dark-4x1x1-w03");
    CHECK(m.recents()[1] == "joint-timber-d50-n4-h13-w03");

    for (int i = 0; i < 40; ++i) {
        m.note_used("part-" + std::to_string(i) + "-x");
    }
    CHECK(m.recents().size() == PaletteModel::recents_limit());
    CHECK(m.recents().front() == "part-39-x");
}

TEST_CASE("only-favourites is a filter and stacks with the rest") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    m.set_map_id("houses/demo");
    m.toggle_favourite("joint-timber-d50-n4-h13-w03");
    m.toggle_favourite("beam-dark-4x1x1-w03");
    m.set_only_favourites(true);
    CHECK(m.result_count() == 2);
    m.set_search("joint");
    CHECK(m.result_count() == 1);
    // The control: the same search without the favourites filter finds three.
    m.set_only_favourites(false);
    CHECK(m.result_count() == 3);
}

TEST_CASE("quick slots take 1..9 and silently ignore anything else") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    m.set_map_id("houses/demo");
    m.set_quick_slot(3, "beam-dark-4x1x1-w03");
    CHECK(m.quick_slot(3) == "beam-dark-4x1x1-w03");
    CHECK(m.take_quick_slot(3));
    CHECK(m.selected() == "beam-dark-4x1x1-w03");
    CHECK_FALSE(m.take_quick_slot(4)); // empty slot picks nothing

    // A STRAY KEY MUST NOT OVERWRITE SLOT 1. Clamping an out-of-range slot is
    // the tempting one-liner and it is the bug: slot 0 and slot 10 would both
    // land on the row the builder uses most.
    m.set_quick_slot(1, "wall-log-timber-12x1x13-blind-w03");
    m.set_quick_slot(0, "joint-timber-d50-n4-h13-w03");
    m.set_quick_slot(10, "joint-timber-d50-n4-h13-w03");
    CHECK(m.quick_slot(1) == "wall-log-timber-12x1x13-blind-w03");
}

TEST_CASE("a name is found on the shelf without scanning it") {
    // The favourites and recents strips resolve a dozen names EVERY FRAME. A
    // scan would be thirty thousand string compares a frame at 2411 rows, for
    // two rows of thumbnails — so index_of binary-searches, which is only
    // correct while the shelf is sorted by name. This arm holds that invariant.
    PaletteModel m;
    m.set_parts(toy_shelf());
    for (std::size_t i = 0; i < m.part_count(); ++i) {
        CHECK(m.index_of(m.part(i).name) == i);
        if (i > 0) {
            // The precondition the binary search rests on, asserted rather than
            // assumed: set_parts sorts, and a future edit that stops sorting
            // would make index_of quietly miss instead of quietly slow.
            CHECK(m.part(i - 1).name < m.part(i).name);
        }
    }
    // THE CONTROL: a name that is not there answers part_count(), not 0 — the
    // strips draw a greyed-out row on that answer, and a 0 would silently show
    // the first part of the shelf as somebody's favourite.
    CHECK(m.index_of("nothing-of-the-sort-w03") == m.part_count());
    CHECK(m.index_of("") == m.part_count());
}

TEST_CASE("the cursor clamps at both ends and Enter takes what it points at") {
    PaletteModel m;
    m.set_parts(toy_shelf());
    m.set_search("joint");
    REQUIRE(m.result_count() == 3);
    m.move_cursor(-5);
    CHECK(m.cursor() == 0);
    m.move_cursor(99);
    // CLAMPED, NOT WRAPPED: in a 2411-row list a wrap teleports the reader to
    // the far end and he has no way to know it happened.
    CHECK(m.cursor() == 2);
    CHECK(m.take_cursor());
    CHECK(m.selected() == "joint-timber-d50-n4-h13-w03");

    // The cursor must survive the list shrinking under it.
    m.set_search("нетакого");
    CHECK(m.result_count() == 0);
    CHECK_FALSE(m.take_cursor());
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

TEST_CASE("the kept state survives a restart, and one map does not import another's") {
    PaletteModel a;
    a.set_parts(toy_shelf());
    a.set_map_id("houses/demo");
    a.toggle_favourite("joint-timber-d50-n4-h13-w03");
    a.set_quick_slot(2, "beam-dark-4x1x1-w03");
    a.select("wall-log-timber-12x1x13-blind-w03");
    a.set_map_id("trees/glade");
    a.toggle_favourite("beam-dark-4x1x1-w03");
    a.select("beam-dark-4x1x1-w03");

    const std::string text = a.state_text();

    PaletteModel b;
    b.set_parts(toy_shelf());
    b.load_state_text(text);
    b.set_map_id("houses/demo");
    CHECK(b.selected() == "wall-log-timber-12x1x13-blind-w03");
    CHECK(b.is_favourite("joint-timber-d50-n4-h13-w03"));
    CHECK(b.quick_slot(2) == "beam-dark-4x1x1-w03");
    // THE ARM THAT WOULD GO RED ON A ONE-LIST FILE: the beam is the glade's
    // favourite and must not be the demo's.
    CHECK_FALSE(b.is_favourite("beam-dark-4x1x1-w03"));
    b.set_map_id("trees/glade");
    CHECK(b.is_favourite("beam-dark-4x1x1-w03"));
    CHECK_FALSE(b.is_favourite("joint-timber-d50-n4-h13-w03"));

    // A file from another version, or one a human edited into nonsense, must
    // leave a usable menu rather than a crash or a half-loaded one.
    PaletteModel c;
    c.set_parts(toy_shelf());
    c.load_state_text("# only a comment\nfav=orphan-before-any-map-w03\nnonsense\n");
    c.set_map_id("houses/demo");
    CHECK(c.favourites().empty());
    CHECK(c.result_count() == 8);
}

TEST_CASE("the state file round-trips through a disk") {
    const fs::path path =
        fs::temp_directory_path() / "dfn_palette_state_test.cfg";
    std::error_code ec;
    fs::remove(path, ec);

    PaletteModel a;
    a.set_parts(toy_shelf());
    a.set_map_id("houses/demo");
    a.toggle_favourite("joint-timber-d50-n4-h13-w03");
    REQUIRE(a.save_state(path.string()));

    PaletteModel b;
    b.set_parts(toy_shelf());
    CHECK(b.load_state(path.string()));
    b.set_map_id("houses/demo");
    CHECK(b.is_favourite("joint-timber-d50-n4-h13-w03"));

    // A MISSING FILE IS NOT A FAILURE — a first run has no favourites, and an
    // editor that refuses to start over that is an editor nobody starts.
    fs::remove(path, ec);
    PaletteModel c;
    CHECK_FALSE(c.load_state(path.string()));
    c.set_map_id("houses/demo");
    CHECK(c.favourites().empty());
}

// ---------------------------------------------------------------------------
// The number that decides the design
// ---------------------------------------------------------------------------

TEST_CASE("a keystroke over the whole shelf is measured, not hoped for") {
    // THE WHOLE MENU RESTS ON THIS: at 2411 rows, browsing is not an option, so
    // the search has to answer between one letter and the next. The arm reports
    // the number and fails only on a ceiling loose enough that only a change of
    // COMPLEXITY can reach it — a timing threshold tight enough to be
    // interesting is a threshold that goes red on a loaded machine.
    std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        // The synthetic shelf keeps the arm meaningful without a bake: same
        // count, same shape of name.
        for (int i = 0; i < 2411; ++i) {
            names.push_back("wall-log-timber-" + std::to_string(i % 16 + 1) + "x1x13-blind-w0" +
                            std::to_string(i % 9 + 1));
        }
    }
    PaletteModel m;
    m.set_parts(names);

    const std::string typed = "wall stone door";
    double worst_us = 0.0;
    for (std::size_t n = 1; n <= typed.size(); ++n) {
        const auto t0 = std::chrono::steady_clock::now();
        m.set_search(typed.substr(0, n));
        const std::size_t got = m.result_count();
        (void)m.facet_values(FacetKind::Material); // the chips are recomputed too
        const auto t1 = std::chrono::steady_clock::now();
        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        worst_us = std::max(worst_us, us);
        MESSAGE("«" << typed.substr(0, n) << "» -> " << got << " деталей, " << us << " мкс");
    }
    CHECK(worst_us < 50000.0);
}

// ---------------------------------------------------------------------------
// The two arms that check the whole thing rather than a piece of it
// ---------------------------------------------------------------------------

TEST_CASE("every chip's count is the number of rows clicking it actually leaves") {
    // THE CHIP COUNT IS A PROMISE, and it is the one number in this menu that a
    // human cannot check for himself: he sees "wall (768)" and either trusts it
    // or stops reading the numbers. So it is not spot-checked here — EVERY chip
    // of EVERY kind is clicked and the result counted, on the real shelf.
    //
    // This is also the arm that would catch the facet-count skip being applied
    // to the wrong kind: a count computed with its own selection left IN reads
    // as the already-shown list, which is right for the chip you just ticked
    // and wrong for all its siblings.
    std::vector<std::string> names = shelf_names();
    const bool real_shelf = !names.empty();
    if (!real_shelf) {
        names = toy_shelf();
        MESSAGE("полка не испечена — рукав идёт по игрушечной");
    }
    PaletteModel m;
    m.set_parts(names);

    std::size_t chips = 0;
    for (std::size_t k = 0; k < static_cast<std::size_t>(FacetKind::Count); ++k) {
        const FacetKind kind = static_cast<FacetKind>(k);
        // Copied, not referenced: ticking a chip recomputes the vector's counts
        // underneath us, and the values we are walking must be the ones we read.
        const std::vector<FacetValue> values = m.facet_values(kind);
        for (const FacetValue& v : values) {
            const std::size_t promised = v.count;
            m.set_facet(kind, v.value, true);
            const std::size_t got = m.result_count();
            m.set_facet(kind, v.value, false);
            ++chips;
            if (promised != got) {
                MESSAGE("фишка врёт: " << v.value << " обещала " << promised << ", дала " << got);
            }
            CHECK(promised == got);
        }
    }
    REQUIRE(chips > 0);
    MESSAGE("проверено фишек: " << chips << " на " << m.part_count() << " деталях");

    // THE CONTROL, and it is what makes the run above a test rather than a
    // tautology: a chip that is NOT ticked must not equal the current result,
    // or "the count matches" would hold for any implementation that simply
    // reported the list length. Two chips of one kind cannot both be the whole.
    const std::vector<FacetValue> fam = m.facet_values(FacetKind::Family);
    if (fam.size() >= 2) {
        CHECK(fam[0].count + fam[1].count <= m.part_count());
        CHECK(fam[0].count < m.part_count());
    }
}

TEST_CASE("the user's own sentence, start to finish") {
    // «когда я его открываю я стою на месте, а мышкой кликаю по меню и выбираю
    // блок, которым буду строить» — the pieces are each held by an arm above;
    // this one holds that they COMPOSE, which is the property that breaks when
    // two correct pieces disagree about whose job something is.
    std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        names = toy_shelf();
    }
    PaletteModel session;
    session.set_parts(names);
    session.set_map_id("houses/demo");

    // He opens it, types the word he has in his head, and narrows by clicking.
    session.set_search("wall");
    const std::size_t after_word = session.result_count();
    REQUIRE(after_word > 0);
    session.set_facet(FacetKind::Material, "timber", true);
    const std::size_t after_chip = session.result_count();
    CHECK(after_chip > 0);
    CHECK(after_chip <= after_word); // a chip narrows; it never widens

    // He picks the first one, stars it, and puts it on the quick key.
    const std::string picked = session.part(session.results().front()).name;
    session.select(picked);
    session.toggle_favourite(picked);
    session.set_quick_slot(1, picked);
    CHECK(session.selected() == picked);
    CHECK(session.recents().front() == picked);

    // He builds with it, wanders off to another map, comes back.
    session.note_used(picked);
    session.set_map_id("trees/glade");
    CHECK(session.selected().empty());
    session.set_map_id("houses/demo");
    CHECK(session.selected() == picked);

    // He quits. Tomorrow the editor starts cold, reads the file, and the part
    // is still in his hand — WITHOUT him touching the search box, which is the
    // half that would go unnoticed: a menu that remembers the favourite and
    // forgets the selection looks like it remembered.
    const std::string saved = session.state_text();
    PaletteModel tomorrow;
    tomorrow.set_parts(names);
    tomorrow.load_state_text(saved);
    tomorrow.set_map_id("houses/demo");
    CHECK(tomorrow.selected() == picked);
    CHECK(tomorrow.is_favourite(picked));
    CHECK(tomorrow.quick_slot(1) == picked);
    CHECK(tomorrow.take_quick_slot(1));
    // And the shelf came back whole: yesterday's filter is not still applied.
    CHECK(tomorrow.result_count() == tomorrow.part_count());
}
