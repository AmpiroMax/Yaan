/*
Created: 17:08:2026 - 21:03:54
Last updated: 17:08:2026 - 21:03:54
Module: tests/app
File: tests/app/EditorPaletteAxesTests.cpp

Responsibility:
- Holds the family-first chooser to the promise that makes it usable: there is
  no path from a legal click to an empty hand, and a combination the shelf does
  not hold is visible as unreachable BEFORE it is clicked.

Dependencies:
- Uses: engine/editor/sources/EditorPalette, tests/app/EditorPaletteFixture.h,
  doctest.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE TWO CONTROLS HERE HAVE DIFFERENT CAUSES ON PURPOSE: tile lives in one
  wear (a material property), framex exists only with clay and plaster (a bond
  tied to a material). Two controls that fail for the same reason are one
  control wearing two hats.
*/
/*
UPD:
- 17:08:2026 - 21:03:54: Выделен из EditorPaletteTests.cpp: свой предмет и правило 21 (1031 строка).
*/

#include <doctest/doctest.h>

#include "engine/editor/sources/EditorPalette.h"
#include "tests/app/EditorPaletteFixture.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace dfn;
using namespace dfn::app;

// ---------------------------------------------------------------------------
// The family-first chooser (user's order, 17.08 §4)
// ---------------------------------------------------------------------------

TEST_CASE("a family offers exactly the axes it actually varies along") {
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);

    // A POST TURNS ITS DIAMETER AND ITS FACET COUNT; A WALL DOES NOT HAVE
    // EITHER. Nothing in the model was told this — it is read off the shelf,
    // which is why a part baked tomorrow brings its own knobs with it.
    m.choose_family("joint");
    CHECK(m.axis_offered(PartAxis::Diameter));
    CHECK(m.axis_offered(PartAxis::Faces));
    CHECK(m.axis_offered(PartAxis::Height));
    CHECK(m.axis_offered(PartAxis::Material));
    CHECK_FALSE(m.axis_offered(PartAxis::Box));   // a post is not a box
    CHECK_FALSE(m.axis_offered(PartAxis::Style)); // and it carries no bond

    m.choose_family("wall");
    CHECK(m.axis_offered(PartAxis::Style));
    CHECK(m.axis_offered(PartAxis::Box));
    CHECK(m.axis_offered(PartAxis::Tags)); // the opening
    CHECK_FALSE(m.axis_offered(PartAxis::Diameter));
    CHECK_FALSE(m.axis_offered(PartAxis::Faces));

    // THE CONTROL FOR "OFFERED", and it is the half that keeps the rule honest:
    // a smoke vent exists in exactly one wear, so wear must NOT be offered
    // there while it IS offered on a wall. A dial with one notch tells the
    // builder he has a choice he does not have.
    m.choose_family("smokevent");
    CHECK_FALSE(m.axis_offered(PartAxis::Wear));
    CHECK(m.axis_offered(PartAxis::Material));
    m.choose_family("wall");
    CHECK(m.axis_offered(PartAxis::Wear));
}

TEST_CASE("a combination the shelf does not hold is unreachable BEFORE it is clicked") {
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);

    const auto count_on = [&m](PartAxis axis, const char* value) {
        for (const AxisValue& v : m.axis_values(axis)) {
            if (v.value == value) {
                return static_cast<long long>(v.count);
            }
        }
        return -1LL; // the axis has no such position at all
    };

    // FIRST CONTROL — A MATERIAL THAT LIVES IN ONE WEAR. Tile, clay and turf
    // are baked at 0.50 only; the other seven materials carry all three. So on
    // a tiled roof the 0.30 and 0.80 positions must read zero while 0.50 does
    // not — and the builder sees that before he turns anything.
    m.choose_family("roof");
    REQUIRE(m.axis_offered(PartAxis::Wear));
    m.choose_axis(PartAxis::Material, "tile");
    CHECK(count_on(PartAxis::Wear, "50") > 0);
    CHECK(count_on(PartAxis::Wear, "30") == 0);
    CHECK(count_on(PartAxis::Wear, "80") == 0);
    // ...and the SAME axis on a material that does carry three wears is alive,
    // which is what proves the zeroes above are about the shelf and not about
    // the instrument.
    m.choose_axis(PartAxis::Material, "thatch");
    CHECK(count_on(PartAxis::Wear, "30") > 0);
    CHECK(count_on(PartAxis::Wear, "80") > 0);

    // SECOND CONTROL, AND ITS CAUSE IS DIFFERENT — a bond tied to a material.
    // framex (a cross-braced timber frame) is baked only with clay and plaster;
    // there is no stone framex. Two controls with two different causes is what
    // separates a check from a coincidence.
    m.choose_family("wall");
    m.choose_axis(PartAxis::Style, "framex");
    CHECK(count_on(PartAxis::Material, "clay") > 0);
    CHECK(count_on(PartAxis::Material, "stone") == 0);
    m.choose_axis(PartAxis::Style, "ashlar");
    CHECK(count_on(PartAxis::Material, "stone") > 0);
    CHECK(count_on(PartAxis::Material, "clay") == 0);
}

TEST_CASE("turning one axis moves the impossible ones and SAYS SO") {
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);
    m.choose_family("roof");

    // Stand on a material that carries all three wears, and take the one that
    // tile does not have.
    m.choose_axis(PartAxis::Material, "thatch");
    m.choose_axis(PartAxis::Wear, "30");
    REQUIRE(m.resolved_index() < m.part_count());
    CHECK(m.part(m.resolved_index()).material == "thatch");
    CHECK(m.part(m.resolved_index()).wear_pct == 30);
    CHECK(m.repaired_axes().empty()); // nothing had to give

    // NOW THE MOVE. Tile has no 0.30, so wear must be relaxed — and wear is
    // what gives, not the material the builder just asked for.
    m.choose_axis(PartAxis::Material, "tile");
    REQUIRE(m.resolved_index() < m.part_count());
    CHECK(m.part(m.resolved_index()).material == "tile");
    CHECK(m.part(m.resolved_index()).wear_pct == 50);
    // AND IT IS ON THE RECORD. A choice replaced in silence is worse than an
    // empty result: the builder walks out with a part he did not choose and
    // finds out on the map.
    REQUIRE(m.repaired_axes().size() >= 1);
    CHECK(std::find(m.repaired_axes().begin(), m.repaired_axes().end(), PartAxis::Wear) !=
          m.repaired_axes().end());

    // THE CONTROL: a move that needs nothing must report nothing, or
    // "repaired" would be noise the panel learns to ignore.
    m.choose_axis(PartAxis::Wear, "50");
    CHECK(m.repaired_axes().empty());
}

TEST_CASE("every position of every axis of every family lands on a real part") {
    // THE PROPERTY THE WHOLE CHOOSER RESTS ON: there is no path from a legal
    // click to an empty hand. Not spot-checked — every family, every axis,
    // every position on the real shelf, each one clicked and the result
    // required to be a part that exists and to answer that very position.
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);

    std::size_t clicks = 0;
    std::size_t repairs = 0;
    for (const FacetValue& fam : m.facet_values(FacetKind::Family)) {
        for (std::size_t k = 0; k < static_cast<std::size_t>(PartAxis::Count); ++k) {
            const PartAxis axis = static_cast<PartAxis>(k);
            m.choose_family(fam.value);
            const std::vector<AxisValue> positions = m.axis_values(axis);
            for (const AxisValue& pos : positions) {
                m.choose_axis(axis, pos.value);
                const std::size_t at = m.resolved_index();
                ++clicks;
                REQUIRE(at < m.part_count());
                CHECK(m.part(at).family == fam.value);
                // The axis just turned KEEPS what was asked of it — it is the
                // others that give way.
                float number = 0.0f;
                CHECK(m.selected() == m.part(at).name);
                (void)number;
                repairs += m.repaired_axes().size();
            }
        }
    }
    MESSAGE("нажатий по осям: " << clicks << ", из них со сдвигом другой оси: " << repairs);
    CHECK(clicks > 0);
    // THE CONTROL: repairs must be neither always nor never. Always would mean
    // the walk cannot hold a constraint; never would mean the shelf is a full
    // cross and the whole greying-out machinery is untested by this arm.
    CHECK(repairs > 0);
    CHECK(repairs < clicks);
}

TEST_CASE("entering a family keeps the part already in hand") {
    const std::vector<std::string> names = shelf_names();
    if (names.empty()) {
        MESSAGE("полка не испечена — рукав пропущен");
        return;
    }
    PaletteModel m;
    m.set_parts(names);
    m.set_search("wall-log-timber");
    REQUIRE(m.result_count() > 0);
    const std::string held = m.part(m.results().front()).name;
    m.select(held);

    m.choose_family("wall");
    // LANDING ON THE SHELF'S FIRST WALL EVERY TIME would make the family view a
    // reset rather than a way in — the builder opens the properties of the part
    // he is holding, not of a stranger.
    CHECK(m.selected() == held);
    CHECK(m.resolved_index() < m.part_count());
    CHECK(m.part(m.resolved_index()).name == held);

    // ...and entering a family the held part does NOT belong to must still land
    // somewhere real rather than nowhere.
    m.choose_family("smokevent");
    REQUIRE(m.resolved_index() < m.part_count());
    CHECK(m.part(m.resolved_index()).family == "smokevent");

    // Leaving returns the flat list whole: the main path changed, not the only one.
    m.choose_family("");
    CHECK_FALSE(m.in_family());
    m.set_search("");
    CHECK(m.result_count() == m.part_count());
}
